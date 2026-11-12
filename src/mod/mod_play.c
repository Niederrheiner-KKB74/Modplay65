/*
 * Modplay65 - MOD replay engine (ProTracker effect set).
 */
#include "mod/mod_play.h"

#include <string.h>

#include "mod/mod_tables.h"

#define PERIOD_MIN 113
#define PERIOD_MAX 856
#define NO_NOTE 0xFF

/* Find the note whose finetune-0 period is closest to period. */
static uint8_t note_of_period(uint16_t period)
{
    uint8_t i;

    for (i = 0; i < MOD_NOTES; ++i) {
        if (period >= mod_period_table[0][i]) {
            if (i > 0 && (uint16_t)(mod_period_table[0][i - 1] - period) <
                             (uint16_t)(period - mod_period_table[0][i])) {
                return (uint8_t)(i - 1);
            }
            return i;
        }
    }
    return MOD_NOTES - 1;
}

static uint16_t period_of(const struct mod_channel *c, uint8_t note)
{
    if (note >= MOD_NOTES) {
        note = MOD_NOTES - 1;
    }
    return mod_period_table[c->finetune][note];
}

static void trigger(struct mod_player *p, struct mod_channel *c, uint8_t note)
{
    const struct mod_sample *s;
    uint16_t offset = 0;

    c->note = note;
    c->period = period_of(c, note);
    if (c->sample == 0) {
        return;
    }
    s = &p->info->sample[c->sample - 1];
    if (c->effect == 0x9) {
        if (c->param) {
            c->offset_memory = c->param;
        }
        offset = (uint16_t)c->offset_memory << 8;
    }
    if (c->vib_wave < 4) {
        c->vib_pos = 0;
    }
    if (c->trem_wave < 4) {
        c->trem_pos = 0;
    }
    if (s->length == 0 || offset >= s->length) {
        c->period = 0; /* nothing to play: silence the channel */
        return;
    }
    c->out_flags |= MOD_OUT_TRIGGER;
    c->out_sample = c->sample;
    c->out_offset = offset;
}

static void volume_slide(struct mod_channel *c, uint8_t param)
{
    uint8_t up = param >> 4;
    uint8_t down = param & 0x0F;

    if (up) {
        c->volume = (uint8_t)(c->volume + up > 64 ? 64 : c->volume + up);
    } else {
        c->volume = (uint8_t)(c->volume > down ? c->volume - down : 0);
    }
}

static void slide_up(struct mod_channel *c, uint8_t amount)
{
    if (c->period == 0) {
        return;
    }
    c->period = (uint16_t)(c->period > PERIOD_MIN + amount
                               ? c->period - amount : PERIOD_MIN);
}

static void slide_down(struct mod_channel *c, uint8_t amount)
{
    if (c->period == 0) {
        return;
    }
    c->period = (uint16_t)(c->period + amount < PERIOD_MAX
                               ? c->period + amount : PERIOD_MAX);
}

static void tone_portamento(struct mod_channel *c)
{
    if (c->period == 0 || c->porta_target == 0) {
        return;
    }
    if (c->period < c->porta_target) {
        c->period = (uint16_t)(c->period + c->porta_speed);
        if (c->period > c->porta_target) {
            c->period = c->porta_target;
        }
    } else if (c->period > c->porta_target) {
        c->period = (uint16_t)(c->period > c->porta_speed
                                   ? c->period - c->porta_speed : 0);
        if (c->period < c->porta_target) {
            c->period = c->porta_target;
        }
    }
}

/* Waveform value 0..255 and sign for position 0..63. */
static uint8_t wave(uint8_t type, uint8_t pos, uint8_t *negative)
{
    uint8_t half = pos & 31;

    *negative = (uint8_t)(pos & 32);
    switch (type & 3) {
    case 0: return mod_sine_table[half];
    case 1: return (uint8_t)(*negative ? 255 - (half << 3) : half << 3);
    default: return 255; /* square; "random" is played as square */
    }
}

