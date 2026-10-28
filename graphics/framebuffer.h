#ifndef KELPOS_GRAPHICS_FRAMEBUFFER_H
#define KELPOS_GRAPHICS_FRAMEBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct graphics_framebuffer {
    volatile uint32_t *pixels;
    size_t width;
    size_t height;
    size_t pitch_bytes;
};

bool graphics_framebuffer_initialize(
    struct graphics_framebuffer *framebuffer,
    volatile void *address,
    size_t width,
    size_t height,
    size_t pitch_bytes);
bool graphics_put_pixel(
    struct graphics_framebuffer *framebuffer,
    size_t x,
    size_t y,
    uint32_t color);
bool graphics_fill_rect(
    struct graphics_framebuffer *framebuffer,
    size_t x,
    size_t y,
    size_t width,
    size_t height,
    uint32_t color);

#endif