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
 * Switch the 80-column screen to 16-bit character mode with a RAM copy of
 * the lower/upper case character set, and clear it.
 */
void screen_init(void);

/* Put back the video settings found by screen_init(). */
void screen_restore(void);

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

/*
 * Pixel canvas: a rectangle of character cells whose glyphs are private
 * RAM, so it can be drawn into pixel by pixel (one colour per cell, set
 * with screen_colour). Only one canvas is on screen at a time.
 */
#define SCREEN_CANVAS_CELLS 576U /* e.g. 36 x 16 cells = 288 x 128 pixels */

/*
 * Put a cleared canvas of w x h cells at column x, row y. Returns 0 if it
 * has more than SCREEN_CANVAS_CELLS cells.
 */
uint8_t *screen_canvas_show(uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/*
 * Move one row of w canvas cells from screen row from_y to row to_y
 * (same column x); the cells left behind become spaces. Lets a caller
 * leave text rows between parts of the canvas.
 */
void screen_canvas_move_row(uint8_t x, uint8_t from_y, uint8_t to_y,
                            uint8_t w);

/*
 * Set or clear the pixels selected by mask (bit 7 = leftmost of the eight
 * pixels around x) in pixel rows y0..y1 of the canvas.
 */
void screen_canvas_vline(uint16_t x, uint8_t y0, uint8_t y1, uint8_t mask,
                         uint8_t set);

#endif
