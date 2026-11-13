/*
 * Modplay65 - tick timebase derived from the video raster line counter.
 */
#include "hw/timebase.h"

#define VIC_CTRL1   (*(volatile uint8_t *)0xD011) /* bit 7 = raster bit 8 */
#define VIC_RASTER  (*(volatile uint8_t *)0xD012)
#define VIC4_PALNTSC (*(volatile uint8_t *)0xD06F) /* bit 7 = NTSC */

/* 2.5 seconds worth of raster lines: PAL 15625 Hz, NTSC 15734 Hz. */
#define PAL_LINES 312U
#define PAL_THRESHOLD 39063UL
#define NTSC_LINES 263U
#define NTSC_THRESHOLD 39336UL

/* Raster lines per second times 16: PAL 312 x 50, NTSC 263 x 59.94. */
#define PAL_LINES16 249600UL
#define NTSC_LINES16 252224UL

static uint16_t lines_per_frame;
static uint32_t threshold;
static uint32_t threshold16;
static uint16_t last_line;
static uint32_t acc;
static uint8_t frame_flag;

static uint16_t raster(void)
{
    uint8_t hi, lo;

    do {
        hi = VIC_CTRL1 & 0x80;
        lo = VIC_RASTER;
    } while (hi != (VIC_CTRL1 & 0x80));
    return (uint16_t)(lo | (hi ? 0x100 : 0));
}

void timebase_init(void)
{
    if (VIC4_PALNTSC & 0x80) {
        lines_per_frame = NTSC_LINES;
        threshold = NTSC_THRESHOLD;
        threshold16 = NTSC_LINES16;
    } else {
        lines_per_frame = PAL_LINES;
        threshold = PAL_THRESHOLD;
        threshold16 = PAL_LINES16;
    }
    last_line = raster();
    acc = 0;
}

/* Lines since the last call; also notices the start of a new frame. */
static uint16_t lines_elapsed(void)
{
    uint16_t now = raster();
    uint16_t delta = 0;

    if (now != last_line) {
        if (now < last_line) {
            frame_flag = 1;
        }
        delta = (uint16_t)(now >= last_line ? now - last_line
                                            : now + lines_per_frame - last_line);
        if (delta > lines_per_frame) { /* counter beyond the expected range */
            delta = 1;
        }
        last_line = now;
    }
    return delta;
}

uint8_t timebase_poll(uint8_t bpm)
{
    acc += (uint32_t)lines_elapsed() * bpm;
    if (acc >= threshold) {
        acc -= threshold;
        return 1;
    }
    return 0;
}

uint8_t timebase_poll_rate(uint16_t rate16)
{
    acc += (uint32_t)lines_elapsed() * rate16;
    if (acc >= threshold16) {
        acc -= threshold16;
        return 1;
    }
    return 0;
}

uint8_t timebase_frame_rate(void)
{
    return (uint8_t)(lines_per_frame == NTSC_LINES ? 60 : 50);
}

uint8_t timebase_new_frame(void)
{
    if (frame_flag) {
        frame_flag = 0;
        return 1;
    }
    return 0;
}
