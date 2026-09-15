# NexusOS Input Drivers

## Unified input core

`core/input.c` records input by source (`PS/2` or `USB HID`) and device class
(keyboard or mouse). It is intentionally a small common layer: existing shell and
GUI APIs remain unchanged, so PS/2 and USB devices feed the same higher-level state.

## Keyboard

The PS/2 i8042 keyboard supports modifiers, locks, LEDs, extended keys and deferred
IRQ processing. xHCI additionally supports USB HID Boot Protocol keyboards.

## Mouse

The PS/2 mouse supports 3/4-byte packets, wheel detection and framebuffer clamping.
xHCI now also accepts USB HID Boot Protocol mouse reports and feeds movement, buttons
and wheel data into the same mouse state used by the GUI.

## Event model

Hardware IRQ handlers remain capture-only. PS/2 bytes enter the kernel event queue;
xHCI transfer completion is polled from normal kernel context on timer events. No
scheduler context switch or GUI work is performed directly inside an IRQ handler.

Use `inputinfo` to inspect PS/2/USB availability and input event counters.
