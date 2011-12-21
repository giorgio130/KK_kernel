/*
 * Copyright 2008 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2009 Amazon Technologies, Inc. All Rights Reserved.
 * Author: Manish Lachwani (lachwani@lab126.com)
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
 * Support for IDLE and Suspend modes on the MX35 Luigi.
 *
 * In the IDLE mode, we try to cut down the emi_clk that will then
 * reduce the SDRAM current utilization. emi_clk drives several clocks
 * and we need to check the refcnt of those clocks before cutting emi_clk.
 *
 * Couple of places where emi_clk is not gated - playing audio and when USB is
 * active. However, USB autosuspend changes all of this.
 *
 * In suspend mode, we use suspend to RAM or state retention mode of the MX35
 *
 * peri_pll_clk is the parent of usb_clk, sdhc_clk and uart_clk. Gate peri_pll_clk 
 * by gating sdhc_clk and uart_clk. usb_clk cannot be gated. Hence, check if the 
 * host controller is in use or not. this is done by checking the WAN status.
 */

#include <linux/clk.h>
#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
#include <linux/pmic_external.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <net/mwan.h>
#include <asm-generic/cputime.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/proc-fns.h>
#include <asm/system.h>
#include <asm/arch-mxc/clock.h>
#include <asm/hardware.h>
#include <asm/delay.h>
#include <asm/arch-mxc/pmic_power.h>
#include "crm_regs.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>

/*!
 * @defgroup MSL_MX35 i.MX35 Machine Specific Layer (MSL)
 */

/*!
 * @file mach-mx35/system.c
 * @brief This file contains idle and reset functions.
 *
 * @ingroup MSL_MX35
 */

static int clks_initialized = 0;
static struct clk *sdma_clk, *ipu_clk, *usb_clk, *emi_clk, *nfc_clk, *usb_ahb_clk;
static struct clk *ata_clk, *esai_clk, *wdog_clk, *peri_pll_clk, *fec_clk;
static struct clk *spba_clk, *uart_clk, *sdhc_clk, *sdhc1_clk, *sdhc2_clk, *cspi_clk;
static struct clk *i2c1_clk, *i2c2_clk;

static int emi_zero_count = 0, peri_pll_zero = 0;

/*!
* MX35 low-power mode
*/
enum mx35_low_pwr_mode {
	MX35_RUN_MODE,
	MX35_WAIT_MODE,
	MX35_DOZE_MODE,
	MX35_STOP_MODE
};

extern int mxc_jtag_enabled;
extern int kernel_oops_counter;

/* Idle clock masks for the CGR */

/* CSPI1, EMI, ESDHC1, ESDHC2 and ESDHC3 */
#define MX35_CGR0_MASK		0x3c0c0c00

/* GPT, I2C1 and I2C2 */
#define MX35_CGR1_MASK		0x00003F00

#define MX35_CGR1_MASK_I2C	0x00000300

#ifdef CONFIG_FEC

/* CGR1 MASK when Fast Ethernet Controller is enabled */

#define MX35_CGR1_MASK_FEC	0x00003F03

#define MX35_CGR1_MASK_I2C_FEC	0x00000303

#endif

/* IPU - bits 18-19 in CGR1 */
#define MX35_CGR1_IPU_MASK	0xc0000

/* SDMA, SPBA, UART1, USBOTG, MAX, WDOG */
#define MX35_CGR2_MASK		0x0fc303F0

/* SDMA, SPBA, UART1, MAX, WDOG */
#define MX35_CGR2_USBOTG_MASK	0x0f0303F0

/* All gated except the ones marked as reserved and IIM */
#define MX35_CGR3_MASK		0xffffffc0

/* Prototype */
static int check_i2c_clk(void);

/*!
 * This function is used to set cpu low power mode before WFI instruction
 *
 * @param  mode         indicates different kinds of power modes
 */
