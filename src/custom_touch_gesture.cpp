#include "custom_touch_gesture.h"

#ifdef PICO_RP2350
#include "pico.h"
#else
#define __not_in_flash_func(name) name
#endif

namespace {

constexpr uint32_t kPairWindowUs = 120000;
constexpr uint32_t kDecisionWindowUs = 150000;
constexpr uint32_t kReportGapUs = 250000;
constexpr uint32_t kBriefContactLossUs = 12000;
constexpr uint32_t kRecentPairForClickUs = 20000;
// DualSense coordinates are 0..1919 by 0..1079. Permit landing to settle
// before the second contact, then anchor both fingers for the decision window.
constexpr int32_t kLandingX = 72;
constexpr int32_t kLandingY = 48;
constexpr int32_t kStillX = 48;
constexpr int32_t kStillY = 32;
constexpr int32_t kClickMotionX = 96;
constexpr int32_t kClickMotionY = 64;

uint8_t active_count(BridgeControllerState const &report) {
    return static_cast<uint8_t>(report.touch_points[0].active + report.touch_points[1].active);
}

CustomTouchContact contact(BridgeTouchPoint const &point) {
    return {point.contact_id, point.x, point.y};
}

BridgeTouchPoint const *find_contact(BridgeControllerState const &report, uint8_t id) {
    for (auto const &point : report.touch_points) {
        if (point.active && point.contact_id == id) return &point;
    }
    return nullptr;
}

bool stationary(BridgeTouchPoint const &point, CustomTouchContact const &start,
                int32_t max_x, int32_t max_y) {
    const int32_t dx = static_cast<int32_t>(point.x) - start.x;
    const int32_t dy = static_cast<int32_t>(point.y) - start.y;
    return dx >= -max_x && dx <= max_x
        && dy >= -max_y && dy <= max_y;
}

} // namespace

void custom_touch_gesture_reset(CustomTouchGestureState &gesture) {
    gesture = {};
}

