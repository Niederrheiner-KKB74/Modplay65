/*
 * Modplay65 - CPU memory map: the 45GS02 MAP settings Modplay65 uses and
 * the trampoline at $CF00 that switches between them.
 *
 * Normal map (while Modplay65 runs):
 *   $0000-$1FFF  bank 0               $8000-$BFFF  bank 0
 *   $2000-$7FFF  bank 0 (mapped 1:1)  $C000-$DFFF  bank 0 RAM, I/O at $D000
 *                                     $E000-$FFFF  bank 0 RAM (not KERNAL)
 * BASIC map: as normal, but $E000-$FFFF shows the C65 KERNAL ($3E000);
 *   that is the state BASIC 65 starts a program in, and the one the reset
 *   vector must be read through when leaving to BASIC.
 * Tune map: everything except $C000-$DFFF shows bank 4 (SID tunes).
 */
#ifndef MODPLAY65_HW_MEMMAP_H
#define MODPLAY65_HW_MEMMAP_H

#include <stdint.h>

/*
 * Put large zero-initialised buffers into the RAM at $E000-$FFF9. Such a
 * variable is cleared by memmap_init() and must not be used before it.
 */
#define MEMMAP_HIGH __attribute__((section(".hibss")))

/* Address of an interrupt handler that acknowledges all chips and returns. */
#define MEMMAP_IRQ_STUB 0xCF30U

/*
 * Install the trampoline, switch $E000-$FFFF to RAM, point the CPU vectors
 * at the stub and clear the MEMMAP_HIGH variables. Call first in main().
 */
void memmap_init(void);

/* Switch to the BASIC map (before leaving to BASIC). */
void memmap_basic(void);

/*
 * Call a routine of a SID tune: switch to the tune map, JSR address with
 * A = a and X = Y = 0, switch back. Interrupts stay disabled.
 */
void memmap_call_tune(uint16_t address, uint8_t a);

/*
 * Top of the soft stack after start-up: the RAM under the KERNAL, below
 * the CPU vectors, grows down towards the .hibss buffers.
 */
#define MEMMAP_STACK 0xFFFAU

/*
 * Moves the soft stack pointer to MEMMAP_STACK. Use only in a function
 * without stack frame, after memmap_init(), and call the rest of the
 * program from there through a function that is not inlined.
 */
#define MEMMAP_MOVE_STACK()                                                  \
    __asm__ volatile("lda #<0xfffa\n sta __rc0\n lda #>0xfffa\n sta __rc1" \
                     ::: "a", "memory")

#endif
