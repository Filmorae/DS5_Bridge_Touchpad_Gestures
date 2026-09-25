#include "custom_touch_gesture.h"

#include <cassert>
#include <cstdio>

namespace {

struct Harness {
    CustomTouchGestureState gesture{};
    BridgeControllerState report{};

    void touch(unsigned slot, bool active, uint8_t id, uint16_t x = 400, uint16_t y = 300) {
        report.touch_points[slot] = {active, id, x, y};
    }
    CustomTouchResult step(unsigned ms, bool click = false) {
        report.touchpad = click;
        report.left_stick_x = 0x34;
        report.right_trigger = 0x78;
        auto result = custom_touch_gesture_process(gesture, report, ms * 1000);
        assert(report.left_stick_x == 0x34 && report.right_trigger == 0x78);
        assert(report.touchpad == click); // Only the caller may suppress a delivered click.
        return result;
    }
};

void ordinary_input() {
    Harness h;
    h.touch(0, true, 1);
    assert(h.step(0).action == CustomTouchAction::None);
    assert(h.step(200).action == CustomTouchAction::None);
    h.touch(0, false, 1);
    assert(!h.step(210, true).suppress_click);
    assert(h.step(220, true).action == CustomTouchAction::None);
}

void screenshot_and_rearm() {
    Harness h;
    h.touch(0, true, 4);
    h.step(0);
    h.touch(1, true, 5, 900, 600);
    assert(h.step(70).action == CustomTouchAction::None);
    assert(h.step(219).action == CustomTouchAction::None);
    auto result = h.step(220);
    assert(result.action == CustomTouchAction::Screenshot && !result.suppress_click);
    for (unsigned ms = 230; ms < 3000; ms += 100) {
        assert(h.step(ms).action == CustomTouchAction::None);
    }
    h.touch(0, false, 4);
    h.step(3000);
    h.touch(1, false, 5);
    h.step(3010);
    h.touch(1, true, 6, 900, 600);
    h.step(3020);
    h.touch(0, true, 7);
    h.step(3030);
    assert(h.step(3180).action == CustomTouchAction::Screenshot);
}

void recording_and_click_suppression() {
    Harness h;
    h.touch(0, true, 1);
    h.step(0);
    h.touch(1, true, 2, 900, 600);
    h.step(30);
    auto result = h.step(50, true);
    assert(result.action == CustomTouchAction::RecordToggle && result.suppress_click);
    for (unsigned ms = 60; ms < 250; ms += 10) {
        result = h.step(ms, true);
        assert(result.action == CustomTouchAction::None && result.suppress_click);
    }
    assert(!h.step(250, false).suppress_click);
    assert(!h.step(260, true).suppress_click); // A second physical click passes through.
    assert(h.step(300, true).action == CustomTouchAction::None);

    Harness simultaneous;
    simultaneous.touch(0, true, 3);
    simultaneous.touch(1, true, 4, 900, 600);
    auto same_report = simultaneous.step(0, true);
    assert(same_report.action == CustomTouchAction::RecordToggle);
    assert(same_report.suppress_click);

    simultaneous.touch(0, false, 3);
    simultaneous.touch(1, false, 4);
    assert(simultaneous.step(20, true).suppress_click); // Hold continues after fingers lift.
    assert(!simultaneous.step(30, false).suppress_click);
}

void cancel_on_motion_or_lift_or_bad_pair() {
    Harness motion;
    motion.touch(0, true, 1);
    motion.step(0);
    motion.touch(1, true, 2);
    motion.step(30);
    motion.touch(0, true, 1, 460, 300);
    assert(motion.step(40).action == CustomTouchAction::None);
    assert(motion.step(200, true).action == CustomTouchAction::None);
    assert(!motion.step(210, true).suppress_click);

    Harness late;
    late.touch(0, true, 1);
    late.step(0);
    late.touch(1, true, 2);
    late.step(121);
    assert(late.step(280).action == CustomTouchAction::None);

    Harness lifted;
    lifted.touch(0, true, 1);
    lifted.step(0);
    lifted.touch(1, true, 2);
    lifted.step(30);
    lifted.touch(0, false, 1);
    lifted.step(80);
    lifted.touch(0, true, 3);
    assert(lifted.step(200).action == CustomTouchAction::None);
}

void finger_pad_and_press_transients() {
    Harness landing;
    landing.touch(0, true, 20);
    landing.step(0);
    landing.touch(0, true, 20, 450, 320); // Contact settles as the second finger arrives.
    landing.touch(1, true, 21, 900, 600);
    landing.step(100);
    landing.touch(0, true, 20, 480, 340);
    landing.touch(1, true, 21, 925, 619);
    assert(landing.step(250).action == CustomTouchAction::Screenshot);

    Harness press;
    press.touch(0, true, 30);
    press.step(0);
    press.touch(1, true, 31, 900, 600);
    press.step(80);
    press.step(90);
    press.touch(0, false, 30);
    press.touch(1, true, 31, 970, 600); // Press shifts or hides contacts in this report.
    auto pressed = press.step(100, true);
    assert(pressed.action == CustomTouchAction::RecordToggle && pressed.suppress_click);
    assert(press.step(260, true).action == CustomTouchAction::None);

    Harness brief_loss;
    brief_loss.touch(0, true, 40);
    brief_loss.step(0);
    brief_loss.touch(1, true, 41, 900, 600);
    brief_loss.step(80);
    brief_loss.step(95);
    brief_loss.touch(0, false, 40);
    assert(brief_loss.step(100).action == CustomTouchAction::None);
    brief_loss.touch(0, true, 40);
    brief_loss.step(108);
    assert(brief_loss.step(230).action == CustomTouchAction::Screenshot);

    Harness real_lift;
    real_lift.touch(0, true, 50);
    real_lift.step(0);
    real_lift.touch(1, true, 51, 900, 600);
    real_lift.step(80);
    real_lift.step(95);
    real_lift.touch(0, false, 50);
    real_lift.step(100);
    real_lift.step(114);
    real_lift.touch(0, true, 50);
    assert(real_lift.step(230).action == CustomTouchAction::None);

    Harness delayed_return;
    delayed_return.touch(0, true, 60);
    delayed_return.step(0);
    delayed_return.touch(1, true, 61, 900, 600);
    delayed_return.step(80);
    delayed_return.touch(0, false, 60);
    delayed_return.step(100);
    delayed_return.touch(0, true, 60);
    assert(delayed_return.step(115).action == CustomTouchAction::None);
    assert(delayed_return.step(230).action == CustomTouchAction::None);

    Harness swipe_then_click;
    swipe_then_click.touch(0, true, 70);
    swipe_then_click.step(0);
    swipe_then_click.touch(1, true, 71, 900, 600);
    swipe_then_click.step(80);
    swipe_then_click.touch(0, true, 70, 510, 300);
    auto moved_click = swipe_then_click.step(90, true);
    assert(moved_click.action == CustomTouchAction::None);
    assert(!moved_click.suppress_click);
}

void ids_slots_gap_and_reset() {
    Harness h;
    h.touch(0, true, 10);
    h.step(0);
    h.touch(1, true, 11, 900, 600);
    h.step(50);
    h.touch(0, true, 11, 900, 600);
    h.touch(1, true, 10);
    assert(h.step(100).action == CustomTouchAction::None);
    assert(h.step(200).action == CustomTouchAction::Screenshot);

    Harness changed;
    changed.touch(0, true, 10);
    changed.step(0);
    changed.touch(1, true, 11);
    changed.step(50);
    changed.touch(1, true, 12);
    changed.step(100);
    assert(changed.step(200).action == CustomTouchAction::None);

    Harness gap;
    gap.touch(0, true, 10);
    gap.step(0);
    gap.touch(1, true, 11);
    gap.step(50);
    assert(gap.step(400).action == CustomTouchAction::None);
    assert(gap.step(560).action == CustomTouchAction::None);

    Harness reset;
    reset.touch(0, true, 10);
    reset.step(0);
    reset.touch(1, true, 11);
    reset.step(50);
    custom_touch_gesture_reset(reset.gesture);
    reset.touch(0, false, 10);
    reset.touch(1, false, 11);
    assert(reset.step(300).action == CustomTouchAction::None);
}

} // namespace

int main() {
    ordinary_input();
    screenshot_and_rearm();
    recording_and_click_suppression();
    cancel_on_motion_or_lift_or_bad_pair();
    finger_pad_and_press_transients();
    ids_slots_gap_and_reset();
    std::puts("custom touch gesture tests passed");
}
