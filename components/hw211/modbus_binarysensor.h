#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/component.h"
#include "modbus_compat.h"

#include <vector>

namespace esphome {
namespace modbus_controller {

class ModbusBinarySensor : public Component, public binary_sensor::BinarySensor, public SensorItem {
 public:
  ModbusBinarySensor(Hw211RegisterType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
                     uint16_t skip_updates, bool force_new_range) {
    this->bitmask = bitmask;
    const uint8_t register_count =
        (register_type == Hw211RegisterType::COIL || register_type == Hw211RegisterType::DISCRETE_INPUT) ? offset + 1
                                                                                                         : 1;
    hw211_configure_item(this, register_type, SensorValueType::BIT, start_address, offset, register_count, skip_updates,
                         force_new_range);
  }

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  uint16_t entity_count() const override {
    if (this->register_type == Hw211RegisterType::COIL ||
        this->register_type == Hw211RegisterType::DISCRETE_INPUT) {
      return this->offset_from_start_address + 1;
    }
    return 1;
  }
#endif

  void parse_and_publish(Hw211ReadBuffer data) override;
  void set_state(bool state) { this->state = state; }

  void dump_config() override;

  using transform_func_t = optional<bool> (*)(ModbusBinarySensor *, bool, Hw211ReadBuffer);
  void set_template(transform_func_t f) { this->transform_func_ = f; }

 protected:
  optional<transform_func_t> transform_func_{nullopt};
};

}  // namespace modbus_controller
}  // namespace esphome
