/*
 * Modplay65 - load a MOD file from the SD card (MEGA65 only).
 *
 * The whole file is read into Attic RAM, the header is parsed and the
 * samples are copied into chip RAM where audio DMA can play them.
 */
#ifndef MODPLAY65_MOD_FILE_H
#define MODPLAY65_MOD_FILE_H

#include <stdint.h>

#include "mod/mod_load.h"

#define MOD_FILE_ATTIC 0x8000000UL  /* file image in Attic RAM          */
#define MOD_SAMPLE_RAM 0x040000UL   /* chip RAM for samples: bank 4 ... */
#define MOD_SAMPLE_BANKS 2          /* ... and bank 5                   */

enum mod_file_error {
    MOD_FILE_OK = 0,
    MOD_FILE_NOT_FOUND,
    MOD_FILE_TOO_BIG,     /* larger than Attic RAM                    */
    MOD_FILE_NOT_A_MOD,
    MOD_FILE_CHANNELS,    /* not a 4-channel module                   */
    MOD_FILE_SAMPLE_TOO_BIG,
    MOD_FILE_NO_SAMPLE_RAM,
    MOD_FILE_TRUNCATED    /* file ends before the pattern data does   */
};

struct mod_file {
    struct mod_info info;
    uint32_t size;                          /* file size in bytes     */
    uint32_t sample_addr[MOD_MAX_SAMPLES];  /* chip RAM address, 0 = empty */
};

/* Called after each block with the number of bytes read so far. */
typedef void (*mod_file_progress)(uint32_t bytes);

/* filename: 8.3 name in the current SD card directory, upper case. */
enum mod_file_error mod_file_load(const char *filename, struct mod_file *mod,
                                  mod_file_progress progress);

/* Short English text for an error code. */
const char *mod_file_error_text(enum mod_file_error e);

#endif
