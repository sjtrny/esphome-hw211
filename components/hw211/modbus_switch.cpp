
#include "modbus_switch.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <array>

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.switch";

// Maximum bytes to log in verbose hex output
static constexpr size_t MODBUS_SWITCH_MAX_LOG_BYTES = 64;

void ModbusSwitch::setup() {
  optional<bool> initial_state = Switch::get_initial_state_with_restore_mode();
  if (initial_state.has_value()) {
    // if it has a value, restore_mode is not "DISABLED", therefore act on the switch:
    if (initial_state.value()) {
      this->turn_on();
    } else {
      this->turn_off();
    }
  }
}
void ModbusSwitch::dump_config() { LOG_SWITCH(TAG, "Modbus Controller Switch", this); }

void ModbusSwitch::set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

bool ModbusSwitch::assumed_state() { return this->assumed_state_; }

void ModbusSwitch::parse_and_publish(Hw211ReadBuffer data) {
  bool value = false;
  switch (this->register_type) {
    case Hw211RegisterType::DISCRETE_INPUT:
    case Hw211RegisterType::COIL:
      // offset for coil is the actual number of the coil not the byte offset
      value = hw211_bit_from_packed(this->offset, data);
      break;
    default:
      value = hw211_get_u16(data, this->offset) & this->bitmask;
      break;
  }

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->publish_transform_func_) {
    // the lambda can parse the response itself
    auto val = (*this->publish_transform_func_)(this, value, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      value = val.value();
    }
  }

  ESP_LOGV(TAG, "Publish '%s': new value = %s type = %d address = %X offset = %x", this->get_name().c_str(),
           ONOFF(value), (int) this->register_type, this->start_address, this->offset);
  this->publish_state(value);
}

void ModbusSwitch::write_state(bool state) {
  // This will be called every time the user requests a state change.
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 9, 0)
  this->clear_dispatched_();
  this->clear_tx_queue_for_device();
  bool write_value = state;
  std::vector<uint8_t> data;
  if (this->write_transform_func_.has_value()) {
    auto val = (*this->write_transform_func_)(this, state, data);
    if (!val.has_value()) {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
    write_value = val.value();
  }
  if (!data.empty()) {
    ESP_LOGW(TAG, "Custom switch payloads are not supported by this ESPHome version");
    return;
  }

  bool queued;
  if (this->register_type == Hw211RegisterType::COIL) {
    if (this->use_write_multiple_) {
      const std::array<bool, 1> states{write_value};
      queued = this->write_multiple_coils(hw211_write_address(*this), states);
    } else {
      queued = this->write_single_coil(hw211_write_address(*this), write_value);
    }
  } else if (this->use_write_multiple_) {
    const std::array<uint16_t, 1> states{
        static_cast<uint16_t>(write_value ? (0xFFFF & this->bitmask) : 0)};
    queued = this->write_multiple_registers(hw211_write_address(*this), states);
  } else {
    queued = this->write_single_register(hw211_write_address(*this),
                                         write_value ? 0xFFFF & this->bitmask : 0u);
  }
  if (!queued) {
    ESP_LOGW(TAG, "Modbus write for '%s' was refused; state not published", this->get_name().c_str());
    return;
  }
  this->publish_state(state);
#else
  optional<ModbusCommandItem> cmd;
  std::vector<uint8_t> data;
  // Is there are lambda configured?
  if (this->write_transform_func_.has_value()) {
    // data is passed by reference
    // the lambda can fill the empty vector directly
    // in that case the return value is ignored
    auto val = (*this->write_transform_func_)(this, state, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      state = val.value();
    } else {
      ESP_LOGV(TAG, "Communication handled by lambda - exiting control");
      return;
    }
  }
  if (!data.empty()) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(MODBUS_SWITCH_MAX_LOG_BYTES)];
#endif
    ESP_LOGV(TAG, "Modbus Switch write raw: %s",
             format_hex_pretty_to(hex_buf, sizeof(hex_buf), data.data(), data.size()));
    cmd.emplace(ModbusCommandItem::create_custom_command(
        this->parent_, data,
        [this](Hw211RegisterType register_type, uint16_t start_address, Hw211ReadBuffer data) {
          this->parent_->on_write_register_response(register_type, start_address, data);
        }));
  } else {
    ESP_LOGV(TAG, "write_state '%s': new value = %s type = %d address = %X offset = %x", this->get_name().c_str(),
             ONOFF(state), (int) this->register_type, this->start_address, this->offset);
    if (this->register_type == Hw211RegisterType::COIL) {
      // offset for coil and discrete inputs is the coil/register number not bytes
      if (this->use_write_multiple_) {
        std::vector<bool> states{state};
        cmd.emplace(ModbusCommandItem::create_write_multiple_coils(this->parent_, hw211_write_address(*this), states));
      } else {
        cmd.emplace(ModbusCommandItem::create_write_single_coil(this->parent_, hw211_write_address(*this), state));
      }
    } else {
      // since offset is in bytes and a register is 16 bits we get the start by adding offset/2
      if (this->use_write_multiple_) {
        std::vector<uint16_t> bool_states(1, state ? (0xFFFF & this->bitmask) : 0);
        cmd.emplace(ModbusCommandItem::create_write_multiple_command(this->parent_, hw211_write_address(*this), 1,
                                                                     bool_states));
      } else {
        cmd.emplace(ModbusCommandItem::create_write_single_command(
            this->parent_, hw211_write_address(*this), state ? 0xFFFF & this->bitmask : 0u));
      }
    }
  }
  this->parent_->queue_command(std::move(*cmd));
  this->publish_state(state);
#endif
}
// ModbusSwitch end
}  // namespace modbus_controller
}  // namespace esphome
