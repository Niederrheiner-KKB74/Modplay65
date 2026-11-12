/*
 * Modplay65 - MOD and SID player for the MEGA65.
 *
 * Milestone 0 starter: shows system information and offers an audio DMA
 * test tone, so the build, the toolchain and the basic audio path can be
 * checked before the real player is built on top.
 *
 * The program uses no KERNAL services: text goes straight to screen RAM
 * and keys come from the hardware key queue. It does not return to BASIC;
 * reset the machine to leave.
 */
#include <stdint.h>

#include <mega65/memory.h>

#include "hw/audio_dma.h"
#include "hw/keyboard.h"
#include "hw/screen.h"

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
    uint8_t count = 0;

    mega65_io_enable();
    audio_dma_init();
    screen_init();
    keyboard_flush();

    screen_puts(0, 0, "Modplay65 " MODPLAY65_VERSION);
    screen_puts(0, 1, "MOD and SID player for the MEGA65");
    screen_puts(0, 3, "Video timing:");
    screen_puts(18, 3, is_ntsc() ? "NTSC" : "PAL");
    screen_puts(0, 4, "Tone register: $");
    screen_hex(18, 4, TEST_TONE_FREQ, 6);
    screen_puts(0, 6, "T = test tone on/off (440 Hz)");
    screen_puts(0, 8, "Tone:             off");
    screen_puts(0, 9, "Last key code:  $--   keys seen: $00");

    for (;;) {
        key = keyboard_poll();
        if (key == 0) {
            continue;
        }
        screen_hex(17, 9, key, 2);
        screen_hex(34, 9, ++count, 2);
        if (to_lower(key) == 't') {
            playing = !playing;
            if (playing) {
                tone_on();
                screen_puts(18, 8, "ON ");
            } else {
                tone_off();
                screen_puts(18, 8, "off");
            }
        }
    }
}
