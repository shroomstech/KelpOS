#include "../drivers/vga_terminal.h"
#include "../drivers/serial.h"
#include "../boot/multiboot.h"
#include "../graphics/framebuffer.h"
#include "../graphics/desktop.h"
#include "../memory/heap.h"
#include "panic.h"
#include "shell.h"

#include <stdbool.h>
static bool serial_available;

static void kernel_log(const char *message)
{
    terminal_write_string(message);
    if (serial_available) {
        serial_write_string(message);
    }
}

void kernel_main(uint32_t multiboot_info_address)
{
    struct graphics_framebuffer framebuffer;
    bool framebuffer_available = false;
    const struct multiboot_info_v1 *multiboot_info =
        (const struct multiboot_info_v1 *)(uintptr_t)multiboot_info_address;

    terminal_initialize();
    serial_available = serial_initialize();
    kernel_log("KelpOS boot: kernel entry reached.\n");

    kernel_log("KelpOS boot: GRUB Multiboot handoff accepted.\n");
    kernel_log("KelpOS boot: x86_64 long mode active.\n");

    kernel_heap_initialize();
    kernel_log("KelpOS boot: 64 KiB bootstrap heap ready.\n");

    if (multiboot_info != NULL
            && (multiboot_info->flags & MULTIBOOT_INFO_FRAMEBUFFER) != 0
            && multiboot_info->framebuffer_type == MULTIBOOT_FRAMEBUFFER_RGB
            && multiboot_info->framebuffer_bpp == 32) {
        const uint64_t framebuffer_size =
            (uint64_t)multiboot_info->framebuffer_pitch * multiboot_info->framebuffer_height;
        if (framebuffer_size > 0
                && multiboot_info->framebuffer_address < 0x100000000ULL
                && framebuffer_size <= 0x100000000ULL - multiboot_info->framebuffer_address) {
            framebuffer_available = graphics_framebuffer_initialize(
                &framebuffer,
                (volatile void *)(uintptr_t)multiboot_info->framebuffer_address,
                multiboot_info->framebuffer_width,
                multiboot_info->framebuffer_height,
                multiboot_info->framebuffer_pitch);
        }
    }

    kernel_log(serial_available
        ? "KelpOS boot: COM1 serial diagnostics ready.\n"
        : "KelpOS boot: COM1 unavailable; continuing on VGA.\n");

    if (framebuffer_available) {
        kernel_log("KelpOS boot: GRUB RGB framebuffer ready. Starting KelpDE.\n");
        desktop_run(&framebuffer);
    }

    kernel_log("KelpOS boot: VGA text console ready. Starting TTY shell.\n");
    shell_run();
}