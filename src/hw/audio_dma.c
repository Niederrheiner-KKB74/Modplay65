/*
 * Modplay65 - MEGA65 audio DMA channel access.
 */
#include "hw/audio_dma.h"

void audio_dma_stop(uint8_t channel)
{
    AUDIO_DMA_CH[channel].ctrl = 0;
}

void audio_dma_init(void)
{
    uint8_t ch;

    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        audio_dma_stop(ch);
        audio_dma_set_volume(ch, 0, 0);
    }
    AUDIO_DMA_GLOBAL |= AUDIO_DMA_GLOBAL_ENABLE;
}

void audio_dma_shutdown(void)
{
    uint8_t ch;

    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        audio_dma_stop(ch);
    }
    AUDIO_DMA_GLOBAL &= (uint8_t)~AUDIO_DMA_GLOBAL_ENABLE;
}

void audio_dma_set_freq(uint8_t channel, uint32_t freq)
{
    volatile struct audio_dma_channel *c = &AUDIO_DMA_CH[channel];

    c->freq[0] = (uint8_t)freq;
    c->freq[1] = (uint8_t)(freq >> 8);
    c->freq[2] = (uint8_t)(freq >> 16);
}

void audio_dma_set_base(uint8_t channel, uint32_t address)
{
    volatile struct audio_dma_channel *c = &AUDIO_DMA_CH[channel];

    c->base[0] = (uint8_t)address;
    c->base[1] = (uint8_t)(address >> 8);
    c->base[2] = (uint8_t)(address >> 16);
}

void audio_dma_set_volume(uint8_t channel, uint8_t volume, uint8_t crossfeed)
{
    AUDIO_DMA_CH[channel].volume = volume;
    AUDIO_DMA_PAN[channel] = crossfeed;
}

void audio_dma_sine(uint8_t channel, uint32_t freq)
{
    volatile struct audio_dma_channel *c = &AUDIO_DMA_CH[channel];

    c->ctrl = 0;
    /* The sine generator needs no sample data, but give the channel a
     * harmless window in free chip RAM anyway. */
    audio_dma_set_base(channel, 0x040000UL);
    c->top[0] = 0xFF;
    c->top[1] = 0xFF;
    audio_dma_set_freq(channel, freq);
    c->ctrl = AUDIO_DMA_CTRL_ENABLE | AUDIO_DMA_CTRL_LOOP |
              AUDIO_DMA_CTRL_SINE | AUDIO_DMA_CTRL_8BIT;
}

void audio_dma_play(uint8_t channel, uint32_t address, uint16_t length,
                    uint32_t freq, uint8_t loop)
{
    volatile struct audio_dma_channel *c = &AUDIO_DMA_CH[channel];
    uint16_t top = (uint16_t)address + length;

    c->ctrl = 0;
    audio_dma_set_base(channel, address);
    c->top[0] = (uint8_t)top;
    c->top[1] = (uint8_t)(top >> 8);
    /* Writing the base does not move the playhead: set it as well. */
    c->cur[0] = (uint8_t)address;
    c->cur[1] = (uint8_t)(address >> 8);
    c->cur[2] = (uint8_t)(address >> 16);
    audio_dma_set_freq(channel, freq);
    c->ctrl = (uint8_t)(AUDIO_DMA_CTRL_ENABLE | AUDIO_DMA_CTRL_8BIT |
                        (loop ? AUDIO_DMA_CTRL_LOOP : 0));
}
