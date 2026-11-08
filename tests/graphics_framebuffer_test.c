#include "../graphics/framebuffer.h"
#include "../graphics/font.h"
#include "../graphics/desktop.h"

/* The renderer is tested without calling its never-returning keyboard loop. */
bool keyboard_read_char(char *character)
{
    (void)character;
    return false;
}

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

    static uint32_t desktop_pixels[640 * 480];
    const char *lines[6] = {"READY", "", "", "", "", ""};
    assert(graphics_framebuffer_initialize(&framebuffer, desktop_pixels,
        640, 480, 640 * sizeof(uint32_t)));
    desktop_draw(&framebuffer, false, lines, "");
    assert(desktop_pixels[0] == 0x102831); /* top panel */
    assert(desktop_pixels[40 * 640 + 1] == 0x163942); /* wallpaper */
    assert(desktop_pixels[200 * 640 + 300] == 0xE9F0EF); /* welcome window */
    desktop_draw(&framebuffer, true, lines, "help");
    assert(desktop_pixels[200 * 640 + 300] == 0x102831); /* terminal */

    puts("graphics framebuffer/font/desktop tests passed");
    return 0;
}