/*
 * Modplay65 - SID visualisation: per-voice waveform and envelope, rebuilt
 * from the SID register values once per video frame.
 *
 * The SID's real output per voice cannot be read, so each voice is drawn
 * as the waveform its registers select (triangle, saw, pulse, noise) at its
 * frequency, scaled by an emulated envelope (ADSR and gate). Filter and
 * ring modulation are not shown.
 */
#ifndef MODPLAY65_VIS_SIDVIS_H
#define MODPLAY65_VIS_SIDVIS_H

#include <stdint.h>

#define SIDVIS_VOICES 3

/* Start from silence (new song). */
void sidvis_reset(void);

/*
 * Take the 25 registers of a SID as last written and advance the envelopes
 * by one video frame.
 */
void sidvis_frame(const uint8_t *regs);

/* Envelope level of a voice, 0..255. */
uint8_t sidvis_level(uint8_t voice);

/* n samples (-128..127, unscaled) of the voice's waveform. */
void sidvis_wave(uint8_t voice, int8_t *out, uint8_t n);

/*
 * n samples of the three voices mixed, each scaled by its envelope and
 * the whole by the master volume (voice 3 left out when it is switched
 * off). wave[v] must hold the output of sidvis_wave for voice v.
 */
void sidvis_mix(int8_t *out, uint8_t n, int8_t *const wave[SIDVIS_VOICES]);

/* Three-character note name of a SID frequency value ("C-4", "---"). */
void sidvis_note(uint16_t freq, char *out);

/* Registers as last passed to sidvis_frame. */
extern uint8_t sidvis_regs[25];

#endif
