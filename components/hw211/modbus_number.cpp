#include <vector>
#include "modbus_number.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus.number";

// Maximum uint16_t registers to log in verbose hex output
static constexpr size_t MODBUS_NUMBER_MAX_LOG_REGISTERS = 32;

void ModbusNumber::parse_and_publish(Hw211ReadBuffer data) {
  float result = hw211_payload_to_float(data, *this) / this->multiply_by_;

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(this, result, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      result = val.value();
    }
  }
  ESP_LOGD(TAG, "Number new state : %.02f", result);
  // this->sensor_->raw_state = result;
  this->publish_state(result);
}

void ModbusNumber::control(float value) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  this->clear_dispatched_();
  this->clear_tx_queue_for_device();
  std::vector<uint16_t> data;
  float write_value = value;
  if (this->write_transform_func_.has_value()) {
    auto val = (*this->write_transform_func_)(this, value, data);
    if (!val.has_value()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
    ESP_LOGV(TAG, "Value overwritten by lambda");
    write_value = val.value();
  } else {
    write_value = this->multiply_by_ * write_value;
  }

  if (data.empty()) {
    data = hw211_float_to_payload(write_value, this->sensor_value_type);
  }
  if (data.empty()) {
    ESP_LOGW(TAG, "No payload was created for updating number");
    return;
  }

  const uint16_t register_count = hw211_entity_count(*this);
  ESP_LOGD(TAG,
           "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
           this->get_name().c_str(), this->start_address, register_count, value, write_value);

  bool queued;
  if (register_count == 1 && !this->use_write_multiple_) {
    queued = this->write_single_register(hw211_write_address(*this), data[0]);
  } else {
    queued = this->write_multiple_registers(hw211_write_address(*this), data);
  }
  if (!queued) {
    ESP_LOGW(TAG, "Modbus write for '%s' was refused; state not published", this->get_name().c_str());
    return;
  }
  this->publish_state(value);
#else
  optional<ModbusCommandItem> write_cmd;
  std::vector<uint16_t> data;
  float write_value = value;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      write_value = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  } else {
    write_value = this->multiply_by_ * write_value;
  }

  if (!data.empty()) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_uint16_size(MODBUS_NUMBER_MAX_LOG_REGISTERS)];
#endif
    ESP_LOGV(TAG, "Modbus Number write raw: %s",
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
    write_cmd.emplace(ModbusCommandItem::create_custom_command(
        this->parent_, data,
        [this](Hw211RegisterType register_type, uint16_t start_address, Hw211ReadBuffer data) {
          this->parent_->on_write_register_response(register_type, start_address, data);
        }));
  } else {
    data = hw211_float_to_payload(write_value, this->sensor_value_type);

    ESP_LOGD(TAG,
             "Updating register: connected Sensor=%s start address=0x%X register count=%d new value=%.02f (val=%.02f)",
             this->get_name().c_str(), this->start_address, this->register_count, value, write_value);

    // Create and send the write command
    if (this->register_count == 1 && !this->use_write_multiple_) {
      // since offset is in bytes and a register is 16 bits we get the start by adding offset/2
      write_cmd.emplace(ModbusCommandItem::create_write_single_command(
          this->parent_, hw211_write_address(*this), data[0]));
    } else {
      write_cmd.emplace(ModbusCommandItem::create_write_multiple_command(
          this->parent_, hw211_write_address(*this), this->register_count, data));
    }
    // publish new value
    write_cmd->on_data_func = [this, value](Hw211RegisterType register_type, uint16_t start_address,
                                            Hw211ReadBuffer data) {
      // gets called when the write command is ack'd from the device
      this->parent_->on_write_register_response(register_type, start_address, data);
      this->publish_state(value);
    };
  }
  this->parent_->queue_command(std::move(*write_cmd));
  this->publish_state(value);
#endif
}
void ModbusNumber::dump_config() { LOG_NUMBER(TAG, "Modbus Number", this); }

}  // namespace modbus_controller
}  // namespace esphome
