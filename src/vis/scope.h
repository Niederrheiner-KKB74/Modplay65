/*
 * Modplay65 - oscilloscope view: one trace per channel, drawn from the
 * sample data each audio DMA channel is about to play.
 */
#ifndef MODPLAY65_VIS_SCOPE_H
#define MODPLAY65_VIS_SCOPE_H

#include <stdint.h>

#define SCOPE_CHANNELS 4
#define SCOPE_CELLS_W 36 /* width in character cells            */
#define SCOPE_CELLS_H 4  /* height of one trace in cells        */
#define SCOPE_ROW_STEP 5 /* screen rows from one trace to the next */
#define SCOPE_POINTS (SCOPE_CELLS_W * 4) /* samples per trace */

/* Put the scope display at column x, row y; it uses 36 x 20 cells. */
void scope_show(uint8_t x, uint8_t y);

/*
 * Redraw the trace of one channel. dma is the audio DMA channel to read,
 * volume (0..64) scales the amplitude. Takes about a millisecond.
 */
void scope_draw(uint8_t channel, uint8_t dma, uint8_t volume);

/*
 * Redraw the trace of one channel from 144 signed samples; volume (0..64)
 * scales the amplitude as in scope_draw.
 */
void scope_draw_samples(uint8_t channel, const int8_t *samples,
                        uint8_t volume);

/* Draw a flat line for a channel (nothing playing). */
void scope_flat(uint8_t channel);

/*
 * Called every 16 points while a trace is drawn, if set. Drawing one trace
 * takes several milliseconds; a SID tune with a fast play rate uses this
 * to keep its play calls on time.
 */
extern void (*scope_poll)(void);

#endif