CustomTouchResult __not_in_flash_func(custom_touch_gesture_process)(
    CustomTouchGestureState &gesture,
    BridgeControllerState const &report,
    uint32_t now_us
) {
    CustomTouchResult result{};
    const uint8_t count = active_count(report);
    const bool click = report.touchpad;
    const bool click_edge = click && !gesture.last_click;
    if (!click) gesture.suppress_held_click = false;
    gesture.last_click = click;
    result.suppress_click = click && gesture.suppress_held_click;

    // A stalled stream cannot prove that two contacts remained static.
    if (gesture.phase != CustomTouchPhase::Idle
        && static_cast<uint32_t>(now_us - gesture.last_report_us) > kReportGapUs) {
        gesture.phase = count == 0 ? CustomTouchPhase::Idle : CustomTouchPhase::Rejected;
    }
    gesture.last_report_us = now_us;

    if (gesture.phase == CustomTouchPhase::Pending) {
        const uint32_t elapsed = now_us - gesture.second_at_us;
        auto const *first = find_contact(report, gesture.first.id);
        auto const *second = find_contact(report, gesture.second.id);
        const bool valid = count == 2 && first && second && first != second
            && stationary(*first, gesture.first, kStillX, kStillY)
            && stationary(*second, gesture.second, kStillX, kStillY);
        const bool known_contacts = count == 0
            || (count == 1 && (first || second))
            || (count == 2 && first && second && first != second);
        const bool click_motion_ok = (!first || stationary(*first, gesture.first, kClickMotionX, kClickMotionY))
            && (!second || stationary(*second, gesture.second, kClickMotionX, kClickMotionY));

        // The pad can briefly lose or shift a contact as it is physically
        // pressed. Use the immediately preceding valid pair for this edge.
        if (click_edge && elapsed <= kDecisionWindowUs
            && known_contacts && click_motion_ok
            && (!gesture.contact_missing || now_us - gesture.contact_missing_since_us <= kBriefContactLossUs)
            && (valid || now_us - gesture.last_valid_pair_us <= kRecentPairForClickUs)) {
            gesture.phase = CustomTouchPhase::Latched;
            gesture.suppress_held_click = true;
            result.action = CustomTouchAction::RecordToggle;
            result.suppress_click = true;
            return result;
        }
        if (!valid) {
            if (count == 1 && !click && (first || second)) {
                if (!gesture.contact_missing) {
                    gesture.contact_missing = true;
                    gesture.contact_missing_since_us = now_us;
                }
                if (now_us - gesture.contact_missing_since_us <= kBriefContactLossUs) {
                    return result;
                }
            }
            gesture.phase = count == 0 ? CustomTouchPhase::Idle : CustomTouchPhase::Rejected;
            if (count == 0) gesture.contact_missing = false;
            return result;
        }
        if (gesture.contact_missing
            && now_us - gesture.contact_missing_since_us > kBriefContactLossUs) {
            gesture.phase = CustomTouchPhase::Rejected;
            return result;
        }
        gesture.contact_missing = false;
        gesture.last_valid_pair_us = now_us;
        if (elapsed >= kDecisionWindowUs && !click) {
            gesture.phase = CustomTouchPhase::Latched;
            result.action = CustomTouchAction::Screenshot;
        } else if (click) {
            gesture.phase = CustomTouchPhase::Rejected;
        }
        return result;
    }

    if (count == 0) {
        gesture.phase = CustomTouchPhase::Idle;
        gesture.contact_missing = false;
        return result;
    }
    if (gesture.phase == CustomTouchPhase::Rejected) return result;
    if (gesture.phase == CustomTouchPhase::Latched) {
        result.suppress_click = click && gesture.suppress_held_click;
        return result;
    }

    if (gesture.phase == CustomTouchPhase::Idle) {
        if (count == 1 && !click) {
            gesture.first = contact(report.touch_points[report.touch_points[0].active ? 0 : 1]);
            gesture.first_at_us = now_us;
            gesture.phase = CustomTouchPhase::First;
        } else if (count == 2 && (!click || click_edge)
                   && report.touch_points[0].contact_id != report.touch_points[1].contact_id) {
            // Both contacts first arrived in the same report.
            gesture.first = contact(report.touch_points[0]);
            gesture.second = contact(report.touch_points[1]);
            gesture.first_at_us = gesture.second_at_us = now_us;
            gesture.last_valid_pair_us = now_us;
            gesture.contact_missing = false;
            gesture.phase = CustomTouchPhase::Pending;
            if (click_edge) {
                gesture.phase = CustomTouchPhase::Latched;
                gesture.suppress_held_click = true;
                result.action = CustomTouchAction::RecordToggle;
                result.suppress_click = true;
            }
        } else {
            gesture.phase = CustomTouchPhase::Rejected;
        }
        return result;
    }

    if (gesture.phase == CustomTouchPhase::First) {
        auto const *first = find_contact(report, gesture.first.id);
        if (!first || !stationary(*first, gesture.first, kLandingX, kLandingY)
            || static_cast<uint32_t>(now_us - gesture.first_at_us) > kPairWindowUs) {
            gesture.phase = CustomTouchPhase::Rejected;
        } else if (count == 2) {
            auto const &other = report.touch_points[report.touch_points[0].contact_id == gesture.first.id ? 1 : 0];
            if (other.contact_id == gesture.first.id) {
                gesture.phase = CustomTouchPhase::Rejected;
            } else {
                gesture.first = contact(*first);
                gesture.second = contact(other);
                gesture.second_at_us = now_us;
                gesture.last_valid_pair_us = now_us;
                gesture.contact_missing = false;
                gesture.phase = CustomTouchPhase::Pending;
                if (click_edge) {
                    gesture.phase = CustomTouchPhase::Latched;
                    gesture.suppress_held_click = true;
                    result.action = CustomTouchAction::RecordToggle;
                    result.suppress_click = true;
                }
            }
        } else if (click) {
            gesture.phase = CustomTouchPhase::Rejected;
        }
        return result;
    }

    return result;
}
