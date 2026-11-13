/*
 * Modplay65 - load a MOD file from the SD card (MEGA65 only).
 */
#include "mod/mod_file.h"

#include <mega65/fileio.h>
#include <mega65/memory.h>

#define ATTIC_SIZE 0x800000UL
#define BLOCK 512U

static uint8_t block[BLOCK];
static uint8_t header[MOD_HEADER_SIZE];
static char name_buf[64];
static const struct mod_region regions[] = { MOD_SAMPLE_REGIONS };
#define NUM_REGIONS (sizeof(regions) / sizeof(regions[0]))

static uint32_t ptr32(const void *p)
{
    return (uint32_t)(uintptr_t)p;
}

const char *mod_file_error_text(enum mod_file_error e)
{
    switch (e) {
    case MOD_FILE_OK: return "ok";
    case MOD_FILE_NOT_FOUND: return "file not found";
    case MOD_FILE_TOO_BIG: return "file is larger than 8 MB";
    case MOD_FILE_NOT_A_MOD: return "not a MOD file";
    case MOD_FILE_CHANNELS: return "only 4-channel modules are supported";
    case MOD_FILE_SAMPLE_TOO_BIG: return "a sample is larger than 64 KB";
    case MOD_FILE_NO_SAMPLE_RAM: return "samples do not fit into sample memory";
    case MOD_FILE_TRUNCATED: return "file is incomplete";
    }
    return "unknown error";
}

/*
 * Copy a sample at a reduced rate: every 2 (shift 1) or 4 (shift 2) source
 * bytes are averaged into one, which also takes the edge off the aliasing.
 */
static void copy_reduced(uint32_t src, uint32_t dst, uint32_t len,
                         uint8_t shift)
{
    uint8_t step = (uint8_t)(1 << shift);

    while (len >= step) {
        uint16_t n = len > BLOCK ? BLOCK : (uint16_t)len;
        uint16_t in, out = 0;

        n &= (uint16_t)~(step - 1);
        lcopy(src, ptr32(block), n);
        for (in = 0; in < n; in += step) {
            int16_t sum = 0;
            uint8_t k;

            for (k = 0; k < step; ++k) {
                sum += (int8_t)block[in + k];
            }
            block[out++] = (uint8_t)(int8_t)(sum >> shift);
        }
        lcopy(ptr32(block), dst, out);
        src += n;
        dst += out;
        len -= n;
    }
}

enum mod_file_error mod_file_load(const char *filename, struct mod_file *mod,
                                  mod_file_progress progress)
{
    uint8_t fd;
    uint8_t i;
    uint16_t n;
    uint32_t size = 0;
    uint32_t offset;
    enum mod_error err;

    /* open() wants a writable string */
    for (i = 0; filename[i] && i < sizeof(name_buf) - 1; ++i) {
        name_buf[i] = filename[i];
    }
    name_buf[i] = 0;

    closeall();
    fd = open(name_buf);
    if (fd == 0xFF) {
        return MOD_FILE_NOT_FOUND;
    }
    for (;;) {
        n = (uint16_t)read512(block);
        if (n == 0) {
            break;
        }
        if (n > BLOCK) {
            n = BLOCK;
        }
        if (size + n > ATTIC_SIZE) {
            close(fd);
            return MOD_FILE_TOO_BIG;
        }
        lcopy(ptr32(block), MOD_FILE_ATTIC + size, n);
        size += n;
        if (progress) {
            progress(size);
        }
        if (n < BLOCK) {
            break;
        }
    }
    close(fd);
    mod->size = size;

    n = size < MOD_HEADER_SIZE ? (uint16_t)size : (uint16_t)MOD_HEADER_SIZE;
    if (n) {
        lcopy(MOD_FILE_ATTIC, ptr32(header), n);
    }
    err = mod_parse_header(header, n, &mod->info);
    if (err == MOD_ERR_CHANNELS) {
        return MOD_FILE_CHANNELS;
    }
    if (err != MOD_OK) {
        return MOD_FILE_NOT_A_MOD;
    }
    if (size < mod->info.sample_offset) {
        return MOD_FILE_TRUNCATED;
    }
    err = mod_fit_samples(&mod->info, regions, NUM_REGIONS, mod->sample_addr,
                          mod->sample_shift, &mod->reduced);
    if (err == MOD_ERR_SAMPLE_TOO_BIG) {
        return MOD_FILE_SAMPLE_TOO_BIG;
    }
    if (err != MOD_OK) {
        return MOD_FILE_NO_SAMPLE_RAM;
    }

    /* Copy the samples to chip RAM. Many files are a little short at the
     * end: clear the sample RAM first and copy only what the file has. */
    for (i = 0; i < NUM_REGIONS; ++i) {
        lfill(regions[i].start, 0,
              (uint16_t)(regions[i].end - regions[i].start));
    }
    offset = mod->info.sample_offset;
    for (i = 0; i < MOD_MAX_SAMPLES; ++i) {
        uint32_t len = mod->info.sample[i].length;

        if (len == 0) {
            continue;
        }
        if (offset < size) {
            uint32_t avail = size - offset;

            if (avail > len) {
                avail = len;
            }
            if (mod->sample_shift[i] == 0) {
                lcopy(MOD_FILE_ATTIC + offset, mod->sample_addr[i],
                      (uint16_t)avail);
            } else {
                copy_reduced(MOD_FILE_ATTIC + offset, mod->sample_addr[i],
                             avail, mod->sample_shift[i]);
            }
        }
        offset += len;
    }
    return MOD_FILE_OK;
}
