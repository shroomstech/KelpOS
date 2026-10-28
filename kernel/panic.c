#include "panic.h"

#include "../drivers/vga_terminal.h"

_Noreturn void kernel_panic(const char *reason)
{
    terminal_initialize();
    terminal_set_color(0x4F);
    terminal_write_string("KERNEL PANIC\n");
    terminal_set_color(0x0F);
    terminal_write_string(reason != NULL ? reason : "No panic reason supplied.");
    terminal_write_string("\nThe system has halted.\n");

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}