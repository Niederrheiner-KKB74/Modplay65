/*
 * Modplay65 - PSID/RSID file header parsing.
 */
#include "sid/psid.h"

#include <string.h>

static uint16_t be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* Copy a Latin-1 text field as ASCII; umlauts become two letters. */
static void copy_text(char *dst, const uint8_t *src)
{
    static const uint8_t latin1[] = { 0xC4, 0xD6, 0xDC, 0xE4, 0xF6, 0xFC, 0xDF };
    static const char ascii[][3] = { "Ae", "Oe", "Ue", "ae", "oe", "ue", "ss" };
    uint8_t in = 0, out = 0;

    while (in < PSID_TEXT_LEN && src[in] != 0 && out < PSID_TEXT_LEN) {
        uint8_t c = src[in++];
        uint8_t k;

        if (c >= 0x20 && c < 0x7F) {
            dst[out++] = (char)c;
            continue;
        }
        for (k = 0; k < sizeof(latin1); ++k) {
            if (c == latin1[k]) {
                break;
            }
        }
        if (k < sizeof(latin1)) {
            dst[out++] = ascii[k][0];
            if (out < PSID_TEXT_LEN) {
                dst[out++] = ascii[k][1];
            }
        } else {
            dst[out++] = '?';
        }
    }
    dst[out] = 0;
}

/* Second/third SID address byte $xy means $Dxy0; only $D420-$DFE0 valid. */
static uint16_t sid_address(uint8_t b)
{
    if (b < 0x42 || (b >= 0x80 && b < 0xE0) || (b & 1)) {
        return 0;
    }
    return (uint16_t)(0xD000U | ((uint16_t)b << 4));
}

enum psid_error psid_parse(const uint8_t *header, uint16_t header_len,
                           uint32_t file_size, struct psid_info *info)
{
    uint16_t offset;

    memset(info, 0, sizeof(*info));
    if (header_len < 0x76 || file_size < 0x76) {
        return PSID_ERR_TOO_SHORT;
    }
    if (memcmp(header, "PSID", 4) == 0) {
        info->rsid = 0;
    } else if (memcmp(header, "RSID", 4) == 0) {
        info->rsid = 1;
    } else {
        return PSID_ERR_NOT_SID;
    }
    info->version = header[5];
    if (header[4] != 0 || info->version < 1 || info->version > 4) {
        return PSID_ERR_VERSION;
    }
    offset = be16(header + 6);
    if (offset < 0x76 || (info->version >= 2 && header_len < PSID_HEADER_SIZE)) {
        return PSID_ERR_TOO_SHORT;
    }
    info->load = be16(header + 8);
    info->init = be16(header + 0x0A);
    info->play = be16(header + 0x0C);
    info->songs = header[0x0F] ? header[0x0F] : 1;
    if (header[0x0E] != 0) {
        info->songs = 255; /* more than 255 songs: keep the first 255 */
    }
    info->start_song = header[0x11];
    if (header[0x10] != 0 || info->start_song == 0 ||
        info->start_song > info->songs) {
        info->start_song = 1;
    }
    info->speed = ((uint32_t)be16(header + 0x12) << 16) | be16(header + 0x14);
    copy_text(info->name, header + 0x16);
    copy_text(info->author, header + 0x36);
    copy_text(info->released, header + 0x56);

    if (info->version >= 2) {
        uint16_t flags = be16(header + 0x76);

        info->clock = (uint8_t)((flags >> 2) & 3);
        info->model = (uint8_t)((flags >> 4) & 3);
        info->start_page = header[0x78];
        info->page_length = header[0x79];
        if (info->version >= 3) {
            info->sid2 = sid_address(header[0x7A]);
        }
        if (info->version >= 4) {
            info->sid3 = sid_address(header[0x7B]);
        }
    }

    /* Data: with load address 0 the first two bytes hold it. */
    if (file_size <= offset) {
        return PSID_ERR_NO_DATA;
    }
    if (info->load == 0) {
        if (file_size < (uint32_t)offset + 3 || header_len < offset + 2U) {
            return file_size < (uint32_t)offset + 3 ? PSID_ERR_NO_DATA
                                                    : PSID_ERR_TOO_SHORT;
        }
        info->load = (uint16_t)(header[offset] | (header[offset + 1] << 8));
        offset = (uint16_t)(offset + 2);
    }
    info->data_offset = offset;
    if ((uint32_t)info->load + (file_size - offset) > 0x10000UL) {
        return PSID_ERR_TOO_LONG;
    }
    info->length = (uint16_t)(file_size - offset);
    if (info->init == 0) {
        info->init = info->load; /* spec: init 0 means the load address */
    }
    return PSID_OK;
}

uint8_t psid_song_uses_cia(const struct psid_info *info, uint8_t song)
{
    uint8_t bit = (uint8_t)(song > 32 ? 31 : song - 1);

    return (uint8_t)((info->speed >> bit) & 1);
}
