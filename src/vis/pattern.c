/*
 * Modplay65 - pattern view.
 *
 * Row format, 38 characters: "RR" and for each channel " NNNSSEEE":
 * note, sample number (hex), effect and parameter.
 */
#include "vis/pattern.h"

#include "hw/screen.h"
#include "mod/mod_play.h"

static const char note_names[12][3] = {
    "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
};
static const char hex_digits[] = "0123456789ABCDEF";

static char line[PATTERN_VIEW_W + 1];

static void note_text(char *dst, uint16_t period)
{
    uint8_t note;

    if (period == 0) {
        dst[0] = dst[1] = dst[2] = '-';
        return;
    }
    note = mod_note_of_period(period);
    dst[0] = note_names[note % 12][0];
    dst[1] = note_names[note % 12][1];
    dst[2] = (char)('1' + note / 12);
}

void pattern_note_name(uint8_t x, uint8_t y, uint16_t period)
{
    char text[4];

    note_text(text, period);
    text[3] = 0;
    screen_puts(x, y, text);
}

static void format_row(uint8_t row, const uint8_t *d)
{
    char *p = line;
    uint8_t ch;

    *p++ = (char)('0' + row / 10);
    *p++ = (char)('0' + row % 10);
    for (ch = 0; ch < MOD_CHANNELS; ++ch, d += 4) {
        uint8_t sample = (uint8_t)((d[0] & 0xF0) | (d[2] >> 4));
        uint16_t period = (uint16_t)(((uint16_t)(d[0] & 0x0F) << 8) | d[1]);
        uint8_t effect = d[2] & 0x0F;

        *p++ = ' ';
        note_text(p, period);
        p += 3;
        if (sample) {
            *p++ = hex_digits[sample >> 4];
            *p++ = hex_digits[sample & 0x0F];
        } else {
            *p++ = '.';
            *p++ = '.';
        }
        if (effect || d[3]) {
            *p++ = hex_digits[effect];
            *p++ = hex_digits[d[3] >> 4];
            *p++ = hex_digits[d[3] & 0x0F];
        } else {
            *p++ = '.';
            *p++ = '.';
            *p++ = '.';
        }
    }
    *p = 0;
}

void pattern_draw(uint8_t x, uint8_t y, const uint8_t *rows, uint8_t current)
{
    uint8_t i;

    for (i = 0; i < PATTERN_VIEW_ROWS; ++i) {
        int8_t row = (int8_t)(current + i - PATTERN_VIEW_ROWS / 2);
        uint8_t sy = (uint8_t)(y + i);

        if (row < 0 || row >= MOD_ROWS) {
            screen_putn(x, sy, "", PATTERN_VIEW_W);
            continue;
        }
        format_row((uint8_t)row, rows + (uint16_t)row * MOD_ROW_BYTES);
        screen_putn(x, sy, line, PATTERN_VIEW_W);
        if (i == PATTERN_VIEW_ROWS / 2) {
            screen_reverse(x, sy, PATTERN_VIEW_W, 1);
        }
    }
}
