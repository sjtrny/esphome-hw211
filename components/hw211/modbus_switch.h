#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"
#include "modbus_compat.h"

#include <vector>

namespace esphome {
namespace modbus_controller {

class ModbusSwitch : public Component, public switch_::Switch, public SensorItem, public Hw211WriterEntity {
 public:
  ModbusSwitch(Hw211RegisterType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               uint16_t skip_updates, bool force_new_range) {
    this->bitmask = bitmask;
    if (register_type == Hw211RegisterType::HOLDING) {
      start_address += offset / 2;
      offset = 0;
    } else if (register_type == Hw211RegisterType::COIL) {
      start_address += offset;
      offset = 0;
    }
    hw211_configure_item(this, register_type, SensorValueType::BIT, start_address, offset, 1, skip_updates,
                         force_new_range);
  };
  void setup() override;
  void write_state(bool state) override;
  void dump_config() override;
  void set_assumed_state(bool assumed_state);
  void set_state(bool state) { this->state = state; }
  void parse_and_publish(Hw211ReadBuffer data) override;
  void set_parent(ModbusController *parent) {
    this->parent_ = parent;
    this->hw211_set_controller(parent);
  }

  using transform_func_t = optional<bool> (*)(ModbusSwitch *, bool, Hw211ReadBuffer);
  using write_transform_func_t = optional<bool> (*)(ModbusSwitch *, bool, std::vector<uint8_t> &);
  void set_template(transform_func_t f) { this->publish_transform_func_ = f; }
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  bool assumed_state() override;
  ModbusController *parent_{nullptr};
  bool use_write_multiple_{false};
  optional<transform_func_t> publish_transform_func_{nullopt};
  optional<write_transform_func_t> write_transform_func_{nullopt};
  bool assumed_state_{false};
};

}  // namespace modbus_controller
}  // namespace esphome
