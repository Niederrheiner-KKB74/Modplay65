/*
 * Modplay65 - period and waveform tables for the MOD replay engine.
 */
#ifndef MODPLAY65_MOD_TABLES_H
#define MODPLAY65_MOD_TABLES_H

#include <stdint.h>

#define MOD_NOTES 36 /* three octaves, C-1 .. B-3 */

extern const uint16_t mod_period_table[16][MOD_NOTES];
extern const uint8_t mod_sine_table[32];
/* period factor for n semitones up, scaled by 65536 (n = 0..15) */
extern const uint16_t mod_semitone_table[16];

#endif
