/*
 * Modplay65 - direct text screen output.
 */
#include "hw/screen.h"

#include <mega65/memory.h>

/* VIC-IV registers. */
#define VIC4_LINESTEP_LO (*(volatile uint8_t *)0xD058)
#define VIC4_CHRCOUNT    (*(volatile uint8_t *)0xD05E)
#define VIC4_SCRNPTR_LO  (*(volatile uint8_t *)0xD060)
#define VIC4_SCRNPTR_HI  (*(volatile uint8_t *)0xD061)
#define VIC4_CHARPTR     ((volatile uint8_t *)0xD068)

#define COLOUR_RAM 0xFF80000UL
#define COLOUR_WHITE 1

static uint8_t *screen_base;
static uint8_t line_step;

/* ASCII to screen code of the lower/upper case character set. */
static uint8_t to_screencode(char c)
{
    uint8_t u = (uint8_t)c;

    if (u >= 'a' && u <= 'z') {
        return (uint8_t)(u - 'a' + 1);
    }
    if (u >= 'A' && u <= 'Z') {
        return u;
    }
    if (u >= ' ' && u <= '?') {
        return u;
    }
    switch (u) {
    case '@': return 0x00;
    case '[': return 0x1B;
    case ']': return 0x1D;
    case '^': return 0x1E;
    case '_': return 0x64; /* low line; the set has no underscore */
    default: return 0x2E;  /* '.' for anything else */
    }
}

static uint8_t *cell(uint8_t x, uint8_t y)
{
    return screen_base + (uint16_t)y * line_step + x;
}

void screen_clear(void)
{
    uint16_t i;
    uint16_t n = (uint16_t)line_step * SCREEN_ROWS;

    for (i = 0; i < n; ++i) {
        screen_base[i] = 0x20;
    }
    lfill(COLOUR_RAM, COLOUR_WHITE, n);
}

void screen_init(void)
{
    screen_base = (uint8_t *)(uintptr_t)(
        ((uint16_t)VIC4_SCRNPTR_HI << 8) | VIC4_SCRNPTR_LO);
    line_step = VIC4_LINESTEP_LO;
    if (line_step != 40 && line_step != 80) {
        line_step = VIC4_CHRCOUNT;
    }
    if (line_step != 40 && line_step != 80) {
        line_step = 80;
    }
    /* Select the ROM character set with lower and upper case letters. */
    VIC4_CHARPTR[0] = 0x00;
    VIC4_CHARPTR[1] = 0x18;
    VIC4_CHARPTR[2] = 0x00;
    screen_clear();
}

void screen_puts(uint8_t x, uint8_t y, const char *s)
{
    uint8_t *p = cell(x, y);

    while (*s) {
        *p++ = to_screencode(*s++);
    }
}

void screen_hex(uint8_t x, uint8_t y, uint32_t value, uint8_t digits)
{
    uint8_t *p = cell(x, y) + digits;

    while (digits--) {
        uint8_t d = (uint8_t)(value & 0x0F);
        *--p = (uint8_t)(d < 10 ? '0' + d : 'A' + d - 10);
        value >>= 4;
    }
}

void screen_dec(uint8_t x, uint8_t y, uint32_t value, uint8_t width)
{
    uint8_t *p = cell(x, y) + width;

    do {
        *--p = (uint8_t)('0' + (uint8_t)(value % 10));
        value /= 10;
    } while (--width && value);
    while (width--) {
        *--p = 0x20;
    }
}

void screen_clear_span(uint8_t x, uint8_t y, uint8_t len)
{
    uint8_t *p = cell(x, y);

    while (len--) {
        *p++ = 0x20;
    }
}

void screen_reverse(uint8_t x, uint8_t y, uint8_t len, uint8_t on)
{
    uint8_t *p = cell(x, y);

    while (len--) {
        *p = on ? (uint8_t)(*p | 0x80) : (uint8_t)(*p & 0x7F);
        ++p;
    }
}

void screen_palette(uint8_t border, uint8_t background)
{
    POKE(0xD020U, border);
    POKE(0xD021U, background);
}

void screen_colour(uint8_t x, uint8_t y, uint8_t len, uint8_t colour)
{
    if (len) {
        lfill(COLOUR_RAM + (uint16_t)y * line_step + x, colour, len);
    }
}

void screen_putn(uint8_t x, uint8_t y, const char *s, uint8_t width)
{
    uint8_t *p = cell(x, y);

    while (width && *s) {
        *p++ = to_screencode(*s++);
        --width;
    }
    while (width--) {
        *p++ = 0x20;
    }
}

void screen_poke(uint8_t x, uint8_t y, uint8_t code)
{
    *cell(x, y) = code;
}

void screen_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour,
                const char *title)
{
    uint8_t i;
    uint8_t *top = cell(x, y);
    uint8_t *bottom = cell(x, (uint8_t)(y + h - 1));

    for (i = 1; i + 1 < w; ++i) {
        top[i] = 0x40;
        bottom[i] = 0x40;
    }
    top[0] = 0x70;
    top[w - 1] = 0x6E;
    bottom[0] = 0x6D;
    bottom[w - 1] = 0x7D;
    for (i = 1; i + 1 < h; ++i) {
        *cell(x, (uint8_t)(y + i)) = 0x5D;
        *cell((uint8_t)(x + w - 1), (uint8_t)(y + i)) = 0x5D;
    }
    for (i = 0; i < h; ++i) {
        screen_colour(x, (uint8_t)(y + i), w, colour);
    }
    if (title) {
        screen_poke((uint8_t)(x + 2), y, 0x20);
        screen_puts((uint8_t)(x + 3), y, title);
        for (i = 0; title[i]; ++i) {
        }
        screen_poke((uint8_t)(x + 3 + i), y, 0x20);
    }
}

void screen_bar(uint8_t x, uint8_t y, uint8_t len, uint8_t filled)
{
    uint8_t *p = cell(x, y);
    uint8_t i;

    for (i = 0; i < len; ++i) {
        *p++ = (uint8_t)(i < filled ? 0xA0 : 0x20);
    }
}
