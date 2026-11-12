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

#define COLOUR_RAM 0xFF80000UL
#define COLOUR_WHITE 1

static uint8_t *screen_base;
static uint8_t line_step;

static uint8_t to_screencode(char c)
{
    uint8_t u = (uint8_t)c;

    if (u >= 'a' && u <= 'z') {
        return (uint8_t)(u - 'a' + 1);
    }
    if (u >= '@' && u <= '_') {
        return (uint8_t)(u - '@');
    }
    if (u >= ' ' && u <= '?') {
        return u;
    }
    return 0x2E; /* '.' for anything else */
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
        *--p = (uint8_t)(d < 10 ? '0' + d : d - 9); /* A-F = codes 1-6 */
        value >>= 4;
    }
}
