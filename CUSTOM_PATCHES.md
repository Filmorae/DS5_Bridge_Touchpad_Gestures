# Custom firmware patch: two-finger capture gestures

Based on [SundayMoments/DS5_Bridge](https://github.com/SundayMoments/DS5_Bridge),
release **1.7.1**, branch `main`, commit
`5ee08e0984085c99eac5afe14c05f572b6e9dc59`. Build the normal Pico 2 W
Companion firmware with `ENABLE_COMPANION=ON`; the Windows Companion App need
not be running.

Two distinct touch contact IDs must become active within **120 ms**. The
**150 ms** decision window starts when the second contact appears. If both
remain active and essentially still through the window without a click, the
firmware queues one **Win+Alt+Print Screen** Game Bar screenshot. A Touchpad
Click rising edge inside the window instead queues **Alt+F2** for NVIDIA App
and cancels the screenshot. Set Alt+F2 as the recording toggle in NVIDIA App:
its default is Photo Mode, while the default recording shortcut is Alt+F9.
The triggering click bit alone is cleared
from the current and subsequent controller reports until that physical press
is released; other buttons and touch coordinates remain live. A completed or
cancelled gesture cannot rearm until all fingers leave the pad.

The first contact may settle by up to **72 X / 48 Y raw units** before the
second arrives. Both positions are then anchored at the start of the
decision window. During the window, each contact may deviate at most
**48 X / 32 Y units**; more motion cancels the shortcut. The upstream
decoder reads two independent 12-bit XY contacts; its DS4 conversion and the
Sony Linux driver identify DualSense travel as 1920×1080. Window tolerances
are about 2.5% X and 3% Y. One contact may briefly disappear for up to
**12 ms** if the same ID returns inside the movement limits; a screenshot
waits until both are present. A recording click can use a valid pair seen
within the previous **20 ms** to survive contact changes caused by the
physical press, provided the remaining contact has not moved markedly.
IDs are tracked independently of slot order. A report gap above 250 ms or an
unexpected controller report type rejects pending recognition until the pad
is empty.

`src/custom_touch_gesture.{h,cpp}` holds the fixed, allocation-free state
machine. `src/main.cpp` calls it after normal Companion processing and input
decoding, immediately before publishing the unbuffered report. It clears the
click bit in both the raw USB report and decoded state only on a recording
press, and resets state on controller disconnect, persona switch, or invalid
report. `src/companion.{h,cpp}` extends the existing Bridge Keyboard HID
sender with a four-entry fixed shortcut queue. It sends an eight-byte boot
keyboard report for 40 ms and then a zero report to release modifiers and key;
disconnect/switch drops queued actions while preserving a needed release.
`CMakeLists.txt` includes the module in Companion firmware;
`tests/firmware/{CMakeLists.txt,custom_touch_gesture_test.cpp}` covers the
recognition rules.

No USB descriptors, VID/PID, serial handling, interface layout, Bluetooth
transport, audio, haptics, or Companion protocol were changed. When rebasing,
check the `on_bt_data` hook after `companion_process_controller_report`, the
63-byte DualSense report's touch bytes 32–39 and click bit 1 at byte 9, the
contact decoder, the per-persona encoder, `host_persona_keyboard_hid_instance`,
the keyboard sender, and the SRAM hot-path verification. The gestures have
been exercised on hardware; recheck actual finger jitter, host keyboard
bindings, and click suppression in games after any upstream update.
