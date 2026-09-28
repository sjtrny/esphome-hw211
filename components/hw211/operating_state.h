#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "operating_state_logic.h"

namespace esphome {
namespace hw211 {

class Hw211OperatingState : public PollingComponent, public text_sensor::TextSensor {
 public:
  Hw211OperatingState() : PollingComponent(1000) {}
  void set_parent(modbus_controller::ModbusController *parent) { this->parent_ = parent; }
  void set_source(uint8_t index, sensor::Sensor *source) { this->sources_[index] = source; }
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  modbus_controller::ModbusController *parent_{};
  std::array<sensor::Sensor *, 4> sources_{};
  OperatingStateTracker tracker_;
  uint32_t timeout_ms_{0};
};

}  // namespace hw211
}  // namespace esphome
