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

#endif
