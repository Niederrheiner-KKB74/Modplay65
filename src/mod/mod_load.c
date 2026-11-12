/*
 * Modplay65 - MOD header parsing and sample placement.
 */
#include "mod/mod_load.h"

#include <string.h>

#define OLD_HEADER_SIZE 600U /* 15-instrument Soundtracker header */

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void copy_text(char *dst, const uint8_t *src, uint8_t len)
{
    uint8_t i;

    for (i = 0; i < len && src[i] != 0; ++i) {
        dst[i] = (char)src[i];
    }
    dst[i] = 0;
}

static uint8_t sig_is(const uint8_t *sig, const char *text)
{
    return (uint8_t)(memcmp(sig, text, 4) == 0);
}

/* 0 = unknown, 4 = four channels, anything else = unsupported count */
static uint8_t channels_of_signature(const uint8_t *sig)
{
    if (sig_is(sig, "M.K.") || sig_is(sig, "M!K!") || sig_is(sig, "M&K!") ||
        sig_is(sig, "FLT4") || sig_is(sig, "4CHN")) {
        return 4;
    }
    if (sig_is(sig, "FLT8") || sig_is(sig, "CD81") || sig_is(sig, "OCTA") ||
        sig_is(sig, "OKTA")) {
        return 8;
    }
    if (sig[1] == 'C' && sig[2] == 'H' && sig[3] == 'N' &&
        sig[0] >= '1' && sig[0] <= '9') {
        return (uint8_t)(sig[0] - '0');
    }
    if (sig[2] == 'C' && (sig[3] == 'H' || sig[3] == 'N') &&
        sig[0] >= '1' && sig[0] <= '9' && sig[1] >= '0' && sig[1] <= '9') {
        return (uint8_t)((sig[0] - '0') * 10 + (sig[1] - '0'));
    }
    return 0;
}

/* Does this look like a 15-instrument file? There is no signature. */
static uint8_t plausible_old_header(const uint8_t *h)
{
    uint8_t i;
    uint8_t song_length = h[470];

    if (song_length == 0 || song_length > MOD_ORDERS) {
        return 0;
    }
    for (i = 0; i < 15; ++i) {
        const uint8_t *s = h + 20 + 30U * i;

        if (s[25] > 64 || s[24] > 15) { /* volume, finetune */
            return 0;
        }
    }
    for (i = 0; i < MOD_ORDERS; ++i) {
        if (h[472 + i] > 63) {
            return 0;
        }
    }
    return 1;
}

enum mod_error mod_parse_header(const uint8_t *header, uint16_t header_len,
                                struct mod_info *info)
{
    const uint8_t *orders;
    uint8_t channels = 0;
    uint8_t i;
    uint8_t highest = 0;

    memset(info, 0, sizeof(*info));
    if (header_len >= MOD_HEADER_SIZE) {
        channels = channels_of_signature(header + 1080);
    }
    if (channels != 0 && channels != 4) {
        return MOD_ERR_CHANNELS;
    }
    if (channels == 4) {
        info->num_samples = 31;
        memcpy(info->signature, header + 1080, 4);
        info->song_length = header[950];
        orders = header + 952;
        info->pattern_offset = MOD_HEADER_SIZE;
    } else {
        if (header_len < OLD_HEADER_SIZE) {
            return MOD_ERR_TOO_SHORT;
        }
        if (!plausible_old_header(header)) {
            return MOD_ERR_NOT_A_MOD;
        }
        info->num_samples = 15;
        info->song_length = header[470];
        orders = header + 472;
        info->pattern_offset = OLD_HEADER_SIZE;
    }
    if (info->song_length == 0 || info->song_length > MOD_ORDERS) {
        return MOD_ERR_NOT_A_MOD;
    }

    copy_text(info->name, header, MOD_NAME_LEN);
    for (i = 0; i < MOD_ORDERS; ++i) {
        info->order[i] = orders[i];
        if (orders[i] > highest) {
            highest = orders[i];
        }
    }
    info->num_patterns = (uint8_t)(highest + 1);

    for (i = 0; i < info->num_samples; ++i) {
        const uint8_t *s = header + 20 + 30U * i;
        struct mod_sample *out = &info->sample[i];
        uint32_t loop_start = be16(s + 26);
        uint32_t loop_length = (uint32_t)be16(s + 28) * 2;

        copy_text(out->name, s, MOD_SAMPLE_NAME_LEN);
        out->length = (uint32_t)be16(s + 22) * 2;
        out->finetune = (int8_t)(s[24] & 0x0F);
        if (out->finetune > 7) {
            out->finetune = (int8_t)(out->finetune - 16);
        }
        out->volume = s[25] > 64 ? 64 : s[25];
        /* Loop start is in words, except in 15-instrument files. */
        if (info->num_samples == 31) {
            loop_start *= 2;
        }
        if (loop_length <= 2 || loop_start >= out->length) {
            loop_start = 0;
            loop_length = 0;
        } else if (loop_start + loop_length > out->length) {
            loop_length = out->length - loop_start;
        }
        out->loop_start = loop_start;
        out->loop_length = loop_length;
        info->sample_bytes += out->length;
    }
    info->sample_offset = info->pattern_offset +
                          (uint32_t)info->num_patterns * MOD_PATTERN_BYTES;
    return MOD_OK;
}

#define MAX_REGIONS 8

enum mod_error mod_place_samples(const struct mod_info *info,
                                 const struct mod_region *regions,
                                 uint8_t num_regions,
                                 uint32_t address[MOD_MAX_SAMPLES])
{
    uint32_t next[MAX_REGIONS];
    uint8_t done[MOD_MAX_SAMPLES];
    uint32_t largest_region = 0;
    uint8_t i, r, n;

    if (num_regions > MAX_REGIONS) {
        num_regions = MAX_REGIONS;
    }
    for (r = 0; r < num_regions; ++r) {
        next[r] = regions[r].start;
        if (regions[r].end - regions[r].start > largest_region) {
            largest_region = regions[r].end - regions[r].start;
        }
    }
    for (i = 0; i < MOD_MAX_SAMPLES; ++i) {
        address[i] = 0;
        done[i] = (uint8_t)(info->sample[i].length == 0);
        if (info->sample[i].length > largest_region) {
            return MOD_ERR_SAMPLE_TOO_BIG;
        }
    }
    /* First fit, largest sample first. */
    for (n = 0; n < MOD_MAX_SAMPLES; ++n) {
        uint8_t best = 0xFF;
        uint32_t len = 0;

        for (i = 0; i < MOD_MAX_SAMPLES; ++i) {
            if (!done[i] && info->sample[i].length > len) {
                len = info->sample[i].length;
                best = i;
            }
        }
        if (best == 0xFF) {
            break;
        }
        for (r = 0; r < num_regions; ++r) {
            if (next[r] + len <= regions[r].end) {
                address[best] = next[r];
                next[r] += len;
                break;
            }
        }
        if (r == num_regions) {
            return MOD_ERR_NO_MEMORY;
        }
        done[best] = 1;
    }
    return MOD_OK;
}
