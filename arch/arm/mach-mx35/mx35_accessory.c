/*
 * mx35_accessory -- MX35 Accessory Port
 *
 * Copyright 2009-2010 Amazon Technologies Inc., All rights reserved
 * Manish Lachwani (lachwani@lab126.com)
 *
 * Support for the MX35 Accessory Port
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/pmic_external.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/sysdev.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/pmic_external.h>
#include <linux/reboot.h>

#include <asm/hardware.h>
#include <asm/setup.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <asm/arch/board_id.h>

extern void gpio_acc_active(void);
extern int gpio_acc_get_irq(void);
extern void gpio_acc_inactive(void);
extern int gpio_acc_detected(void);
extern int gpio_acc_power_irq(void);

/* Define accessory as a misc device */
#define ACCESSORY_PORT_MINOR		159	/* /dev/accessoryport */

#define CHGDETS				0x40	/* Charger detect bit */

static struct miscdevice accessory_misc_device = {
	ACCESSORY_PORT_MINOR,
	"accessoryport",
	NULL,
};

#define MX35_ACCESSORY_VOLTAGE		3150000	/* 3.15V */
#define MX35_ACCESSORY_MIN_VOLTAGE	1200000	/* 1.2V  */
#define ACC_THRESHOLD			1000	/* 1 second */

#define VGEN1_ENABLE			1
#define VGEN1_DISABLE			0
#define VGEN1_LSH			0
#define VGEN1_MASK			1

#define GPO2_ENABLE			1
#define GPO2_DISABLE			0
#define GPO2_LSH			8
#define GPO2_MASK			1

#define GPO4_ENABLE			1
#define GPO4_DISABLE			0
#define GPO4_LSH			12
#define GPO4_MASK			1

#define GPO4ADIN_DIS			0
#define GPO4ADIN_MASK			1
#define GPO4ADIN_LSH			21

#define VGEN1_NEEDED()			(IS_SHASTA() && IS_EVT() && (GET_BOARD_HW_VERSION() == 1))
#define GPO4_NEEDED()			(IS_SHASTA() && (IS_DVT() || IS_PVT()))

char *vgen1_reg_id = "VGEN1";
static struct regulator *vgen1_regulator;

char *gpo2_reg_id = "GPO2";
static struct regulator *gpo2_regulator;

extern void gpio_acc_charger_enable(int);
extern int gpio_acc_power_ok(void);

static int accessory_irq_state = 0;	/* Disabled by default */

static int mx35_accessory_reboot(struct notifier_block *self, unsigned long action, void *cpu);

static struct notifier_block mx35_accessory_reboot_nb =
{
	.notifier_call = mx35_accessory_reboot,
};

/*!
 * This function sets the accessory voltage
 * on VGEN1
 */
static void accessory_regulator_vgen1_voltage(int enable)
{
	int err = 0;

	if (enable) {
		pmic_write_reg(REG_MODE_0, (VGEN1_ENABLE << VGEN1_LSH),
				(VGEN1_MASK << VGEN1_LSH));
		err = regulator_set_voltage(vgen1_regulator, MX35_ACCESSORY_VOLTAGE, MX35_ACCESSORY_VOLTAGE);
		if (err < 0)
			printk(KERN_ERR "do_accessory_work: Could not set VGEN1 voltage\n");
	}
	else {
		err = regulator_set_voltage(vgen1_regulator, MX35_ACCESSORY_MIN_VOLTAGE, MX35_ACCESSORY_MIN_VOLTAGE);
		if (err < 0)
			printk(KERN_ERR "do_accessory_work: Could not set VGEN1 voltage\n");
		
		pmic_write_reg(REG_MODE_0, (VGEN1_DISABLE << VGEN1_LSH),
				(VGEN1_MASK << VGEN1_LSH));
	}
}

/*!
 * Check if charger connected
 */
static int pmic_connected_charger(void)
{
	int sense_0 = 0;
	int ret = 0; /* Default: no host */
	
	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);

	if (sense_0 & CHGDETS)
		ret = 1;

	return ret;
}

/*!
 * This function sets the accessory voltage
 * on GPO2
 */
static void accessory_regulator_gpo2_voltage(int enable)
{
	if (enable) {
		pmic_write_reg(REG_POWER_MISC, (GPO2_ENABLE << GPO2_LSH),
				(GPO2_MASK << GPO2_LSH));

		if (GPO4_NEEDED()) {
			if (pmic_connected_charger()) {
				pmic_write_reg(REG_POWER_MISC, (GPO4_ENABLE << GPO4_LSH),
					(GPO4_MASK << GPO4_LSH));
			}
			else {
				pmic_write_reg(REG_POWER_MISC, (GPO4_DISABLE << GPO4_LSH),
					(GPO4_MASK << GPO4_LSH));
			}
		}
	}
	else {
		pmic_write_reg(REG_POWER_MISC, (GPO2_DISABLE << GPO2_LSH),
				(GPO2_MASK << GPO2_LSH));
	}
}

