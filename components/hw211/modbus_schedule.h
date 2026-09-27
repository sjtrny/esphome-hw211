#pragma once

#include "esphome/components/datetime/time_entity.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "modbus_compat.h"

#include <cstdint>

namespace esphome {
namespace modbus_controller {

class Hw211ScheduleTime : public datetime::TimeEntity,
                          public Component,
                          public SensorItem,
                          public Hw211WriterEntity {
 public:
  Hw211ScheduleTime(Hw211RegisterType register_type, uint16_t start_address) {
    hw211_configure_item(this, register_type, SensorValueType::U_DWORD, start_address, 0, 2, 0, false);
  }

  void dump_config() override;
  void parse_and_publish(Hw211ReadBuffer data) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_parent(ModbusController *parent) {
    this->parent_ = parent;
    this->hw211_set_controller(parent);
  }

 protected:
  void control(const datetime::TimeCall &call) override;

  ModbusController *parent_{nullptr};
};

class Hw211ScheduleEnableSwitch : public switch_::Switch,
                                  public Component,
                                  public SensorItem,
                                  public Hw211WriterEntity {
 public:
  Hw211ScheduleEnableSwitch(Hw211RegisterType register_type, uint16_t start_address, uint16_t enable_mask)
      : enable_mask_(enable_mask) {
    this->bitmask = enable_mask;
    hw211_configure_item(this, register_type, SensorValueType::U_WORD, start_address, 0, 1, 0, false);
  }

  void dump_config() override;
  void parse_and_publish(Hw211ReadBuffer data) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_parent(ModbusController *parent) {
    this->parent_ = parent;
    this->hw211_set_controller(parent);
  }
  void set_peer(Hw211ScheduleEnableSwitch *peer) { this->peer_ = peer; }

 protected:
  void write_state(bool state) override;
  void cache_register_value_(uint16_t value, bool publish);

  ModbusController *parent_{nullptr};
  Hw211ScheduleEnableSwitch *peer_{nullptr};
  uint16_t enable_mask_;
  uint16_t register_value_{0};
  bool has_register_value_{false};
};

}  // namespace modbus_controller
}  // namespace esphome
