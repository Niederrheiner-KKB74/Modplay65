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

/* Put the scope display at column x, row y; it uses 36 x 20 cells. */
void scope_show(uint8_t x, uint8_t y);

/*
 * Redraw the trace of one channel. dma is the audio DMA channel to read,
 * volume (0..64) scales the amplitude. Takes about a millisecond.
 */
void scope_draw(uint8_t channel, uint8_t dma, uint8_t volume);

/* Draw a flat line for a channel (nothing playing). */
void scope_flat(uint8_t channel);

#endif
