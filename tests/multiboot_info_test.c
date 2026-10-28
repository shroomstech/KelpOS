#include "../boot/multiboot.h"

#include <stddef.h>
#include <stdio.h>

_Static_assert(offsetof(struct multiboot_info_v1, framebuffer_address) == 88,
    "Multiboot framebuffer address offset must match the v1 specification");
_Static_assert(offsetof(struct multiboot_info_v1, framebuffer_pitch) == 96,
    "Multiboot framebuffer pitch offset must match the v1 specification");
_Static_assert(offsetof(struct multiboot_info_v1, framebuffer_bpp) == 108,
    "Multiboot framebuffer depth offset must match the v1 specification");

int main(void)
{
    puts("Multiboot framebuffer info layout tests passed");
    return 0;
}