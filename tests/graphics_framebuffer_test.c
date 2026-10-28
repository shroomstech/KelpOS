#include "../graphics/framebuffer.h"
#include "../graphics/font.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint32_t storage[4 * 3];
    memset(storage, 0, sizeof(storage));
    struct graphics_framebuffer framebuffer;

    assert(!graphics_framebuffer_initialize(&framebuffer, storage, 0, 3, 16));
    assert(!graphics_framebuffer_initialize(&framebuffer, storage, 4, 3, 15));
    assert(!graphics_framebuffer_initialize(&framebuffer, NULL, 4, 3, 16));
    assert(graphics_framebuffer_initialize(&framebuffer, storage, 3, 3, 16));

    assert(graphics_put_pixel(&framebuffer, 2, 1, 0x11223344));
    assert(storage[1 * 4 + 2] == 0x11223344);
    assert(storage[1 * 4 + 3] == 0);
    assert(!graphics_put_pixel(&framebuffer, 3, 1, 0xFFFFFFFF));

    assert(graphics_fill_rect(&framebuffer, 1, 0, 2, 2, 0xAABBCCDD));
    assert(storage[1] == 0xAABBCCDD);
    assert(storage[2] == 0xAABBCCDD);
    assert(storage[5] == 0xAABBCCDD);
    assert(storage[6] == 0xAABBCCDD);
    assert(storage[3] == 0);
    assert(!graphics_fill_rect(&framebuffer, 2, 2, 2, 1, 0));

    uint32_t font_storage[8 * 8];
    memset(font_storage, 0, sizeof(font_storage));
    assert(graphics_framebuffer_initialize(&framebuffer, font_storage, 8, 8, 8 * sizeof(uint32_t)));
    assert(graphics_draw_text(&framebuffer, 0, 0, "A", 0x12345678, 1));
    assert(font_storage[1] == 0x12345678);
    assert(font_storage[0] == 0);
    assert(graphics_draw_text(&framebuffer, 0, 0, "a", 0xABCDEF01, 1));
    assert(font_storage[1] == 0xABCDEF01);

    puts("graphics framebuffer/font tests passed");
    return 0;
}