/*
 * cpufreq.c -- MX35 CPUfreq driver.
 * MX35 cpufreq driver
 *
 * Copyright 2009 Lab126, Inc.  All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 *
 * Support for CPUFreq on the Luigi. It supports two operating points:
 * 266MHz and 532MHz. Along with the CPU Frequency scaling, the voltage
 * is also adjusted to be 1.2V (266MHz) or 1.4V (532MHz).
 * 
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/cacheflush.h>

#include "crm_regs.h"

static struct clk *cpu_clk;
DEFINE_RAW_SPINLOCK(cpufreq_lock);

#define MXC_PERCLK_DIVIDER 0x1000

/*
 * turn debug on/off
 */
#undef MX3_CPU_FREQ_DEBUG

static struct cpufreq_frequency_table mx35_freq_table_con[] = {
	{0x01,  266000},
	{0x02,  532000},
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
	return 532000;
}

/* 
 * Set the destination CPU frequency target
 */
static int mx35_set_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	struct cpufreq_freqs freqs;
	long freq;
	unsigned long flags;
	unsigned long pdr0;
	int orig_cpu_rate = clk_get_rate(cpu_clk);

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

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

#ifdef MX3_CPU_FREQ_DEBUG
	printk ("ARM frequency: %dHz\n", (int)clk_get_rate(cpu_clk));
#endif
	spin_lock_irqsave(&cpufreq_lock, flags);
	/*
	 * MCU clk
	 */
	clk_set_rate(cpu_clk, freq);

	pdr0 = __raw_readl(MXC_CCM_PDR0);
	pdr0 |= MXC_PERCLK_DIVIDER;
	__raw_writel(pdr0, MXC_CCM_PDR0);
	
	spin_unlock_irqrestore(&cpufreq_lock, flags);


#ifdef MX3_CPU_FREQ_DEBUG
	printk ("ARM frequency after change: %dHz\n", (int)clk_get_rate(cpu_clk));
#endif
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);

	clk_put(cpu_clk);
	return 0;
}

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
	policy->cpuinfo.min_freq = 266000;
	policy->cpuinfo.max_freq = 532000;

	/* Set the transition latency to 50 us */
	policy->cpuinfo.transition_latency = 5 * 10000;

	ret = cpufreq_frequency_table_cpuinfo(policy, mx35_freq_table_con);

	if (ret < 0) 
		return ret;

	clk_put(cpu_clk);

	cpufreq_frequency_table_get_attr(mx35_freq_table_con, policy->cpu);
	return 0;
}

static int mx35_cpufreq_driver_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);
	clk_set_rate(cpu_clk, 532000 * 1000);

	clk_put(cpu_clk);
	return 0;
}

static struct cpufreq_driver mx35_driver = {
	.flags		= CPUFREQ_STICKY,
	.verify		= mx35_verify_speed,
	.target		= mx35_set_target,
	.get		= mx35_get_speed,
	.init		= mx35_cpufreq_driver_init,
	.exit		= mx35_cpufreq_driver_exit,
	.name		= "MX31",
};

static int __init mx35_cpufreq_init(void)
{
	return cpufreq_register_driver(&mx35_driver);
}

arch_initcall(mx35_cpufreq_init);
