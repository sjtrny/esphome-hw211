#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace esphome {
namespace hw211 {

enum class OperatingState { UNKNOWN, FAULT, DEFROSTING, HEATING_BOOST, HEATING, ELECTRIC_HEATING, OFF, IDLE };
enum class StateInput : uint8_t { POWER, COMPONENTS, FUNCTIONS, FAULTS, COUNT };

inline OperatingState derive_operating_state(uint16_t power, uint16_t components, uint16_t functions, uint16_t faults) {
  if (faults != 0)
    return OperatingState::FAULT;
  if (functions & (1U << 2))
    return OperatingState::DEFROSTING;
  const bool compressor = components & (1U << 8);
  const bool element = components & (1U << 9);
  if (compressor && element)
    return OperatingState::HEATING_BOOST;
  if (compressor)
    return OperatingState::HEATING;
  if (element)
    return OperatingState::ELECTRIC_HEATING;
  // Active outputs take precedence over an off command (e.g. protective operation).
  return power == 0 ? OperatingState::OFF : OperatingState::IDLE;
}

inline const char *operating_state_name(OperatingState state) {
  switch (state) {
    case OperatingState::FAULT:
      return "Fault / protection";
    case OperatingState::DEFROSTING:
      return "Defrosting";
    case OperatingState::HEATING_BOOST:
      return "Heating + boost";
    case OperatingState::HEATING:
      return "Heating";
    case OperatingState::ELECTRIC_HEATING:
      return "Electric heating";
    case OperatingState::OFF:
      return "Off";
    case OperatingState::IDLE:
      return "Idle";
    default:
      return "Unknown";
  }
}

// Consume complete sets of register reports, not transient mixtures of old and
// new fields while Modbus is delivering a poll in separate response frames.
class OperatingStateTracker {
 public:
  void accept(StateInput input, float value, uint32_t now) {
    const auto index = static_cast<uint8_t>(input);
    if (index >= static_cast<uint8_t>(StateInput::COUNT) || !std::isfinite(value) || value < 0 || value > 65535 ||
        value != std::floor(value) || (input == StateInput::POWER && value > 1)) {
      this->invalidate();
      return;
    }
    this->values_[index] = static_cast<uint16_t>(value);
    this->updated_[index] = now;
    this->received_ |= 1U << index;
    this->pending_ |= 1U << index;
    if (this->pending_ == ALL_INPUTS) {
      this->state_ = derive_operating_state(this->values_[0], this->values_[1], this->values_[2], this->values_[3]);
      this->pending_ = 0;
    }
  }

  void invalidate() {
    this->received_ = 0;
    this->pending_ = 0;
    this->state_ = OperatingState::UNKNOWN;
  }

  OperatingState state(uint32_t now, uint32_t timeout) const {
    if (this->received_ != ALL_INPUTS)
      return OperatingState::UNKNOWN;
    for (const auto updated : this->updated_) {
      // Unsigned subtraction also handles millis() rollover.
      if (now - updated > timeout)
        return OperatingState::UNKNOWN;
    }
    return this->state_;
  }

 protected:
  static constexpr uint8_t ALL_INPUTS = 0x0F;
  std::array<uint16_t, 4> values_{};
  std::array<uint32_t, 4> updated_{};
  uint8_t received_{0};
  uint8_t pending_{0};
  OperatingState state_{OperatingState::UNKNOWN};
};

}  // namespace hw211
}  // namespace esphome