/* Decode the row for one channel and run the tick-0 effects. */
static void channel_row(struct mod_player *p, struct mod_channel *c,
                        const uint8_t *d)
{
    uint8_t sample = (uint8_t)((d[0] & 0xF0) | (d[2] >> 4));
    uint16_t period = (uint16_t)(((uint16_t)(d[0] & 0x0F) << 8) | d[1]);
    uint8_t hi, lo;

    c->effect = d[2] & 0x0F;
    c->param = d[3];
    hi = c->param >> 4;
    lo = c->param & 0x0F;
    c->delayed_note = NO_NOTE;

    if (sample && sample <= p->info->num_samples) {
        const struct mod_sample *s = &p->info->sample[sample - 1];

        c->sample = sample;
        c->volume = s->volume;
        c->finetune = (uint8_t)(s->finetune & 0x0F);
    }
    if (c->effect == 0xE && hi == 0x5) {
        c->finetune = lo;
    }
    if (period) {
        uint8_t note = note_of_period(period);

        if (c->effect == 0x3 || c->effect == 0x5) {
            c->porta_target = period_of(c, note);
            if (c->period == 0) { /* nothing playing yet: start the note */
                trigger(p, c, note);
            }
        } else if (c->effect == 0xE && hi == 0xD && lo != 0) {
            c->delayed_note = note;
        } else {
            trigger(p, c, note);
        }
    }

    switch (c->effect) {
    case 0x3:
        if (c->param) {
            c->porta_speed = c->param;
        }
        break;
    case 0x4:
        if (hi) {
            c->vib_speed = hi;
        }
        if (lo) {
            c->vib_depth = lo;
        }
        break;
    case 0x7:
        if (hi) {
            c->trem_speed = hi;
        }
        if (lo) {
            c->trem_depth = lo;
        }
        break;
    case 0xB:
        p->jump_order = c->param;
        p->jump_row = 0;
        break;
    case 0xC:
        c->volume = c->param > 64 ? 64 : c->param;
        break;
    case 0xD:
        if (p->jump_order == 0xFF) {
            p->jump_order = (uint8_t)(p->order + 1);
        }
        p->jump_row = (uint8_t)(hi * 10 + lo);
        if (p->jump_row >= MOD_ROWS) {
            p->jump_row = 0;
        }
        break;
    case 0xF:
        if (c->param == 0) {
            break;
        }
        if (c->param < 0x20) {
            p->speed = c->param;
        } else {
            p->tempo = c->param;
        }
        break;
    case 0xE:
        switch (hi) {
        case 0x1: slide_up(c, lo); break;
        case 0x2: slide_down(c, lo); break;
        case 0x4: c->vib_wave = lo; break;
        case 0x6:
            if (lo == 0) {
                c->loop_row = p->row;
            } else if (c->loop_count == 0) {
                c->loop_count = lo;
                p->loop_pending = 1;
                p->jump_row = c->loop_row;
            } else if (--c->loop_count) {
                p->loop_pending = 1;
                p->jump_row = c->loop_row;
            }
            break;
        case 0x7: c->trem_wave = lo; break;
        case 0xA: volume_slide(c, (uint8_t)(lo << 4)); break;
        case 0xB: volume_slide(c, lo); break;
        case 0xC:
            if (lo == 0) {
                c->volume = 0;
            }
            break;
        case 0xE: p->pattern_delay = lo; break;
        default: break;
        }
        break;
    default:
        break;
    }
}

/* Effects that run on every tick after the first one of a row. */
static void channel_tick(struct mod_player *p, struct mod_channel *c)
{
    uint8_t hi = c->param >> 4;
    uint8_t lo = c->param & 0x0F;

    switch (c->effect) {
    case 0x1: slide_up(c, c->param); break;
    case 0x2: slide_down(c, c->param); break;
    case 0x3: tone_portamento(c); break;
    case 0x4: c->vib_pos = (uint8_t)((c->vib_pos + c->vib_speed) & 63); break;
    case 0x5: tone_portamento(c); volume_slide(c, c->param); break;
    case 0x6:
        c->vib_pos = (uint8_t)((c->vib_pos + c->vib_speed) & 63);
        volume_slide(c, c->param);
        break;
    case 0x7:
        c->trem_pos = (uint8_t)((c->trem_pos + c->trem_speed) & 63);
        break;
    case 0xA: volume_slide(c, c->param); break;
    case 0xE:
        switch (hi) {
        case 0x9:
            if (lo && c->note != NO_NOTE && (p->tick % lo) == 0) {
                uint8_t effect = c->effect;

                c->effect = 0; /* retrigger from the sample start */
                trigger(p, c, c->note);
                c->effect = effect;
            }
            break;
        case 0xC:
            if (p->tick == lo) {
                c->volume = 0;
            }
            break;
        case 0xD:
            if (p->tick == lo && c->delayed_note != NO_NOTE) {
                trigger(p, c, c->delayed_note);
                c->delayed_note = NO_NOTE;
            }
            break;
        default: break;
        }
        break;
    default:
        break;
    }
}

