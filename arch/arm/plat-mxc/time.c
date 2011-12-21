/*
 *  linux/arch/arm/plat-mxc/time.c
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
 *  Copyright (C) 2006-2007 Pavel Pisa (ppisa@pikron.com)
 *  Copyright (C) 2008 Juergen Beisert (kernel@pengutronix.de)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clk.h>

#include <mach/hardware.h>
#include <asm/mach/time.h>
#include <mach/common.h>

/* defines common for all i.MX */
#define MXC_TCTL		0x00
#define MXC_TCTL_TEN		(1 << 0)
#define MXC_TPRER		0x04

/* MX1, MX21, MX27 */
#define MX1_2_TCTL_CLK_PCLK1	(1 << 1)
#define MX1_2_TCTL_IRQEN	(1 << 4)
#define MX1_2_TCTL_FRR		(1 << 8)
#define MX1_2_TCMP		0x08
#define MX1_2_TCN		0x10
#define MX1_2_TSTAT		0x14

/* MX21, MX27 */
#define MX2_TSTAT_CAPT		(1 << 1)
#define MX2_TSTAT_COMP		(1 << 0)

/* MX31, MX35 */
#define MX3_TCTL_WAITEN		(1 << 3)
#define MX3_TCTL_CLK_IPG	(1 << 6)
#define MX3_TCTL_CLK_PER	(2 << 6)
#define MX3_TCTL_FRR		(1 << 9)
#define MX3_IR			0x0c
#define MX3_TSTAT		0x08
#define MX3_TSTAT_OF1		(1 << 0)
#define MX3_TCN			0x24
#define MX3_TCMP		0x10

#define timer_is_v2()	(!(cpu_is_mx1() || cpu_is_mx2()) || cpu_is_mx25())

/*!
 * GPT register address definitions
 */
#define GPT_BASE_ADDR		(IO_ADDRESS(GPT1_BASE_ADDR))
#define MXC_GPT_GPTCR		(GPT_BASE_ADDR + 0x00)
#define MXC_GPT_GPTPR		(GPT_BASE_ADDR + 0x04)
#define MXC_GPT_GPTSR		(GPT_BASE_ADDR + 0x08)
#define MXC_GPT_GPTIR		(GPT_BASE_ADDR + 0x0C)
#define MXC_GPT_GPTOCR1		(GPT_BASE_ADDR + 0x10)
#define MXC_GPT_GPTOCR2		(GPT_BASE_ADDR + 0x14)
#define MXC_GPT_GPTOCR3		(GPT_BASE_ADDR + 0x18)
#define MXC_GPT_GPTICR1		(GPT_BASE_ADDR + 0x1C)
#define MXC_GPT_GPTICR2		(GPT_BASE_ADDR + 0x20)
#define MXC_GPT_GPTCNT		(GPT_BASE_ADDR + 0x24)

/*!
 * GPT Control register bit definitions
 */
#define GPTCR_FO3			(1 << 31)
#define GPTCR_FO2			(1 << 30)
#define GPTCR_FO1			(1 << 29)

#define GPTCR_SWR			(1 << 15)
#define GPTCR_FRR			(1 << 9)

#define GPTCR_CLKSRC_SHIFT		6
#define GPTCR_CLKSRC_MASK		(7 << GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_NOCLOCK		(0 << GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_HIGHFREQ		(2 << GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_CLKIN		(3 << GPTCR_CLKSRC_SHIFT)
#define GPTCR_CLKSRC_CLK32K		(4 << GPTCR_CLKSRC_SHIFT)

#define GPTCR_STOPEN			(1 << 5)
#define GPTCR_DOZEN			(1 << 4)
#define GPTCR_WAITEN			(1 << 3)
#define GPTCR_DBGEN			(1 << 2)
#define GPTCR_ENMOD			(1 << 1)
#define GPTCR_ENABLE			(1 << 0)

#define	GPTSR_OF1			(1 << 0)
#define	GPTSR_OF2			(1 << 1)
#define	GPTSR_OF3			(1 << 2)
#define	GPTSR_IF1			(1 << 3)
#define	GPTSR_IF2			(1 << 4)
#define	GPTSR_ROV			(1 << 5)

#define	GPTIR_OF1IE			GPTSR_OF1
#define	GPTIR_OF2IE			GPTSR_OF2
#define	GPTIR_OF3IE			GPTSR_OF3
#define	GPTIR_IF1IE			GPTSR_IF1
#define	GPTIR_IF2IE			GPTSR_IF2
#define	GPTIR_ROVIE			GPTSR_ROV


