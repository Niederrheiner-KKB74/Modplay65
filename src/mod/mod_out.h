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

/* Silence all channels. */
void mod_out_stop(void);

#endif