void mxc_cpu_lp_set(enum mxc_cpu_pwr_mode mode)
{
	unsigned int lpm;
	unsigned long reg;

	/*read CCMR value */
	reg = __raw_readl(MXC_CCM_CCMR);

	switch (mode) {
	case WAIT_UNCLOCKED_POWER_OFF:
		lpm = MX35_DOZE_MODE;
		reg |= MXC_CCM_CCMR_WBEN;
		break;

	case STOP_POWER_ON:
	case STOP_POWER_OFF:
		lpm = MX35_STOP_MODE;
		/* Enabled Standby */
		reg |= MXC_CCM_CCMR_VSTBY | MXC_CCM_CCMR_WBEN;
		break;

	case WAIT_CLOCKED:
	case WAIT_UNCLOCKED:
	default:
		/* Wait is the default mode used when idle. */
		lpm = MX35_WAIT_MODE;
		break;
	}

	/* program LPM bit */
	reg = (reg & (~MXC_CCM_CCMR_LPM_MASK)) | lpm << MXC_CCM_CCMR_LPM_OFFSET;
	/* program Interrupt holdoff bit */
	reg = reg | MXC_CCM_CCMR_WFI;
	/* TBD: PMIC has put the voltage back to Normal if the voltage ready */
	/* counter finished */
	reg = reg | MXC_CCM_CCMR_STBY_EXIT_SRC;

	__raw_writel(reg, MXC_CCM_CCMR);
}

EXPORT_SYMBOL(mxc_cpu_lp_set);

/* Is SD doing any DMA? */
//extern int sd_turn_of_dma;

static inline cputime64_t get_cpu_idle_time(unsigned int cpu)
{
	cputime64_t idle_time;
	cputime64_t cur_jiffies;
	cputime64_t busy_time;

	cur_jiffies = jiffies64_to_cputime64(get_jiffies_64());
	busy_time = cputime64_add(kstat_cpu(cpu).cpustat.user,
			kstat_cpu(cpu).cpustat.system);

	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.irq);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.softirq);
	busy_time = cputime64_add(busy_time, kstat_cpu(cpu).cpustat.steal);

	busy_time = cputime64_add(busy_time,
			kstat_cpu(cpu).cpustat.nice);
	idle_time = cputime64_sub(cur_jiffies, busy_time);
	return jiffies_to_usecs(idle_time);
}

/*
 * sysfs interface to CPU idle counts
 */
static ssize_t
sysfs_show_idle_count(struct sys_device *dev, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "sdma_clk usecount: %d\n", clk_get_usecount(sdma_clk));
	curr += sprintf(curr, "usb_ahb_clk usecount: %d\n", clk_get_usecount(usb_ahb_clk));
	curr += sprintf(curr, "emi_clk_gating_count: %d\n", emi_zero_count);
	curr += sprintf(curr, "ipu_clk usecount: %d\n", clk_get_usecount(ipu_clk));
//	curr += sprintf(curr, "Internal SD DMA: %d\n", sd_turn_of_dma);
	curr += sprintf(curr, "peri_pll_zero_count: %d\n", peri_pll_zero);
	curr += sprintf(curr, "emi_clk: %d\n", clk_get_usecount(emi_clk));
	curr += sprintf(curr, "idle_time: %llu us\n", get_cpu_idle_time(0));
	curr += sprintf(curr, "\n");
	return curr - buf;
}

/*
 * Sysfs setup bits
 */
static SYSDEV_ATTR(idle_count, 0600, sysfs_show_idle_count, NULL);

static struct sysdev_class cpu_idle_sysclass = {
	.name = "cpu_idle_count",
};

static struct sys_device device_cpu_idle = {
	.id	= 0,
	.cls	= &cpu_idle_sysclass,
};

static int __init init_cpu_idle_sysfs(void)
{
	int error = sysdev_class_register(&cpu_idle_sysclass);

	if (!error)
		error = sysdev_register(&device_cpu_idle);

	if (!error)
		error = sysdev_create_file(
				&device_cpu_idle, &attr_idle_count);
	return error;
}

device_initcall(init_cpu_idle_sysfs);

