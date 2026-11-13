/*
 * Modplay65 - play PSID tunes on the MEGA65's SID chips.
 */
#include "sid/sid_play.h"

#include <mega65/memory.h>

/* ---- trampoline ------------------------------------------------------ */

/*
 * Machine code for $CF00. $C000-$DFFF is never mapped, so this page is
 * visible both with the normal map and with the tune's map.
 *
 * MAP operands (45GS02): A = lower offset bits 15-8, X = blocks 3-0 to map
 * (bits 7-4) and offset bits 19-16, Y and Z the same for blocks 7-4.
 *   tune map:   blocks 0-3 and 4, 5, 7 -> +$40000 (bank 4)
 *   normal map: blocks 1-3 identity, block 7 -> +$30000 (C65 KERNAL),
 *               the state BASIC 65 leaves for a program started with RUN.
 * With block 0 mapped, $00/$01 are plain RAM, not the CPU port, so the
 * tune cannot change the real ROM/I/O banking.
 */
#define TRAMPOLINE ((volatile uint8_t *)0xCF00)
#define T_SONG 0x0D   /* operand of "lda #song"        */
#define T_TARGET 0x13 /* operand of "jsr target"       */
#define T_HOME 0x25   /* entry: set the normal map     */
#define T_IRQ 0x30    /* interrupt stub for the tune's vectors */

static const uint8_t trampoline_code[] = {
    /* $00 call tune */
    0x08,             /* php                                        */
    0x78,             /* sei                                        */
    0xA9, 0x00,       /* lda #$00                                   */
    0xA2, 0xF4,       /* ldx #$F4   blocks 0-3, +$40000              */
    0xA0, 0x00,       /* ldy #$00                                   */
    0xA3, 0xB4,       /* ldz #$B4   blocks 4, 5, 7, +$40000          */
    0x5C,             /* map                                        */
    0xEA,             /* eom        now running in the tune's map    */
    0xA9, 0x00,       /* $0C: lda #song                              */
    0xA2, 0x00,       /* ldx #0                                     */
    0xA0, 0x00,       /* ldy #0                                     */
    0x20, 0x00, 0x00, /* $12: jsr target                             */
    /* $15 back to the normal map */
    0x78,             /* sei        the tune may have done cli       */
    0xD8,             /* cld        ... or left decimal mode on      */
    0xA9, 0x00,       /* lda #$00                                   */
    0xA2, 0xE0,       /* ldx #$E0   blocks 1-3, offset 0             */
    0xA0, 0x00,       /* ldy #$00                                   */
    0xA3, 0x83,       /* ldz #$83   block 7, +$30000                 */
    0x5C,             /* map                                        */
    0xEA,             /* eom                                        */
    0xA3, 0x00,       /* ldz #0     compiled code expects Z = 0      */
    0x28,             /* plp                                        */
    0x60,             /* rts                                        */
    /* $25 entry: set the normal map */
    0x08,             /* php                                        */
    0x4C, 0x15, 0xCF, /* jmp $CF15                                  */
    /* $29 padding up to the interrupt stub */
    0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
    /* $30 interrupt stub: acknowledge everything and return */
    0x48,             /* pha                                        */
    0xAD, 0x0D, 0xDC, /* lda $DC0D                                  */
    0xAD, 0x0D, 0xDD, /* lda $DD0D                                  */
    0xA9, 0xFF,       /* lda #$FF                                   */
    0x8D, 0x19, 0xD0, /* sta $D019                                  */
    0x68,             /* pla                                        */
    0x40              /* rti                                        */
};

_Static_assert(sizeof(trampoline_code) == T_IRQ + 14,
               "trampoline layout changed: check the T_* offsets");

typedef void (*tramp_fn)(void);
#define CALL_TUNE() (((tramp_fn)0xCF00)())
#define SET_HOME_MAP() (((tramp_fn)(0xCF00 + T_HOME))())

/* ---- hardware -------------------------------------------------------- */

