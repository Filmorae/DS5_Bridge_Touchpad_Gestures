#include "custom_touch_gesture.h"
#include "dualsense_input_decoder.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>

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

void repeat_screenshots_follow_the_remaining_contact() {
    Harness h;
    h.touch(0, true, 4);
    h.touch(1, true, 5, 900, 600);
    h.step(0);
    assert(h.step(150).action == CustomTouchAction::Screenshot);
    h.touch(0, false, 99); // An inactive slot can carry an unrelated ID.
    assert(h.step(160).action == CustomTouchAction::None);
    h.touch(0, true, 4);
    assert(h.step(168).action == CustomTouchAction::None); // Brief return of the original finger.
    h.touch(0, false, 4);
    h.step(170);
    h.touch(0, true, 6);
    h.touch(1, true, 5, 110, 1000);
    assert(h.step(176).action == CustomTouchAction::Screenshot); // New ID can tap immediately.
    assert(h.step(300).action == CustomTouchAction::None);
    h.touch(0, true, 5, 1820, 50); // The held ID moves and swaps slots.
    h.touch(1, false, 6);
    h.step(310);
    h.touch(1, true, 7);
    assert(h.step(320).action == CustomTouchAction::Screenshot);

    h.touch(0, false, 5);
    assert(h.step(330).action == CustomTouchAction::None);
    h.touch(0, true, 5, 50, 1050);
    assert(h.step(340).action == CustomTouchAction::None); // Brief loss of the held ID.
    h.touch(0, false, 5);
    h.step(350);
    h.step(363);
    h.touch(0, true, 8);
    assert(h.step(370).action == CustomTouchAction::None); // The tapping finger cannot take over.
    assert(h.step(520).action == CustomTouchAction::None);
    h.touch(0, false, 8);
    h.touch(1, false, 7);
    h.step(530);
    h.touch(0, true, 9);
    h.touch(1, true, 10);
    assert(h.step(540).action == CustomTouchAction::None);
    assert(h.step(690).action == CustomTouchAction::Screenshot);
}

void repeat_screenshot_click_passthrough_and_disarm() {
    Harness h;
    h.touch(0, true, 11);
    h.touch(1, true, 12);
    h.step(0);
    assert(h.step(150).action == CustomTouchAction::Screenshot);
    h.touch(1, false, 12);
    h.step(160);
    h.touch(1, true, 13);
    const auto click = h.step(170, true);
    assert(click.action == CustomTouchAction::None && !click.suppress_click);
    assert(h.step(180).action == CustomTouchAction::None); // Click release cannot start a tap.
    h.touch(1, false, 13);
    h.step(190);
    h.touch(1, true, 14);
    assert(h.step(200).action == CustomTouchAction::Screenshot);
    h.touch(1, false, 14);
    h.step(210);
    custom_touch_gesture_disarm(h.gesture);
    h.touch(1, true, 15);
    assert(h.step(220).action == CustomTouchAction::None);
    assert(h.step(370).action == CustomTouchAction::None);
}

void repeat_screenshot_has_no_single_finger_timeout() {
    Harness h;
    h.touch(0, true, 21);
    h.touch(1, true, 22);
    h.step(0);
    assert(h.step(150).action == CustomTouchAction::Screenshot);
    h.touch(1, false, 22);
    for (unsigned ms = 200; ms <= 5000; ms += 100) {
        assert(h.step(ms).action == CustomTouchAction::None);
    }
    h.touch(1, true, 22);
    assert(h.step(5010).action == CustomTouchAction::Screenshot);

    h.touch(1, false, 22);
    h.step(5020);
    h.touch(1, true, 23);
    assert(h.step(5300).action == CustomTouchAction::None); // Report stream went stale.
}

