#include "framebuffer.h"

#include <stdint.h>

bool graphics_framebuffer_initialize(
    struct graphics_framebuffer *framebuffer,
    volatile void *address,
    size_t width,
    size_t height,
    size_t pitch_bytes)
{
    if (framebuffer == NULL || address == NULL || width == 0 || height == 0
            || width > SIZE_MAX / sizeof(uint32_t)
            || pitch_bytes < width * sizeof(uint32_t)
            || pitch_bytes % sizeof(uint32_t) != 0
            || height > SIZE_MAX / pitch_bytes) {
        return false;
    }

    framebuffer->pixels = (volatile uint32_t *)address;
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->pitch_bytes = pitch_bytes;
    return true;
}

bool graphics_put_pixel(
    struct graphics_framebuffer *framebuffer,
    size_t x,
    size_t y,
    uint32_t color)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL
            || x >= framebuffer->width || y >= framebuffer->height) {
        return false;
    }

    const size_t pitch_pixels = framebuffer->pitch_bytes / sizeof(uint32_t);
    framebuffer->pixels[y * pitch_pixels + x] = color;
    return true;
}

bool graphics_fill_rect(
    struct graphics_framebuffer *framebuffer,
    size_t x,
    size_t y,
    size_t width,
    size_t height,
    uint32_t color)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL
            || x > framebuffer->width || y > framebuffer->height
            || width > framebuffer->width - x
            || height > framebuffer->height - y) {
        return false;
    }

    for (size_t row = 0; row < height; row++) {
        for (size_t column = 0; column < width; column++) {
            if (!graphics_put_pixel(framebuffer, x + column, y + row, color)) {
                return false;
            }
        }
    }
    return true;
}