static struct clock_event_device clockevent_mxc;
static enum clock_event_mode clockevent_mode = CLOCK_EVT_MODE_UNUSED;

static void __iomem *timer_base;

static inline void gpt_irq_disable(void)
{
	unsigned int tmp;

	if (timer_is_v2())
		__raw_writel(0, timer_base + MX3_IR);
	else {
		tmp = __raw_readl(timer_base + MXC_TCTL);
		__raw_writel(tmp & ~MX1_2_TCTL_IRQEN, timer_base + MXC_TCTL);
	}
}

static inline void gpt_irq_enable(void)
{
	if (timer_is_v2())
		__raw_writel(1<<0, timer_base + MX3_IR);
	else {
		__raw_writel(__raw_readl(timer_base + MXC_TCTL) | MX1_2_TCTL_IRQEN,
			timer_base + MXC_TCTL);
	}
}

static void gpt_irq_acknowledge(void)
{
	if (cpu_is_mx1())
		__raw_writel(0, timer_base + MX1_2_TSTAT);
	if (cpu_is_mx2())
		__raw_writel(MX2_TSTAT_CAPT | MX2_TSTAT_COMP, timer_base + MX1_2_TSTAT);
	if (timer_is_v2())
		__raw_writel(MX3_TSTAT_OF1, timer_base + MX3_TSTAT);
}

static cycle_t mx1_2_get_cycles(struct clocksource *cs)
{
	return __raw_readl(timer_base + MX1_2_TCN);
}

static cycle_t mx3_get_cycles(struct clocksource *cs)
{
	return __raw_readl(timer_base + MX3_TCN);
}

static struct clocksource clocksource_mxc = {
	.name 		= "mxc_timer1",
	.rating		= 200,
	.read		= mx1_2_get_cycles,
	.mask		= CLOCKSOURCE_MASK(32),
	.shift 		= 20,
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

unsigned long long sched_clock(void)
{
	unsigned long long ret;

	if (!timer_base)
		return 0;

	ret = (unsigned long long)clocksource_mxc.read(&clocksource_mxc);
	ret = (ret * clocksource_mxc.mult_orig) >> clocksource_mxc.shift;
	return ret;
}

static int __init mxc_clocksource_init(struct clk *timer_clk)
{
	unsigned int c = clk_get_rate(timer_clk);

	if (timer_is_v2())
		clocksource_mxc.read = mx3_get_cycles;

	clocksource_mxc.mult = clocksource_hz2mult(c,
					clocksource_mxc.shift);
	clocksource_register(&clocksource_mxc);

	return 0;
}

/* clock event */

static int mx1_2_set_next_event(unsigned long evt,
			      struct clock_event_device *unused)
{
	unsigned long tcmp;

	tcmp = __raw_readl(timer_base + MX1_2_TCN) + evt;

	__raw_writel(tcmp, timer_base + MX1_2_TCMP);

	return (int)(tcmp - __raw_readl(timer_base + MX1_2_TCN)) < 0 ?
				-ETIME : 0;
}

static int mx3_set_next_event(unsigned long evt,
			      struct clock_event_device *unused)
{
	unsigned long tcmp;

	tcmp = __raw_readl(timer_base + MX3_TCN) + evt;

	__raw_writel(tcmp, timer_base + MX3_TCMP);

	return (int)(tcmp - __raw_readl(timer_base + MX3_TCN)) < 0 ?
				-ETIME : 0;
}

#ifdef DEBUG
static const char *clock_event_mode_label[] = {
	[CLOCK_EVT_MODE_PERIODIC] = "CLOCK_EVT_MODE_PERIODIC",
	[CLOCK_EVT_MODE_ONESHOT]  = "CLOCK_EVT_MODE_ONESHOT",
	[CLOCK_EVT_MODE_SHUTDOWN] = "CLOCK_EVT_MODE_SHUTDOWN",
	[CLOCK_EVT_MODE_UNUSED]   = "CLOCK_EVT_MODE_UNUSED"
};
#endif /* DEBUG */

static void mxc_set_mode(enum clock_event_mode mode,
				struct clock_event_device *evt)
{
	unsigned long flags;

	/*
	 * The timer interrupt generation is disabled at least
	 * for enough time to call mxc_set_next_event()
	 */
	local_irq_save(flags);

	/* Disable interrupt in GPT module */
	gpt_irq_disable();

