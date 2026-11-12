/*
 * Modplay65 - drive the audio DMA channels from the replay engine.
 */
#include "mod/mod_out.h"

#include "hw/audio_dma.h"

/*
 * Amiga channels 0 and 3 are left, 1 and 2 right.
 * DMA channels 0 and 1 are left, 2 and 3 right.
 */
static const uint8_t dma_of[MOD_CHANNELS] = { 0, 2, 3, 1 };

/*
 * Sample rate = 3546895 / period (PAL Amiga); frequency register =
 * rate * 2^24 / 40500000 = 1469309 / period.
 */
static uint32_t freq_of(uint16_t period)
{
    return 1469309UL / period;
}

void mod_out_stop(void)
{
    uint8_t ch;

    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        audio_dma_stop(ch);
    }
}

void mod_out_apply(const struct mod_player *p, const struct mod_file *mod)
{
    uint8_t i;

    for (i = 0; i < MOD_CHANNELS; ++i) {
        const struct mod_channel *c = &p->ch[i];
        uint8_t dma = dma_of[i];
        uint8_t volume = (uint8_t)(c->out_volume << 1);

        if (c->out_period == 0) {
            if (c->out_flags & (MOD_OUT_TRIGGER | MOD_OUT_PERIOD)) {
                audio_dma_stop(dma);
            }
            continue;
        }
        if (c->out_flags & MOD_OUT_TRIGGER) {
            const struct mod_sample *s = &mod->info.sample[c->out_sample - 1];
            uint32_t base = mod->sample_addr[c->out_sample - 1];
            uint32_t start = base + c->out_offset;
            uint32_t freq = freq_of(c->out_period);

            audio_dma_set_volume(dma, volume, 0);
            if (s->loop_length) {
                uint32_t loop_start = base + s->loop_start;
                uint32_t loop_end = loop_start + s->loop_length;

                if (start >= loop_end) {
                    start = loop_start;
                }
                audio_dma_play_looped(dma, start, loop_start, loop_end, freq);
            } else {
                audio_dma_play(dma, start,
                               (uint16_t)(s->length - c->out_offset), freq, 0);
            }
            continue;
        }
        if (c->out_flags & MOD_OUT_PERIOD) {
            audio_dma_set_freq(dma, freq_of(c->out_period));
        }
        if (c->out_flags & MOD_OUT_VOLUME) {
            audio_dma_set_volume(dma, volume, 0);
        }
    }
}
