/****************************************************************************
 * STM32N6 High Resolution Timer — TIM5 hardware implementation
 *
 * Ported from platforms/nuttx/src/px4/stm/stm32_common/hrt/hrt.c
 * Adapted for STM32N6 TIM5 (32-bit, APB1).
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
#define rEGR   (*(volatile uint32_t *)(HRT_TIMER_BASE + 0x0014))

/* Minimum/maximum compare intervals */
#define HRT_INTERVAL_MIN   50        /* 50 us */
#define HRT_INTERVAL_MAX   50000     /* 50 ms */

/* Callout queue */
static struct sq_queue_s callout_queue;

/****************************************************************************
 * hrt_absolute_time — read TIM5 counter (microseconds since boot)
 ****************************************************************************/

hrt_abstime hrt_absolute_time(void)
{
	return (hrt_abstime)rCNT;
}

/****************************************************************************
 * hrt_call_reschedule — set CC1 for next deadline (reference impl)
 ****************************************************************************/

static void hrt_call_reschedule(void)
{
	hrt_abstime now = hrt_absolute_time();
	struct hrt_call *next = (struct hrt_call *)sq_peek(&callout_queue);
	hrt_abstime deadline = now + HRT_INTERVAL_MAX;

	if (next != NULL) {
		if (next->deadline <= (now + HRT_INTERVAL_MIN)) {
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
 * hrt_call_enter — insert entry sorted into callout queue
 ****************************************************************************/

static void hrt_call_enter(struct hrt_call *entry)
{
	struct hrt_call *call, *next;

	call = (struct hrt_call *)sq_peek(&callout_queue);

	if ((call == NULL) || (entry->deadline < call->deadline)) {
		sq_addfirst(&entry->link, &callout_queue);
		/* we changed the next deadline, reschedule the timer event */
		hrt_call_reschedule();

	} else {
		do {
			next = (struct hrt_call *)sq_next(&call->link);

			if ((next == NULL) || (entry->deadline < next->deadline)) {
				sq_addafter(&call->link, &entry->link, &callout_queue);
				break;
			}
		} while ((call = next) != NULL);
	}
}

/****************************************************************************
 * hrt_call_invoke — dispatch expired callouts (reference impl)
 ****************************************************************************/

static void hrt_call_invoke(void)
{
	struct hrt_call *call;
	hrt_abstime deadline;

	while (true) {
		hrt_abstime now = hrt_absolute_time();

		call = (struct hrt_call *)sq_peek(&callout_queue);

		if (call == NULL) {
			break;
		}

		if (call->deadline > now) {
			break;
		}

		sq_rem(&call->link, &callout_queue);

		/* save the intended deadline for periodic calls */
		deadline = call->deadline;

		/* zero the deadline, as the call has occurred */
		call->deadline = 0;

		/* invoke the callout (if there is one) */
		if (call->callout) {
			call->callout(call->arg);
		}

		/* if the callout has a non-zero period, it has to be re-entered */
		if (call->period != 0) {
			/* re-check call->deadline to allow for
			 * callouts to re-schedule themselves
			 * using hrt_call_delay() */
			if (call->deadline <= now) {
				call->deadline = deadline + call->period;
			}

			hrt_call_enter(call);
		}
	}
}

/****************************************************************************
 * TIM5 CC1 interrupt handler
 ****************************************************************************/

static int hrt_tim_isr(int irq, void *context, void *arg)
{
	(void)irq;
	(void)context;
	(void)arg;

	uint32_t sr = rSR;

	if (sr & (1 << 1)) {  /* CC1IF */
		rSR = ~(1 << 1);  /* Clear CC1IF */

		/* run any callouts that have met their deadline */
		hrt_call_invoke();

		/* and schedule the next interrupt */
		hrt_call_reschedule();
	}

	return 0;
}

/****************************************************************************
 * hrt_call_internal — common implementation for call_after/call_at/call_every
 ****************************************************************************/

static void
hrt_call_internal(struct hrt_call *entry, hrt_abstime deadline,
		  hrt_abstime interval, hrt_callout callout, void *arg)
{
	irqstate_t flags = enter_critical_section();

	/* if the entry is currently queued, remove it */
	if (entry->deadline != 0) {
		sq_rem(&entry->link, &callout_queue);
	}

	entry->deadline = deadline;
	entry->period = interval;
	entry->callout = callout;
	entry->arg = arg;

	hrt_call_enter(entry);

	leave_critical_section(flags);
}

/****************************************************************************
 * Public API — matches reference stm32_common/hrt/hrt.c
 ****************************************************************************/

void hrt_call_after(struct hrt_call *entry, hrt_abstime delay,
		    hrt_callout callout, void *arg)
{
	hrt_call_internal(entry,
			  hrt_absolute_time() + delay,
			  0,
			  callout,
			  arg);
}

void hrt_call_at(struct hrt_call *entry, hrt_abstime calltime,
		 hrt_callout callout, void *arg)
{
	hrt_call_internal(entry, calltime, 0, callout, arg);
}

void hrt_call_every(struct hrt_call *entry, hrt_abstime delay,
		    hrt_abstime interval, hrt_callout callout, void *arg)
{
	hrt_call_internal(entry,
			  hrt_absolute_time() + delay,
			  interval,
			  callout,
			  arg);
}

bool hrt_called(struct hrt_call *entry)
{
	return (entry->deadline == 0);
}

void hrt_cancel(struct hrt_call *entry)
{
	irqstate_t flags = enter_critical_section();

	sq_rem(&entry->link, &callout_queue);
	entry->deadline = 0;

	/* if this is a periodic call being removed by the callout, prevent it
	 * from being re-entered when the callout returns. */
	entry->period = 0;

	leave_critical_section(flags);
}

void hrt_call_delay(struct hrt_call *entry, hrt_abstime delay)
{
	entry->deadline = hrt_absolute_time() + delay;
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

	/* Set initial CC1 compare a little ways out */
	rCCR1 = 1000;
	rDIER = (1 << 1);  /* CC1IE */

	/* Attach and enable IRQ */
	irq_attach(HRT_TIMER_IRQ, hrt_tim_isr, NULL);
	up_enable_irq(HRT_TIMER_IRQ);

	/* Start timer */
	rCR1 = 1;  /* CEN */
}

void hrt_store_absolute_time(volatile hrt_abstime *t)
{
	*t = hrt_absolute_time();
}