/* Work out what the hardware should play after this tick. */
static void channel_output(struct mod_player *p, struct mod_channel *c)
{
    uint16_t period = c->period;
    uint8_t volume = c->volume;
    uint8_t negative;
    uint16_t delta;

    if (period) {
        if (c->effect == 0x0 && c->param && c->note != NO_NOTE) {
            switch (p->tick % 3) {
            case 1: period = period_of(c, (uint8_t)(c->note + (c->param >> 4))); break;
            case 2: period = period_of(c, (uint8_t)(c->note + (c->param & 0x0F))); break;
            default: break;
            }
        } else if (c->effect == 0x4 || c->effect == 0x6) {
            delta = (uint16_t)(((uint16_t)wave(c->vib_wave, c->vib_pos,
                                               &negative) * c->vib_depth) >> 7);
            period = (uint16_t)(negative ? period - delta : period + delta);
        }
    }
    if (c->effect == 0x7) {
        delta = (uint16_t)(((uint16_t)wave(c->trem_wave, c->trem_pos,
                                           &negative) * c->trem_depth) >> 6);
        if (negative) {
            volume = (uint8_t)(volume > delta ? volume - delta : 0);
        } else {
            volume = (uint8_t)(volume + delta > 64 ? 64 : volume + delta);
        }
    }

    c->out_period = period;
    c->out_volume = volume;
    if (period != c->last_period) {
        c->out_flags |= MOD_OUT_PERIOD;
        c->last_period = period;
    }
    if (volume != c->last_volume) {
        c->out_flags |= MOD_OUT_VOLUME;
        c->last_volume = volume;
    }
}

static void goto_order(struct mod_player *p, uint8_t order, uint8_t row)
{
    if (order >= p->info->song_length) {
        order = 0;
        p->song_looped = 1;
    } else if (order <= p->order && !p->loop_pending) {
        p->song_looped = 1; /* backwards jump: the song repeats */
    }
    p->order = order;
    p->row = row;
    p->pattern = p->info->order[order];
}

static void next_row(struct mod_player *p)
{
    uint8_t i;

    if (p->loop_pending) {
        p->row = p->jump_row; /* E6x: stay in this pattern */
        p->loop_pending = 0;
        p->jump_order = 0xFF;
        return;
    }
    if (p->jump_order != 0xFF) {
        uint8_t order = p->jump_order;

        p->jump_order = 0xFF;
        goto_order(p, order, p->jump_row);
    } else if (++p->row >= MOD_ROWS) {
        goto_order(p, (uint8_t)(p->order + 1), 0);
    } else {
        return;
    }
    for (i = 0; i < MOD_CHANNELS; ++i) { /* new pattern: reset E6x loops */
        p->ch[i].loop_row = 0;
        p->ch[i].loop_count = 0;
    }
}

void mod_player_init(struct mod_player *p, const struct mod_info *info,
                     uint8_t order)
{
    uint8_t i;

    memset(p, 0, sizeof(*p));
    p->info = info;
    p->speed = 6;
    p->tempo = 125;
    p->jump_order = 0xFF;
    if (order >= info->song_length) {
        order = 0;
    }
    p->order = order;
    p->pattern = info->order[order];
    for (i = 0; i < MOD_CHANNELS; ++i) {
        p->ch[i].note = NO_NOTE;
        p->ch[i].delayed_note = NO_NOTE;
        p->ch[i].last_volume = 0xFF; /* force a first volume report */
    }
}

void mod_player_tick(struct mod_player *p)
{
    uint8_t i;

    for (i = 0; i < MOD_CHANNELS; ++i) {
        p->ch[i].out_flags = 0;
    }
    p->row_started = 0;

    if (p->tick == 0) {
        p->row_started = 1;
        p->jump_row = 0;
        mod_read_row(p->pattern, p->row, p->row_data);
        for (i = 0; i < MOD_CHANNELS; ++i) {
            channel_row(p, &p->ch[i], p->row_data + 4 * i);
        }
    } else {
        for (i = 0; i < MOD_CHANNELS; ++i) {
            channel_tick(p, &p->ch[i]);
        }
    }
    for (i = 0; i < MOD_CHANNELS; ++i) {
        channel_output(p, &p->ch[i]);
    }

    if (++p->tick >= (uint16_t)p->speed * (uint16_t)(p->pattern_delay + 1)) {
        p->tick = 0;
        p->pattern_delay = 0;
        next_row(p);
    }
}
