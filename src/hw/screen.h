/*
 * Modplay65 - direct text screen output.
 *
 * Writes screen codes straight into screen RAM. No KERNAL calls: the
 * KERNAL's CHROUT produced scrambled output when called from an llvm-mos
 * program (see docs/HARDWARE-NOTES.md).
 */
#ifndef MODPLAY65_HW_SCREEN_H
#define MODPLAY65_HW_SCREEN_H

#include <stdint.h>

#define SCREEN_ROWS 25

/*
 * Read the screen location and line width from the VIC-IV, select the
 * lower/upper case character set and clear the screen.
 */
void screen_init(void);

/* Fill the screen with spaces. */
void screen_clear(void);

/* Write an ASCII string at column x, row y. */
void screen_puts(uint8_t x, uint8_t y, const char *s);

/* Write value as hex with the given number of digits (1-8). */
void screen_hex(uint8_t x, uint8_t y, uint32_t value, uint8_t digits);

/* Write value as decimal, right-aligned in width characters. */
void screen_dec(uint8_t x, uint8_t y, uint32_t value, uint8_t width);

/* Fill len characters from column x, row y with spaces. */
void screen_clear_span(uint8_t x, uint8_t y, uint8_t len);

/* Show len characters from column x, row y reversed (on != 0) or normal. */
void screen_reverse(uint8_t x, uint8_t y, uint8_t len, uint8_t on);

/* Colours of the default palette. */
#define COL_BLACK 0
#define COL_WHITE 1
#define COL_RED 2
#define COL_CYAN 3
#define COL_PURPLE 4
#define COL_GREEN 5
#define COL_BLUE 6
#define COL_YELLOW 7
#define COL_ORANGE 8
#define COL_BROWN 9
#define COL_PINK 10
#define COL_DGREY 11
#define COL_MGREY 12
#define COL_LGREEN 13
#define COL_LBLUE 14
#define COL_LGREY 15

/* Set border and background colour. */
void screen_palette(uint8_t border, uint8_t background);

/* Set the colour of len characters from column x, row y. */
void screen_colour(uint8_t x, uint8_t y, uint8_t len, uint8_t colour);

/* Write s into a field of exactly width characters: cut or pad with spaces. */
void screen_putn(uint8_t x, uint8_t y, const char *s, uint8_t width);

/* Draw a frame of w x h characters with an optional title in the top edge. */
void screen_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t colour,
                const char *title);

/* Draw a horizontal bar: filled of len cells solid, the rest empty. */
void screen_bar(uint8_t x, uint8_t y, uint8_t len, uint8_t filled);

/* Put one raw screen code. */
void screen_poke(uint8_t x, uint8_t y, uint8_t code);

#endif
