/*
 * Modplay65 - periodic interrupt that drives playback.
 */
#include "hw/irq.h"

#include <mega65/memory.h>
#include <string.h>

#define CIA1_TBLO (*(volatile uint8_t *)0xDC06)
#define CIA1_TBHI (*(volatile uint8_t *)0xDC07)
#define CIA1_ICR (*(volatile uint8_t *)0xDC0D)
#define CIA1_CRB (*(volatile uint8_t *)0xDC0F)
#define IRQ_VECTOR ((volatile uint8_t *)0xFFFE)

/* About 2 kHz: 0.5 ms at the CIA clock of roughly 1 MHz. */
#define TIMER_PERIOD 500U

static void (*volatile irq_hook)(void);

/*
 * The hook may use lcopy() (MOD pattern rows). mega65-libc keeps a single
 * DMA list; if the interrupt arrives while the main program is filling in
 * that list, its job would be lost. Saving and restoring the list around
 * the hook makes this safe: the list address in $D701/$D702 is the same
 * for both, so the main program's trigger then runs its own job.
 */
static struct dmagic_dmalist saved_list;

__attribute__((interrupt)) static void irq_handler(void)
{
    (void)CIA1_ICR; /* acknowledge */
    memcpy(&saved_list, (const void *)&dmalist, sizeof(saved_list));
    irq_hook();
    memcpy((void *)&dmalist, &saved_list, sizeof(saved_list));
}

void irq_timer_on(void)
{
    CIA1_ICR = 0x01; /* timer A interrupt off (a SID init may enable it) */
    CIA1_TBLO = (uint8_t)TIMER_PERIOD;
    CIA1_TBHI = (uint8_t)(TIMER_PERIOD >> 8);
    CIA1_CRB = 0x11; /* force load, start, continuous, count clock cycles */
    (void)CIA1_ICR;
    CIA1_ICR = 0x82; /* timer B interrupt on */
}

void irq_init(void (*hook)(void))
{
    irq_off();
    irq_hook = hook;
    IRQ_VECTOR[0] = (uint8_t)(uintptr_t)irq_handler;
    IRQ_VECTOR[1] = (uint8_t)((uintptr_t)irq_handler >> 8);
    irq_timer_on();
    irq_on();
}

void irq_shutdown(void)
{
    irq_off();
    CIA1_ICR = 0x7F;
    CIA1_CRB = 0;
    (void)CIA1_ICR;
}
