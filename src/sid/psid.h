/*
 * Modplay65 - PSID/RSID file header parsing.
 *
 * Portable C: no hardware access, also built and tested on the host.
 * Format reference: the SID file format description of the High Voltage
 * SID Collection (SID_file_format.txt).
 */
#ifndef MODPLAY65_SID_PSID_H
#define MODPLAY65_SID_PSID_H

#include <stdint.h>

/* Bytes of the file needed by psid_parse (version 2 and later header). */
#define PSID_HEADER_SIZE 0x7C
#define PSID_TEXT_LEN 32

/* Clock and SID model fields (flags bits 2-3 and 4-5). */
#define PSID_CLOCK_UNKNOWN 0
#define PSID_CLOCK_PAL 1
#define PSID_CLOCK_NTSC 2
#define PSID_CLOCK_BOTH 3
#define PSID_MODEL_UNKNOWN 0
#define PSID_MODEL_6581 1
#define PSID_MODEL_8580 2
#define PSID_MODEL_BOTH 3

enum psid_error {
    PSID_OK = 0,
    PSID_ERR_TOO_SHORT, /* file shorter than its header says         */
    PSID_ERR_NOT_SID,   /* no PSID/RSID signature                    */
    PSID_ERR_VERSION,   /* unknown header version                    */
    PSID_ERR_NO_DATA,   /* no C64 data after the header              */
    PSID_ERR_TOO_LONG   /* data would run past $FFFF                 */
};

struct psid_info {
    uint8_t rsid;          /* 1 = RSID (needs a real C64 environment)  */
    uint8_t version;       /* 1..4                                     */
    uint16_t data_offset;  /* file offset of the C64 data proper       */
    uint16_t load;         /* C64 load address                         */
    uint16_t length;       /* bytes of C64 data                        */
    uint16_t init;
    uint16_t play;         /* 0 = tune installs its own interrupt      */
    uint8_t songs;         /* 1..255                                   */
    uint8_t start_song;    /* 1..songs                                 */
    uint32_t speed;        /* bit n set: song n+1 is timed by CIA      */
    uint8_t clock;         /* PSID_CLOCK_*                             */
    uint8_t model;         /* PSID_MODEL_* of the first SID            */
    uint8_t start_page;    /* free memory: 0 = see spec, $FF = none     */
    uint8_t page_length;
    uint16_t sid2;         /* address of a second SID, 0 = none        */
    uint16_t sid3;         /* address of a third SID, 0 = none         */
    /* Title, author, released: ASCII (Latin-1 umlauts transliterated). */
    char name[PSID_TEXT_LEN + 1];
    char author[PSID_TEXT_LEN + 1];
    char released[PSID_TEXT_LEN + 1];
};

/*
 * Parse a SID file header. header holds the first header_len bytes of the
 * file (at least PSID_HEADER_SIZE, or the whole file if shorter);
 * file_size is the size of the whole file.
 */
enum psid_error psid_parse(const uint8_t *header, uint16_t header_len,
                           uint32_t file_size, struct psid_info *info);

/* Is song (1-based) timed by CIA timer A instead of the video frame? */
uint8_t psid_song_uses_cia(const struct psid_info *info, uint8_t song);

#endif