void screenshot_and_recording_share_pair_tolerance() {
    Harness h;
    h.touch(0, true, 1);
    h.touch(1, true, 2, 900, 600);
    h.step(0);
    h.touch(0, true, 1, 472, 348);
    h.touch(1, true, 2, 828, 552);
    assert(h.step(25).action == CustomTouchAction::None);
    assert(h.step(150).action == CustomTouchAction::Screenshot);

    Harness record;
    record.touch(0, true, 1);
    record.touch(1, true, 2);
    record.step(0);
    record.touch(0, true, 1, 472, 348);
    record.touch(1, true, 2, 328, 252);
    assert(record.step(25).action == CustomTouchAction::None);
    // This is beyond the 20 ms fallback from the original landing, so the
    // moved pair must itself qualify before and at the click.
    const auto click = record.step(50, true);
    assert(click.action == CustomTouchAction::RecordToggle && click.suppress_click);

    Harness drift;
    drift.touch(0, true, 1);
    drift.touch(1, true, 2);
    drift.step(0);
    drift.touch(1, true, 2, 473, 300);
    assert(drift.step(25).action == CustomTouchAction::None);
    drift.touch(1, true, 2);
    const auto rejected_click = drift.step(30, true);
    assert(rejected_click.action == CustomTouchAction::None && !rejected_click.suppress_click);

    Harness y_limit;
    y_limit.touch(0, true, 1);
    y_limit.touch(1, true, 2);
    y_limit.step(0);
    y_limit.touch(1, true, 2, 400, 349);
    assert(y_limit.step(150).action == CustomTouchAction::None);
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
    motion.touch(0, true, 1, 473, 300);
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
    press.touch(1, true, 31, 972, 648);
    assert(press.step(110).action == CustomTouchAction::None);
    press.touch(0, false, 30);
    press.touch(1, true, 31, 996, 664); // Press shifts or hides contacts in this report.
    auto pressed = press.step(120, true);
    assert(pressed.action == CustomTouchAction::RecordToggle && pressed.suppress_click);
    assert(press.step(260, true).action == CustomTouchAction::None);

    Harness excess_press;
    excess_press.touch(0, true, 32);
    excess_press.touch(1, true, 33);
    excess_press.step(0);
    excess_press.step(25);
    excess_press.touch(1, true, 33, 497, 300);
    const auto excess_click = excess_press.step(30, true);
    assert(excess_click.action == CustomTouchAction::None && !excess_click.suppress_click);

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

void click_suppression_updates_both_report_views() {
    CustomTouchGestureState gesture{};
    uint8_t report[kDualSenseUsbInputReportSize]{};
    report[0] = 0x34;
    report[9] = 0x07; // PS, touchpad click, mute.
    report[32] = 1;
    report[36] = 2;
    uint8_t expected[sizeof(report)];
    std::memcpy(expected, report, sizeof(report));
    expected[9] = 0x05;

    BridgeControllerState state{};
    assert(dualsense_decode_usb_input_report(report, sizeof(report), state));
    assert(custom_touch_gesture_apply(gesture, state, report, 1000) == CustomTouchAction::RecordToggle);
    assert(std::memcmp(report, expected, sizeof(report)) == 0);
    assert(std::memcmp(state.dualsense_report, expected, sizeof(report)) == 0);
    assert(!state.touchpad && state.home && state.mute && state.left_stick_x == 0x34);

    // A transport interruption cannot rearm a held pair, but also cannot
    // leak a click that was already consumed.
    custom_touch_gesture_disarm(gesture);
    report[9] = 0x07;
    assert(dualsense_decode_usb_input_report(report, sizeof(report), state));
    assert(custom_touch_gesture_apply(gesture, state, report, 2000) == CustomTouchAction::None);
    assert(!state.touchpad && report[9] == 0x05);

    assert(dualsense_decode_usb_input_report(report, sizeof(report), state));
    assert(custom_touch_gesture_apply(gesture, state, report, 3000) == CustomTouchAction::None);
    report[9] = 0x07;
    assert(dualsense_decode_usb_input_report(report, sizeof(report), state));
    assert(custom_touch_gesture_apply(gesture, state, report, 4000) == CustomTouchAction::None);
    assert(state.touchpad && report[9] == 0x07);
}

} // namespace

int main() {
    ordinary_input();
    screenshot_and_rearm();
    repeat_screenshots_follow_the_remaining_contact();
    repeat_screenshot_click_passthrough_and_disarm();
    repeat_screenshot_has_no_single_finger_timeout();
    screenshot_and_recording_share_pair_tolerance();
    recording_and_click_suppression();
    cancel_on_motion_or_lift_or_bad_pair();
    finger_pad_and_press_transients();
    ids_slots_gap_and_reset();
    click_suppression_updates_both_report_views();
    std::puts("custom touch gesture tests passed");
}
