#ifndef KELPOS_GRAPHICS_FONT_H
#define KELPOS_GRAPHICS_FONT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "framebuffer.h"

bool graphics_draw_text(
    struct graphics_framebuffer *framebuffer,
    size_t x,
    size_t y,
    const char *text,
    uint32_t color,
    size_t scale);

#endif