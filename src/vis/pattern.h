/*
 * Modplay65 - pattern view: the rows around the one being played, in
 * tracker notation.
 */
#ifndef MODPLAY65_VIS_PATTERN_H
#define MODPLAY65_VIS_PATTERN_H

#include <stdint.h>

#define PATTERN_VIEW_W 38
#define PATTERN_VIEW_ROWS 21

/*
 * Draw PATTERN_VIEW_ROWS rows of a pattern at column x, row y, with row
 * `current` in the middle and highlighted. rows points to the whole
 * pattern (64 rows of 16 bytes).
 */
void pattern_draw(uint8_t x, uint8_t y, const uint8_t *rows, uint8_t current);

/* Write the three-character name of a period ("C-2", "---") at x, y. */
void pattern_note_name(uint8_t x, uint8_t y, uint16_t period);

#endif
