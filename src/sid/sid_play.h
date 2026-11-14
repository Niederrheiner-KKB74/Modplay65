/*
 * Modplay65 - play PSID tunes on the MEGA65's SID chips (MEGA65 only).
 *
 * The tune runs in its own 64 KB "C64 memory" in chip RAM bank 4. Each
 * call of the tune's init or play routine goes through memmap_call_tune(),
 * which shows bank 4 in place of bank 0 except for $C000-$DFFF, so the I/O
 * area (and with it the SID at $D400) stays visible. See hw/memmap.h and
 * docs/ARCHITECTURE.md, section 7.
 */
#ifndef MODPLAY65_SID_SID_PLAY_H
#define MODPLAY65_SID_SID_PLAY_H

#include <stdint.h>

#include "hw/fileload.h"
#include "sid/psid.h"

#define SID_C64_RAM 0x040000UL /* the tune's 64 KB of C64 memory */

enum sid_error {
    SID_OK = 0,
    SID_ERR_NOT_FOUND,
    SID_ERR_TOO_BIG,
    SID_ERR_FORMAT,  /* not a valid PSID/RSID file                     */
    SID_ERR_RSID,    /* RSID: needs a complete C64 environment          */
    SID_ERR_PLAY0,   /* play address 0: tune brings its own interrupt   */
    SID_ERR_MEMORY,  /* tune lies in $C000-$DFFF, which stays unmapped  */
    SID_ERR_SID2     /* second or third SID at an address MEGA65 lacks  */
};

struct sid_tune {
    struct psid_info info;
    uint32_t size;     /* file size                                     */
    uint8_t song;      /* current song, 1-based                         */
    uint8_t cia;       /* current song is timed by CIA timer A          */
    uint16_t rate16;   /* play calls per second times 16                */
};

/* Remember the SID settings found at start-up. Call once. */
void sid_system_init(void);

/* Load a SID file and check that it can be played. */
enum sid_error sid_load(const char *filename, struct sid_tune *tune,
                        fileload_progress progress);

/* Short English text for an error code. */
const char *sid_error_text(enum sid_error e);

/*
 * Start a song (1-based): fresh copy of the tune in bank 4, SIDs cleared,
 * init routine called. Sets tune->rate16.
 */
void sid_start_song(struct sid_tune *tune, uint8_t song);

/* Call the play routine once. */
void sid_play_tick(void);

/* Silence all four SIDs completely (stop): all registers zero. */
void sid_silence(void);

/*
 * Silence the voices for a pause: waveform and gate of every voice off,
 * everything else (volume, filter, envelopes) unchanged. Many tunes set
 * the volume only in their init routine, so it must survive a pause; the
 * tune's next play call switches the voices on again.
 */
void sid_pause(void);

/* Silence and put back the SID model setting found at sid_system_init. */
void sid_stop(void);

#endif
