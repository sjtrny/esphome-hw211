#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace esphome {
namespace hw211 {

inline uint16_t wrap_schedule_minutes(int value) { return static_cast<uint16_t>((value % 1440 + 1440) % 1440); }

inline bool valid_utc_offset(int value) { return value >= -840 && value <= 840; }

inline bool parse_utc_offset(const std::string &value, int16_t &minutes) {
  if (value.size() != 6 || (value[0] != '+' && value[0] != '-') || value[3] != ':')
    return false;
  for (const auto index : {1, 2, 4, 5}) {
    if (value[index] < '0' || value[index] > '9')
      return false;
  }
  const int hour = (value[1] - '0') * 10 + value[2] - '0';
  const int minute = (value[4] - '0') * 10 + value[5] - '0';
  const int result = (hour * 60 + minute) * (value[0] == '-' ? -1 : 1);
  if (minute > 59 || !valid_utc_offset(result))
    return false;
  minutes = static_cast<int16_t>(result);
  return true;
}

// Persist the translation already applied to the controller and any unfinished
// write together. A restart between saving, writing and readback must never
// apply the offset twice. Times in a pending transaction are LOCAL wall times.
struct ScheduleClockState {
  uint32_t version{0x48574301};
  int16_t controller_offset{0};
  int16_t applied_delta{0};
  int16_t target_delta{0};
  std::array<uint16_t, 4> desired{};
  uint8_t pending{0};
};

class ScheduleClockModel {
 public:
  void initialize(int16_t controller_offset) {
    this->state_ = {};
    this->state_.controller_offset = controller_offset;
    this->dirty_ = true;
    this->invalidate_readings();
  }

  bool restore(const ScheduleClockState &state) {
    if (state.version != 0x48574301 || !valid_utc_offset(state.controller_offset) || state.pending > 1 ||
        state.applied_delta < -1680 || state.applied_delta > 1680 ||
        state.target_delta < -1680 || state.target_delta > 1680)
      return false;
    for (auto value : state.desired) {
      if (value >= 1440)
        return false;
    }
    this->state_ = state;
    this->dirty_ = false;
    this->invalidate_readings();
    return true;
  }

  void invalidate_readings() {
    this->received_ = 0;
    this->ready_ = false;
  }

  bool observe(uint8_t index, uint16_t minutes) {
    if (index >= 4 || minutes >= 1440) {
      this->invalidate_readings();
      return false;
    }
    this->observed_[index] = minutes;
    this->received_ |= 1U << index;
    if (this->received_ != 0x0F)
      return false;
    this->received_ = 0;
    this->raw_ = this->observed_;
    this->ready_ = true;
    if (this->state_.pending && this->raw_ == this->target_times()) {
      this->state_.applied_delta = this->state_.target_delta;
      this->state_.pending = 0;
      this->dirty_ = true;
    }
    return true;
  }

  // Preserve displayed wall times when the local UTC offset changes (DST), or
  // when the configured fixed controller offset is changed.
  void reconcile(int16_t local_offset) {
    if (!this->ready_ || !valid_utc_offset(local_offset))
      return;
    const int16_t delta = local_offset - this->state_.controller_offset;
    if (this->state_.pending) {
      if (delta != this->state_.target_delta) {
        this->state_.target_delta = delta;
        this->dirty_ = true;
      }
    } else if (delta != this->state_.applied_delta) {
      this->capture_local_times_();
      this->state_.target_delta = delta;
      this->state_.pending = 1;
      this->dirty_ = true;
    }
  }

  bool set_controller_offset(int16_t controller_offset, int16_t local_offset) {
    if (!this->ready_ || !valid_utc_offset(controller_offset) || !valid_utc_offset(local_offset))
      return false;
    if (controller_offset != this->state_.controller_offset) {
      this->state_.controller_offset = controller_offset;
      this->dirty_ = true;
    }
    this->reconcile(local_offset);
    return true;
  }

  bool set_local_time(uint8_t index, uint16_t minutes, int16_t local_offset) {
    if (!this->ready_ || index >= 4 || minutes >= 1440 || !valid_utc_offset(local_offset))
      return false;
    if (!this->state_.pending)
      this->capture_local_times_();
    this->state_.desired[index] = minutes;
    this->state_.target_delta = local_offset - this->state_.controller_offset;
    this->state_.pending = 1;
    this->dirty_ = true;
    return true;
  }

  uint16_t local_time(uint8_t index) const {
    return this->state_.pending ? this->state_.desired[index]
                                : wrap_schedule_minutes(this->raw_[index] + this->state_.applied_delta);
  }

  std::array<uint16_t, 4> target_times() const {
    std::array<uint16_t, 4> target{};
    for (uint8_t i = 0; i < 4; i++)
      target[i] = wrap_schedule_minutes(this->state_.desired[i] - this->state_.target_delta);
    return target;
  }

  const ScheduleClockState &state() const { return this->state_; }
  const std::array<uint16_t, 4> &raw_times() const { return this->raw_; }
  bool ready() const { return this->ready_; }
  bool dirty() const { return this->dirty_; }
  void mark_saved() { this->dirty_ = false; }

 protected:
  void capture_local_times_() {
    for (uint8_t i = 0; i < 4; i++)
      this->state_.desired[i] = wrap_schedule_minutes(this->raw_[i] + this->state_.applied_delta);
  }

  ScheduleClockState state_{};
  std::array<uint16_t, 4> raw_{};
  std::array<uint16_t, 4> observed_{};
  uint8_t received_{0};
  bool ready_{false};
  bool dirty_{false};
};

}  // namespace hw211
}  // namespace esphome
