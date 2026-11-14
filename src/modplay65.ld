/*
 * Modplay65 linker script: the llvm-mos MEGA65 PRG layout, except that the
 * program area ends at $B4FF. $B500-$CEFF is reserved for the glyph block
 * of the screen (character set copy and pixel canvas, see hw/screen.c) and
 * $CF00-$CFFF for the memory map trampoline (see hw/memmap.c). Large
 * buffers go to the RAM at $E000-$FFF9 (section .hibss).
 *
 * The glyph block has a fixed address on purpose: the VIC-IV reads glyph
 * data from the character ROM instead of RAM whenever the glyph address is
 * in $1000-$1FFF or $9000-$9FFF, so the block must stay clear of those
 * ranges.
 */

__basic_zp_start = 0x0002;
__basic_zp_end = 0x0090;

MEMORY {
    ram (rw) : ORIGIN = 0x2001, LENGTH = 0x94ff
    /* RAM under the KERNAL ROM, switched in by memmap_init(); ends below
     * the CPU vectors at $FFFA. */
    hiram (rw) : ORIGIN = 0xe000, LENGTH = 0x1ffa
}

INCLUDE commodore.ld

INPUT(unmap-basic.o)

/* Zero-initialised buffers placed with MEMMAP_HIGH (hw/memmap.h). Not part
 * of the PRG; memmap_init() clears them. */
SECTIONS {
    .hibss (NOLOAD) : {
        __hibss_start = .;
        *(.hibss .hibss.*)
        __hibss_end = .;
    } >hiram
}

/* Soft stack: grows down from the start of the glyph block. */
__stack = 0xb500;

OUTPUT_FORMAT {
    /* Tells the LOAD command where to place the file's contents. */
    SHORT(0x2001)
    TRIM(ram)
}