/* Flag to check if USB HC1 is gated or not */
int usb_hc1_gated = 0;
EXPORT_SYMBOL(usb_hc1_gated);

/* Flag to check if USB OTG is gated or not */
int usb_otg_gated_off = 0;
EXPORT_SYMBOL(usb_otg_gated_off);

static void disable_clks_for_good(void)
{
	clk_disable(ata_clk);
	clk_disable(esai_clk);
}

static void pmcr2_disable_oscaudio(void)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_PMCR2) | MXC_CCM_PMCR2_OSC_AUDIO_DOWN;
	__raw_writel(reg, MXC_CCM_PMCR2);

	/* Turn off DPTC */
	reg = __raw_readl(MXC_CCM_PMCR0) & ~(0x1);
	__raw_writel(reg, MXC_CCM_PMCR0);
}

static void enable_cgr_clks(void)
{
	unsigned long reg = 0xffffffff;

	__raw_writel(reg, MXC_CCM_CGR0);
	__raw_writel(reg, MXC_CCM_CGR1);
	__raw_writel(reg, MXC_CCM_CGR2);
	__raw_writel(reg, MXC_CCM_CGR3);
	
}

static int eink_doze_counter(void);
static int eink_doze_disable_cnt = 0;

/*!
 * Disable CGR clocks during IDLE
 */
static inline void disable_cgr_clks(void)
{
	unsigned long reg;

	reg = MX35_CGR0_MASK;

	__raw_writel(reg, MXC_CCM_CGR0);

	if (check_i2c_clk()) {
#ifdef CONFIG_FEC
		reg = MX35_CGR1_MASK_FEC;
#else
		reg = MX35_CGR1_MASK;
#endif
	} else {
#ifdef CONFIG_FEC
		reg = MX35_CGR1_MASK_FEC;
#else
		reg = MX35_CGR1_MASK;
#endif
	}
	if (eink_doze_disable_cnt)
		reg &= ~(MX35_CGR1_IPU_MASK);

	__raw_writel(reg, MXC_CCM_CGR1);

	if (usb_ahb_clk->usecount)
		reg = MX35_CGR2_MASK;
	else
		reg = MX35_CGR2_USBOTG_MASK;

	if (!uart_clk->usecount)
		reg &= ~(0x30000);

	__raw_writel(reg, MXC_CCM_CGR2);

	__raw_writel(MX35_CGR3_MASK, MXC_CCM_CGR3);
	
}

/* Enable the watchdog clock */
void watchdog_enable_clock(void)
{
	unsigned long reg = __raw_readl(MXC_CCM_CGR2);

	/* Enable the watchdog clock - bits 24, 25 */
	reg |= 0x03000000;
	__raw_writel(reg, MXC_CCM_CGR2);
}
EXPORT_SYMBOL(watchdog_enable_clock);

/*!
 * cko_clk needed during audio and video
 */
void noinline cko1_enable(void)
{
        u32 reg = __raw_readl(MXC_CCM_COSR) | MXC_CCM_COSR_CLKOEN;
        __raw_writel(reg, MXC_CCM_COSR);
}

EXPORT_SYMBOL(cko1_enable);

void noinline cko1_disable(void)
{
	u32 reg = __raw_readl(MXC_CCM_COSR) & ~MXC_CCM_COSR_CLKOEN;
	/* Turn off ASRC AUDIO_EN */
	reg &= ~(0x01000000);
	__raw_writel(reg, MXC_CCM_COSR);
}

EXPORT_SYMBOL(cko1_disable);

