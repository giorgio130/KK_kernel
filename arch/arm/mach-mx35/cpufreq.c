/*
 * cpufreq.c -- MX35 CPUfreq driver.
 * MX35 cpufreq driver
 *
 * Copyright 2009 Lab126, Inc.  All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 *
 * Support for CPUFreq on the Luigi. It supports two operating points:
 * 256MHz and 512MHz. Along with the CPU Frequency scaling, the voltage
 * is also adjusted to be 1.275V (256MHz) or 1.4V (512MHz).
 * 
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/cacheflush.h>

#include "crm_regs.h"

static struct clk *cpu_clk;
static struct regulator *core_regulator;

char *core_reg_id = "SW2";

int cpufreq_suspended = 0;
EXPORT_SYMBOL(cpufreq_suspended);

#define MAX_CPU_FREQUENCY	512000000	/* MAX speed of the CPU */

DEFINE_SPINLOCK(cpufreq_lock);

#define MXC_PERCLK_DIVIDER	0x1000
#define MX35_CPU_MIN_FREQ	256000
#define MX35_CPU_MAX_FREQ	512000

/*
 * Use one PMIC call to set the SW2 and SW2_DVS voltage levels. We
 * don't want to use the regulator_* functions as it issues multiple PMIC call
 * and causes unnecessary SPI traffic.
 *
 * SW2 and SW2_DVS min voltage of 1.275V and max voltage of 1.4V. 
 * Bits 0-9 in Register 25
 */
#define MX35_CPU_MIN_VOLTAGE	0xE7  /* 1.275V */
#define MX35_CPU_MAX_VOLTAGE	0x1CE /* 1.4V */

/* Bits 0-9 mask for register 25 */
#define SW2_VOLTAGE_MASK	0x3ff

#define MX35_VPLL_MIN_VOLTAGE	0x1	/* 1.25 V */
#define MX35_VPLL_MAX_VOLTAGE	0x2	/* 1.5V */	
#define MX35_VPLL_VOLTAGE_MASK	0x3	/* Bits 9 and 10 */
#define MX35_VPLL_LSH		9

/* Check if SW2HI is set or not */
extern void mx35_check_sw2hi_bit(void);

/*
 * turn debug on/off
 */
#undef MX35_CPU_FREQ_DEBUG

static struct cpufreq_frequency_table mx35_freq_table_con[] = {
	{0x01,  MX35_CPU_MIN_FREQ},
	{0x02,  MX35_CPU_MAX_FREQ},
	{0,     CPUFREQ_TABLE_END},
};

static int mx35_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu != 0)
		return -EINVAL;

	return cpufreq_frequency_table_verify(policy, mx35_freq_table_con);
}

static unsigned int mx35_get_speed(unsigned int cpu)
{
	if (cpu)
		return 0;
	return clk_get_rate(cpu_clk) / 1000;
}

static int calc_frequency_con(int target, unsigned int relation)
{
	int i = 0;
	
	if (relation == CPUFREQ_RELATION_H) {
		for (i = 1; i >= 0; i--) {
			if (mx35_freq_table_con[i].frequency <= target)
				return mx35_freq_table_con[i].frequency;
		}
	} else if (relation == CPUFREQ_RELATION_L) {
		for (i = 0; i <= 1; i++) {
			if (mx35_freq_table_con[i].frequency >= target)
				return mx35_freq_table_con[i].frequency;
		}
	}
	printk(KERN_ERR "Error: No valid cpufreq relation\n");
	return MX35_CPU_MAX_FREQ;
}

/* 
 * Set the destination CPU frequency target
 */
int set_cpu_freq(int freq)
{
	if (freq == MX35_CPU_MIN_FREQ * 1000)
		CHECK_ERROR(pmic_write_reg(REG_SW_1, MX35_CPU_MIN_VOLTAGE, SW2_VOLTAGE_MASK));
	else
		CHECK_ERROR(pmic_write_reg(REG_SW_1, MX35_CPU_MAX_VOLTAGE, SW2_VOLTAGE_MASK));

	udelay(20);	/* 20 us for the stepping to finish up */

#ifdef MX35_CPU_FREQ_DEBUG
	printk ("ARM frequency: %dHz\n", (int)clk_get_rate(cpu_clk));
#endif
	clk_set_rate(cpu_clk, freq);

#ifdef MX35_CPU_FREQ_DEBUG
	printk ("ARM frequency after change: %dHz\n", (int)clk_get_rate(cpu_clk));
#endif
	clk_put(cpu_clk);
	return 0;
}
EXPORT_SYMBOL(set_cpu_freq);

/* 
 * Set the destination CPU frequency target
 */
