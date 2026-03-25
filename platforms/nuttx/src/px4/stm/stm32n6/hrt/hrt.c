/****************************************************************************
 * STM32N6 High Resolution Timer — TIM5 hardware implementation
 *
 * TIM5 is a 32-bit general purpose timer on APB1.
 * Configured as free-running 1MHz counter (1us resolution).
 * CC1 used for callout scheduling (output compare interrupt).
 ****************************************************************************/

#include <px4_platform_common/px4_config.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <chip.h>
#include <arm_internal.h>
#include <stm32_gpio.h>
#include <hardware/stm32n6xxx_tim.h>
#include <hardware/stm32n6xxx_rcc.h>

#include <drivers/drv_hrt.h>

/* TIM5: 32-bit timer on APB1 */
#define HRT_TIMER_BASE    STM32_TIM5_BASE
#define HRT_TIMER_CLOCK   STM32_APB1_TIM_FREQUENCY
#define HRT_TIMER_IRQ     STM32_IRQ_TIM5

/* Register accessors */
#define rCR1   (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_CR1_OFFSET))
#define rDIER  (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_DIER_OFFSET))
#define rSR    (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_SR_OFFSET))
#define rCNT   (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_CNT_OFFSET))
#define rPSC   (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_PSC_OFFSET))
#define rARR   (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_ARR_OFFSET))
#define rCCR1  (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_CCR1_OFFSET))
#define rCCMR1 (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_CCMR1_OFFSET))
#define rCCER  (*(volatile uint32_t *)(HRT_TIMER_BASE + STM32_GTIM_CCER_OFFSET))
#define rEGR   (*(volatile uint32_t *)(HRT_TIMER_BASE + 0x0014))

/* Callout queue */
static struct sq_queue_s callout_queue;

/****************************************************************************
 * hrt_absolute_time — read TIM5 counter (microseconds since boot)
 ****************************************************************************/

hrt_abstime hrt_absolute_time(void)
{
	return (hrt_abstime)rCNT;
}

/* Minimum compare interval — avoids setting CCR1 in the past */
#define HRT_INTERVAL_MIN   50        /* 50 µs */

/* Maximum compare interval — safety net to keep callouts flowing */
#define HRT_INTERVAL_MAX   50000     /* 50 ms */

/****************************************************************************
 * Internal: set CC1 for next deadline or a safety-net max interval
 ****************************************************************************/

static void hrt_call_reschedule(void)
{
	hrt_abstime now = hrt_absolute_time();
	struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);
	hrt_abstime deadline = now + HRT_INTERVAL_MAX;

	if (next) {
		if (next->deadline <= (now + HRT_INTERVAL_MIN)) {
			/* Already expired or about to — fire ASAP */
			deadline = now + HRT_INTERVAL_MIN;
		} else if (next->deadline < deadline) {
			deadline = next->deadline;
		}
	}

	rCCR1 = (uint32_t)(deadline & 0xFFFFFFFF);
	rSR = ~(1 << 1);       /* Clear CC1IF */
	rDIER |= (1 << 1);     /* Enable CC1IE */
}

/****************************************************************************
 * TIM5 CC1 interrupt handler — dispatch expired callouts
 ****************************************************************************/