#define SID_BASE 0xD400U
#define SID_COUNT 4
#define SIDMODE (*(volatile uint8_t *)0xD63C) /* bits 0-3: 1 = 8580 */
#define CIA1_TALO (*(volatile uint8_t *)0xDC04)
#define CIA1_TAHI (*(volatile uint8_t *)0xDC05)
#define CIA1_ICR (*(volatile uint8_t *)0xDC0D)
#define CIA1_CRA (*(volatile uint8_t *)0xDC0E)
#define CIA2_ICR (*(volatile uint8_t *)0xDD0D)
#define VIC_IRQ (*(volatile uint8_t *)0xD019)
#define VIC_IRQ_MASK (*(volatile uint8_t *)0xD01A)

#define PAL_CLOCK16 15763968UL  /* 985248 Hz x 16  */
#define NTSC_CLOCK16 16363632UL /* 1022727 Hz x 16 */
#define PAL_VBI16 802           /* 50.125 Hz x 16  */
#define NTSC_VBI16 957          /* 59.826 Hz x 16  */

static uint8_t saved_sidmode;
static const struct psid_info *current;

void sid_system_init(void)
{
    uint8_t i;

    for (i = 0; i < sizeof(trampoline_code); ++i) {
        TRAMPOLINE[i] = trampoline_code[i];
    }
    SET_HOME_MAP();
    saved_sidmode = SIDMODE;
}

void sid_silence(void)
{
    uint8_t s, r;

    for (s = 0; s < SID_COUNT; ++s) {
        volatile uint8_t *sid = (volatile uint8_t *)(SID_BASE + s * 0x20U);

        for (r = 0; r < 0x19; ++r) {
            sid[r] = 0;
        }
    }
}

void sid_pause(void)
{
    uint8_t s;

    for (s = 0; s < SID_COUNT; ++s) {
        volatile uint8_t *sid = (volatile uint8_t *)(SID_BASE + s * 0x20U);

        sid[0x04] = 0;
        sid[0x0B] = 0;
        sid[0x12] = 0;
    }
}

void sid_stop(void)
{
    sid_silence();
    SIDMODE = saved_sidmode;
    current = 0;
}

const char *sid_error_text(enum sid_error e)
{
    switch (e) {
    case SID_OK: return "ok";
    case SID_ERR_NOT_FOUND: return "file not found";
    case SID_ERR_TOO_BIG: return "file is too large";
    case SID_ERR_FORMAT: return "not a PSID/RSID file";
    case SID_ERR_RSID: return "RSID tunes are not supported yet";
    case SID_ERR_PLAY0: return "tune needs its own interrupt handler (not supported yet)";
    case SID_ERR_MEMORY: return "tune uses $C000-$DFFF (not supported yet)";
    case SID_ERR_SID2: return "tune needs a SID address the MEGA65 lacks";
    }
    return "unknown error";
}

/* Does [a, a + len) touch $C000-$DFFF? */
static uint8_t in_block6(uint16_t a, uint16_t len)
{
    uint32_t end = (uint32_t)a + len; /* exclusive */

    return (uint8_t)(a < 0xE000U && end > 0xC000UL);
}

static uint8_t sid_address_ok(uint16_t a)
{
    return (uint8_t)(a == 0 || a == 0xD420U || a == 0xD440U || a == 0xD460U);
}

enum sid_error sid_load(const char *filename, struct sid_tune *tune,
                        fileload_progress progress)
{
    uint8_t *header = fileload_buffer; /* free again after loading */
    uint16_t n;

    switch (fileload_to_attic(filename, &tune->size, progress)) {
    case FILELOAD_NOT_FOUND: return SID_ERR_NOT_FOUND;
    case FILELOAD_TOO_BIG: return SID_ERR_TOO_BIG;
    default: break;
    }
    n = tune->size < FILELOAD_BLOCK ? (uint16_t)tune->size : FILELOAD_BLOCK;
    lcopy(FILELOAD_ATTIC, (uint32_t)(uintptr_t)header, n);
    if (psid_parse(header, n, tune->size, &tune->info) != PSID_OK) {
        return SID_ERR_FORMAT;
    }
    if (tune->info.rsid) {
        return SID_ERR_RSID;
    }
    if (tune->info.play == 0) {
        return SID_ERR_PLAY0;
    }
    if (in_block6(tune->info.load, tune->info.length) ||
        in_block6(tune->info.init, 1) || in_block6(tune->info.play, 1)) {
        return SID_ERR_MEMORY;
    }
    if (!sid_address_ok(tune->info.sid2) || !sid_address_ok(tune->info.sid3)) {
        return SID_ERR_SID2;
    }
    return SID_OK;
}

