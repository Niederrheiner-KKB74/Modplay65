/*
 * Modplay65 - direct text screen output and a pixel canvas.
 *
 * The screen runs in the VIC-IV's 16-bit character mode: every cell has two
 * bytes of screen RAM (character number) and two bytes of colour RAM
 * (attributes, colour). Characters 0-255 are a RAM copy of the ROM set with
 * lower and upper case; characters from 256 on are the cells of the pixel
 * canvas, whose glyphs follow the copy in the same RAM block.
 */
#include "hw/screen.h"

#include <string.h>

#include <mega65/memory.h>

/* VIC-IV registers. */
#define VIC4_CTRL54      (*(volatile uint8_t *)0xD054) /* bit 0 = CHR16 */
#define VIC4_LINESTEP    ((volatile uint8_t *)0xD058)
#define VIC4_SCRNPTR_LO  (*(volatile uint8_t *)0xD060)
#define VIC4_SCRNPTR_HI  (*(volatile uint8_t *)0xD061)
#define VIC4_CHARPTR     ((volatile uint8_t *)0xD068)

#define COLOUR_RAM 0xFF80000UL
#define CHAR_ROM_LOWER 0xFF7E800UL /* ROM set with lower and upper case */
#define COLOUR_WHITE 1
#define COLUMNS 80
#define LINE_STEP (COLUMNS * 2)
#define FIRST_CANVAS_CHAR 256U

static uint8_t *screen_base;

/*
 * 256 text glyphs, then the canvas glyphs, 8 bytes each, at a fixed address
 * that the linker script keeps free ($B500-$CEFF).
 *
 * The address matters: the VIC-IV takes glyph data from the character ROM
 * instead of RAM when the glyph address is in $1000-$1FFF or $9000-$9FFF
 * (viciv.vhdl). A block that touches those ranges shows ROM characters.
 */
#define GLYPH_BLOCK 0xB500U
#define GLYPH_BLOCK_SIZE ((256U + SCREEN_CANVAS_CELLS) * 8U)
_Static_assert(GLYPH_BLOCK + GLYPH_BLOCK_SIZE <= 0xCF00U,
               "glyph block must end below the SID trampoline page");
#define glyphs ((uint8_t *)GLYPH_BLOCK)
#define CANVAS_PIXELS (glyphs + 256U * 8U)

/* offset of each canvas cell row in the glyph block */
#define CANVAS_MAX_ROWS 32
static uint16_t canvas_row_offset[CANVAS_MAX_ROWS];

/* Saved video settings, restored by screen_restore(). */
static uint8_t saved_ctrl54;
static uint8_t saved_linestep[2];
static uint8_t saved_charptr[3];

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

/* Pointer to the low byte of a cell; the high byte follows. */
static uint8_t *cell(uint8_t x, uint8_t y)
{
    return screen_base + (uint16_t)y * LINE_STEP + (uint16_t)x * 2;
}

/* Fill count cells of far memory with the byte pair (b0, b1). */
static void fill_pairs(uint32_t address, uint8_t b0, uint8_t b1,
                       uint16_t count)
{
    if (count == 0) {
        return;
    }
    lpoke(address, b0);
    lpoke(address + 1, b1);
    if (count > 1) {
        /* forward overlapping copy repeats the first pair */
        lcopy(address, address + 2, (uint16_t)((count - 1) * 2));
    }
}

void screen_clear(void)
{
    fill_pairs((uint32_t)(uintptr_t)screen_base, 0x20, 0x00,
               COLUMNS * SCREEN_ROWS);
    fill_pairs(COLOUR_RAM, 0x00, COLOUR_WHITE, COLUMNS * SCREEN_ROWS);
}

void screen_init(void)
{
    uint32_t glyph_address = GLYPH_BLOCK;

    saved_ctrl54 = VIC4_CTRL54;
    saved_linestep[0] = VIC4_LINESTEP[0];
    saved_linestep[1] = VIC4_LINESTEP[1];
    saved_charptr[0] = VIC4_CHARPTR[0];
    saved_charptr[1] = VIC4_CHARPTR[1];
    saved_charptr[2] = VIC4_CHARPTR[2];

    screen_base = (uint8_t *)(uintptr_t)(
        ((uint16_t)VIC4_SCRNPTR_HI << 8) | VIC4_SCRNPTR_LO);
    lcopy(CHAR_ROM_LOWER, glyph_address, 256U * 8U);
    memset(CANVAS_PIXELS, 0, SCREEN_CANVAS_CELLS * 8U);
    screen_clear();

    VIC4_CHARPTR[0] = (uint8_t)glyph_address;
    VIC4_CHARPTR[1] = (uint8_t)(glyph_address >> 8);
    VIC4_CHARPTR[2] = 0;
    VIC4_LINESTEP[0] = LINE_STEP;
    VIC4_LINESTEP[1] = 0;
    VIC4_CTRL54 |= 0x01;
}

void screen_restore(void)
{
    VIC4_CTRL54 = saved_ctrl54;
    VIC4_LINESTEP[0] = saved_linestep[0];
    VIC4_LINESTEP[1] = saved_linestep[1];
    VIC4_CHARPTR[0] = saved_charptr[0];
    VIC4_CHARPTR[1] = saved_charptr[1];
    VIC4_CHARPTR[2] = saved_charptr[2];
}

