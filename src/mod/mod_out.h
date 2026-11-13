/*
 * Modplay65 - drive the audio DMA channels from the replay engine
 * (MEGA65 only).
 */
#ifndef MODPLAY65_MOD_OUT_H
#define MODPLAY65_MOD_OUT_H

#include "mod/mod_file.h"
#include "mod/mod_play.h"

/* Apply the outputs of the last mod_player_tick() to the hardware. */
void mod_out_apply(const struct mod_player *p, const struct mod_file *mod);

/*
 * Start sample index (0-based) on a DMA channel from the given byte offset.
 * freq is the frequency register value for a sample stored at full rate;
 * the caller shifts it right by mod->sample_shift[index].
 */
void mod_out_start(uint8_t dma, const struct mod_file *mod, uint8_t index,
                   uint16_t offset, uint32_t freq);

/* Master volume, 0..64 (default 64). */
void mod_out_set_master(uint8_t volume);

/*
 * Stereo separation in quarters: 4 = full left/right (default),
 * 0 = mono. Lower values feed part of each channel to the other side.
 */
void mod_out_set_separation(uint8_t quarters);

/* Write the volume of all channels again (after a setting changed). */
void mod_out_refresh(const struct mod_player *p);

/* Set all channel volumes to zero without stopping the samples (pause). */
void mod_out_mute(void);

/* The audio DMA channel (0..3) that plays engine channel i. */
uint8_t mod_out_dma_channel(uint8_t i);

/* Silence all channels. */
void mod_out_stop(void);

#endif
