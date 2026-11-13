/*
 * Modplay65 - oscilloscope view.
 *
 * All four traces share the screen canvas; trace n uses canvas cell rows
 * 4n..4n+3. On screen the traces are SCOPE_ROW_STEP rows apart, the row in
 * between is free for a label.
 */
#include "vis/scope.h"

#include "hw/audio_dma.h"
#include "hw/screen.h"

#define POINTS (SCOPE_CELLS_W * 4) /* one sample every two pixels */
#define HEIGHT (SCOPE_CELLS_H * 8) /* pixels                       */
#define CENTRE (HEIGHT / 2)

static const uint8_t colours[SCOPE_CHANNELS] = {
    COL_LGREEN, COL_CYAN, COL_YELLOW, COL_PINK
};
static const uint8_t masks[4] = { 0xC0, 0x30, 0x0C, 0x03 };

/* Pixel rows currently lit in each column, to erase them again. */
static uint8_t old_lo[SCOPE_CHANNELS][POINTS];
static uint8_t old_hi[SCOPE_CHANNELS][POINTS];
static uint8_t samples[POINTS];
static uint8_t shown;

void scope_show(uint8_t x, uint8_t y)
{
    uint8_t ch, row;
    uint8_t i;

    /* The canvas is one block of cells; the label rows between the traces
     * are not part of it, so place it trace by trace. */
    shown = (uint8_t)(screen_canvas_show(x, y, SCOPE_CELLS_W,
                                         SCOPE_CELLS_H * SCOPE_CHANNELS) != 0);
    if (!shown) {
        return;
    }
    /* screen_canvas_show put the 16 cell rows on consecutive screen rows;
     * move traces 1..3 down to leave the label rows free. */
    for (ch = SCOPE_CHANNELS - 1; ch > 0; --ch) {
        for (row = SCOPE_CELLS_H; row > 0; --row) {
            screen_canvas_move_row(
                x, (uint8_t)(y + ch * SCOPE_CELLS_H + row - 1),
                (uint8_t)(y + ch * SCOPE_ROW_STEP + row - 1), SCOPE_CELLS_W);
        }
    }
    for (ch = 0; ch < SCOPE_CHANNELS; ++ch) {
        for (row = 0; row < SCOPE_CELLS_H; ++row) {
            screen_colour(x, (uint8_t)(y + ch * SCOPE_ROW_STEP + row),
                          SCOPE_CELLS_W, colours[ch]);
        }
        for (i = 0; i < POINTS; ++i) {
            old_lo[ch][i] = 1; /* nothing lit */
            old_hi[ch][i] = 0;
        }
        scope_flat(ch);
    }
}

/*
 * Pixel offset from the centre line for every sample byte at the volume
 * last used: sample * volume / 256, limited to the trace height. That is
 * a gain of two, so a half-scale sample at full volume fills the trace.
 * Built with additions only; the CPU has no fast multiply.
 */
static int8_t scaled[256];
static uint8_t scaled_volume = 0xFF;

static void build_scale(uint8_t volume)
{
    uint16_t acc = 0; /* n * volume */
    uint8_t n;

    for (n = 0; n < 128; ++n) {
        uint8_t v = (uint8_t)(acc >> 8);

        if (v > CENTRE - 1) {
            v = CENTRE - 1;
        }
        scaled[n] = (int8_t)v;                    /* sample +n */
        scaled[(uint8_t)(0 - n)] = (int8_t)(-(int8_t)v); /* sample -n */
        acc = (uint16_t)(acc + volume);
    }
    scaled[128] = (int8_t)(-(CENTRE - 1));
    scaled_volume = volume;
}

static void draw(uint8_t channel, uint8_t volume)
{
    uint8_t base = (uint8_t)(channel * HEIGHT);
    uint8_t prev = CENTRE;
    uint8_t i;

    if (volume != scaled_volume) {
        build_scale(volume);
    }

    for (i = 0; i < POINTS; ++i) {
        int8_t v = scaled[samples[i]];
        uint8_t y;
        uint8_t lo, hi;
        uint16_t x = (uint16_t)i * 2;
        uint8_t mask = masks[i & 3];

        y = (uint8_t)(CENTRE - v);
        /* join this point to the previous one with a vertical stroke */
        if (y > prev + 1) {
            lo = (uint8_t)(prev + 1);
            hi = y;
        } else if (y + 1 < prev) {
            lo = y;
            hi = (uint8_t)(prev - 1);
        } else {
            lo = hi = y;
        }
        prev = y;
        if (lo != old_lo[channel][i] || hi != old_hi[channel][i]) {
            if (old_lo[channel][i] <= old_hi[channel][i]) {
                screen_canvas_vline(x, (uint8_t)(base + old_lo[channel][i]),
                                    (uint8_t)(base + old_hi[channel][i]),
                                    mask, 0);
            }
            screen_canvas_vline(x, (uint8_t)(base + lo), (uint8_t)(base + hi),
                                mask, 1);
            old_lo[channel][i] = lo;
            old_hi[channel][i] = hi;
        }
    }
}

void scope_draw(uint8_t channel, uint8_t dma, uint8_t volume)
{
    if (!shown) {
        return;
    }
    audio_dma_peek(dma, samples, POINTS);
    draw(channel, volume);
}

void scope_flat(uint8_t channel)
{
    uint8_t i;

    if (!shown) {
        return;
    }
    for (i = 0; i < POINTS; ++i) {
        samples[i] = 0;
    }
    draw(channel, 0);
}