	if (mode != clockevent_mode) {
		/* Set event time into far-far future */
		if (timer_is_v2())
			__raw_writel(__raw_readl(timer_base + MX3_TCN) - 3,
					timer_base + MX3_TCMP);
		else
			__raw_writel(__raw_readl(timer_base + MX1_2_TCN) - 3,
					timer_base + MX1_2_TCMP);

		/* Clear pending interrupt */
		gpt_irq_acknowledge();
	}

#ifdef DEBUG
	printk(KERN_INFO "mxc_set_mode: changing mode from %s to %s\n",
		clock_event_mode_label[clockevent_mode],
		clock_event_mode_label[mode]);
#endif /* DEBUG */

	/* Remember timer mode */
	clockevent_mode = mode;
	local_irq_restore(flags);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		printk(KERN_ERR"mxc_set_mode: Periodic mode is not "
				"supported for i.MX\n");
		break;
	case CLOCK_EVT_MODE_ONESHOT:
	/*
	 * Do not put overhead of interrupt enable/disable into
	 * mxc_set_next_event(), the core has about 4 minutes
	 * to call mxc_set_next_event() or shutdown clock after
	 * mode switching
	 */
		local_irq_save(flags);
		gpt_irq_enable();
		local_irq_restore(flags);
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_RESUME:
		/* Left event sources disabled, no more interrupts appear */
		break;
	}
}

/*
 * IRQ handler for the timer
 */
static irqreturn_t mxc_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = &clockevent_mxc;
	uint32_t tstat;

	if (timer_is_v2())
		tstat = __raw_readl(timer_base + MX3_TSTAT);
	else
		tstat = __raw_readl(timer_base + MX1_2_TSTAT);

	gpt_irq_acknowledge();

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction mxc_timer_irq = {
	.name		= "i.MX Timer Tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler	= mxc_timer_interrupt,
};

static struct clock_event_device clockevent_mxc = {
	.name		= "mxc_timer1",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	.set_mode	= mxc_set_mode,
	.set_next_event	= mx1_2_set_next_event,
	.rating		= 200,
};

static int __init mxc_clockevent_init(struct clk *timer_clk)
{
	unsigned int c = clk_get_rate(timer_clk);

	if (timer_is_v2())
		clockevent_mxc.set_next_event = mx3_set_next_event;

	clockevent_mxc.mult = div_sc(c, NSEC_PER_SEC,
					clockevent_mxc.shift);
	clockevent_mxc.max_delta_ns =
			clockevent_delta2ns(0xfffffffe, &clockevent_mxc);
	clockevent_mxc.min_delta_ns =
			clockevent_delta2ns(0xff, &clockevent_mxc);

	clockevent_mxc.cpumask = cpumask_of(0);

	clockevents_register_device(&clockevent_mxc);

	return 0;
}

void __init mxc_timer_init(struct clk *timer_clk, void __iomem *base, int irq)
{
	uint32_t tctl_val;

	clk_enable(timer_clk);

	if (cpu_is_mx1()) {
#ifdef CONFIG_ARCH_MX1
		timer_base = IO_ADDRESS(TIM1_BASE_ADDR);
		irq = TIM1_INT;
#endif
	} else if (cpu_is_mx2()) {
#ifdef CONFIG_ARCH_MX2
		timer_base = IO_ADDRESS(GPT1_BASE_ADDR);
		irq = MXC_INT_GPT1;
#endif
	} else if (cpu_is_mx3()) {
#ifdef CONFIG_ARCH_MX3
		timer_base = IO_ADDRESS(GPT1_BASE_ADDR);
		irq = MXC_INT_GPT;
#endif
	}
	if (base) {
		timer_base = base;
	}

	/*
	 * Initialise to a known state (all timers off, and timing reset)
	 */

	__raw_writel(0, timer_base + MXC_TCTL);
	__raw_writel(0, timer_base + MXC_TPRER); /* see datasheet note */

	if (timer_is_v2())
		tctl_val = MX3_TCTL_CLK_PER | MX3_TCTL_FRR | MX3_TCTL_WAITEN | MXC_TCTL_TEN;
	else
		tctl_val = MX1_2_TCTL_FRR | MX1_2_TCTL_CLK_PCLK1 | MXC_TCTL_TEN;

	__raw_writel(tctl_val, timer_base + MXC_TCTL);

	/* init and register the timer to the framework */
	mxc_clocksource_init(timer_clk);
	mxc_clockevent_init(timer_clk);

	/* Make irqs happen */
	setup_irq(irq, &mxc_timer_irq);
}


extern unsigned long clk_early_get_timer_rate(void);

