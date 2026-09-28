#include "operating_state.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome {
namespace hw211 {

static const char *const TAG = "hw211.operating_state";

void Hw211OperatingState::setup() {
  // Permit ordinary retries, but never report Idle from indefinitely stale data.
  const uint64_t timeout = static_cast<uint64_t>(this->parent_->get_update_interval()) * 3;
  this->timeout_ms_ = static_cast<uint32_t>(std::max<uint64_t>(30000, std::min<uint64_t>(timeout, 0x7FFFFFFF)));
  this->parent_->add_on_offline_callback([this](int, int) {
    this->tracker_.invalidate();
    this->update();
  });
  for (uint8_t index = 0; index < this->sources_.size(); index++) {
    this->sources_[index]->add_on_state_callback([this, index](float value) {
      this->tracker_.accept(static_cast<StateInput>(index), value, millis());
      this->update();
    });
  }
  this->update();
}

void Hw211OperatingState::update() {
  const char *value = operating_state_name(this->tracker_.state(millis(), this->timeout_ms_));
  if (!this->has_state() || this->state != value)
    this->publish_state(value);
}

void Hw211OperatingState::dump_config() {
  LOG_TEXT_SENSOR("", "HW211 operating state", this);
  ESP_LOGCONFIG(TAG, "  Source freshness limit: %u ms", static_cast<unsigned>(this->timeout_ms_));
}

}  // namespace hw211
}  // namespace esphome
