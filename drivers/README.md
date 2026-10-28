# Drivers

- `vga_terminal.c` writes early text output to VGA text memory.
- `keyboard.c` polls PS/2 scan-code set 1 and translates basic US keyboard input.
- `serial.c` initializes COM1 and provides bounded-polling serial diagnostics.
- `x86_io.c` provides the x86 port read/write instructions used by drivers.

Port I/O is privileged and must only be called by kernel code. The root build
compiles these objects but does not execute them on the host. The serial port is
optional; VGA output remains available if COM1 is not detected.

Keyboard input is polled rather than interrupt-driven. The TTY shell uses it
without an IDT; this is suitable for the current single-core prototype, not a
final input stack.
