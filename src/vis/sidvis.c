/*
 * Modplay65 - SID visualisation.
 */
#include "vis/sidvis.h"

#include <string.h>

uint8_t sidvis_regs[25];

enum { ENV_RELEASE, ENV_ATTACK, ENV_DECAY };

static uint16_t level[SIDVIS_VOICES]; /* 0..65535 */
static uint8_t state[SIDVIS_VOICES];
static uint8_t gate[SIDVIS_VOICES];

/*
 * Envelope change per 20 ms frame in 1/65535 of full scale, from the SID's
 * attack times 2 ms .. 8 s; decay and release take three times as long.
 * Linear, while the real SID's decay and release are exponential.
 */
static const uint16_t attack_step[16] = {
    65535, 65535, 65535, 54612, 34492, 23405, 19275, 16384,
    13107, 5243, 2621, 1638, 1311, 437, 262, 164
};
static const uint16_t decay_step[16] = {
    65535, 54612, 27306, 18204, 11497, 7802, 6425, 5461,
    4369, 1748, 874, 546, 437, 146, 87, 55
};

void sidvis_reset(void)
{
    memset(level, 0, sizeof(level));
    memset(state, 0, sizeof(state));
    memset(gate, 0, sizeof(gate));
    memset(sidvis_regs, 0, sizeof(sidvis_regs));
}

static uint16_t down(uint16_t value, uint16_t step, uint16_t floor)
{
    return (uint16_t)(value > (uint16_t)(floor + step) ? value - step : floor);
}

void sidvis_frame(const uint8_t *regs)
{
    uint8_t v;

    memcpy(sidvis_regs, regs, sizeof(sidvis_regs));
    for (v = 0; v < SIDVIS_VOICES; ++v) {
        const uint8_t *r = regs + v * 7;
        uint8_t g = (uint8_t)(r[4] & 1);
        uint16_t sustain = (uint16_t)((r[6] >> 4) * 0x1111U);

        if (g && !gate[v]) {
            state[v] = ENV_ATTACK;
        } else if (!g) {
            state[v] = ENV_RELEASE;
        }
        gate[v] = g;
        switch (state[v]) {
        case ENV_ATTACK:
            if (level[v] >= (uint16_t)(65535U - attack_step[r[5] >> 4])) {
                level[v] = 65535U;
                state[v] = ENV_DECAY;
            } else {
                level[v] = (uint16_t)(level[v] + attack_step[r[5] >> 4]);
            }
            break;
        case ENV_DECAY:
            level[v] = level[v] > sustain
                           ? down(level[v], decay_step[r[5] & 15], sustain)
                           : sustain;
            break;
        default:
            level[v] = down(level[v], decay_step[r[6] & 15], 0);
            break;
        }
    }
}

uint8_t sidvis_level(uint8_t voice)
{
    return (uint8_t)(level[voice] >> 8);
}

void sidvis_wave(uint8_t voice, int8_t *out, uint8_t n)
{
    const uint8_t *r = sidvis_regs + voice * 7;
    uint16_t freq = (uint16_t)(r[0] | (r[1] << 8));
    uint16_t pw = (uint16_t)(r[2] | ((r[3] & 15) << 8));
    uint8_t ctrl = r[4];
    /* 16-bit phase step per point for 8000 points per second (PAL clock):
     * freq x 985248 / 2^24 / 8000 x 2^16 = freq x 0.481 */
    uint16_t step = (uint16_t)(((uint32_t)freq * 123U) >> 8);
    uint16_t phase = 0;
    uint16_t noise = 0xACE1;
    uint8_t i;

    if ((ctrl & 0xF0) == 0 || (ctrl & 0x08)) {
        memset(out, 0, n); /* no waveform, or the test bit holds it */
        return;
    }
    for (i = 0; i < n; ++i) {
        uint8_t u = 0xFF;
        uint8_t p = (uint8_t)(phase >> 8);
        uint16_t next = (uint16_t)(phase + step);

        if (ctrl & 0x10) {
            u &= (p & 0x80) ? (uint8_t)~(p << 1) : (uint8_t)(p << 1);
        }
        if (ctrl & 0x20) {
            u &= p;
        }
        if (ctrl & 0x40) {
            u &= (uint16_t)(phase >> 4) >= pw ? 0xFF : 0x00;
        }
        if (ctrl & 0x80) {
            if ((next ^ phase) & 0x0800) {
                noise = (uint16_t)((noise >> 1) ^ ((noise & 1) ? 0xB400U : 0));
            }
            u &= (uint8_t)(noise >> 8);
        }
        out[i] = (int8_t)(u ^ 0x80);
        phase = next;
    }
}

void sidvis_mix(int8_t *out, uint8_t n, int8_t *const wave[SIDVIS_VOICES])
{
    uint8_t volume = (uint8_t)(sidvis_regs[24] & 15);
    uint8_t voices = (uint8_t)((sidvis_regs[24] & 0x80) ? 2 : 3);
    uint8_t lv[SIDVIS_VOICES];
    uint8_t v, i;

    for (v = 0; v < SIDVIS_VOICES; ++v) {
        lv[v] = sidvis_level(v);
    }
    for (i = 0; i < n; ++i) {
        int16_t sum = 0;

        for (v = 0; v < voices; ++v) {
            sum += (int16_t)(((int16_t)wave[v][i] * lv[v]) >> 8);
        }
        /* three full voices: +-381; scale to +-127 times volume / 16 */
        sum = (int16_t)(((int32_t)sum * volume) / 48);
        out[i] = (int8_t)(sum > 127 ? 127 : sum < -128 ? -128 : sum);
    }
}

/* Upper limits of C..A# within the octave of C-4 (PAL frequency values). */
static const uint16_t note_bounds[11] = {
    4586, 4858, 5147, 5453, 5778, 6121, 6485, 6871, 7279, 7712, 8171
};
static const char note_names[12][3] = {
    "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"
};

void sidvis_note(uint16_t freq, char *out)
{
    uint32_t f = freq;
    uint8_t octave = 4;
    uint8_t k = 0;

    if (freq < 260) { /* below C-0, or silent */
        out[0] = out[1] = out[2] = '-';
        return;
    }
    while (f < 4328) {
        f <<= 1;
        --octave;
    }
    while (f >= 8656) {
        f >>= 1;
        ++octave;
    }
    while (k < 11 && f >= note_bounds[k]) {
        ++k;
    }
    out[0] = note_names[k][0];
    out[1] = note_names[k][1];
    out[2] = (char)('0' + octave);
}
