/*
 * Modplay65 - periodic interrupt that drives playback.
 *
 * CIA 1 timer B interrupts about 2000 times per second. The handler only
 * calls a hook; the hook decides from the raster line counter (timebase)
 * whether a MOD tick or a SID play call is due, so the CIA's exact rate
 * does not matter (Xemu does not run the CIAs at the hardware rate).
 *
 * Timer A stays free for SID tunes with CIA timing, which program it in
 * their init routine.
 */
#ifndef MODPLAY65_HW_IRQ_H
#define MODPLAY65_HW_IRQ_H

#include <stdint.h>

/* Install the handler, start timer B and enable interrupts. */
void irq_init(void (*hook)(void));

/* Enable the timer B interrupt again, e.g. after a SID init routine. */
void irq_timer_on(void);

/* Disable the timer interrupt and interrupts as a whole (for exit). */
void irq_shutdown(void);

/* Keep the handler out of a short critical section. */
static inline void irq_off(void)
{
    __asm__ volatile("sei" ::: "memory");
}

static inline void irq_on(void)
{
    __asm__ volatile("cli" ::: "memory");
}

#endif