static void initialize_clocks(void)
{
	sdma_clk = clk_get(NULL, "sdma_ahb_clk");
	usb_ahb_clk = clk_get(NULL, "usb_ahb_clk");

	emi_clk = clk_get(NULL, "emi_clk");
	ipu_clk = clk_get(NULL, "ipu_clk");
	nfc_clk = clk_get(NULL, "nfc_clk");
	usb_clk = clk_get(NULL, "usb_clk");
	ata_clk = clk_get(NULL, "ata_clk");
	esai_clk = clk_get(NULL, "esai_clk");
	wdog_clk = clk_get(NULL, "wdog_clk");
	peri_pll_clk = clk_get(NULL, "peri_pll");
	fec_clk = clk_get(NULL, "fec_clk");
	spba_clk = clk_get(NULL, "spba_clk");
	uart_clk = clk_get(NULL, "uart_clk.0");

	sdhc_clk = clk_get(NULL, "sdhc_clk.0");
	sdhc1_clk = clk_get(NULL, "sdhc_clk.1");
	sdhc2_clk = clk_get(NULL, "sdhc_clk.2");

	cspi_clk = clk_get(NULL, "cspi_clk.0");
	i2c1_clk = clk_get(NULL, "i2c_clk.0");
	i2c2_clk = clk_get(NULL, "i2c_clk.1");
}

static int check_sdhc_clk(void)
{
	return (sdhc_clk->usecount |
		sdhc1_clk->usecount| 
		sdhc2_clk->usecount);
}

static int check_i2c_clk(void)
{
	return (i2c1_clk->usecount |
		i2c2_clk->usecount);
}

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

static void reduce_nfc_clk_speed(void)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_PDR4);
	reg |= (16 << MXC_CCM_PDR4_NFC_PODF_OFFSET);
	__raw_writel(reg, MXC_CCM_PDR4);
}

static void noinline peri_pll_enable(void)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CCMR);
	reg |= MXC_CCM_CCMR_UPE;
	__raw_writel(reg, MXC_CCM_CCMR);

	udelay(80);
}

static void noinline peri_pll_disable(void)
{
	u32 reg;

	reg = __raw_readl(MXC_CCM_CCMR);
	reg &= ~MXC_CCM_CCMR_UPE;
	__raw_writel(reg, MXC_CCM_CCMR);
}

/*
 * Don't deep idle if audio is playing
 */
extern int mx35_luigi_audio_playing_flag; 
static int doze_lpm_flag = 0;

void eink_doze_enable(int enable)
{
	eink_doze_disable_cnt = enable;
}

EXPORT_SYMBOL(eink_doze_enable);

static int eink_doze_counter(void)
{
	return eink_doze_disable_cnt;
}

void doze_disable(void)
{
	doze_lpm_flag = 1;
}
EXPORT_SYMBOL(doze_disable);

void doze_enable(void)
{
	doze_lpm_flag = 0;
}
EXPORT_SYMBOL(doze_enable);

/*!
 * Disable the SPBA clock
 */
static void mxc_spba_disable(void)
{
	unsigned long reg;

	reg = __raw_readl(MXC_CCM_CGR2);
	reg &= ~0xC0;
	__raw_writel(reg, MXC_CCM_CGR2);
}

/*!
 * Enable the SPBA clock
 */
static void mxc_spba_enable(void)
{
	unsigned long reg;

	reg = __raw_readl(MXC_CCM_CGR2);
	reg |= 0xC0;
	__raw_writel(reg, MXC_CCM_CGR2);
}
 
/*!
 * This function puts the CPU into idle mode. It is called by default_idle()
 * in process.c file.
 */
