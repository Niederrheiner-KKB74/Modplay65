/*
 * Modplay65 - directory listing of the SD card (MEGA65 only).
 */
#include "ui/dir.h"

#include <mega65/dirent.h>
#include <mega65/fileio.h>
#include <mega65/memory.h>

/*
 * The hypervisor writes its directory entry to page $0400 (the library's
 * readdir asks for that page):
 *   $00-$3F long file name, $40 its length, $41-$4D 8.3 name,
 *   $4E-$51 start cluster, $52-$55 file length, $56 FAT attributes.
 * The library's own copy has the attribute byte off by one (it returns
 * the top byte of the file length), so the raw entry is used here.
 */
#define RAW_DIRENT ((const uint8_t *)0x0400)
#define RAW_NAME_LEN 0x40
#define RAW_ATTR 0x56

#define FAT_ATTR_VOLUME 0x08
#define FAT_ATTR_DIR 0x10
#define FAT_ATTR_HIDDEN 0x02

/* The list lives in Attic RAM, far above the module data. */
#define DIR_STORE 0x87E0000UL
#define ENTRY_SIZE ((uint32_t)sizeof(struct dir_entry))

uint8_t dir_count;

static uint8_t order[DIR_MAX_ENTRIES]; /* sorted position -> stored slot */
static struct dir_entry current;       /* buffer handed out by dir_entry */
static struct dir_entry other;         /* second buffer for comparisons  */
static uint8_t current_slot = 0xFF;

static void fetch(uint8_t slot, struct dir_entry *dst)
{
    lcopy(DIR_STORE + slot * ENTRY_SIZE, (uint32_t)(uintptr_t)dst,
          sizeof(struct dir_entry));
}

const struct dir_entry *dir_entry(uint8_t index)
{
    uint8_t slot = order[index];

    if (slot != current_slot) {
        fetch(slot, &current);
        current_slot = slot;
    }
    return &current;
}

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

uint8_t dir_is_sid(const char *name)
{
    uint8_t len = 0;

    while (name[len]) {
        ++len;
    }
    return (uint8_t)(len >= 5 && name[len - 4] == '.' &&
                     lower((uint8_t)name[len - 3]) == 's' &&
                     lower((uint8_t)name[len - 2]) == 'i' &&
                     lower((uint8_t)name[len - 1]) == 'd');
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
    uint8_t fd;
    uint8_t i, j;
    uint16_t guard = 0;

    dir_count = 0;
    current_slot = 0xFF;
    closeall();
    fd = opendir();
    if (fd == 0xFF) {
        return 0;
    }
    while (dir_count < DIR_MAX_ENTRIES && ++guard < 4000) {
        const char *name = (const char *)RAW_DIRENT;
        uint8_t len;
        uint8_t attr;

        if (readdir(fd) == 0) {
            break;
        }
        len = RAW_DIRENT[RAW_NAME_LEN];
        attr = RAW_DIRENT[RAW_ATTR];
        if (len == 0) {
            break;
        }
        if (len > DIR_NAME_LEN) {
            len = DIR_NAME_LEN;
        }
        if (attr & (FAT_ATTR_VOLUME | FAT_ATTR_HIDDEN)) {
            continue;
        }
        for (i = 0; i < len; ++i) {
            other.name[i] = name[i];
        }
        other.name[len] = 0;
        other.is_dir = (uint8_t)((attr & FAT_ATTR_DIR) != 0);
        if (other.is_dir) {
            if (other.name[0] == '.' && other.name[1] == 0) {
                continue;
            }
        } else if (!dir_is_mod(other.name) && !dir_is_sid(other.name)) {
            continue;
        }
        lcopy((uint32_t)(uintptr_t)&other, DIR_STORE + dir_count * ENTRY_SIZE,
              sizeof(struct dir_entry));
        ++dir_count;
    }
    closedir(fd);

    /* Insertion sort of the index list; the entries stay where they are. */
    for (i = 0; i < dir_count; ++i) {
        fetch(i, &other);
        for (j = i; j > 0; --j) {
            fetch(order[j - 1], &current);
            if (!before(&other, &current)) {
                break;
            }
            order[j] = order[j - 1];
        }
        order[j] = i;
    }
    current_slot = 0xFF;
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
