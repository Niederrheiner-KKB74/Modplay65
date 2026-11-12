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

/* SID register blocks: $D400 (SID 1) and $D440 (SID 3). */
#define SID_REG(base, n) (*(volatile uint8_t *)((base) + (n)))

static void sid_off(void)
{
    SID_REG(0xD400U, 4) = 0;
    SID_REG(0xD440U, 4) = 0;
}

/* 440 Hz triangle on voice 1 of the SID at base. */
static void sid_tone(uint16_t base)
{
    SID_REG(base, 24) = 0x0F; /* volume */
    SID_REG(base, 0) = 0x45;  /* frequency $1D45 = 440 Hz (PAL clock) */
    SID_REG(base, 1) = 0x1D;
    SID_REG(base, 5) = 0x00;  /* attack/decay */
    SID_REG(base, 6) = 0xF0;  /* sustain/release */
    SID_REG(base, 4) = 0x11;  /* triangle + gate */
}

static void all_off(void)
{
    uint8_t ch;

    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        audio_dma_stop(ch);
    }
    sid_off();
}

/* channels: bit n = DMA channel n */
static void start(uint8_t channels, uint8_t mode)
{
    uint8_t ch;

    all_off();
    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        if (!(channels & (1 << ch))) {
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
    screen_puts(0, 5, "A B C D = steady tone on DMA channel 0 1 2 3 only");
    screen_puts(0, 6, "L = SID 1 ($D400) tone    R = SID 3 ($D440) tone");
    screen_puts(0, 7, "SPACE = all off");
    screen_puts(0, 9, "Last action:  -");
    screen_puts(0, 11, "Ch0 control $--  position $------");
    screen_puts(0, 12, "Ch1 control $--  position $------");
    screen_puts(0, 13, "Ch2 control $--  position $------");
    screen_puts(0, 14, "Ch3 control $--  position $------");
    screen_puts(0, 16, "D711 $--  D712 $--  D71C-F $-- -- -- --");

    for (;;) {
        show_channel(11, 0);
        show_channel(12, 1);
        show_channel(13, 2);
        show_channel(14, 3);
        screen_hex(6, 16, PEEK(0xD711U), 2);
        screen_hex(16, 16, PEEK(0xD712U), 2);
        screen_hex(28, 16, PEEK(0xD71CU), 2);
        screen_hex(31, 16, PEEK(0xD71DU), 2);
        screen_hex(34, 16, PEEK(0xD71EU), 2);
        screen_hex(37, 16, PEEK(0xD71FU), 2);
        key = to_lower(keyboard_poll());
        switch (key) {
        case 't': start(5, 0); screen_puts(14, 9, "T steady   "); break;
        case '1': start(5, 1); screen_puts(14, 9, "1 one-shot "); break;
        case '2': start(5, 2); screen_puts(14, 9, "2 loop tail"); break;
        case 'a': start(1, 0); screen_puts(14, 9, "A channel 0"); break;
        case 'b': start(2, 0); screen_puts(14, 9, "B channel 1"); break;
        case 'c': start(4, 0); screen_puts(14, 9, "C channel 2"); break;
        case 'd': start(8, 0); screen_puts(14, 9, "D channel 3"); break;
        case 'l': all_off(); sid_tone(0xD400U);
                  screen_puts(14, 9, "L SID 1    "); break;
        case 'r': all_off(); sid_tone(0xD440U);
                  screen_puts(14, 9, "R SID 3    "); break;
        case ' ': all_off();   screen_puts(14, 9, "off        "); break;
        default: break;
        }
    }
}