/*!
 * Enable GPO4 to start accessory charging 
 */
void enable_accessory_gpo4(void)
{
	if (GPO4_NEEDED() && !gpio_acc_detected()) {
		pmic_write_reg(REG_POWER_MISC, (GPO4_DISABLE << GPO4_LSH),
			(GPO4_MASK << GPO4_LSH));
	}
}

EXPORT_SYMBOL(enable_accessory_gpo4);

/*!
 * Disable GPO4 to stop accessory charging
 */
void disable_accessory_gpo4(void)
{
	if (GPO4_NEEDED()) {
		pmic_write_reg(REG_POWER_MISC, (GPO4_ENABLE << GPO4_LSH),
			(GPO4_MASK << GPO4_LSH));
	}
}
EXPORT_SYMBOL(disable_accessory_gpo4);

static void enable_accessory_pmic(void)
{
	if (!gpio_acc_detected()) {
		printk(KERN_INFO "cover: I def:acc:accessory_type=lighted_cover:\n");

		if (VGEN1_NEEDED())
			accessory_regulator_vgen1_voltage(1);
		else
			accessory_regulator_gpo2_voltage(1);

		set_irq_type(gpio_acc_get_irq(), IRQF_TRIGGER_HIGH);

		if (!gpio_acc_power_ok()) {
			kobject_uevent_atomic(&accessory_misc_device.this_device->kobj, KOBJ_ONLINE);
			gpio_acc_charger_enable(0);
		}
		else
			kobject_uevent_atomic(&accessory_misc_device.this_device->kobj, KOBJ_OFFLINE);

	}
	else {
		printk(KERN_INFO "No Accessory Found\n");

		if (VGEN1_NEEDED())
			accessory_regulator_vgen1_voltage(0);
		else
			accessory_regulator_gpo2_voltage(0);

		set_irq_type(gpio_acc_get_irq(), IRQF_TRIGGER_LOW);
		gpio_acc_charger_enable(1);
		kobject_uevent_atomic(&accessory_misc_device.this_device->kobj, KOBJ_OFFLINE);
	}
}

static void do_accessory_work(struct work_struct *dummy)
{
	enable_accessory_pmic();

	if (!accessory_irq_state) {
		enable_irq(gpio_acc_get_irq());
		accessory_irq_state = 1;
	}
}
static DECLARE_DELAYED_WORK(accessory_work, do_accessory_work);

static void do_power_ok_work(struct work_struct *dummy)
{
	int power_ok = gpio_acc_power_irq();

	if (!gpio_acc_power_ok()) {
		set_irq_type(power_ok, IRQF_TRIGGER_HIGH);
		gpio_acc_charger_enable(0);
		kobject_uevent_atomic(&accessory_misc_device.this_device->kobj, KOBJ_ONLINE);
	}
	else {
		set_irq_type(power_ok, IRQF_TRIGGER_LOW);
		gpio_acc_charger_enable(1);
		kobject_uevent_atomic(&accessory_misc_device.this_device->kobj, KOBJ_OFFLINE);
	}

	enable_irq(power_ok);
}
static DECLARE_DELAYED_WORK(power_ok_work, do_power_ok_work);

static irqreturn_t accessory_port_irq(int irq, void *devid)
{
	if (accessory_irq_state) {
		disable_irq(irq);
		accessory_irq_state = 0;
	}

	/* Debounce for 1 second */
	schedule_delayed_work(&accessory_work, msecs_to_jiffies(ACC_THRESHOLD));

	return IRQ_HANDLED;
}

static irqreturn_t accessory_power_ok_irq(int irq, void *devid)
{
	disable_irq(irq);

	/* Debounce for 1 second */
	schedule_delayed_work(&power_ok_work, msecs_to_jiffies(ACC_THRESHOLD));

	return IRQ_HANDLED;
}

/*!
 * sysfs
 */
void mx35_accessory_enable(int enable)
{
	if (!enable) {
		printk(KERN_INFO "Accessory port disabled!\n");

		if (VGEN1_NEEDED())
			accessory_regulator_vgen1_voltage(0);
		else
			accessory_regulator_gpo2_voltage(0);

		if (accessory_irq_state) {
			disable_irq(gpio_acc_get_irq());
			accessory_irq_state = 0;
		}
	}
	else {
		printk(KERN_INFO "Accessory port enable\n");
		/* Schedule a 1 second workqueue */
		schedule_delayed_work(&accessory_work, msecs_to_jiffies(ACC_THRESHOLD));
        }
}
EXPORT_SYMBOL(mx35_accessory_enable);

static ssize_t
mx35_show_accessory_charging(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", !gpio_acc_power_ok());
}
static SYSDEV_ATTR(mx35_accessory_charging, 0644, mx35_show_accessory_charging, NULL);