static int hrt_tim_isr(int irq, void *context, void *arg)
{
	(void)irq;
	(void)context;
	(void)arg;

	uint32_t sr = rSR;

	if (sr & (1 << 1)) {  /* CC1IF */
		rSR = ~(1 << 1);  /* Clear CC1IF */

		hrt_abstime now = hrt_absolute_time();

		while (sq_peek(&callout_queue) != NULL) {
			struct hrt_call *entry =
				(struct hrt_call *)sq_peek(&callout_queue);

			if ((int32_t)(entry->deadline - now) > 0) {
				break;  /* Not yet due (handles wrap) */
			}

			sq_remfirst(&callout_queue);

			hrt_callout cb = entry->callout;
			void *cb_arg = entry->arg;
			hrt_abstime period = entry->period;

			if (cb) {
				cb(cb_arg);
			}

			/* Re-arm periodic */
			if (period > 0 && entry->callout) {
				entry->deadline = hrt_absolute_time() + period;

				/* Insert sorted */
				struct hrt_call *p = NULL;
				struct hrt_call *n =
					(struct hrt_call *)sq_peek(&callout_queue);

				while (n && (int32_t)(n->deadline - entry->deadline) <= 0) {
					p = n;
					n = (struct hrt_call *)sq_next(&n->link);
				}

				if (p == NULL) {
					sq_addfirst(&entry->link, &callout_queue);
				} else {
					sq_addafter(&p->link, &entry->link,
						    &callout_queue);
				}
			}

			now = hrt_absolute_time();
		}

		hrt_call_reschedule();
	}

	return 0;
}

/****************************************************************************
 * hrt_call_after
 ****************************************************************************/

void hrt_call_after(struct hrt_call *entry, hrt_abstime delay,
		    hrt_callout callout, void *arg)
{
	irqstate_t flags = enter_critical_section();

	sq_rem(&entry->link, &callout_queue);

	entry->callout = callout;
	entry->arg = arg;
	entry->period = 0;
	entry->deadline = hrt_absolute_time() + delay;

	/* Insert sorted by deadline */
	struct hrt_call *p = NULL;
	struct hrt_call *n = (struct hrt_call *)sq_peek(&callout_queue);

	while (n && (int32_t)(n->deadline - entry->deadline) <= 0) {
		p = n;
		n = (struct hrt_call *)sq_next(&n->link);
	}

	if (p == NULL) {
		sq_addfirst(&entry->link, &callout_queue);
	} else {
		sq_addafter(&p->link, &entry->link, &callout_queue);
	}

	hrt_call_reschedule();
	leave_critical_section(flags);
}

/****************************************************************************
 * hrt_call_every
 ****************************************************************************/

void hrt_call_every(struct hrt_call *entry, hrt_abstime delay,
		    hrt_abstime interval, hrt_callout callout, void *arg)
{
	entry->period = interval;
	hrt_call_after(entry, delay, callout, arg);
}

/****************************************************************************
 * hrt_cancel
 ****************************************************************************/

void hrt_cancel(struct hrt_call *entry)
{
	irqstate_t flags = enter_critical_section();
	sq_rem(&entry->link, &callout_queue);
	entry->callout = NULL;
	entry->period = 0;
	hrt_call_reschedule();
	leave_critical_section(flags);
}

void hrt_call_init(struct hrt_call *entry)
{
	memset(entry, 0, sizeof(*entry));
}

/****************************************************************************
 * hrt_init — configure TIM5 as 1MHz free-running counter
 ****************************************************************************/

void hrt_init(void)
{
	sq_init(&callout_queue);

	/* Enable TIM5 clock via SET register (STM32N6 RIF-safe) */
	putreg32(RCC_APB1ENR1_TIM5EN, STM32_RCC_APB1ENSR1);

	/* Reset timer */
	rCR1 = 0;
	rDIER = 0;
	rSR = 0;

	/* Prescaler: timer_clk / (PSC+1) = 1MHz */
	rPSC = (HRT_TIMER_CLOCK / 1000000) - 1;

	/* Auto-reload: max 32-bit (free-running) */
	rARR = 0xFFFFFFFF;

	/* Force update to load PSC */
	rEGR = 1;

	/* Clear status */
	rSR = 0;

	/* Set initial CC1 compare a short ways out so first ISR fires */
	rCCR1 = 1000;
	rDIER = (1 << 1);  /* CC1IE */

	/* Attach and enable IRQ */
	irq_attach(HRT_TIMER_IRQ, hrt_tim_isr, NULL);
	up_enable_irq(HRT_TIMER_IRQ);

	/* Start timer (upcounting, no other features) */
	rCR1 = 1;  /* CEN */
}

void hrt_store_absolute_time(volatile hrt_abstime *t)
{
	*t = hrt_absolute_time();
}
