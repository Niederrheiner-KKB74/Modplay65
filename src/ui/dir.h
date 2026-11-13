/*
 * Modplay65 - directory listing of the SD card (MEGA65 only).
 */
#ifndef MODPLAY65_UI_DIR_H
#define MODPLAY65_UI_DIR_H

#include <stdint.h>

#define DIR_MAX_ENTRIES 100
#define DIR_NAME_LEN 64

struct dir_entry {
    char name[DIR_NAME_LEN + 1];
    uint8_t is_dir;
};

extern struct dir_entry dir_entries[DIR_MAX_ENTRIES];
extern uint8_t dir_count;

/*
 * Read the current directory: sub-directories and music files, sorted by
 * name with directories first. Returns the number of entries.
 */
uint8_t dir_read(void);

/* Change into a sub-directory of the current directory (or ".."). */
uint8_t dir_enter(const char *name);

/* Does the name look like a MOD file ("name.mod" or "mod.name")? */
uint8_t dir_is_mod(const char *name);

#endif
