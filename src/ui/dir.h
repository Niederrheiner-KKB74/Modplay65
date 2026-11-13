/*
 * Modplay65 - directory listing of the SD card (MEGA65 only).
 */
#ifndef MODPLAY65_UI_DIR_H
#define MODPLAY65_UI_DIR_H

#include <stdint.h>

#define DIR_MAX_ENTRIES 255
#define DIR_NAME_LEN 64

struct dir_entry {
    char name[DIR_NAME_LEN + 1];
    uint8_t is_dir;
};

/* Number of entries found by the last dir_read(). */
extern uint8_t dir_count;

/*
 * Entry number index of the sorted list. The entries are kept in Attic
 * RAM; the pointer refers to a one-entry buffer that the next call
 * overwrites.
 */
const struct dir_entry *dir_entry(uint8_t index);

/*
 * Read the current directory: sub-directories and music files, sorted by
 * name with directories first. Returns the number of entries.
 */
uint8_t dir_read(void);

/* Change into a sub-directory of the current directory (or ".."). */
uint8_t dir_enter(const char *name);

/* Does the name look like a MOD file ("name.mod" or "mod.name")? */
uint8_t dir_is_mod(const char *name);

/* Does the name end in ".sid"? */
uint8_t dir_is_sid(const char *name);

#endif
