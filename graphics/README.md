# Graphics

`framebuffer.c` provides pitch-aware pixel and rectangle drawing over a
caller-supplied 32-bit framebuffer. `font.c` adds a small built-in 5x7 font;
`desktop.c` draws the KelpDE desktop and a keyboard-operated graphical terminal.
GRUB's Multiboot video request supplies the framebuffer to the graphical boot
entry. This is a simple single-screen desktop prototype, without mouse input or
window management.

Run its host tests with `make graphics-test`.