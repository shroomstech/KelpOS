#include "font.h"

struct glyph {
    char character;
    uint8_t rows[7];
};

static const struct glyph glyphs[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}}, {'J', {7, 2, 2, 2, 18, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}}, {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}}, {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}}, {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}}, {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {':', {0, 4, 4, 0, 4, 4, 0}}, {'.', {0, 0, 0, 0, 0, 6, 6}},
    {'-', {0, 0, 0, 31, 0, 0, 0}}, {'!', {4, 4, 4, 4, 4, 0, 4}},
    {'?', {14, 17, 1, 2, 4, 0, 4}}, {'/', {1, 2, 2, 4, 8, 8, 16}},
    {'>', {16, 8, 4, 2, 4, 8, 16}}, {'<', {1, 2, 4, 8, 4, 2, 1}},
    {'+', {0, 4, 4, 31, 4, 4, 0}}, {'_', {0, 0, 0, 0, 0, 0, 31}},
    {'(', {2, 4, 8, 8, 8, 4, 2}}, {')', {8, 4, 2, 2, 2, 4, 8}},
    {'\'', {4, 4, 2, 0, 0, 0, 0}}, {'=', {0, 31, 0, 31, 0, 0, 0}},
};

static const struct glyph *find_glyph(char character)
{
    if (character >= 'a' && character <= 'z') {
        character = (char)(character - 'a' + 'A');
    }
    for (size_t index = 0; index < sizeof(glyphs) / sizeof(glyphs[0]); index++) {
        if (glyphs[index].character == character) {
            return &glyphs[index];
        }
    }
    return NULL;
}

bool graphics_draw_text(
    struct graphics_framebuffer *framebuffer,
    size_t x,
    size_t y,
    const char *text,
    uint32_t color,
    size_t scale)
{
    if (framebuffer == NULL || framebuffer->pixels == NULL || text == NULL || scale == 0) {
        return false;
    }

    size_t origin_x = x;
    for (size_t index = 0; text[index] != '\0'; index++) {
        if (text[index] == '\n') {
            x = origin_x;
            if (y > SIZE_MAX - 9 * scale) {
                return false;
            }
            y += 9 * scale;
            continue;
        }
        if (x > framebuffer->width || 6 * scale > framebuffer->width - x) {
            return false;
        }
        const struct glyph *glyph = find_glyph(text[index]);
        if (glyph != NULL) {
            for (size_t row = 0; row < 7; row++) {
                for (size_t column = 0; column < 5; column++) {
                    if ((glyph->rows[row] & (1u << (4 - column))) != 0
                            && !graphics_fill_rect(framebuffer,
                                x + column * scale, y + row * scale,
                                scale, scale, color)) {
                        return false;
                    }
                }
            }
        }
        x += 6 * scale;
    }
    return true;
}