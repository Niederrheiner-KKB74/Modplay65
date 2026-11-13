/*
 * Modplay65 linker script: the llvm-mos MEGA65 PRG layout, except that the
 * program area ends at $B4FF. $B500-$CEFF is reserved for the glyph block
 * of the screen (character set copy and pixel canvas, see hw/screen.c) and
 * $CF00-$CFFF for the SID call trampoline (see sid/sid_play.c).
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
}

INCLUDE commodore.ld

INPUT(unmap-basic.o)

/* Soft stack: grows down from the start of the glyph block. */
__stack = 0xb500;

OUTPUT_FORMAT {
    /* Tells the LOAD command where to place the file's contents. */
    SHORT(0x2001)
    TRIM(ram)
}