static int mx35_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_freqs freqs;
	long freq;
	int orig_cpu_rate = clk_get_rate(cpu_clk);
	int ret = 0;

	if (cpufreq_suspended)
		return ret;

	if (target_freq < policy->cpuinfo.min_freq)
		target_freq = policy->cpuinfo.min_freq;

	if (target_freq < policy->min)
		target_freq = policy->min;

	freq = calc_frequency_con(target_freq, relation) * 1000;
	freqs.old = clk_get_rate(cpu_clk) / 1000;
	freqs.new = (freq + 500) / 1000;
	freqs.cpu = 0;
	freqs.flags = 0;

	if (freq == orig_cpu_rate)
		return 0;

	if (freq == MX35_CPU_MIN_FREQ * 1000)
		pmic_write_reg(REG_SW_1, MX35_CPU_MIN_VOLTAGE, SW2_VOLTAGE_MASK);
	else
		pmic_write_reg(REG_SW_1, MX35_CPU_MAX_VOLTAGE, SW2_VOLTAGE_MASK);

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

#ifdef MX35_CPU_FREQ_DEBUG
	printk ("ARM frequency: %dHz\n", (int)clk_get_rate(cpu_clk));
#endif
	/*
	 * MCU clk
	 */
	clk_set_rate(cpu_clk, freq);

#ifdef MX35_CPU_FREQ_DEBUG
	printk ("ARM frequency after change: %dHz\n", (int)clk_get_rate(cpu_clk));
#endif
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	clk_put(cpu_clk);
	return ret;
}

/*!
 * Disable CPUFReq now that a charger has been detected
 */
void disable_cpufreq(void)
{
	struct cpufreq_freqs freqs;
	int org_freq = clk_get_rate(cpu_clk);

	freqs.old = org_freq / 1000;
	freqs.new = MAX_CPU_FREQUENCY / 1000;
	freqs.cpu = 0;
	freqs.flags = 0;

	cpufreq_suspended = 1;
	
	if (clk_get_rate(cpu_clk) != MAX_CPU_FREQUENCY) {
		set_cpu_freq(MAX_CPU_FREQUENCY);
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}
}
EXPORT_SYMBOL(disable_cpufreq);

/*!
 * Re-enable CPUFreq now that the charger is removed
 */
void enable_cpufreq(void)
{
	cpufreq_suspended = 0;
}
EXPORT_SYMBOL(enable_cpufreq);

/* 
 * Driver initialization
 */
static int __init mx35_cpufreq_driver_init(struct cpufreq_policy *policy)
{
	int ret = 0;

	printk("Luigi/Freescale MX35 CPUFREQ driver\n");

	if (policy->cpu != 0)
		return -EINVAL;
		
	cpu_clk = clk_get(NULL, "cpu_clk");

	if (IS_ERR(cpu_clk))
		return PTR_ERR(cpu_clk);
		
	policy->cur = policy->min = policy->max = 
		clk_get_rate(cpu_clk) / 1000;
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
	policy->cpuinfo.min_freq = MX35_CPU_MIN_FREQ;
	policy->cpuinfo.max_freq = MX35_CPU_MAX_FREQ;

	/* Set the transition latency to 50 us */
	policy->cpuinfo.transition_latency = 5 * 10000;

	ret = cpufreq_frequency_table_cpuinfo(policy, mx35_freq_table_con);

	if (ret < 0) 
		return ret;

	clk_put(cpu_clk);
	cpufreq_frequency_table_get_attr(mx35_freq_table_con, policy->cpu);

	core_regulator = regulator_get(NULL, core_reg_id);
	if (IS_ERR(core_regulator)) {
		printk(KERN_ERR "%s: failed to get core regulator\n", __func__);
		return PTR_ERR(core_regulator);
	}

	/* Change the stepping on SW2 to 25mV every 2 us */
	pmic_write_reg(REG_SW_4, (0x0 << 16), (0x3 << 16));

	return 0;
}

static int mx35_cpufreq_driver_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);
	clk_set_rate(cpu_clk, MX35_CPU_MAX_FREQ * 1000);

	clk_put(cpu_clk);
	regulator_put(core_regulator);
	return 0;
}

static struct cpufreq_driver mx35_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= mx35_verify_speed,
	.target		= mx35_set_target,
	.get		= mx35_get_speed,
	.init		= mx35_cpufreq_driver_init,
	.exit		= mx35_cpufreq_driver_exit,
	.name		= "MX35",
};

static int __init mx35_cpufreq_init(void)
{
	return cpufreq_register_driver(&mx35_driver);
}

static void __exit mx35_cpufreq_exit(void)
{
	cpufreq_unregister_driver(&mx35_driver);
}

module_init(mx35_cpufreq_init);
module_exit(mx35_cpufreq_exit);

MODULE_AUTHOR("Manish Lachwani");
MODULE_DESCRIPTION("MX35 Luigi CPUFreq Driver");
MODULE_LICENSE("GPL");
