/*
 * Modplay65 - leaving the program.
 */
#include "hw/system.h"

#include <stdint.h>

#include "hw/audio_dma.h"

void system_exit_to_basic(void)
{
    audio_dma_shutdown();
    /* Undo what the llvm-mos startup code changed (see unmap-basic.S in
     * the SDK): BASIC ROM back in, C65 ROMs mapped as before. */
    *(volatile uint8_t *)0x0001 = 0x3F;
    *(volatile uint8_t *)0xD030 = 0x64;
    __asm__("jmp ($fffc)");
    for (;;) {
    }
}
