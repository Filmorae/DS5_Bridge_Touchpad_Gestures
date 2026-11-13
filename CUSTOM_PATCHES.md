# Custom firmware patch: two-finger capture gestures

Based on [SundayMoments/DS5_Bridge](https://github.com/SundayMoments/DS5_Bridge),
tag **v2.0.1**, commit `ba839b5104ffb775a4f20a3f5426be98b387c1a8`.
The gesture behavior comes from the 1.7.1 fork at `ff564bc`. Build the standard Pico 2 W
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
is released. Other buttons and touch coordinates remain live.

The first contact may settle by up to **72 X / 48 Y raw units** before the
second arrives. Both positions are then anchored at the start of the
decision window. During the window, each contact may deviate at most
**72 X / 48 Y units** for a screenshot. The upstream decoder reads two
independent 12-bit XY contacts across a 1920×1080 touch area.
One contact may briefly disappear for up to
**12 ms** if the same ID returns inside the movement limits; a screenshot
waits until both are present. A recording click can use a valid pair seen
within the previous **20 ms** to survive contact changes caused by the
physical press. Recording retains the **48 X / 32 Y** pair tolerance and the
**96 X / 64 Y** allowance at the click edge.
IDs are tracked independently of slot order. A report gap above 250 ms
rejects pending recognition until the pad is empty. An unexpected or
truncated controller report also cancels waiting gesture shortcuts.

After the first screenshot, observing one original finger lift makes the
surviving contact ID the anchor. It can move anywhere on the pad. Each new
tapping contact triggers another screenshot as it lands. The pad must report
the anchor alone between taps. The previous tapping ID returning within
**12 ms** counts as a brief loss and sends nothing. An anchor missing for more
than **12 ms** while a tapping finger remains ends the session and requires a
clear pad. An empty pad ends the session immediately. A click on the same
report as a tapping contact's arrival passes through and skips that shot.

`src/custom_touch_gesture.{h,cpp}` holds the allocation-free recognizer.
`src/main.cpp` stamps controller packets at Bluetooth receive time. The
receive batch carries each timestamp and any input discontinuity into
`process_controller_input_packet`. Recognition runs after normal Companion
processing and decoding, before the USB delivery queue. Only a matched
recording press clears the click from the raw report and decoded state.
Discontinuities and persona, USB or Wake-on-LAN transitions cancel the
gesture while keeping any existing click suppression until release. A
Bluetooth disconnect resets the recognizer for the new controller session.

`src/companion.{h,cpp}` submits the two shortcuts to the Bridge Keyboard
HID sender. `BridgeKeyboardShortcuts` uses the upstream
`KeyboardTapSequencer` for both mute and gesture reports, with a four-entry
gesture queue. Only one tap owns the keyboard until its key and modifiers
are released. Mute takes priority between gestures, and cancellation of
touch shortcuts retains any needed release and preserves the mute queue.
`CMakeLists.txt` includes the recognizer in the Companion firmware and
relocates it to SRAM. The host tests cover recognition, click filtering,
packet metadata and serialized keyboard delivery.

When rebasing, check the hook after `companion_process_controller_report`,
the 63-byte DualSense report's touch bytes 32–39 and click bit 1 at byte 9,
the contact decoder, the per-persona encoder, the Bridge Keyboard sender and
the SRAM verification. On Windows, check actual finger jitter, keyboard
bindings, click suppression and reconnect behavior on a Pico 2 W.
