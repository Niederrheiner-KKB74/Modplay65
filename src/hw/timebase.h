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

/*
 * Returns 1 each time 1 / rate seconds have elapsed; rate16 is the rate
 * in Hz times 16 (e.g. 802 for 50.125 Hz). Used for SID play calls.
 */
uint8_t timebase_poll_rate(uint16_t rate16);

/* Number of complete video frames per second (50 or 60). */
uint8_t timebase_frame_rate(void);

/*
 * Returns 1 once per video frame (when the raster counter has wrapped
 * since the last call that returned 1). Updated by timebase_poll().
 */
uint8_t timebase_new_frame(void);

#endif