void screen_puts(uint8_t x, uint8_t y, const char *s)
{
    uint8_t *p = cell(x, y);

    while (*s) {
        p[0] = to_screencode(*s++);
        p[1] = 0;
        p += 2;
    }
}

void screen_putn(uint8_t x, uint8_t y, const char *s, uint8_t width)
{
    uint8_t *p = cell(x, y);

    while (width && *s) {
        p[0] = to_screencode(*s++);
        p[1] = 0;
        p += 2;
        --width;
    }
    while (width--) {
        p[0] = 0x20;
        p[1] = 0;
        p += 2;
    }
}

void screen_poke(uint8_t x, uint8_t y, uint8_t code)
{
    uint8_t *p = cell(x, y);

    p[0] = code;
    p[1] = 0;
}

void screen_hex(uint8_t x, uint8_t y, uint32_t value, uint8_t digits)
{
    uint8_t *p = cell(x, y) + digits * 2;

    while (digits--) {
        uint8_t d = (uint8_t)(value & 0x0F);

        p -= 2;
        p[0] = (uint8_t)(d < 10 ? '0' + d : 'A' + d - 10);
        p[1] = 0;
        value >>= 4;
    }
}

void screen_dec(uint8_t x, uint8_t y, uint32_t value, uint8_t width)
{
    uint8_t *p = cell(x, y) + width * 2;

    do {
        p -= 2;
        p[0] = (uint8_t)('0' + (uint8_t)(value % 10));
        p[1] = 0;
        value /= 10;
    } while (--width && value);
    while (width--) {
        p -= 2;
        p[0] = 0x20;
        p[1] = 0;
    }
}

void screen_clear_span(uint8_t x, uint8_t y, uint8_t len)
{
    screen_putn(x, y, "", len);
}

void screen_reverse(uint8_t x, uint8_t y, uint8_t len, uint8_t on)
{
    uint8_t *p = cell(x, y);

    while (len--) {
        *p = on ? (uint8_t)(*p | 0x80) : (uint8_t)(*p & 0x7F);
        p += 2;
    }
}

void screen_palette(uint8_t border, uint8_t background)
{
    POKE(0xD020U, border);
    POKE(0xD021U, background);
}

void screen_colour(uint8_t x, uint8_t y, uint8_t len, uint8_t colour)
{
    fill_pairs(COLOUR_RAM + (uint16_t)y * LINE_STEP + (uint16_t)x * 2, 0x00,
               colour, len);
}

void screen_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour,
                const char *title)
{
    uint8_t i;

    for (i = 1; i + 1 < w; ++i) {
        screen_poke((uint8_t)(x + i), y, 0x40);
        screen_poke((uint8_t)(x + i), (uint8_t)(y + h - 1), 0x40);
    }
    screen_poke(x, y, 0x70);
    screen_poke((uint8_t)(x + w - 1), y, 0x6E);
    screen_poke(x, (uint8_t)(y + h - 1), 0x6D);
    screen_poke((uint8_t)(x + w - 1), (uint8_t)(y + h - 1), 0x7D);
    for (i = 1; i + 1 < h; ++i) {
        screen_poke(x, (uint8_t)(y + i), 0x5D);
        screen_poke((uint8_t)(x + w - 1), (uint8_t)(y + i), 0x5D);
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
        p[0] = (uint8_t)(i < filled ? 0xA0 : 0x20);
        p[1] = 0;
        p += 2;
    }
}

/* ---- pixel canvas --------------------------------------------------- */

uint8_t *screen_canvas_show(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint16_t number = FIRST_CANVAS_CHAR;
    uint8_t cx, cy;

    if ((uint16_t)w * h > SCREEN_CANVAS_CELLS || h > CANVAS_MAX_ROWS) {
        return 0;
    }
    for (cy = 0; cy < h; ++cy) {
        canvas_row_offset[cy] = (uint16_t)cy * w * 8U;
    }
    memset(CANVAS_PIXELS, 0, (uint16_t)w * h * 8U);
    for (cy = 0; cy < h; ++cy) {
        uint8_t *p = cell(x, (uint8_t)(y + cy));

        for (cx = 0; cx < w; ++cx) {
            p[0] = (uint8_t)number;
            p[1] = (uint8_t)(number >> 8);
            p += 2;
            ++number;
        }
    }
    return CANVAS_PIXELS;
}

void screen_canvas_move_row(uint8_t x, uint8_t from_y, uint8_t to_y,
                            uint8_t w)
{
    uint8_t *from = cell(x, from_y);
    uint8_t *to = cell(x, to_y);

    if (from == to) {
        return;
    }
    while (w--) {
        to[0] = from[0];
        to[1] = from[1];
        from[0] = 0x20;
        from[1] = 0;
        from += 2;
        to += 2;
    }
}

void screen_canvas_vline(uint16_t x, uint8_t y0, uint8_t y1, uint8_t mask,
                         uint8_t set)
{
    /* glyph bytes of the cell column, one cell row after the other;
     * no multiplications in here, this runs for every scope column */
    uint8_t *column = CANVAS_PIXELS + ((x >> 3) << 3);
    uint8_t *p = column + canvas_row_offset[y0 >> 3] + (y0 & 7);
    uint8_t y = y0;

    for (;;) {
        if (set) {
            *p |= mask;
        } else {
            *p &= (uint8_t)~mask;
        }
        if (y == y1) {
            break;
        }
        ++y;
        if (y & 7) {
            ++p;
        } else {
            p = column + canvas_row_offset[y >> 3];
        }
    }
}
