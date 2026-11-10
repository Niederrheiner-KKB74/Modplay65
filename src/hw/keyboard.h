/*
 * Modplay65 - MEGA65 hardware keyboard queue.
 *
 * $D610 holds the ASCII code of the oldest queued key, or 0 if the queue is
 * empty. Writing any value removes that key from the queue. This works
 * without the KERNAL and with interrupts disabled, which matters because the
 * llvm-mos startup code runs main() with interrupts off, so KERNAL GETIN
 * never sees a key.
 */
#ifndef MODPLAY65_HW_KEYBOARD_H
#define MODPLAY65_HW_KEYBOARD_H

#include <stdint.h>

#define KEYBOARD_ASCIIKEY (*(volatile uint8_t *)0xD610)

/* Return the next key as ASCII, or 0 if no key is waiting. */
static inline uint8_t keyboard_poll(void)
{
    uint8_t key = KEYBOARD_ASCIIKEY;

    if (key != 0) {
        KEYBOARD_ASCIIKEY = 0;
    }
    return key;
}

/* Throw away all queued keys. */
static inline void keyboard_flush(void)
{
    while (keyboard_poll() != 0) {
    }
}

#endif