void arch_idle(void)
{
	int emi_gated_off = 0, peri_pll_gated = 0;
	int usb_hc1_gated_off = 0;
	
	if (mxc_jtag_enabled)
		return;

	/*
	 * This should do all the clock switching
	 * and wait for interrupt tricks.
	 */
	if (!mx35_luigi_audio_playing_flag && !doze_lpm_flag) {
		if (clks_initialized == 0) {
			clks_initialized = 1;
			initialize_clocks();
			disable_clks_for_good();
			pmcr2_disable_oscaudio();
			reduce_nfc_clk_speed();
		}

		local_fiq_disable();

		disable_cgr_clks();
		if (eink_doze_counter())
			cko1_disable();
		clk_disable(ipu_clk);

		if (check_wan_status())
			usb_hc1_gated_off = 1;
		else {
			if (usb_hc1_gated)
				usb_hc1_gated_off = 1;
		}

		/*
		 * Check if emi_clk can be gated
		 */
		if ((!sdma_clk->usecount)
			&& (ipu_clk->usecount <= 1)
			&& (usb_hc1_gated_off == 1)
			&& (usb_otg_gated_off == 1)
			&& (nfc_clk->usecount == 0)
//			&& (sd_turn_of_dma == 0)
			&& (fec_clk->usecount == 0) ) {
				emi_gated_off = 1;
				clk_disable(emi_clk);
		}

		/*
		 * Check if the peri_pll_clk can be gated
		 */
		if (!check_sdhc_clk()
			&& (emi_gated_off == 1)
			&& (!usb_ahb_clk->usecount)
			&& (!uart_clk->usecount)) {
				peri_pll_disable();
				peri_pll_gated = 1;
		}
	
		mxc_cpu_lp_set(WAIT_UNCLOCKED);
		cpu_do_idle();

		if (peri_pll_gated == 1)
			peri_pll_enable();

		if (emi_gated_off == 1) {
			clk_enable(emi_clk);
		}

		clk_enable(ipu_clk);
		cko1_enable();
		enable_cgr_clks();

		local_fiq_enable();
	}
	else {
		mxc_cpu_lp_set(WAIT_UNCLOCKED);
		cpu_do_idle();
		enable_cgr_clks();
	}
}

extern int cpufreq_suspended;
extern int set_cpu_freq(int wp);
extern void mx35_power_off(void);

/*
 * This function resets the system. It is called by machine_restart().
 *
 * @param  mode         indicates different kinds of resets
 */
void arch_reset(char mode)
{
	unsigned long reg;
	unsigned long reg_1 = 0xffffffff;

	doze_disable();

	reg = __raw_readl(MXC_CCM_CGR0);
	reg |= (MXC_CCM_CGR0_ESDHC1_MASK | MXC_CCM_CGR0_ESDHC2_MASK |
		MXC_CCM_CGR0_ESDHC3_MASK | MXC_CCM_CGR0_CSPI1_MASK |
		MXC_CCM_CGR0_CSPI2_MASK);
	__raw_writel(reg, MXC_CCM_CGR0);

	reg = __raw_readl(MXC_CCM_CGR3);
	reg |= MXC_CCM_CGR3_IIM_MASK;
	__raw_writel(reg, MXC_CCM_CGR3);

	printk(KERN_INFO "MX35 arch_reset\n");

	if (in_interrupt() || kernel_oops_counter) {
		mxc_wd_reset();
		return;
	}

	local_irq_enable();
	/* Clear out bit #1 in MEMA */
	pmic_write_reg(REG_MEM_A, (0 << 1), (1 << 1));

	/* Turn on bit #1 */
	pmic_write_reg(REG_MEM_A, (1 << 1), (1 << 1));

	/* Turn off VSD */
	pmic_write_reg(REG_MODE_1, (0 << 18), (1 << 18));

	cpufreq_suspended = 1;
	set_cpu_freq(512000000);

	mdelay(100);

	peri_pll_enable();

        __raw_writel(reg_1, MXC_CCM_CGR0);
        __raw_writel(reg_1, MXC_CCM_CGR1);
        __raw_writel(reg_1, MXC_CCM_CGR2);
        __raw_writel(reg_1, MXC_CCM_CGR3);

	mdelay(100);

	/* Memory Hold Mode */
	mx35_power_off();
}
EXPORT_SYMBOL(arch_reset);

#ifdef CONFIG_MACH_LUIGI_LAB126
EXPORT_SYMBOL(sys_open);
EXPORT_SYMBOL(sys_write);
EXPORT_SYMBOL(sys_read);

#include <llog.h>
unsigned long einkfb_logging = _LLOG_LEVEL_DEFAULT;
EXPORT_SYMBOL(einkfb_logging);
#endif
