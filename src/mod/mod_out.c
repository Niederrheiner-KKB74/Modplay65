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

static uint8_t master = 64;
static uint8_t separation = 4;

/* Engine volume 0..64 to DMA volume 0..128, with cross-feed. */
static void set_volume(uint8_t dma, uint8_t volume)
{
    uint8_t v = (uint8_t)(((uint16_t)volume * master) >> 5);

    audio_dma_set_volume(dma, v,
                         (uint8_t)(((uint16_t)v * (uint8_t)(4 - separation)) >> 2));
}

void mod_out_set_master(uint8_t volume)
{
    master = volume > 64 ? 64 : volume;
}

void mod_out_set_separation(uint8_t quarters)
{
    separation = quarters > 4 ? 4 : quarters;
}

void mod_out_refresh(const struct mod_player *p)
{
    uint8_t i;

    for (i = 0; i < MOD_CHANNELS; ++i) {
        set_volume(dma_of[i], p->ch[i].out_volume);
    }
}

void mod_out_mute(void)
{
    uint8_t ch;

    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        audio_dma_set_volume(ch, 0, 0);
    }
}

/* Rate reduction of the sample each engine channel is playing. */
static uint8_t shift_of[MOD_CHANNELS];

void mod_out_start(uint8_t dma, const struct mod_file *mod, uint8_t index,
                   uint16_t offset, uint32_t freq)
{
    const struct mod_sample *s = &mod->info.sample[index];
    uint8_t shift = mod->sample_shift[index];
    uint32_t base = mod->sample_addr[index];
    uint32_t start = base + (offset >> shift);

    if (s->loop_length >> shift) {
        uint32_t loop_start = base + (s->loop_start >> shift);
        uint32_t loop_end = loop_start + (s->loop_length >> shift);

        if (start >= loop_end) {
            start = loop_start;
        }
        audio_dma_play_looped(dma, start, loop_start, loop_end, freq);
    } else {
        audio_dma_play(dma, start,
                       (uint16_t)((s->length >> shift) - (offset >> shift)),
                       freq, 0);
    }
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

        if (c->out_flags & MOD_OUT_STOP) {
            audio_dma_stop(dma);
        }
        if (c->out_period == 0) {
            continue;
        }
        if (c->out_flags & MOD_OUT_TRIGGER) {
            shift_of[i] = mod->sample_shift[c->out_sample - 1];
            set_volume(dma, c->out_volume);
            mod_out_start(dma, mod, (uint8_t)(c->out_sample - 1),
                          c->out_offset, freq_of(c->out_period) >> shift_of[i]);
            continue;
        }
        if (c->out_flags & MOD_OUT_PERIOD) {
            audio_dma_set_freq(dma, freq_of(c->out_period) >> shift_of[i]);
        }
        if (c->out_flags & MOD_OUT_VOLUME) {
            set_volume(dma, c->out_volume);
        }
    }
}
