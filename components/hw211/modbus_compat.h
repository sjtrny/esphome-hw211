#pragma once

#include "esphome/components/modbus_controller/modbus_controller.h"
#include "esphome/core/version.h"

#include <cstdint>
#include <vector>

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
#include <span>
#endif

namespace esphome {
namespace modbus_controller {

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
using Hw211RegisterType = modbus::EntityType;
using Hw211ReadBuffer = std::span<const uint8_t>;
#else
using Hw211RegisterType = ModbusRegisterType;
using Hw211ReadBuffer = const std::vector<uint8_t> &;
#endif

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
class Hw211WriterEntity : public WriterEntity {
 protected:
  void hw211_set_controller(ModbusController *controller) { this->set_controller_(controller); }
};
#else
class Hw211WriterEntity {
 protected:
  void hw211_set_controller(ModbusController *controller) { (void) controller; }
};
#endif

inline void hw211_configure_item(SensorItem *item, Hw211RegisterType register_type, SensorValueType value_type,
                                 uint16_t start_address, uint8_t offset, uint8_t register_count,
                                 uint16_t skip_updates, bool force_new_range) {
  item->register_type = register_type;
  item->sensor_value_type = value_type;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  item->set_address(start_address);
  item->set_offset_from_start_address(offset);
#else
  item->start_address = start_address;
  item->offset = offset;
#endif
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  item->reuse_previous_range = force_new_range ? RangeReuse::NEVER : RangeReuse::AUTO;
  (void) register_count;
  (void) skip_updates;
#else
  item->register_count = register_count;
  item->skip_updates = skip_updates;
  item->force_new_range = force_new_range;
#endif
}

inline float hw211_payload_to_float(Hw211ReadBuffer data, const SensorItem &item) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  return payload_to_float(data, item, item.offset);
#else
  return payload_to_float(data, item);
#endif
}

inline bool hw211_bit_from_packed(size_t bit, Hw211ReadBuffer data) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  return modbus::helpers::bit_from_packed(bit, data);
#else
  return modbus::helpers::coil_from_vector(bit, data);
#endif
}

inline uint16_t hw211_get_u16(Hw211ReadBuffer data, size_t offset) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  return modbus::helpers::get_data<uint16_t>(data.data(), offset);
#else
  return modbus::helpers::get_data<uint16_t>(data, offset);
#endif
}

inline int64_t hw211_payload_to_number(Hw211ReadBuffer data, SensorValueType value_type, uint8_t offset,
                                       uint32_t bitmask) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  return modbus::helpers::payload_to_number(data, value_type, offset, bitmask).value_or(0);
#else
  return modbus::helpers::payload_to_number(data, value_type, offset, bitmask);
#endif
}

inline std::vector<uint16_t> hw211_float_to_payload(float value, SensorValueType value_type) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  std::vector<uint16_t> data;
  modbus::helpers::float_to_payload(data, value, value_type);
  return data;
#else
  return modbus::helpers::float_to_payload(value, value_type);
#endif
}

inline uint16_t hw211_entity_count(const SensorItem &item) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  return item.entity_count();
#else
  return item.register_count;
#endif
}

inline uint16_t hw211_write_address(const SensorItem &item) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 8, 0)
  return item.write_address();
#else
  if (item.register_type == Hw211RegisterType::COIL || item.register_type == Hw211RegisterType::DISCRETE_INPUT) {
    return item.start_address + item.offset;
  }
  return item.start_address + item.offset / 2;
#endif
}

}  // namespace modbus_controller
}  // namespace esphome
