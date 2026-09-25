#ifndef DS5_BRIDGE_CUSTOM_TOUCH_GESTURE_H
#define DS5_BRIDGE_CUSTOM_TOUCH_GESTURE_H

#include <cstdint>

#include "controller_state.h"

enum class CustomTouchAction : uint8_t { None, Screenshot, RecordToggle };
enum class CustomTouchPhase : uint8_t { Idle, First, Pending, Latched, Rejected };

struct CustomTouchContact {
    uint8_t id = 0;
    uint16_t x = 0;
    uint16_t y = 0;
};

struct CustomTouchGestureState {
    CustomTouchPhase phase = CustomTouchPhase::Idle;
    CustomTouchContact first{};
    CustomTouchContact second{};
    uint32_t first_at_us = 0;
    uint32_t second_at_us = 0;
    uint32_t last_valid_pair_us = 0;
    uint32_t contact_missing_since_us = 0;
    uint32_t last_report_us = 0;
    bool contact_missing = false;
    bool last_click = false;
    bool suppress_held_click = false;
};

struct CustomTouchResult {
    CustomTouchAction action = CustomTouchAction::None;
    bool suppress_click = false;
};

// Called on every complete controller report; never holds or queues the report.
CustomTouchResult custom_touch_gesture_process(
    CustomTouchGestureState &gesture,
    BridgeControllerState const &report,
    uint32_t now_us
);
void custom_touch_gesture_reset(CustomTouchGestureState &gesture);

#endif
