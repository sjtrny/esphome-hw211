#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"
#include "modbus_compat.h"

#include <vector>

namespace esphome {
namespace modbus_controller {

using value_to_data_t = std::function<float>(float);

class ModbusNumber : public number::Number, public Component, public SensorItem, public Hw211WriterEntity {
 public:
  ModbusNumber(Hw211RegisterType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               SensorValueType value_type, int register_count, uint16_t skip_updates, bool force_new_range) {
    this->bitmask = bitmask;
    hw211_configure_item(this, register_type, value_type, start_address, offset, register_count, skip_updates,
                         force_new_range);
  };

  void dump_config() override;
  void parse_and_publish(Hw211ReadBuffer data) override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void set_parent(ModbusController *parent) {
    this->parent_ = parent;
    this->hw211_set_controller(parent);
  }
  void set_write_multiply(float factor) { this->multiply_by_ = factor; }

  using transform_func_t = optional<float> (*)(ModbusNumber *, float, Hw211ReadBuffer);
  using write_transform_func_t = optional<float> (*)(ModbusNumber *, float, std::vector<uint16_t> &);
  void set_template(transform_func_t f) { this->transform_func_ = f; }
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }

 protected:
  void control(float value) override;
  optional<transform_func_t> transform_func_{nullopt};
  optional<write_transform_func_t> write_transform_func_{nullopt};
  ModbusController *parent_{nullptr};
  float multiply_by_{1.0};
  bool use_write_multiple_{false};
};

}  // namespace modbus_controller
}  // namespace esphome
