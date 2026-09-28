#include "schedule_clock.h"

#ifdef USE_HW211_SCHEDULE_CLOCK

#include "modbus_schedule.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstdio>

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "hw211.schedule_clock";

void Hw211ScheduleClock::setup() {
  const uint64_t interval = this->parent_->get_update_interval();
  this->freshness_ms_ = std::max<uint64_t>(30000, std::min<uint64_t>(interval * 3, 0x7FFFFFFF));
  this->retry_ms_ = std::max<uint64_t>(15000, std::min<uint64_t>(interval * 2, 0x7FFFFFFF));
  this->preference_ = global_preferences->make_preference<hw211::ScheduleClockState>(
      this->get_object_id_hash() ^ 0x48574301);
  hw211::ScheduleClockState restored;
  if (!this->preference_.load(&restored) || !this->model_.restore(restored)) {
    // First opt-in keeps the existing displayed timer numbers as local intent.
    this->model_.initialize(this->initial_offset_);
  }
  this->parent_->add_on_offline_callback([this](int, int) {
    this->model_.invalidate_readings();
    this->has_sent_ = false;
  });
  this->clock_->add_on_time_sync_callback([this]() { this->update(); });
  this->save_();
  this->publish_offset_();
}

bool Hw211ScheduleClock::get_local_offset_(int16_t &offset) const {
  if (!this->clock_->now().is_valid())
    return false;
  const int32_t seconds = ESPTime::timezone_offset();
  if (seconds % 60 != 0 || !hw211::valid_utc_offset(seconds / 60))
    return false;
  offset = seconds / 60;
  return true;
}

bool Hw211ScheduleClock::readings_fresh_() const {
  if (!this->model_.ready())
    return false;
  for (const auto updated : this->updated_) {
    if (millis() - updated > this->freshness_ms_)
      return false;
  }
  return true;
}

bool Hw211ScheduleClock::save_() {
  if (!this->model_.dirty())
    return true;
  if (!this->preference_.save(&this->model_.state()) || !global_preferences->sync()) {
    ESP_LOGW(TAG, "Cannot save timer correction; controller writes are deferred");
    return false;
  }
  this->model_.mark_saved();
  return true;
}

void Hw211ScheduleClock::publish_offset_() {
  const int value = this->model_.state().controller_offset;
  const int magnitude = value < 0 ? -value : value;
  char formatted[16];
  snprintf(formatted, sizeof(formatted), "%c%02d:%02d", value < 0 ? '-' : '+', magnitude / 60, magnitude % 60);
  if (!this->has_state() || this->state != formatted)
    this->publish_state(formatted);
}

void Hw211ScheduleClock::publish_times_() {
  if (!this->model_.ready())
    return;
  for (uint8_t i = 0; i < 4; i++)
    this->timers_[i]->publish_local_minutes(this->model_.local_time(i));
}

void Hw211ScheduleClock::observe(uint8_t index, uint16_t hour, uint16_t minute) {
  if (index >= 4 || hour > 23 || minute > 59) {
    this->model_.invalidate_readings();
    ESP_LOGW(TAG, "Invalid controller timer readback; correction is deferred");
    return;
  }
  this->updated_[index] = millis();
  const bool first_read = !this->model_.ready();
  const bool was_pending = this->model_.state().pending;
  const auto previous_raw = this->model_.raw_times();
  if (this->model_.observe(index, hour * 60 + minute)) {
    const auto &raw = this->model_.raw_times();
    if (first_read || raw != previous_raw || (was_pending && !this->model_.state().pending)) {
      ESP_LOGI(TAG, "Controller timers: %02u:%02u-%02u:%02u, %02u:%02u-%02u:%02u; applied correction %d min%s",
               raw[0] / 60, raw[0] % 60, raw[1] / 60, raw[1] % 60,
               raw[2] / 60, raw[2] % 60, raw[3] / 60, raw[3] % 60,
               this->model_.state().applied_delta, this->model_.state().pending ? " (update pending)" : "");
    }
  }
  // Never queue a write from within a Modbus read callback. The polling update
  // handles persistence and writes after the complete response has been parsed.
  this->publish_times_();
}

void Hw211ScheduleClock::update() {
  int16_t local_offset;
  if (!this->readings_fresh_() || !this->get_local_offset_(local_offset))
    return;
  this->model_.reconcile(local_offset);
  if (!this->save_())
    return;
  this->publish_offset_();
  this->publish_times_();
  this->write_pending_();
}

void Hw211ScheduleClock::write_pending_() {
  if (!this->model_.state().pending) {
    this->has_sent_ = false;
    return;
  }
  const auto target = this->model_.target_times();
  if (this->has_sent_ && target == this->last_sent_ && millis() - this->last_write_ < this->retry_ms_)
    return;
  std::array<uint16_t, 8> values{};
  for (uint8_t i = 0; i < 4; i++) {
    values[i * 2] = target[i] / 60;
    values[i * 2 + 1] = target[i] % 60;
  }
  this->last_sent_ = target;
  this->last_write_ = millis();
  this->has_sent_ = true;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  this->clear_dispatched_();
  this->clear_tx_queue_for_device();
  if (!this->write_multiple_registers(1134, values)) {
    ESP_LOGW(TAG, "Timer correction write was refused; a later retry requires fresh readings");
    return;
  }
#else
  const std::vector<uint16_t> payload(values.begin(), values.end());
  auto command = ModbusCommandItem::create_write_multiple_command(this->parent_, 1134, payload.size(), payload);
  this->parent_->queue_command(std::move(command));
#endif
  ESP_LOGD(TAG, "Writing local timers in controller time (local minus controller offset: %d min)",
           this->model_.state().target_delta);
}

void Hw211ScheduleClock::control(const std::string &value) {
  int16_t controller_offset;
  int16_t local_offset;
  if (!hw211::parse_utc_offset(value, controller_offset)) {
    ESP_LOGW(TAG, "Use a signed UTC offset from -14:00 to +14:00, for example +09:30");
    return;
  }
  if (!this->readings_fresh_() || !this->get_local_offset_(local_offset)) {
    ESP_LOGW(TAG, "Wait for synchronized local time and all timer readbacks before changing the clock offset");
    return;
  }
  this->model_.set_controller_offset(controller_offset, local_offset);
  if (!this->save_())
    return;
  this->publish_offset_();
  this->publish_times_();
  this->write_pending_();
}

void Hw211ScheduleClock::set_local_time(uint8_t index, uint8_t hour, uint8_t minute) {
  int16_t local_offset;
  if (!this->readings_fresh_() || !this->get_local_offset_(local_offset)) {
    ESP_LOGW(TAG, "Wait for synchronized local time and all timer readbacks before changing a local timer");
    return;
  }
  if (!this->model_.set_local_time(index, hour * 60 + minute, local_offset) || !this->save_())
    return;
  this->publish_times_();
  this->write_pending_();
}

void Hw211ScheduleClock::dump_config() {
  LOG_TEXT("", "HW211 controller clock UTC offset", this);
  ESP_LOGCONFIG(TAG, "  Corrects timer registers only; controller clock is never set");
}

}  // namespace modbus_controller
}  // namespace esphome

#endif
