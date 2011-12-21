/*
 *  Copyright 2008 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/* 
 * Implement low power state retention mode for MX35 Luigi
 *
 * State Retention mode is the STOP mode of the MX35. Here is how the processor
 * transitions to STOP mode.
 * 
 * The CLKMOD is set to 11. This should be sufficient to gate the mcu_pll clock.
 * The 24MHz oscillator disabled in STOP mode. Once OSC24 is disabled, mcu_pll
 * can be gated. The clock source will shift to the 32KHz coming from Atlas. This
 * ckil_clk is alternate source.
 * Once mcu_pll is gated, VPLL can be put in low power
 *
 * Copyright (C) 2009 Amazon Technologies, Inc. All Rights Reserved.
 * Manish Lachwani (lachwani@lab126.com)
 */
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#if defined(CONFIG_CPU_FREQ)
#include <linux/cpufreq.h>
#endif
#include <net/mwan.h>

#include <linux/pmic_external.h>
#include "crm_regs.h"
#include "iomux.h"

#include <asm/arch/hardware.h>
#include <asm/io.h>

extern int get_pmic_revision(void);

extern void gpio_papyrus_configure(int enable);
extern void gpio_pm_set_value(int val);

extern void cko1_enable(void);
extern void cko1_disable(void);

static struct cpu_wp *cpu_wp_tbl;
static struct clk *cpu_clk;
extern struct cpu_wp *get_cpu_wp(int *wp);

extern void gpio_iomux_32khz(int);

extern void doze_disable(void);
extern void doze_enable(void);

/*!
 * Turn off accessory on suspend and enable on resume
 */
extern void mx35_accessory_enable(int enable);


/*!
 * @defgroup MSL_MX35 i.MX35 Machine Specific Layer (MSL)
 */

#if defined(CONFIG_CPU_FREQ)

#define MAX_CPU_FREQUENCY	512000000
#define MIN_CPU_FREQUENCY	256000000

extern int cpufreq_suspended;
extern int set_cpu_freq(int wp);

#endif

/*! Workaround the Broken mode switch on Atlas parts */
static int mc13892_broken_mode_switch = 1;

/*!
 * Defines for SW2 - CPU
 */
#define SW2HI_LSH		23	/* Bit 23 in Register 25 */
#define SW2NORMAL_LSH		0	/* Normal mode voltage for SW2 */
#define SW2STANDBY_LSH		10	/* Standby mode voltage for SW2 */
#define SW2_NORMAL_MASK		0x3ff	/* SW2 Voltage Mask */
#define SW2_STANDBY_MASK	0x1f
#define SW2HI_VOLTAGE		0x35A	/* 1.25V with SW2HI bit 0 */
#define SW2LOW_VOLTAGE		0x10	/* 1.0V with SW2HI bit 0 */

#define MX35_CPU_MAX_VOLTAGE	0x16B	/* 1.47V */
#define SW2_VOLTAGE_MASK	0x3ff	/* Bits 0-9 mask for register 25 */

/*!
 * Charger detect
 */
#define CHGDETS			0x40	/* Sense bit for CHGDET */

/*!
 * Track down device suspends
 */
atomic_t mxc_device_suspended = ATOMIC_INIT(0);

/*!
 * Generic GPIO disable on system suspend
 */
static void mx35_suspend_gpio(void)
{
	gpio_papyrus_configure(0);

	/* Turn on the 32KHz clock */
	gpio_iomux_32khz(1);

	/* Pull the test point low */
	gpio_pm_set_value(0);
}

/*!
 * Generic GPIO enable on system resume
 */
static void mx35_resume_gpio(void)
{
	/* Pull the test point high */
	gpio_pm_set_value(1);

	gpio_papyrus_configure(1);

	/* Reset the 32Khz GPIO */
	gpio_iomux_32khz(0);
}

/*!
 * Check if Atlas Rev 3.1
 */
static int check_atlas_rev31(void)
{
	int reg = 0;

	pmic_read_reg(REG_IDENTIFICATION, &reg, 0xffffffff);

	if (reg & 0x1)
		return 1;
	else
		return 0;
}

/*!
 * @file mach-mx35/pm.c
 * @brief This file contains suspend operations
 *
 * @ingroup MSL_MX35
 */
