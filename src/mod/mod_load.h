/*
 * Modplay65 - MOD header parsing and sample placement.
 *
 * Portable C: no hardware access, also built and tested on the host.
 */
#ifndef MODPLAY65_MOD_LOAD_H
#define MODPLAY65_MOD_LOAD_H

#include <stdint.h>

#define MOD_MAX_SAMPLES 31
#define MOD_NAME_LEN 20
#define MOD_SAMPLE_NAME_LEN 22
#define MOD_ORDERS 128
#define MOD_PATTERN_BYTES 1024U /* 64 rows * 4 channels * 4 bytes */
/* Bytes of the file needed by mod_parse_header (31-instrument header). */
#define MOD_HEADER_SIZE 1084U

enum mod_error {
    MOD_OK = 0,
    MOD_ERR_TOO_SHORT,     /* fewer header bytes than needed          */
    MOD_ERR_NOT_A_MOD,     /* no signature and not a plausible 15-instrument file */
    MOD_ERR_CHANNELS,      /* a MOD, but not a 4-channel one          */
    MOD_ERR_SAMPLE_TOO_BIG,/* one sample exceeds a 64 KB window       */
    MOD_ERR_NO_MEMORY      /* samples do not fit into the given memory */
};

struct mod_sample {
    char name[MOD_SAMPLE_NAME_LEN + 1];
    uint32_t length;      /* bytes                                    */
    uint32_t loop_start;  /* bytes from sample start                  */
    uint32_t loop_length; /* bytes, 0 = sample does not loop          */
    uint8_t volume;       /* 0..64                                    */
    int8_t finetune;      /* -8..7                                    */
};

struct mod_info {
    char name[MOD_NAME_LEN + 1];
    char signature[5];        /* e.g. "M.K.", empty for 15-instrument files */
    uint8_t num_samples;      /* 15 or 31                               */
    uint8_t song_length;      /* number of used order entries, 1..128   */
    uint8_t num_patterns;     /* patterns stored in the file            */
    uint8_t order[MOD_ORDERS];
    uint32_t pattern_offset;  /* file offset of the first pattern       */
    uint32_t sample_offset;   /* file offset of the first sample        */
    uint32_t sample_bytes;    /* sum of all sample lengths              */
    struct mod_sample sample[MOD_MAX_SAMPLES];
};

/*
 * Parse the start of a MOD file. header must hold at least
 * MOD_HEADER_SIZE bytes, or the whole file if it is shorter.
 */
enum mod_error mod_parse_header(const uint8_t *header, uint16_t header_len,
                                struct mod_info *info);

/*
 * Assign each sample a playback address inside [first, first + banks * 64K).
 * first must be a multiple of 64 KB. A sample never crosses a 64 KB bank
 * boundary and never ends on the last byte of a bank, because the audio
 * DMA end address has only 16 bits. address[i] is 0 for empty samples.
 */
enum mod_error mod_place_samples(const struct mod_info *info, uint32_t first,
                                 uint8_t banks,
                                 uint32_t address[MOD_MAX_SAMPLES]);

#endif
