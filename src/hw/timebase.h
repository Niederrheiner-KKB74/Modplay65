/*
 * Modplay65 - tick timebase derived from the video raster line counter.
 *
 * The raster counter runs at a known line rate on PAL and NTSC, on real
 * hardware and in Xemu alike, and needs no interrupts. (Xemu's CIA timers
 * do not run at the hardware rate, so they are not used.)
 * timebase_poll() must be called at least once per video frame.
 */
#ifndef MODPLAY65_HW_TIMEBASE_H
#define MODPLAY65_HW_TIMEBASE_H

#include <stdint.h>

/* Detect PAL/NTSC and start counting from now. */
void timebase_init(void);

/* Returns 1 when a tick of 2.5 / bpm seconds has elapsed, else 0. */
uint8_t timebase_poll(uint8_t bpm);

#endif
