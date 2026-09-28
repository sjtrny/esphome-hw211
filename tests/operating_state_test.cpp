#include "operating_state_logic.h"

#include <cassert>
#include <limits>

using namespace esphome::hw211;

static void report(OperatingStateTracker &tracker, uint32_t time, uint16_t power, uint16_t components,
                   uint16_t functions = 0, uint16_t faults = 0) {
  tracker.accept(StateInput::POWER, power, time);
  tracker.accept(StateInput::COMPONENTS, components, time);
  tracker.accept(StateInput::FUNCTIONS, functions, time);
  tracker.accept(StateInput::FAULTS, faults, time);
}

int main() {
  // Real controller words contain unrelated input/fan bits as well as outputs.
  assert(derive_operating_state(1, 48, 1, 0) == OperatingState::IDLE);
  assert(derive_operating_state(1, 2096, 0, 0) == OperatingState::IDLE);  // fan only
  assert(derive_operating_state(1, 2352, 0, 0) == OperatingState::HEATING);
  assert(derive_operating_state(1, 3376, 0, 0) == OperatingState::HEATING);  // reversing valve is not proof of defrost
  assert(derive_operating_state(1, 48 | 512, 0, 0) == OperatingState::ELECTRIC_HEATING);
  assert(derive_operating_state(1, 2352 | 512, 0, 0) == OperatingState::HEATING_BOOST);
  assert(derive_operating_state(0, 48, 0, 0) == OperatingState::OFF);
  assert(derive_operating_state(0, 2352, 0, 0) == OperatingState::HEATING);  // report active outputs before off command
  assert(derive_operating_state(1, 2352 | 512, 4, 0) == OperatingState::DEFROSTING);
  assert(derive_operating_state(0, 2352 | 512, 4, 1) == OperatingState::FAULT);
  assert(derive_operating_state(1, 48, 1, 0x8000) == OperatingState::FAULT);

  OperatingStateTracker tracker;
  assert(tracker.state(0, 90000) == OperatingState::UNKNOWN);
  tracker.accept(StateInput::POWER, 1, 10);
  tracker.accept(StateInput::COMPONENTS, 48, 10);
  tracker.accept(StateInput::FUNCTIONS, 1, 10);
  assert(tracker.state(10, 90000) == OperatingState::UNKNOWN);  // no false Idle before the fault word arrives
  tracker.accept(StateInput::FAULTS, 0, 10);
  assert(tracker.state(10, 90000) == OperatingState::IDLE);

  // Avoid publishing a partially received heating-to-defrost transition.
  report(tracker, 100, 1, 2352);
  tracker.accept(StateInput::COMPONENTS, 48, 200);
  assert(tracker.state(200, 90000) == OperatingState::HEATING);
  tracker.accept(StateInput::POWER, 1, 200);
  tracker.accept(StateInput::FUNCTIONS, 4, 200);
  assert(tracker.state(200, 90000) == OperatingState::HEATING);
  tracker.accept(StateInput::FAULTS, 0, 200);
  assert(tracker.state(200, 90000) == OperatingState::DEFROSTING);

  // Every source must stay fresh. Repeating three words cannot refresh a missing fourth.
  tracker.accept(StateInput::POWER, 1, 90201);
  tracker.accept(StateInput::COMPONENTS, 48, 90201);
  tracker.accept(StateInput::FUNCTIONS, 1, 90201);
  assert(tracker.state(90201, 90000) == OperatingState::UNKNOWN);
  tracker.accept(StateInput::FAULTS, 0, 90202);
  assert(tracker.state(90202, 90000) == OperatingState::IDLE);

  tracker.invalidate();  // controller-offline callback
  assert(tracker.state(90203, 90000) == OperatingState::UNKNOWN);
  report(tracker, 90204, 1, 2352);
  assert(tracker.state(90204, 90000) == OperatingState::HEATING);

  for (float invalid : {-1.0f, 0.5f, 65536.0f, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    report(tracker, 100000, 1, 48);
    tracker.accept(StateInput::COMPONENTS, invalid, 100001);
    assert(tracker.state(100001, 90000) == OperatingState::UNKNOWN);
  }
  report(tracker, 100000, 1, 48);
  tracker.accept(StateInput::POWER, 2, 100001);
  assert(tracker.state(100001, 90000) == OperatingState::UNKNOWN);

  report(tracker, 0xFFFFFFF0, 1, 48);
  assert(tracker.state(0x00000010, 40) == OperatingState::IDLE);
  assert(tracker.state(0x00000030, 40) == OperatingState::UNKNOWN);
}
