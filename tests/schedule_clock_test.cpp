#include "schedule_clock_logic.h"

#include <cassert>
#include <iostream>

using namespace esphome::hw211;

static void observe(ScheduleClockModel &model, const std::array<uint16_t, 4> &raw) {
  for (uint8_t i = 0; i < 4; i++)
    model.observe(i, raw[i]);
}

static void local_equals(const ScheduleClockModel &model, const std::array<uint16_t, 4> &expected) {
  for (uint8_t i = 0; i < 4; i++)
    assert(model.local_time(i) == expected[i]);
}

int main() {
  int16_t offset = 0;
  for (const auto &example : std::array<std::pair<const char *, int16_t>, 8>{
           {{"+09:00", 540}, {"+09:30", 570}, {"+05:45", 345}, {"-03:30", -210},
            {"-00:30", -30}, {"-00:00", 0}, {"+14:00", 840}, {"-14:00", -840}}}) {
    assert(parse_utc_offset(example.first, offset));
    assert(offset == example.second);
  }
  for (const auto *invalid : {"09:00", "+9:00", "+09:60", "+14:01", "-14:01", "+25:00", "+00:-1", "+09:00x", ""})
    assert(!parse_utc_offset(invalid, offset));

  // Exhaustive daily round trips, including negative, half- and quarter-hour
  // offsets and differences crossing either side of midnight.
  for (int delta = -1680; delta <= 1680; delta += 15) {
    for (int local = 0; local < 1440; local++) {
      const auto raw = wrap_schedule_minutes(local - delta);
      assert(raw < 1440);
      assert(wrap_schedule_minutes(raw + delta) == local);
    }
  }

  const std::array<uint16_t, 4> intended{390, 1439, 0, 0};
  ScheduleClockModel model;
  model.initialize(540);  // Fixed controller UTC+09:00.
  model.reconcile(600);
  assert(!model.ready() && !model.state().pending);
  assert(!model.set_local_time(0, 360, 600));
  observe(model, intended);
  model.reconcile(600);  // Sydney standard time, UTC+10:00.
  assert(model.state().pending);
  local_equals(model, intended);
  const std::array<uint16_t, 4> standard{330, 1379, 1380, 1380};
  assert(model.target_times() == standard);

  // Old readbacks must not shift the displayed times or undo an unconfirmed
  // write. Nor may a partial new register response confirm all four endpoints.
  observe(model, intended);
  assert(model.state().pending);
  for (uint8_t i = 0; i < 3; i++)
    model.observe(i, standard[i]);
  assert(model.state().pending);
  local_equals(model, intended);
  model.observe(3, standard[3]);
  assert(!model.state().pending && model.state().applied_delta == 60);
  local_equals(model, intended);
  model.mark_saved();
  observe(model, standard);
  model.reconcile(600);
  assert(!model.dirty() && !model.state().pending);  // No repetitive writes/flash wear.

  // Spring forward, fall back, and a half-hour DST change preserve wall times.
  model.reconcile(660);
  const std::array<uint16_t, 4> summer{270, 1319, 1320, 1320};
  assert(model.target_times() == summer);
  local_equals(model, intended);
  observe(model, summer);
  model.reconcile(600);
  assert(model.target_times() == standard);
  observe(model, standard);
  model.reconcile(630);
  assert(model.target_times()[0] == 300);
  observe(model, model.target_times());
  local_equals(model, intended);

  // Editing a timer uses local time. Changing the fixed controller offset also
  // keeps that local intent, including a UTC-negative fractional offset.
  assert(model.set_local_time(0, 360, 630));
  assert(model.target_times()[0] == 270);
  observe(model, model.target_times());
  assert(model.set_controller_offset(-210, 630));
  assert(model.target_times()[0] == 960);  // 06:00 local -> 16:00 previous controller day.
  observe(model, model.target_times());
  assert(model.local_time(0) == 360);

  // Native-panel timer edits are reflected using the last applied mapping.
  auto native_edit = model.target_times();
  native_edit[1] = 600;
  observe(model, native_edit);
  assert(model.local_time(1) == 0);

  // A saved transaction survives power loss before the write and after the
  // write but before its readback, without applying the correction twice.
  ScheduleClockModel before_write;
  before_write.initialize(540);
  observe(before_write, intended);
  before_write.reconcile(600);
  const auto transaction = before_write.state();
  ScheduleClockModel restart;
  assert(restart.restore(transaction));
  assert(!restart.ready());
  observe(restart, intended);
  restart.reconcile(600);
  assert(restart.target_times() == standard);
  local_equals(restart, intended);
  ScheduleClockModel after_write;
  assert(after_write.restore(transaction));
  observe(after_write, standard);
  after_write.reconcile(600);
  assert(!after_write.state().pending);
  local_equals(after_write, intended);

  // Restart across DST, including an unfinished pre-DST transaction.
  ScheduleClockModel across_dst;
  assert(across_dst.restore(after_write.state()));
  observe(across_dst, standard);
  across_dst.reconcile(660);
  assert(across_dst.target_times() == summer);
  local_equals(across_dst, intended);
  ScheduleClockModel pending_across_dst;
  assert(pending_across_dst.restore(transaction));
  observe(pending_across_dst, intended);
  pending_across_dst.reconcile(660);
  assert(pending_across_dst.target_times() == summer);
  local_equals(pending_across_dst, intended);

  // Invalid/stale/offline data cannot create a new correction or accept edits.
  model.invalidate_readings();
  assert(!model.set_controller_offset(600, 660));
  assert(!model.set_local_time(0, 360, 660));
  model.observe(0, 1440);
  assert(!model.ready());
  auto bad = transaction;
  bad.version = 0;
  assert(!model.restore(bad));
  bad = transaction;
  bad.controller_offset = 841;
  assert(!model.restore(bad));
  bad = transaction;
  bad.desired[1] = 1440;
  assert(!model.restore(bad));

  std::cout << "Schedule offset, DST, readback, restart and recovery checks passed\n";
}