/* Point the tune's interrupt vectors at the stub, unless the tune is there. */
static void set_vectors(const struct psid_info *info)
{
    uint8_t v;

    if ((uint32_t)info->load + info->length > 0xFFFAUL) {
        return;
    }
    for (v = 0; v < 6; v += 2) {
        lpoke(SID_C64_RAM + 0xFFFAUL + v, (uint8_t)(0xCF00U + T_IRQ));
        lpoke(SID_C64_RAM + 0xFFFBUL + v, (uint8_t)((0xCF00U + T_IRQ) >> 8));
    }
}

static void call(uint16_t address, uint8_t a)
{
    TRAMPOLINE[T_SONG] = a;
    TRAMPOLINE[T_TARGET] = (uint8_t)address;
    TRAMPOLINE[T_TARGET + 1] = (uint8_t)(address >> 8);
    CALL_TUNE();
}

void sid_start_song(struct sid_tune *tune, uint8_t song)
{
    const struct psid_info *info = &tune->info;
    uint8_t ntsc = (uint8_t)(info->clock == PSID_CLOCK_NTSC);
    uint16_t latch;

    if (song == 0 || song > info->songs) {
        song = info->start_song;
    }
    tune->song = song;
    current = info;
    sid_silence();
    SIDMODE = (uint8_t)((saved_sidmode & 0xF0) |
                        (info->model == PSID_MODEL_8580 ? 0x0F : 0x00));

    /* Fresh C64 memory: init routines change their own data. */
    lfill(SID_C64_RAM, 0, 0x8000);
    lfill(SID_C64_RAM + 0x8000UL, 0, 0x8000);
    lcopy(FILELOAD_ATTIC + info->data_offset, SID_C64_RAM + info->load,
          info->length);
    lpoke(SID_C64_RAM + 0, 0x2F); /* C64 defaults of the CPU port */
    lpoke(SID_C64_RAM + 1, 0x37);
    set_vectors(info);

    /* No interrupts from the chips; Modplay65 itself never uses them. */
    CIA1_ICR = 0x7F;
    CIA2_ICR = 0x7F;
    VIC_IRQ_MASK = 0;
    /* Stop CIA1 timer A and load $FFFF: an init routine that sets the
     * timer for CIA timing leaves its value in the counter. */
    CIA1_CRA = 0;
    CIA1_TALO = 0xFF;
    CIA1_TAHI = 0xFF;

    call(info->init, (uint8_t)(song - 1));

    (void)CIA1_ICR;
    (void)CIA2_ICR;
    VIC_IRQ = 0xFF;
    tune->cia = psid_song_uses_cia(info, song);
    if (tune->cia) {
        latch = CIA1_TALO;
        latch = (uint16_t)(latch | ((uint16_t)CIA1_TAHI << 8));
        CIA1_CRA = 0;
        if (latch == 0xFFFF || latch < 0x0400) {
            latch = 0x4025; /* not set by init: C64 default, 60 Hz */
        }
        tune->rate16 = (uint16_t)((ntsc ? NTSC_CLOCK16 : PAL_CLOCK16) /
                                  ((uint32_t)latch + 1));
    } else {
        tune->rate16 = ntsc ? NTSC_VBI16 : PAL_VBI16;
    }
}

void sid_play_tick(void)
{
    if (current) {
        call(current->play, 0);
    }
}
