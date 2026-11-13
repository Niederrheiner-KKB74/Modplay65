/*
 * Modplay65 - leaving the program.
 */
#ifndef MODPLAY65_HW_SYSTEM_H
#define MODPLAY65_HW_SYSTEM_H

/*
 * Leave the program by restarting the machine's BASIC through the reset
 * vector. A plain return to BASIC is not possible: the program uses
 * BASIC's zero page and bank 1, and changes video and memory settings.
 * The program is gone from memory afterwards; the SD card and mounted
 * disk images are not affected. Does not return.
 */
void system_exit_to_basic(void);

#endif
