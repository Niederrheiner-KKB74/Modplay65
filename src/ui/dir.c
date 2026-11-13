/*
 * Modplay65 - directory listing of the SD card (MEGA65 only).
 */
#include "ui/dir.h"

#include <mega65/dirent.h>
#include <mega65/fileio.h>

#define FAT_ATTR_VOLUME 0x08
#define FAT_ATTR_DIR 0x10
#define FAT_ATTR_HIDDEN 0x02

struct dir_entry dir_entries[DIR_MAX_ENTRIES];
uint8_t dir_count;

static char name_buf[DIR_NAME_LEN + 1];

static uint8_t lower(uint8_t c)
{
    return (c >= 'A' && c <= 'Z') ? (uint8_t)(c | 0x20) : c;
}

uint8_t dir_is_mod(const char *name)
{
    uint8_t len = 0;

    while (name[len]) {
        ++len;
    }
    if (len < 5) {
        return 0;
    }
    if (name[len - 4] == '.' && lower((uint8_t)name[len - 3]) == 'm' &&
        lower((uint8_t)name[len - 2]) == 'o' &&
        lower((uint8_t)name[len - 1]) == 'd') {
        return 1;
    }
    return (uint8_t)(lower((uint8_t)name[0]) == 'm' &&
                     lower((uint8_t)name[1]) == 'o' &&
                     lower((uint8_t)name[2]) == 'd' && name[3] == '.');
}

/* Order: directories first, then by name without regard to case. */
static uint8_t before(const struct dir_entry *a, const struct dir_entry *b)
{
    uint8_t i;

    if (a->is_dir != b->is_dir) {
        return a->is_dir;
    }
    for (i = 0;; ++i) {
        uint8_t ca = lower((uint8_t)a->name[i]);
        uint8_t cb = lower((uint8_t)b->name[i]);

        if (ca != cb) {
            return (uint8_t)(ca < cb);
        }
        if (ca == 0) {
            return 0;
        }
    }
}

uint8_t dir_read(void)
{
    static struct dir_entry tmp;
    uint8_t fd;
    uint8_t i, j;
    uint16_t guard = 0;

    dir_count = 0;
    closeall();
    fd = opendir();
    if (fd == 0xFF) {
        return 0;
    }
    while (dir_count < DIR_MAX_ENTRIES && ++guard < 2000) {
        struct m65_dirent *e = readdir(fd);
        uint8_t attr;
        struct dir_entry *out;

        if (e == 0 || e->d_name[0] == 0) {
            break;
        }
        attr = (uint8_t)e->d_type;
        if (attr & (FAT_ATTR_VOLUME | FAT_ATTR_HIDDEN)) {
            continue;
        }
        if (attr & FAT_ATTR_DIR) {
            if (e->d_name[0] == '.' && e->d_name[1] == 0) {
                continue;
            }
        } else if (!dir_is_mod(e->d_name)) {
            continue;
        }
        out = &dir_entries[dir_count++];
        for (i = 0; i < DIR_NAME_LEN && e->d_name[i]; ++i) {
            out->name[i] = e->d_name[i];
        }
        out->name[i] = 0;
        out->is_dir = (uint8_t)((attr & FAT_ATTR_DIR) != 0);
    }
    closedir(fd);

    /* insertion sort */
    for (i = 1; i < dir_count; ++i) {
        tmp = dir_entries[i];
        for (j = i; j > 0 && before(&tmp, &dir_entries[j - 1]); --j) {
            dir_entries[j] = dir_entries[j - 1];
        }
        dir_entries[j] = tmp;
    }
    return dir_count;
}

uint8_t dir_enter(const char *name)
{
    uint8_t i;
    uint8_t result;

    for (i = 0; i < DIR_NAME_LEN && name[i]; ++i) {
        name_buf[i] = name[i];
    }
    name_buf[i] = 0;
    closeall();
    result = chdir(name_buf);
    closeall(); /* the library's chdir leaves a file handle open */
    return (uint8_t)(result != 0xFF);
}