static ssize_t
mx35_store_accessory_state(struct sys_device *dev, const char *buf, size_t size)
{
	int value;

	if ((sscanf(buf, "%d", &value) > 0) &&
		((value == 0) || (value == 1))) {
			mx35_accessory_enable(value);
			return strlen(buf);
	}

	return -EINVAL;
}
static SYSDEV_ATTR(mx35_accessory_state, 0644, NULL, mx35_store_accessory_state);

static struct sysdev_class mx35_accessory_sysclass = {
	.name = "mx35_accessory",
};

static struct sys_device mx35_accessory_device = {
	.id = 0,
	.cls = &mx35_accessory_sysclass,
};

static int mx35_accessory_sysdev_ctrl_init(void)
{
	int err = 0;

	err = sysdev_class_register(&mx35_accessory_sysclass);
	if (!err)
		err = sysdev_register(&mx35_accessory_device);
	if (!err) {
		sysdev_create_file(&mx35_accessory_device, &attr_mx35_accessory_state);
		sysdev_create_file(&mx35_accessory_device, &attr_mx35_accessory_charging);
	}

	return err;
}

static void mx35_accessory_sysdev_ctrl_exit(void)
{
	sysdev_remove_file(&mx35_accessory_device, &attr_mx35_accessory_charging);
	sysdev_remove_file(&mx35_accessory_device, &attr_mx35_accessory_state);
	sysdev_unregister(&mx35_accessory_device);
	sysdev_class_unregister(&mx35_accessory_sysclass);
}

static int __init accessory_port_init(void)
{
	int err = 0, irq = gpio_acc_get_irq();
	int power_ok = gpio_acc_power_irq();

	printk(KERN_INFO "Initializing MX35 Luigi/Shasta Accessory Port\n");

	gpio_acc_active();

	if (!gpio_acc_detected())
		set_irq_type(irq, IRQF_TRIGGER_HIGH);
	else
		set_irq_type(irq, IRQF_TRIGGER_LOW);

	err = request_irq(irq, accessory_port_irq, 0, "MX35_Accessory", NULL);

	if (err != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get IRQ %d\n", irq);
		return err;
	}

	accessory_irq_state = 1;

	if (!gpio_acc_power_ok())
		set_irq_type(power_ok, IRQF_TRIGGER_HIGH);
	else
		set_irq_type(power_ok, IRQF_TRIGGER_LOW);

	err = request_irq(power_ok, accessory_power_ok_irq, 0, "Accessory_Charger", NULL);
	if (err != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get IRQ %d\n", power_ok);
		return err;
	}

	if (VGEN1_NEEDED()) {
		vgen1_regulator = regulator_get(NULL, vgen1_reg_id);
		if (IS_ERR(vgen1_regulator)) {
			printk(KERN_ERR "%s: failed to get vgen1 regulator\n", __func__);
			return PTR_ERR(vgen1_regulator);
		}
	}
	else {
		gpo2_regulator = regulator_get(NULL, gpo2_reg_id);
		if (IS_ERR(gpo2_regulator)) {
			printk(KERN_ERR "%s: failed to get gpo2 regulator\n", __func__);
			return PTR_ERR(gpo2_regulator);
		}
	}

	if (misc_register (&accessory_misc_device)) {
		printk (KERN_WARNING "accessoryport: Couldn't register device 10, "
				"%d.\n", ACCESSORY_PORT_MINOR);
		return -EBUSY;
	}

	enable_accessory_pmic();
	if (mx35_accessory_sysdev_ctrl_init() < 0)
		printk(KERN_ERR "mx35_accessory: Could not create sysfs interface\n");

	/* Disable GPO4ADIN */
	pmic_write_reg(REG_POWER_MISC, (GPO4ADIN_DIS << GPO4ADIN_LSH),
			(GPO4ADIN_MASK << GPO4ADIN_LSH));

	register_reboot_notifier(&mx35_accessory_reboot_nb);

	return err;
}

static void accessory_port_cleanup(void)
{
	mx35_accessory_enable(0);

	accessory_irq_state = 0;
	misc_deregister(&accessory_misc_device);
	gpio_acc_inactive();
	if (VGEN1_NEEDED())
		regulator_put(vgen1_regulator);
	else
		regulator_put(gpo2_regulator);
	free_irq(gpio_acc_get_irq(), NULL);
	mx35_accessory_sysdev_ctrl_exit();
}

static int mx35_accessory_reboot(struct notifier_block *self, unsigned long action, void *cpu)
{
	printk(KERN_INFO "MX35 Accessory shutdown\n");
	accessory_port_cleanup();
	return 0;
}

static void __exit accessory_port_remove(void)
{
	accessory_port_cleanup();
	unregister_reboot_notifier(&mx35_accessory_reboot_nb);
}

module_init(accessory_port_init);
module_exit(accessory_port_remove);

MODULE_AUTHOR("Manish Lachwani");
MODULE_DESCRIPTION("MX35 Accessory Port");
MODULE_LICENSE("GPL");
