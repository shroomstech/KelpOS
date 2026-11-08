#ifndef KELPOS_GRAPHICS_DESKTOP_H
#define KELPOS_GRAPHICS_DESKTOP_H

#include "framebuffer.h"

void desktop_draw(struct graphics_framebuffer *framebuffer, bool terminal_open,
    const char *const lines[6], const char *input);
_Noreturn void desktop_run(struct graphics_framebuffer *framebuffer);

#endif