static int mx35_suspend_enter(suspend_state_t state)
{
	unsigned int ccmr = __raw_readl(MXC_CCM_CCMR); /* Known good value */
	unsigned int reg;
	unsigned int pmcr2 = __raw_readl(MXC_CCM_PMCR2);

	local_irq_disable();
	__raw_writel(MXC_INT_GPT, AVIC_INTDISNUM);
	local_fiq_disable();

	/* Increment the suspended counter */
	atomic_set(&mxc_device_suspended, 1);
	
	mx35_suspend_gpio();

	/*
	 * Turn off the CAN modules 0 and 1. This is in the CGR0, bits 4, 5, 6, 7
	 * The CAN bus is driven from the 24MHz Audio oscillator. Since the Luigi
	 * V3 boards have the audio oscillator removed, the CAN bus needs to be gated
	 * first
	 */
	reg = __raw_readl(MXC_CCM_CGR0);
	reg &= 0xfffffC3f;
	__raw_writel(reg, MXC_CCM_CGR0);

	pmcr2 |= 0x10000; /* Set the OSC24MDOWN bit to 1 */
	pmcr2 |= 0x20000; /* Set the OSC_AUDIO_DOWN to 0 */
	pmcr2 |= 0xff800000;	/* Set the OSD_RDY_CNT */
	__raw_writel(pmcr2, MXC_CCM_PMCR2);

	reg = ccmr & ~(MXC_CCM_CCMR_UPE);
	reg |= (0xF << MXC_CCM_CCMR_VOL_RDY_CNT_OFFSET);
	__raw_writel(reg, MXC_CCM_CCMR); /* Gate the peri_pll_clk */

	switch (state) {
	case PM_SUSPEND_MEM:
		mxc_cpu_lp_set(STOP_POWER_OFF);
		break;
	case PM_SUSPEND_STANDBY:
		mxc_cpu_lp_set(STOP_POWER_ON);
		break;
	default:
		return -EINVAL;
	}

	cko1_disable();

	/* Executing CP15 (Wait-for-Interrupt) Instruction */
	cpu_do_idle();

	/* kernel resumes and enable peri_pll */
	__raw_writel(ccmr | MXC_CCM_CCMR_UPE, MXC_CCM_CCMR);
	udelay(1000);

	cko1_enable();

	mx35_resume_gpio();

	local_fiq_enable();
	__raw_writel(MXC_INT_GPT, AVIC_INTENNUM);
	local_irq_enable();

	return 0;
}
/* Is there a charger connected? */
static int charger_connected(void)
{
	int sense_0 = 0;
	int ret = 0; /* Default: no charger */
	
	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);
	if (sense_0 & CHGDETS)
		ret = 1;

	return ret;
}

/* Read the SW2 HI bit */
static int read_sw2hi_bit(void)
{
	int sw2hi = 0;

	pmic_read_reg(REG_SW_1, &sw2hi, 0xffffff);
	if (sw2hi & 0x800000)
		return 1;
	else
		return 0;
}

void mx35_check_sw2hi_bit(void)
{
	/* SW HI bit switching does not occur on Atlas parts < 3.1 */
	if (!check_atlas_rev31())
		return;

	if (!read_sw2hi_bit())
		pmic_write_reg(REG_SW_1, 1 << SW2HI_LSH, 1 << SW2HI_LSH);
}
EXPORT_SYMBOL(mx35_check_sw2hi_bit);

/*!
 * Check the current status of the WAN. This is needed for the
 * peri_pll_clk gating logic. peri_pll_clk is the parent of the
 * usb_clk.
 */
static int check_wan_status(void)
{
	if ( (wan_get_power_status() == WAN_OFF) ||
		(wan_get_power_status() == WAN_INVALID)) {
			return 1;
	}
	else
		return 0;
}

/*
 * Called after processes are frozen, but before we shut down devices.
 */
static int mx35_suspend_prepare(suspend_state_t state)
{
	/* Disable doze mode */
	doze_disable();

	cpufreq_suspended = 1;
	
	return 0;
}

/*
 * Called after devices are re-setup, but before processes are thawed.
 */
static void mx35_suspend_finish(void)
{
	/* Re-enable doze mode */
	doze_enable();

	cpufreq_suspended = 0;
}

static int mx35_pm_valid(suspend_state_t state)
{
	return (state > PM_SUSPEND_ON && state <= PM_SUSPEND_MAX);
}

struct platform_suspend_ops mx35_suspend_ops = {
	.valid = mx35_pm_valid,
	.begin = mx35_suspend_prepare,
	.enter = mx35_suspend_enter,
	.end = mx35_suspend_finish,
};

static int __init mx35_pm_init(void)
{
	int cpu_wp_nr;

	pr_info("Static Power Management for Freescale i.MX35\n");
	suspend_set_ops(&mx35_suspend_ops);

	cpu_wp_tbl = get_cpu_wp(&cpu_wp_nr);

	cpu_clk = clk_get(NULL, "cpu_clk");
	if (IS_ERR(cpu_clk)) {
		printk(KERN_DEBUG "%s: failed to get cpu_clk\n", __func__);
		return PTR_ERR(cpu_clk);
	}

	return 0;
}

late_initcall(mx35_pm_init);
