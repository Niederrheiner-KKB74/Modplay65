/*
 * Modplay65 - read a whole file from the SD card into Attic RAM.
 */
#include "hw/fileload.h"

#include <mega65/fileio.h>
#include <mega65/memory.h>

uint8_t fileload_buffer[FILELOAD_BLOCK];
static char name_buf[66];

enum fileload_result fileload_to_attic(const char *name, uint32_t *size,
                                       fileload_progress progress)
{
    uint8_t fd;
    uint8_t i;
    uint16_t n;
    uint32_t total = 0;

    /* open() wants a writable string */
    for (i = 0; name[i] && i < sizeof(name_buf) - 1; ++i) {
        name_buf[i] = name[i];
    }
    name_buf[i] = 0;

    closeall();
    fd = open(name_buf);
    if (fd == 0xFF) {
        return FILELOAD_NOT_FOUND;
    }
    for (;;) {
        n = (uint16_t)read512(fileload_buffer);
        if (n == 0) {
            break;
        }
        if (n > FILELOAD_BLOCK) {
            n = FILELOAD_BLOCK;
        }
        if (total + n > FILELOAD_MAX) {
            close(fd);
            return FILELOAD_TOO_BIG;
        }
        lcopy((uint32_t)(uintptr_t)fileload_buffer, FILELOAD_ATTIC + total, n);
        total += n;
        if (progress) {
            progress(total);
        }
        if (n < FILELOAD_BLOCK) {
            break;
        }
    }
    close(fd);
    *size = total;
    return FILELOAD_OK;
}
