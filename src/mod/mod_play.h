/*
 * Modplay65 - MOD replay engine.
 *
 * Portable C: no hardware access. The engine is driven by calling
 * mod_player_tick() once per tick (2.5 / BPM seconds). After each call the
 * caller reads the per-channel output fields and drives the sound hardware.
 */
#ifndef MODPLAY65_MOD_PLAY_H
#define MODPLAY65_MOD_PLAY_H

#include <stdint.h>

#include "mod/mod_load.h"

#define MOD_CHANNELS 4
#define MOD_ROWS 64
#define MOD_ROW_BYTES 16 /* 4 channels * 4 bytes */

/* Output flags, set by mod_player_tick() for the tick just processed. */
#define MOD_OUT_TRIGGER 0x01 /* start the sample at out_offset        */
#define MOD_OUT_PERIOD  0x02 /* out_period changed                    */
#define MOD_OUT_VOLUME  0x04 /* out_volume changed                    */

struct mod_channel {
    /* output for the hardware layer */
    uint8_t out_flags;
    uint8_t out_sample;   /* 1..31, valid with MOD_OUT_TRIGGER          */
    uint16_t out_offset;  /* start offset in bytes for the trigger      */
    uint16_t out_period;  /* Amiga period, 0 = silent                   */
    uint8_t out_volume;   /* 0..64                                      */

    /* engine state */
    uint8_t sample;       /* current instrument, 0 = none               */
    uint8_t finetune;     /* 0..15, row index of the period table       */
    uint8_t note;         /* index into the period table, 0xFF = none   */
    uint8_t volume;       /* 0..64                                      */
    uint16_t period;      /* current period without vibrato/arpeggio    */
    uint8_t effect;
    uint8_t param;
    uint16_t porta_target;
    uint8_t porta_speed;
    uint8_t vib_pos, vib_speed, vib_depth, vib_wave;
    uint8_t trem_pos, trem_speed, trem_depth, trem_wave;
    uint8_t offset_memory;
    uint8_t loop_row, loop_count;
    uint8_t delayed_note; /* note waiting for EDx, 0xFF = none          */
    uint16_t last_period; /* last values reported, to detect changes    */
    uint8_t last_volume;
};

struct mod_player {
    const struct mod_info *info;
    struct mod_channel ch[MOD_CHANNELS];
    uint8_t speed;        /* ticks per row                              */
    uint8_t tempo;        /* BPM                                        */
    uint8_t tick;
    uint8_t row;
    uint8_t order;        /* position in the order list                 */
    uint8_t pattern;
    uint8_t pattern_delay;
    uint8_t row_started;  /* set on the tick that started a new row     */
    uint8_t song_looped;  /* set once the song has wrapped around       */
    /* pending position changes */
    uint8_t jump_order;   /* 0xFF = none                                */
    uint8_t jump_row;
    uint8_t loop_pending;
    uint8_t row_data[MOD_ROW_BYTES];
};

/*
 * Supplied by the platform: copy one pattern row (MOD_ROW_BYTES bytes)
 * to dst.
 */
void mod_read_row(uint8_t pattern, uint8_t row, uint8_t *dst);

/* Start at the given order position with default speed 6, tempo 125. */
void mod_player_init(struct mod_player *p, const struct mod_info *info,
                     uint8_t order);

/* Advance the song by one tick. */
void mod_player_tick(struct mod_player *p);

#endif
