#include "modbus_schedule.h"

#include "esphome/core/log.h"
#include "esphome/core/version.h"

#include <array>
#include <vector>

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "hw211.schedule";

void Hw211ScheduleTime::dump_config() { LOG_DATETIME_TIME("", "HW211 Timer", this); }

void Hw211ScheduleTime::parse_and_publish(Hw211ReadBuffer data) {
  if (data.size() < this->offset + 4) {
    ESP_LOGW(TAG, "Timer '%s' response is too short (%u bytes)", this->get_name().c_str(),
             static_cast<unsigned>(data.size()));
    return;
  }

  this->hour_ = hw211_get_u16(data, this->offset);
  this->minute_ = hw211_get_u16(data, this->offset + 2);
  this->second_ = 0;
  this->publish_state();
}

void Hw211ScheduleTime::control(const datetime::TimeCall &call) {
  const auto requested_hour = call.get_hour();
  const auto requested_minute = call.get_minute();
  if ((!requested_hour.has_value() || !requested_minute.has_value()) && !this->has_state()) {
    ESP_LOGW(TAG, "Cannot partially update timer '%s' before its current value has been read",
             this->get_name().c_str());
    return;
  }

  const uint8_t hour = requested_hour.value_or(this->hour_);
  const uint8_t minute = requested_minute.value_or(this->minute_);
  if (call.get_second().value_or(0) != 0) {
    ESP_LOGW(TAG, "Timer '%s' supports minute precision; seconds were ignored", this->get_name().c_str());
  }

  const std::array<uint16_t, 2> values{hour, minute};
  const uint16_t write_address = hw211_write_address(*this);

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  this->clear_dispatched_();
  this->clear_tx_queue_for_device();
  if (!this->write_multiple_registers(write_address, values)) {
    ESP_LOGW(TAG, "Modbus write for timer '%s' was refused; state not published", this->get_name().c_str());
    return;
  }
#else
  const std::vector<uint16_t> payload(values.begin(), values.end());
  auto command =
      ModbusCommandItem::create_write_multiple_command(this->parent_, write_address, payload.size(), payload);
  this->parent_->queue_command(std::move(command));
#endif

  ESP_LOGD(TAG, "Set timer '%s' to %02u:%02u", this->get_name().c_str(), hour, minute);
  this->hour_ = hour;
  this->minute_ = minute;
  this->second_ = 0;
  this->publish_state();
}

void Hw211ScheduleEnableSwitch::dump_config() {
  LOG_SWITCH(TAG, "HW211 Timer Enable", this);
  ESP_LOGCONFIG(TAG, "  Enable mask: 0x%04X", this->enable_mask_);
}

void Hw211ScheduleEnableSwitch::cache_register_value_(uint16_t value, bool publish) {
  this->register_value_ = value;
  this->has_register_value_ = true;
  if (publish) {
    this->publish_state((value & this->enable_mask_) == this->enable_mask_);
  }
}

void Hw211ScheduleEnableSwitch::parse_and_publish(Hw211ReadBuffer data) {
  if (data.size() < this->offset + 2) {
    ESP_LOGW(TAG, "Timer enable response is too short (%u bytes)", static_cast<unsigned>(data.size()));
    return;
  }

  const uint16_t value = hw211_get_u16(data, this->offset);
  const uint16_t enabled_bits = value & this->enable_mask_;
  if (enabled_bits != 0 && enabled_bits != this->enable_mask_) {
    ESP_LOGW(TAG, "Timer '%s' has only one of its start/end events enabled (register value 0x%04X)",
             this->get_name().c_str(), value);
  }
  this->cache_register_value_(value, true);
}

void Hw211ScheduleEnableSwitch::write_state(bool state) {
  if (!this->has_register_value_) {
    ESP_LOGW(TAG, "Cannot update timer '%s' before enable register 1133 has been read", this->get_name().c_str());
    return;
  }

  const uint16_t value = state ? (this->register_value_ | this->enable_mask_)
                               : (this->register_value_ & ~this->enable_mask_);
  const uint16_t write_address = hw211_write_address(*this);
  const std::array<uint16_t, 1> values{value};

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  this->clear_dispatched_();
  this->clear_tx_queue_for_device();
  if (!this->write_multiple_registers(write_address, values)) {
    ESP_LOGW(TAG, "Modbus write for timer '%s' was refused; state not published", this->get_name().c_str());
    return;
  }
#else
  const std::vector<uint16_t> payload(values.begin(), values.end());
  auto command =
      ModbusCommandItem::create_write_multiple_command(this->parent_, write_address, payload.size(), payload);
  this->parent_->queue_command(std::move(command));
#endif

  ESP_LOGD(TAG, "%s timer '%s' (enable register 0x%04X)", state ? "Enabled" : "Disabled",
           this->get_name().c_str(), value);
  this->cache_register_value_(value, true);
  if (this->peer_ != nullptr) {
    this->peer_->cache_register_value_(value, false);
  }
}

}  // namespace modbus_controller
}  // namespace esphome
