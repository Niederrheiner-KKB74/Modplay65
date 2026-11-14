/*
 * Modplay65 - leaving the program.
 */
#include "hw/system.h"

#include <stdint.h>

#include "hw/audio_dma.h"
#include "hw/memmap.h"
#include "hw/screen.h"

void system_exit_to_basic(void)
{
    audio_dma_shutdown();
    screen_restore();
    /* The reset vector must come from the C65 KERNAL, so switch to the
     * BASIC map. Do it first: the $D030 value below shows ROM at $C000,
     * over the trampoline. Then undo what the llvm-mos startup code
     * changed: BASIC ROM back in, C65 ROMs mapped as before. */
    memmap_basic();
    *(volatile uint8_t *)0x0001 = 0x3F;
    *(volatile uint8_t *)0xD030 = 0x64;
    __asm__("jmp ($fffc)");
    for (;;) {
    }
}
