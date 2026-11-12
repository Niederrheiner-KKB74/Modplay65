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

/* Silence all channels. */
void mod_out_stop(void);

#endif