static int mxc_gpt_set_next_event(unsigned long cycles,
				  struct clock_event_device *evt)
{
	unsigned long flags, expires, now;
	
	raw_local_irq_save(flags);

	now = __raw_readl(MXC_GPT_GPTCNT);
	expires = now + cycles;
	__raw_writel(expires, MXC_GPT_GPTOCR1);

	raw_local_irq_restore(flags);

	return 0;
}

static void mxc_gpt_set_mode(enum clock_event_mode mode,
			     struct clock_event_device *evt)
{
	u32 reg;

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		panic("MXC GPT: CLOCK_EVT_MODE_PERIODIC not supported\n");
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* enable interrupt */
		reg = __raw_readl(MXC_GPT_GPTIR);
		reg |= GPTIR_OF1IE;
		__raw_writel(reg, MXC_GPT_GPTIR);
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
		/* Disable interrupts */
		reg = __raw_readl(MXC_GPT_GPTIR);
		reg &= ~GPTIR_OF1IE;
		__raw_writel(reg, MXC_GPT_GPTIR);
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static struct clock_event_device gpt_clockevent = {
	.name = "mxc_gpt",
	.features = CLOCK_EVT_FEAT_ONESHOT,
	.rating = 300,
	.shift = 32,
	.set_next_event = mxc_gpt_set_next_event,
	.set_mode = mxc_gpt_set_mode,
};


/*!
 * The clockevents timer interrupt structure.
 */
static struct irqaction timer_irq = {
	.name = "gpt-irq",
	.flags = IRQF_DISABLED | IRQF_TIMER,
	.handler = mxc_timer_interrupt,
};

static cycle_t mxc_gpt_read(void)
{
	return __raw_readl(MXC_GPT_GPTCNT);
}

static struct clocksource gpt_clocksrc = {
	.name = "mxc_gpt",
	.rating = 300,
	.read = mxc_gpt_read,
	.mask = CLOCKSOURCE_MASK(32),
	.shift = 24,
	.flags = CLOCK_SOURCE_IS_CONTINUOUS | CLOCK_SOURCE_VALID_FOR_HRES,
};

/*!
 * This function is used to initialize the GPT as a clocksource and clockevent.
 * It is called by the start_kernel() during system startup.
 */
void __init mxc_init_time(void)
{
	int ret;
	unsigned long rate;
	u32 reg, div;

	/* Reset GPT */
	__raw_writel(GPTCR_SWR, MXC_GPT_GPTCR);
	while ((__raw_readl(MXC_GPT_GPTCR) & GPTCR_SWR) != 0)
		mb();

	/* Normal clk api are not yet initialized, so use early verion */
	rate = clk_early_get_timer_rate();
	if (rate == 0)
		panic("MXC GPT: Can't get timer clock rate\n");

#ifdef CLOCK_TICK_RATE
	div = rate / CLOCK_TICK_RATE;
	WARN_ON((div * CLOCK_TICK_RATE) != rate);
#else				/* Hopefully CLOCK_TICK_RATE will go away soon */
	div = 1;
	while ((rate / div) > 20000000) {
		div++;
	}
#endif
	rate /= div;
	__raw_writel(div - 1, MXC_GPT_GPTPR);

	reg = GPTCR_FRR | GPTCR_CLKSRC_HIGHFREQ | GPTCR_STOPEN |
	    GPTCR_DOZEN | GPTCR_WAITEN | GPTCR_ENMOD | GPTCR_ENABLE;
	__raw_writel(reg, MXC_GPT_GPTCR);

	gpt_clocksrc.mult = clocksource_hz2mult(rate, gpt_clocksrc.shift);
	ret = clocksource_register(&gpt_clocksrc);
	if (ret < 0) {
		goto err;
	}

	gpt_clockevent.mult = div_sc(rate, NSEC_PER_SEC, gpt_clockevent.shift);
	gpt_clockevent.max_delta_ns = clockevent_delta2ns(-1, &gpt_clockevent);
	gpt_clockevent.min_delta_ns = clockevent_delta2ns(32, &gpt_clockevent);

	gpt_clockevent.cpumask = &cpumask_of_cpu(0);
	clockevents_register_device(&gpt_clockevent);

	ret = setup_irq(MXC_INT_GPT, &timer_irq);
	if (ret < 0) {
		goto err;
	}

	pr_info("MXC GPT timer initialized, rate = %lu\n", rate);
	return;
      err:
	panic("Unable to initialize timer\n");
}

struct sys_timer mxc_timer = {
	.init = mxc_init_time,
};
