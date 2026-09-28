#pragma once

#include "esphome/core/defines.h"

#ifdef USE_HW211_SCHEDULE_CLOCK

#include "esphome/components/text/text.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/preferences.h"
#include "modbus_compat.h"
#include "schedule_clock_logic.h"

namespace esphome {
namespace modbus_controller {

class Hw211ScheduleTime;

class Hw211ScheduleClock : public text::Text, public PollingComponent, public Hw211WriterEntity {
 public:
  Hw211ScheduleClock() : PollingComponent(1000) {}
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_parent(ModbusController *parent) {
    this->parent_ = parent;
    this->hw211_set_controller(parent);
  }
  void set_time_source(time::RealTimeClock *clock) { this->clock_ = clock; }
  void set_initial_offset(int16_t minutes) { this->initial_offset_ = minutes; }
  void set_timer(uint8_t index, Hw211ScheduleTime *timer) { this->timers_[index] = timer; }
  void observe(uint8_t index, uint16_t hour, uint16_t minute);
  void set_local_time(uint8_t index, uint8_t hour, uint8_t minute);

 protected:
  void control(const std::string &value) override;
  bool get_local_offset_(int16_t &offset) const;
  bool readings_fresh_() const;
  bool save_();
  void publish_times_();
  void publish_offset_();
  void write_pending_();

  ModbusController *parent_{nullptr};
  time::RealTimeClock *clock_{nullptr};
  std::array<Hw211ScheduleTime *, 4> timers_{};
  hw211::ScheduleClockModel model_;
  ESPPreferenceObject preference_;
  std::array<uint32_t, 4> updated_{};
  std::array<uint16_t, 4> last_sent_{};
  int16_t initial_offset_{0};
  uint32_t freshness_ms_{90000};
  uint32_t retry_ms_{60000};
  uint32_t last_write_{0};
  bool has_sent_{false};
};

}  // namespace modbus_controller
}  // namespace esphome

#endif
