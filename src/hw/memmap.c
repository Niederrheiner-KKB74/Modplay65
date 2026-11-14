/*
 * Modplay65 - CPU memory map and the trampoline at $CF00.
 */
#include "hw/memmap.h"

#include <string.h>

/*
 * Machine code for $CF00. $C000-$DFFF is mapped the same way in all three
 * maps, so this page is visible whichever map is active.
 *
 * MAP operands (45GS02): A = lower offset bits 15-8, X = blocks 3-0 to map
 * (bits 7-4) and offset bits 19-16; Y and Z the same for blocks 7-4.
 *   tune map:   A=$00 X=$F4 Y=$00 Z=$B4  blocks 0-5 and 7 -> +$40000
 *   normal map: A=$00 X=$E0 Y=$00 Z=$00  blocks 1-3 1:1, nothing above
 *   BASIC map:  A=$00 X=$E0 Y=$00 Z=$83  as normal, block 7 -> +$30000
 */
#define TRAMPOLINE ((volatile uint8_t *)0xCF00)
#define T_SONG 0x0D   /* operand of "lda #a"         */
#define T_TARGET 0x13 /* operand of "jsr target"     */
#define T_NORMAL 0x25 /* entry: set the normal map   */
#define T_BASIC 0x3E  /* entry: set the BASIC map    */

static const uint8_t trampoline_code[] = {
    /* $00 call a tune routine */
    0x08,             /* php                                        */
    0x78,             /* sei                                        */
    0xA9, 0x00,       /* lda #$00                                   */
    0xA2, 0xF4,       /* ldx #$F4                                   */
    0xA0, 0x00,       /* ldy #$00                                   */
    0xA3, 0xB4,       /* ldz #$B4                                   */
    0x5C,             /* map                                        */
    0xEA,             /* eom        now running in the tune's map    */
    0xA9, 0x00,       /* $0C: lda #a                                 */
    0xA2, 0x00,       /* ldx #0                                     */
    0xA0, 0x00,       /* ldy #0                                     */
    0x20, 0x00, 0x00, /* $12: jsr target                             */
    /* $15 normal map */
    0x78,             /* sei        the tune may have done cli       */
    0xD8,             /* cld        ... or left decimal mode on      */
    0xA9, 0x00,       /* lda #$00                                   */
    0xA2, 0xE0,       /* ldx #$E0                                   */
    0xA0, 0x00,       /* ldy #$00                                   */
    0xA3, 0x00,       /* ldz #$00                                   */
    0x5C,             /* map                                        */
    0xEA,             /* eom                                        */
    0xA3, 0x00,       /* ldz #0     compiled code expects Z = 0      */
    0x28,             /* plp                                        */
    0x60,             /* rts                                        */
    /* $25 entry: normal map */
    0x08,             /* php                                        */
    0x4C, 0x15, 0xCF, /* jmp $CF15                                  */
    /* $29 padding */
    0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA,
    /* $30 interrupt stub: acknowledge everything and return */
    0x48,             /* pha                                        */
    0xAD, 0x0D, 0xDC, /* lda $DC0D                                  */
    0xAD, 0x0D, 0xDD, /* lda $DD0D                                  */
    0xA9, 0xFF,       /* lda #$FF                                   */
    0x8D, 0x19, 0xD0, /* sta $D019                                  */
    0x68,             /* pla                                        */
    0x40,             /* rti                                        */
    /* $3E entry: BASIC map */
    0x08,             /* php                                        */
    0x78,             /* sei                                        */
    0xA9, 0x00,       /* lda #$00                                   */
    0xA2, 0xE0,       /* ldx #$E0                                   */
    0xA0, 0x00,       /* ldy #$00                                   */
    0xA3, 0x83,       /* ldz #$83                                   */
    0x5C,             /* map                                        */
    0xEA,             /* eom                                        */
    0xA3, 0x00,       /* ldz #0                                     */
    0x28,             /* plp                                        */
    0x60              /* rts                                        */
};

_Static_assert(sizeof(trampoline_code) == T_BASIC + 16,
               "trampoline layout changed: check the T_* offsets");
_Static_assert(0xCF00U + 0x30U == MEMMAP_IRQ_STUB, "stub address");

typedef void (*tramp_fn)(void);
#define ENTRY(offset) (((tramp_fn)(0xCF00U + (offset)))())

#define CPU_PORT (*(volatile uint8_t *)0x0001)
#define VECTORS ((volatile uint8_t *)0xFFFA)

extern char __hibss_start[];
extern char __hibss_end[];

void memmap_init(void)
{
    uint8_t i;

    for (i = 0; i < sizeof(trampoline_code); ++i) {
        TRAMPOLINE[i] = trampoline_code[i];
    }
    ENTRY(T_NORMAL);
    /* LORAM=1 HIRAM=0 CHAREN=1: I/O at $D000, RAM at $A000 and $E000
     * ($D030 bit 7, the C65 KERNAL at $E000, is already clear). */
    CPU_PORT = 0x35;
    for (i = 0; i < 6; i += 2) {
        VECTORS[i] = (uint8_t)MEMMAP_IRQ_STUB;
        VECTORS[i + 1] = (uint8_t)(MEMMAP_IRQ_STUB >> 8);
    }
    memset(__hibss_start, 0, (size_t)(__hibss_end - __hibss_start));
}

void memmap_basic(void)
{
    ENTRY(T_BASIC);
}

void memmap_call_tune(uint16_t address, uint8_t a)
{
    TRAMPOLINE[T_SONG] = a;
    TRAMPOLINE[T_TARGET] = (uint8_t)address;
    TRAMPOLINE[T_TARGET + 1] = (uint8_t)(address >> 8);
    ENTRY(0);
}
