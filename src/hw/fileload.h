/*
 * Modplay65 - read a whole file from the SD card into Attic RAM
 * (MEGA65 only).
 */
#ifndef MODPLAY65_HW_FILELOAD_H
#define MODPLAY65_HW_FILELOAD_H

#include <stdint.h>

/* Where files are loaded: the start of Attic RAM. */
#define FILELOAD_ATTIC 0x8000000UL
#define FILELOAD_MAX 0x7E0000UL /* below the directory list (ui/dir.c) */
#define FILELOAD_BLOCK 512U

enum fileload_result {
    FILELOAD_OK = 0,
    FILELOAD_NOT_FOUND,
    FILELOAD_TOO_BIG
};

/* Called after each block with the number of bytes read so far. */
typedef void (*fileload_progress)(uint32_t bytes);

/*
 * Read the file (name in the current SD card directory) to FILELOAD_ATTIC.
 * On success *size holds the file size.
 */
enum fileload_result fileload_to_attic(const char *name, uint32_t *size,
                                       fileload_progress progress);

/* 512-byte work buffer, free for other use between loads. */
extern uint8_t fileload_buffer[FILELOAD_BLOCK];

#endif
