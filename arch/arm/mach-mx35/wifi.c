/*
 * wifi.c -- MX35 WiFi driver.
 *
 * Copyright 2009 Lab126, Inc.  All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 *
 * Support for WiFi on MX35 Luigi
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/cpufreq.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/pmic_external.h>
#include <linux/sysdev.h>
#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/cacheflush.h>

/*
 * Turn on the WiFi Power and Reset GPIO
 */	
extern void gpio_wifi_enable(int enable);
extern int get_pmic_revision(void);
extern void gpio_wifi_do_reset(void);

#define SW1_VOLTAGE	1200000	/* SW1 voltage level in normal mode */
#define SW1_REGISTER	28	/* SW1 register to turn it off/on */
#define SW1_VALUE	24	/* SW1 register to set voltage */

static int luigi_wifi_status = 0;

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

static void mx35_sw1_power_state(int enable)
{
	static struct regulator *sw1_wifi_reg;
	
	sw1_wifi_reg = regulator_get(NULL, "SW1");

	if (enable == 1) {
		/* Enable SW1: Auto for Normal and Auto in standby */
		if (check_atlas_rev31())
			pmic_write_reg(SW1_REGISTER, 0x4, 0xf << 0);
		else
			pmic_write_reg(SW1_REGISTER, 0x6, 0xf << 0);

		regulator_enable(sw1_wifi_reg);
		regulator_set_voltage(sw1_wifi_reg, SW1_VOLTAGE, SW1_VOLTAGE);
	}
	else {
		regulator_disable(sw1_wifi_reg);
		regulator_set_voltage(sw1_wifi_reg, 0, 0);

		/* Set to off in Normal mode */
		pmic_write_reg(SW1_REGISTER, enable, 0xf << 0);

		/* Set 0V in Normal mode */ 
		pmic_write_reg(SW1_VALUE, 0, 0x1f << 0);
	}
	
	mdelay(10);
}

/* Reset the WiFi chip */
void mx35_wifi_reset(void)
{
	printk(KERN_INFO "Resetting Atheros WiFi\n");
	gpio_wifi_do_reset();

}
EXPORT_SYMBOL(mx35_wifi_reset);

static void luigi_wifi_enable(int enable)
{
	luigi_wifi_status = enable;
	mx35_sw1_power_state(enable);
	gpio_wifi_enable(enable);
	mdelay(10);
}

static ssize_t wifi_enable_store(struct sys_device *dev, const char *buf,
				 size_t size)
{
	int value;
	
	if (sscanf(buf, "%d", &value) > 0) {
		luigi_wifi_enable(value);
		return strlen(buf);
	}

	return -EINVAL;
}

static ssize_t wifi_enable_show(struct sys_device *dev, char *buf)
{
	char *curr = buf;
	
	curr += sprintf(curr, "WiFi status: %d\n", luigi_wifi_status);
	curr += sprintf(curr, "\n");

	return (curr - buf);
}

static SYSDEV_ATTR(wifi_enable, 0644, wifi_enable_show, wifi_enable_store);

static ssize_t wifi_reset_store(struct sys_device *dev, const char *buf,
				size_t size)
{
	int value;

	if (sscanf(buf, "%d", &value) > 0) {
		mx35_wifi_reset();
		return strlen(buf);
	}

	return -EINVAL;
}
static SYSDEV_ATTR(wifi_reset, 0644, NULL, wifi_reset_store);	

static struct sysdev_class wifi_sysclass = {
	.name = "wifi",
};

static struct sys_device wifi_device = {
	.id = 0,
	.cls = &wifi_sysclass,
};

static int luigi_wifi_sysdev_ctrl_init(void)
{
	int err = 0;
	
	err = sysdev_class_register(&wifi_sysclass);
	if (!err)
		err = sysdev_register(&wifi_device);
	if (!err) {
		err = sysdev_create_file(&wifi_device, &attr_wifi_enable);
		err = sysdev_create_file(&wifi_device, &attr_wifi_reset);
	}

	return err;
}

void luigi_wifi_sysdev_ctrl_exit(void)
{
	sysdev_remove_file(&wifi_device, &attr_wifi_enable);
	sysdev_remove_file(&wifi_device, &attr_wifi_reset);
	sysdev_unregister(&wifi_device);
	sysdev_class_unregister(&wifi_sysclass);
}

static int __init wifi_init(void)
{
	int err = 0;

	printk(KERN_INFO "Initializing Luigi WiFi \n");

	if (luigi_wifi_sysdev_ctrl_init() < 0) {
		printk(KERN_ERR "Luigi Wifi: Error setting up sysdev entries\n");
		return err;
	}

	luigi_wifi_enable(1);
	return err;
}

static void __exit wifi_cleanup(void)
{
	luigi_wifi_enable(0);
}

module_init(wifi_init);
module_exit(wifi_cleanup);

MODULE_AUTHOR("Manish Lachwani");
MODULE_DESCRIPTION("WiFi driver");
MODULE_LICENSE("GPL");
