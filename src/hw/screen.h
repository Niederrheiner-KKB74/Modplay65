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

/* Read the screen location and line width from the VIC-IV, clear it. */
void screen_init(void);

/* Fill the screen with spaces. */
void screen_clear(void);

/* Write an ASCII string at column x, row y. Letters show in one case. */
void screen_puts(uint8_t x, uint8_t y, const char *s);

/* Write value as hex with the given number of digits (1-8). */
void screen_hex(uint8_t x, uint8_t y, uint32_t value, uint8_t digits);

/* Write value as decimal, right-aligned in width characters. */
void screen_dec(uint8_t x, uint8_t y, uint32_t value, uint8_t width);

/* Fill len characters from column x, row y with spaces. */
void screen_clear_span(uint8_t x, uint8_t y, uint8_t len);

/* Show len characters from column x, row y reversed (on != 0) or normal. */
void screen_reverse(uint8_t x, uint8_t y, uint8_t len, uint8_t on);

#endif
