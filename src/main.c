/*
 * Modplay65 - MOD and SID player for the MEGA65.
 *
 * Milestone 0 starter: prints system information and offers an audio DMA
 * test tone, so the build, the toolchain and the basic audio path can be
 * checked before the real player is built on top.
 */
#include <stdint.h>
#include <stdio.h>

#include <mega65/memory.h>

#include "hw/audio_dma.h"
#include "hw/keyboard.h"

#ifndef MODPLAY65_VERSION
#define MODPLAY65_VERSION "dev"
#endif

/* The built-in sine wave has 32 samples per cycle: 440 Hz * 32 = 14080 Hz. */
#define TEST_TONE_FREQ AUDIO_DMA_FREQ(14080)
#define TEST_TONE_VOLUME 0x60

/* Fold ASCII letters to lower case so shifted keys work too. */
static uint8_t to_lower(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c | 0x20) : c;
}

static uint8_t is_ntsc(void)
{
    return (uint8_t)(PEEK(0xD06FU) & 0x80);
}

static void tone_on(void)
{
    /* Channel 0 and channel 2 feed opposite sides: tone on both. */
    audio_dma_set_volume(0, TEST_TONE_VOLUME, 0);
    audio_dma_set_volume(2, TEST_TONE_VOLUME, 0);
    audio_dma_sine(0, TEST_TONE_FREQ);
    audio_dma_sine(2, TEST_TONE_FREQ);
}

static void tone_off(void)
{
    audio_dma_stop(0);
    audio_dma_stop(2);
}

int main(void)
{
    uint8_t playing = 0;
    uint8_t key;

    mega65_io_enable();
    audio_dma_init();
    keyboard_flush();

    printf("\nModplay65 %s\n", MODPLAY65_VERSION);
    printf("MOD and SID player for the MEGA65\n\n");
    printf("Video timing: %s\n", is_ntsc() ? "NTSC" : "PAL");
    printf("Test tone value: %lu\n\n", (unsigned long)TEST_TONE_FREQ);
    printf("T = test tone on/off (440 Hz)\n");
    printf("Q = quit\n");

    for (;;) {
        key = to_lower(keyboard_poll());
        if (key == 't') {
            playing = !playing;
            if (playing) {
                tone_on();
                printf("Tone on\n");
            } else {
                tone_off();
                printf("Tone off\n");
            }
        } else if (key == 'q') {
            break;
        }
    }

    audio_dma_shutdown();
    printf("Bye.\n");
    return 0;
}
