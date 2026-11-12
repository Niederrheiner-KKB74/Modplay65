/*
 * Modplay65 - MOD and SID player for the MEGA65.
 *
 * Milestone 1 test bench for audio DMA sample playback: steady tone,
 * one-shot sample, sample with a looped tail, and single-side output.
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

/* Sample rate of all test sounds. A 32-byte cycle gives 440 Hz. */
#define TEST_FREQ AUDIO_DMA_FREQ(14080)
#define TEST_VOLUME 0x60

/*
 * Test sample in free chip RAM (bank 4):
 *   $040000-$041FFF  "blip": 880 Hz (16-byte cycle) fading out, 0.58 s
 *   $042000-$04201F  one 440 Hz sine cycle, used as loop
 */
#define BLIP_ADDR 0x040000UL
#define BLIP_LEN 8192U
#define LOOP_ADDR 0x042000UL
#define LOOP_LEN 32U

static const int8_t sine32[32] = {
    0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
    0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90,
    -71, -49, -25
};

static uint8_t chunk[256];

static void build_samples(void)
{
    uint16_t i;

    for (i = 0; i < BLIP_LEN; ++i) {
        /* every second table entry = 16-byte cycle; linear fade 127..0 */
        int16_t v = (int16_t)sine32[(i << 1) & 31];
        int16_t amp = (int16_t)((BLIP_LEN - 1 - i) >> 6);

        chunk[(uint8_t)i] = (uint8_t)((v * amp) >> 7);
        if ((uint8_t)i == 0xFF) {
            lcopy((uint32_t)(uintptr_t)chunk, BLIP_ADDR + (i & 0xFF00U), 256);
        }
    }
    lcopy((uint32_t)(uintptr_t)sine32, LOOP_ADDR, LOOP_LEN);
}

static uint8_t to_lower(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c | 0x20) : c;
}

static void all_off(void)
{
    audio_dma_stop(0);
    audio_dma_stop(2);
}

/* sides: bit 0 = channel 0, bit 1 = channel 2 */
static void start(uint8_t sides, uint8_t mode)
{
    uint8_t ch;

    all_off();
    for (ch = 0; ch < 4; ch += 2) {
        if (!(sides & (ch ? 2 : 1))) {
            continue;
        }
        audio_dma_set_volume(ch, TEST_VOLUME, 0);
        if (mode == 0) {
            audio_dma_play(ch, LOOP_ADDR, LOOP_LEN, TEST_FREQ, 1);
        } else if (mode == 1) {
            audio_dma_play(ch, BLIP_ADDR, BLIP_LEN, TEST_FREQ, 0);
        } else {
            audio_dma_play_looped(ch, BLIP_ADDR, LOOP_ADDR,
                                  LOOP_ADDR + LOOP_LEN, TEST_FREQ);
        }
    }
}

static void show_channel(uint8_t row, uint8_t ch)
{
    screen_hex(13, row, AUDIO_DMA_CH[ch].ctrl, 2);
    screen_hex(27, row, audio_dma_position(ch), 6);
}

int main(void)
{
    uint8_t key;

    mega65_io_enable();
    audio_dma_init();
    screen_init();
    screen_puts(0, 0, "Modplay65 " MODPLAY65_VERSION " - audio DMA test bench");
    screen_puts(0, 2, "Preparing samples ...");
    build_samples();
    keyboard_flush();

    screen_puts(0, 2, "T = steady 440 Hz tone, both sides");
    screen_puts(0, 3, "1 = one-shot: fading high tone, must stop by itself");
    screen_puts(0, 4, "2 = fading high tone, then steady low tone (loop)");
    screen_puts(0, 5, "A = steady tone on channel 0 only");
    screen_puts(0, 6, "B = steady tone on channel 2 only");
    screen_puts(0, 7, "SPACE = all off");
    screen_puts(0, 9, "Last action:  -");
    screen_puts(0, 11, "Ch0 control $--  position $------");
    screen_puts(0, 12, "Ch2 control $--  position $------");

    for (;;) {
        show_channel(11, 0);
        show_channel(12, 2);
        key = to_lower(keyboard_poll());
        switch (key) {
        case 't': start(3, 0); screen_puts(14, 9, "T steady   "); break;
        case '1': start(3, 1); screen_puts(14, 9, "1 one-shot "); break;
        case '2': start(3, 2); screen_puts(14, 9, "2 loop tail"); break;
        case 'a': start(1, 0); screen_puts(14, 9, "A channel 0"); break;
        case 'b': start(2, 0); screen_puts(14, 9, "B channel 2"); break;
        case ' ': all_off();   screen_puts(14, 9, "off        "); break;
        default: break;
        }
    }
}
