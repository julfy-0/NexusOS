# NexusOS Input Drivers

This directory contains the low-level input drivers used by the kernel.

## Keyboard

`keyboard/keyboard.c` implements an i8042/PS/2 keyboard driver using translated
Set-1 scancodes. It handles:

- normal ASCII keys and Shift
- Caps Lock, Num Lock and Scroll Lock state
- keyboard LEDs
- Ctrl and Alt modifier state
- E0 extended keys and cursor/navigation keys
- IRQ1 delivery to the shell/GUI

## Mouse

`mouse/mouse.c` implements the i8042 auxiliary PS/2 mouse port. It supports:

- IRQ12 delivery
- standard 3-byte packets
- automatic IntelliMouse wheel detection
- 4-byte wheel packets
- left/right/middle and extra button bits
- signed movement and overflow rejection
- framebuffer-sized coordinate clamping

## Current USB status

NexusOS already has an xHCI HID boot-keyboard path under `drivers/usb/`.
The PS/2 drivers here are independent and remain useful on legacy/virtualized
machines. USB HID mouse support is a separate next step because it requires
USB HID enumeration and an interrupt-IN endpoint for the mouse interface.

## Debugging

Use the shell command `inputinfo` after boot to inspect keyboard modifier/lock
state and the mouse position/button/wheel capability.
