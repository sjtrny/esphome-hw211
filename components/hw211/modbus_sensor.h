#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "modbus_compat.h"

#include <vector>

namespace esphome {
namespace modbus_controller {

class ModbusSensor : public Component, public sensor::Sensor, public SensorItem {
 public:
  // Preserve entity identity when its human-readable name changes.
  void set_api_key(uint32_t key) { this->object_id_hash_ = key; }

  ModbusSensor(Hw211RegisterType register_type, uint16_t start_address, uint8_t offset, uint32_t bitmask,
               SensorValueType value_type, int register_count, uint16_t skip_updates, bool force_new_range) {
    this->bitmask = bitmask;
    hw211_configure_item(this, register_type, value_type, start_address, offset, register_count, skip_updates,
                         force_new_range);
  }

  void parse_and_publish(Hw211ReadBuffer data) override;
  void dump_config() override;
  using transform_func_t = optional<float> (*)(ModbusSensor *, float, Hw211ReadBuffer);

  void set_template(transform_func_t f) { this->transform_func_ = f; }

 protected:
  optional<transform_func_t> transform_func_{nullopt};
};

}  // namespace modbus_controller
}  // namespace esphome
