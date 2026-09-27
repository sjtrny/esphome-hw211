#pragma once

#include <utility>
#include <vector>

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include "modbus_compat.h"

namespace esphome {
namespace modbus_controller {

class ModbusSelect : public Component, public select::Select, public SensorItem, public Hw211WriterEntity {
 public:
  // Preserve entity identity when its human-readable name changes.
  void set_api_key(uint32_t key) { this->object_id_hash_ = key; }

  ModbusSelect(SensorValueType sensor_value_type, uint16_t start_address, uint8_t register_count, uint16_t skip_updates,
               bool force_new_range, std::vector<int64_t> mapping) {
    hw211_configure_item(this, Hw211RegisterType::HOLDING, sensor_value_type, start_address, 0, register_count,
                         skip_updates, force_new_range);
    this->bitmask = 0xFFFFFFFF;  // not configurable
    this->response_bytes = 0;  // not configurable
    this->mapping_ = std::move(mapping);
  }

  using transform_func_t = optional<std::string> (*)(ModbusSelect *const, int64_t, Hw211ReadBuffer);
  using write_transform_func_t = optional<int64_t> (*)(ModbusSelect *const, const std::string &, int64_t,
                                                       std::vector<uint16_t> &);

  void set_parent(ModbusController *const parent) {
    this->parent_ = parent;
    this->hw211_set_controller(parent);
  }
  void set_use_write_mutiple(bool use_write_multiple) { this->use_write_multiple_ = use_write_multiple; }
  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_template(transform_func_t f) { this->transform_func_ = f; }
  void set_write_template(write_transform_func_t f) { this->write_transform_func_ = f; }

  void dump_config() override;
  void parse_and_publish(Hw211ReadBuffer data) override;
  void control(size_t index) override;

 protected:
  std::vector<int64_t> mapping_{};
  ModbusController *parent_{nullptr};
  bool use_write_multiple_{false};
  bool optimistic_{false};
  optional<transform_func_t> transform_func_{nullopt};
  optional<write_transform_func_t> write_transform_func_{nullopt};
};

}  // namespace modbus_controller
}  // namespace esphome
