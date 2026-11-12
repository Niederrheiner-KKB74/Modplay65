/*
 * Modplay65 - MEGA65 audio DMA channel access.
 *
 * Register layout taken from iomap.txt in the mega65-core repository.
 * See docs/HARDWARE-NOTES.md.
 */
#ifndef MODPLAY65_HW_AUDIO_DMA_H
#define MODPLAY65_HW_AUDIO_DMA_H

#include <stdint.h>

#define AUDIO_DMA_CHANNELS 4

/* Control register bits (offset 0 of each channel). */
#define AUDIO_DMA_CTRL_ENABLE   0x80
#define AUDIO_DMA_CTRL_LOOP     0x40
#define AUDIO_DMA_CTRL_UNSIGNED 0x20
#define AUDIO_DMA_CTRL_SINE     0x10
#define AUDIO_DMA_CTRL_STOPPED  0x08
#define AUDIO_DMA_CTRL_16BIT    0x03
#define AUDIO_DMA_CTRL_8BIT     0x02

/* Global control register $D711. */
#define AUDIO_DMA_GLOBAL_ENABLE 0x80

/* Clock that advances the 24-bit frequency accumulator, in Hz. */
#define AUDIO_DMA_CLOCK_HZ 40500000ULL

/*
 * Convert a sample rate in Hz to a frequency register value.
 * Intended for constants; it uses 64-bit arithmetic.
 */
#define AUDIO_DMA_FREQ(hz) \
    ((uint32_t)((((uint64_t)(hz)) << 24) / AUDIO_DMA_CLOCK_HZ))

/* One audio DMA channel, 16 bytes, at $D720 + 16 * channel. */
struct audio_dma_channel {
    uint8_t ctrl;      /* +0     control                          */
    uint8_t base[3];   /* +1..3  sample start address, 24 bit     */
    uint8_t freq[3];   /* +4..6  frequency increment, 24 bit      */
    uint8_t top[2];    /* +7..8  sample end address, low 16 bits  */
    uint8_t volume;    /* +9     playback volume                  */
    uint8_t cur[3];    /* +A..C  current address, 24 bit          */
    uint8_t timing[3]; /* +D..F  timing counter, 24 bit           */
};

_Static_assert(sizeof(struct audio_dma_channel) == 16,
               "audio DMA channel block must be 16 bytes");

#define AUDIO_DMA_CH ((volatile struct audio_dma_channel *)0xD720)
#define AUDIO_DMA_GLOBAL (*(volatile uint8_t *)0xD711)
/* Cross-feed volume of channel n to the opposite side: $D71C + n. */
#define AUDIO_DMA_PAN ((volatile uint8_t *)0xD71C)

/* Stop all channels and switch audio DMA on. */
void audio_dma_init(void);

/* Stop all channels and switch audio DMA off. */
void audio_dma_shutdown(void);

/* Stop one channel. */
void audio_dma_stop(uint8_t channel);

/* Set the 24-bit frequency register of a channel. */
void audio_dma_set_freq(uint8_t channel, uint32_t freq);

/* Set the 24-bit start address of a channel. */
void audio_dma_set_base(uint8_t channel, uint32_t address);

/* Set the playback volume and the cross-feed volume of a channel. */
void audio_dma_set_volume(uint8_t channel, uint8_t volume, uint8_t crossfeed);

/*
 * Play 8-bit signed sample data from chip RAM.
 * address: 24-bit start address; length: bytes, the window must not cross
 * a 64 KB bank boundary; freq: frequency register value for the sample
 * rate; loop: non-zero repeats the whole window.
 */
void audio_dma_play(uint8_t channel, uint32_t address, uint16_t length,
                    uint32_t freq, uint8_t loop);

/*
 * Start the built-in 32-sample sine wave on a channel.
 * freq is a frequency register value for the sample rate, which is
 * 32 times the audible pitch: use AUDIO_DMA_FREQ(pitch_hz * 32).
 * Not emulated by Xemu (it plays the RAM window instead).
 */
void audio_dma_sine(uint8_t channel, uint32_t freq);

#endif
