/*
 * luigi_battery.c
 * Battery driver for MX35 Luigi Board
 *
 * Copyright (C) Amazon Technologies Inc. All rights reserved.
 * Manish Lachwani (lachwani@lab126.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/sysdev.h>

/*
 * I2C registers that need to be read
 */
#define LUIGI_TEMP_LOW			0x06
#define LUIGI_TEMP_HI			0x07
#define LUIGI_VOLTAGE_LOW		0x08
#define LUIGI_VOLTAGE_HI		0x09
#define LUIGI_BATTERY_ID		0x7E
#define LUIGI_AI_LO			0x14	/* Average Current */
#define LUIGI_AI_HI			0x15
#define LUIGI_FLAGS			0x0a
#define LUIGI_BATTERY_RESISTANCE	20
#define LUIGI_CSOC			0x2c	/* Compensated state of charge */
#define LUIGI_CAC_L			0x10	/* milliamp-hour */
#define LUIGI_CAC_H			0x11
#define LUIGI_CYCL_H			0x29
#define LUIGI_CYCL_L			0x28
#define LUIGI_LMD_H			0x0F
#define LUIGI_LMD_L			0x0E
#define LUIGI_CYCT_L			0x2A
#define LUIGI_CYCT_H			0x2B

#define LUIGI_I2C_ADDRESS		0x55	/* Battery I2C address on the bus */
#define LUIGI_TEMP_LOW_THRESHOLD	37
#define LUIGI_TEMP_HI_THRESHOLD		113
#define LUIGI_VOLT_LOW_THRESHOLD	2500
#define LUIGI_VOLT_HI_THRESHOLD		4350
#define LUIGI_BATTERY_INTERVAL		20000	/* 20 second duration */
#define LUIGI_BATTERY_INTERVAL_START	5000	/* 5 second timer on startup */
#define LUIGI_BATTERY_INTERVAL_ERROR	10000	/* 10 second timer after an error */
#define LUIGI_BATTERY_INTERVAL_EARLY	1000	/* 1 second after probe */

#define DRIVER_NAME			"Luigi_Battery"
#define DRIVER_VERSION			"1.0"
#define DRIVER_AUTHOR			"Manish Lachwani"

#define GENERAL_ERROR			0x0001
#define ID_ERROR			0x0002
#define TEMP_RANGE_ERROR		0x0004
#define VOLTAGE_ERROR			0x0008

#define LUIGI_BATTERY_RETRY_THRESHOLD	100	/* Failed retry case - 100 */

#define LUIGI_ERROR_THRESHOLD		4	/* Max of 5 errors at most before sending to userspace */

#define LUIGI_BATTERY_RELAXED_THRESH	1800	/* Every 10 hours or 36000 seconds */
static int luigi_battery_no_stats = 1;

unsigned int luigi_battery_error_flags = 0;
EXPORT_SYMBOL(luigi_battery_error_flags);

static int luigi_lmd_counter = LUIGI_BATTERY_RELAXED_THRESH;

int luigi_battery_id = 0;
int luigi_voltage = 0;
int luigi_temperature = 0;
int luigi_battery_current = 0;
int luigi_battery_capacity = 0;
int luigi_battery_mAH = 0;

EXPORT_SYMBOL(luigi_battery_id);
EXPORT_SYMBOL(luigi_voltage);
EXPORT_SYMBOL(luigi_temperature);
EXPORT_SYMBOL(luigi_battery_current);
EXPORT_SYMBOL(luigi_battery_capacity);
EXPORT_SYMBOL(luigi_battery_mAH);

static int luigi_battery_lmd = 0;
static int luigi_battery_cycl = 0;
static int luigi_battery_cyct = 0;

extern int mxc_i2c_suspended;
static int i2c_error_counter = 0;	/* Retry counter */
static int battery_driver_stopped = 0;	/* Battery stop/start flag */
static int last_suspend_current = 0;	/* Last suspend gasguage reading */

static int temp_error_counter = 0;
static int volt_error_counter = 0;

static int battery_curr_diags = 0;
static int battery_suspend_curr_diags = 0;

/*
 * Create the sysfs entries
 */

static ssize_t
battery_id_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_battery_id);
}
static SYSDEV_ATTR(battery_id, 0644, battery_id_show, NULL);

static ssize_t
battery_current_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_battery_current);
}
static SYSDEV_ATTR(battery_current, 0644, battery_current_show, NULL);

static ssize_t
battery_suspend_current_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", last_suspend_current);
}
static SYSDEV_ATTR(battery_suspend_current, 0644, battery_suspend_current_show, NULL);

static ssize_t
battery_voltage_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_voltage);
}
static SYSDEV_ATTR(battery_voltage, 0644, battery_voltage_show, NULL);

static ssize_t
battery_temperature_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_temperature);
}
static SYSDEV_ATTR(battery_temperature, 0644, battery_temperature_show, NULL);

static ssize_t
battery_capacity_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d%%\n", luigi_battery_capacity);
}
static SYSDEV_ATTR(battery_capacity, 0644, battery_capacity_show, NULL);

static ssize_t
battery_mAH_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_battery_mAH);
}
static SYSDEV_ATTR(battery_mAH, 0644, battery_mAH_show, NULL);

static ssize_t
battery_error_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_battery_error_flags);
}
static SYSDEV_ATTR(battery_error, 0644, battery_error_show, NULL);

static ssize_t
battery_current_diags_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", battery_curr_diags);
}
static SYSDEV_ATTR(battery_current_diags, 0644, battery_current_diags_show, NULL);

static ssize_t
battery_suspend_current_diags_show(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", battery_suspend_curr_diags);
}
static SYSDEV_ATTR(battery_suspend_current_diags, 0644, battery_suspend_current_diags_show, NULL);

/* This is useful for runtime debugging */

static ssize_t
battery_show_temp_thresholds(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "Hi-%dF, Lo-%dF\n", LUIGI_TEMP_LOW_THRESHOLD, LUIGI_TEMP_HI_THRESHOLD);
}
static SYSDEV_ATTR(battery_temp_thresholds, 0644, battery_show_temp_thresholds, NULL);

static ssize_t
battery_show_voltage_thresholds(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "Hi-%dmV, Lo-%dmV\n", LUIGI_VOLT_LOW_THRESHOLD, LUIGI_VOLT_HI_THRESHOLD);
}
static SYSDEV_ATTR(battery_voltage_thresholds, 0644, battery_show_voltage_thresholds, NULL);

static ssize_t
battery_show_lmd(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_battery_lmd);
}
static SYSDEV_ATTR(battery_lmd, 0644, battery_show_lmd, NULL);

static ssize_t
battery_show_cycl(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "0x%x\n", luigi_battery_cycl);
}
static SYSDEV_ATTR(battery_cycl, 0644, battery_show_cycl, NULL);

static ssize_t
battery_show_cyct(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "0x%x\n", luigi_battery_cyct);
}
static SYSDEV_ATTR(battery_cyct, 0644, battery_show_cyct, NULL);

static ssize_t
battery_show_polling_intervals(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "Regular-%dms, Bootup-%dms\n", 
				LUIGI_BATTERY_INTERVAL, LUIGI_BATTERY_INTERVAL_START);
}
static SYSDEV_ATTR(battery_polling_intervals, 0644, battery_show_polling_intervals, NULL);

static ssize_t
battery_show_i2c_address(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "i2c address-0x%x\n", LUIGI_I2C_ADDRESS);
}
static SYSDEV_ATTR(battery_i2c_address, 0644, battery_show_i2c_address, NULL);

static ssize_t
battery_show_resume_stats(struct sys_device *dev, char *buf)
{
	return sprintf(buf, "%d\n", luigi_battery_no_stats);
}

static ssize_t
battery_store_resume_stats(struct sys_device *dev, const char *buf, size_t count)
{
	int value;

	if ((sscanf(buf, "%d", &value) > 0) &&
		((value == 0) || (value == 1))) {
			luigi_battery_no_stats = value;
			return strlen(buf);
	}

	return -EINVAL;
}
static SYSDEV_ATTR(resume_stats, S_IRUGO|S_IWUSR, battery_show_resume_stats, battery_store_resume_stats);
	
static struct sysdev_class luigi_battery_sysclass = {
	.name = "luigi_battery",
};

static struct sys_device luigi_battery_device = {
	.id = 0,
	.cls = &luigi_battery_sysclass,
};

static int luigi_battery_sysdev_ctrl_init(void)
{
	int err = 0;

	err = sysdev_class_register(&luigi_battery_sysclass);
	if (!err)
		err = sysdev_register(&luigi_battery_device);
	if (!err) {
		sysdev_create_file(&luigi_battery_device, &attr_battery_id);
		sysdev_create_file(&luigi_battery_device, &attr_battery_current);
		sysdev_create_file(&luigi_battery_device, &attr_battery_voltage);
		sysdev_create_file(&luigi_battery_device, &attr_battery_temperature);
		sysdev_create_file(&luigi_battery_device, &attr_battery_capacity);
		sysdev_create_file(&luigi_battery_device, &attr_battery_mAH);
		sysdev_create_file(&luigi_battery_device, &attr_battery_voltage_thresholds);
		sysdev_create_file(&luigi_battery_device, &attr_battery_polling_intervals);
		sysdev_create_file(&luigi_battery_device, &attr_battery_temp_thresholds);
		sysdev_create_file(&luigi_battery_device, &attr_battery_i2c_address);
		sysdev_create_file(&luigi_battery_device, &attr_battery_error);
		sysdev_create_file(&luigi_battery_device, &attr_battery_suspend_current);
		sysdev_create_file(&luigi_battery_device, &attr_battery_current_diags);
		sysdev_create_file(&luigi_battery_device, &attr_battery_suspend_current_diags);
		sysdev_create_file(&luigi_battery_device, &attr_battery_cycl);
		sysdev_create_file(&luigi_battery_device, &attr_battery_lmd);
		sysdev_create_file(&luigi_battery_device, &attr_battery_cyct);
		sysdev_create_file(&luigi_battery_device, &attr_resume_stats);
	}

	return err;
}

static void luigi_battery_sysdev_ctrl_exit(void)
{
	sysdev_remove_file(&luigi_battery_device, &attr_battery_id);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_current);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_voltage);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_temperature);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_capacity);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_mAH);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_voltage_thresholds);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_polling_intervals);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_temp_thresholds);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_i2c_address);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_error);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_suspend_current);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_current_diags);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_suspend_current_diags);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_cycl);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_lmd);
	sysdev_remove_file(&luigi_battery_device, &attr_battery_cyct);
	sysdev_remove_file(&luigi_battery_device, &attr_resume_stats);

	sysdev_unregister(&luigi_battery_device);
	sysdev_class_unregister(&luigi_battery_sysclass);
}

static struct i2c_client *luigi_battery_i2c_client;

int luigi_battery_present = 0;
EXPORT_SYMBOL(luigi_battery_present);

static int luigi_i2c_read(unsigned char reg_num, unsigned char *value)
{
	s32 error;

	error = i2c_smbus_read_byte_data(luigi_battery_i2c_client, reg_num);
	if (error < 0) {
		printk(KERN_INFO "luigi_battery: i2c read retry\n");
		return -EIO;
	}

	*value = (unsigned char) (error & 0xFF);
	return 0;
}

static int luigi_battery_read_voltage(int *voltage)
{
	unsigned char hi, lo;
	int volts;
	int err1 = 0, err2 = 0;
	int error = 0;
	
	err1 = luigi_i2c_read(LUIGI_VOLTAGE_LOW, &lo);
	err2 = luigi_i2c_read(LUIGI_VOLTAGE_HI, &hi);

	error = err1 | err2;

	if (!error) {
		volts = (hi << 8) | lo;
		*voltage = volts;
	}
	
	return error;
}

static int luigi_battery_read_current(int *curr)
{
	unsigned char hi, lo, flags;
	int c;
	int err1 = 0, err2 = 0, err3 = 0;
	int sign = 1;
	int error = 0;

	err1 = luigi_i2c_read(LUIGI_AI_LO, &lo);
	err2 = luigi_i2c_read(LUIGI_AI_HI, &hi);
	err3 = luigi_i2c_read(LUIGI_FLAGS, &flags);

	error = err1 | err2 | err3;

	if (!error) {
		if (flags & 0x80)
			sign = 1;
		else
			sign = -1;

		battery_curr_diags = sign * (((hi << 8) | lo) * 357);

		if (!battery_suspend_curr_diags)
			battery_suspend_curr_diags = battery_curr_diags;

		c = sign * ((((hi << 8) | lo) * 357) / (100 * LUIGI_BATTERY_RESISTANCE));
		*curr = c;
	}

	return error;
}

static int luigi_battery_read_capacity(int *capacity)
{
	unsigned char tmp;
	int err = 0;

	err = luigi_i2c_read(LUIGI_CSOC, &tmp);
	if (!err)
		*capacity = tmp;

	return err;
}
	
static int luigi_battery_read_mAH(int *mAH)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;
	
	err1 = luigi_i2c_read(LUIGI_CAC_L, &lo);
	err2 = luigi_i2c_read(LUIGI_CAC_H, &hi);

	error = err1 | err2;

	if (!error)
		*mAH = ((((hi << 8) | lo) * 357) / (100 * LUIGI_BATTERY_RESISTANCE));
	
	return error;
}

/* Read Last Measured Discharge and Learning count */
static void luigi_battery_read_lmd_cyc(int *lmd, int *cycl, int *cyct)
{
	unsigned char hi, lo;
	int err1 = 0, err2 = 0;
	int error = 0;

	err1 = luigi_i2c_read(LUIGI_CYCL_L, &lo);
	err2 = luigi_i2c_read(LUIGI_CYCL_H, &hi);

	error = err1 | err2;

	if (!error)
		*cycl = (hi << 8) | lo;

	/* Clear out hi, lo for next reading */
	hi = lo = 0;

	err1 = luigi_i2c_read(LUIGI_LMD_L, &lo);
	err2 = luigi_i2c_read(LUIGI_LMD_H, &hi);

	error = err1 | err2;

	if (!error)
		*lmd = ((((hi << 8) | lo) * 357) / (100 * LUIGI_BATTERY_RESISTANCE));

	/* Clear out hi, lo for next reading */
	hi = lo = 0;

	err1 = luigi_i2c_read(LUIGI_CYCT_L, &lo);
	err2 = luigi_i2c_read(LUIGI_CYCT_H, &hi);

	error = err1 | err2;

	if (!error)
		*cyct = (hi << 8) | lo;
}

static int luigi_battery_read_temperature(int *temperature)
{
	unsigned char temp_hi, temp_lo;
	int err1 = 0, err2 = 0;
	int celsius, fahrenheit;
	int error = 0;
	
	err1 = luigi_i2c_read(LUIGI_TEMP_LOW, &temp_lo);
	err2 = luigi_i2c_read(LUIGI_TEMP_HI, &temp_hi);
	
	error = err1 | err2;

	if (!error) {
		celsius = ((((temp_hi << 8) | temp_lo) + 2) / 4) - 273;
		fahrenheit = ((celsius * 9) / 5) + 32;
		*temperature = fahrenheit;
	}

	return error;
}

static int luigi_battery_read_id(int *id)
{
	int error = 0;
	unsigned char value = 0xFF;

	error = luigi_i2c_read(LUIGI_BATTERY_ID, &value);
	
	if (!error) {
		*id = value;
	}

	return error;
}

static void battery_handle_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(battery_work, battery_handle_work);

/* Main battery timer task */
static void battery_handle_work(struct work_struct *work)
{
	int err = 0;
	int batt_err_flag = 0;

	/* Is the battery driver stopped? */
	if (battery_driver_stopped)
		return;

	/* Is i2c bus suspended? */
	if (mxc_i2c_suspended)
		goto out_done;

	err = luigi_battery_read_id(&luigi_battery_id);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	err = luigi_battery_read_temperature(&luigi_temperature);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Check for the temperature range violation
	 */
	if ( (luigi_temperature <= LUIGI_TEMP_LOW_THRESHOLD) ||
		(luigi_temperature >= LUIGI_TEMP_HI_THRESHOLD) ) {
			temp_error_counter++;
	}
	else {
		temp_error_counter = 0;
		luigi_battery_error_flags &= ~TEMP_RANGE_ERROR;
	}

	if (temp_error_counter > LUIGI_ERROR_THRESHOLD) {
		luigi_battery_error_flags |= TEMP_RANGE_ERROR;
		printk(KERN_ERR "battery driver temp - %d\n", luigi_temperature);
		temp_error_counter = 0;
	}

	err = luigi_battery_read_voltage(&luigi_voltage);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Check for the battery voltage range violation
	 */
	if ( (luigi_voltage <= LUIGI_VOLT_LOW_THRESHOLD) ||
		(luigi_voltage >= LUIGI_VOLT_HI_THRESHOLD) ) {
			volt_error_counter++;
	}
	else {
		volt_error_counter = 0;
		luigi_battery_error_flags &= ~VOLTAGE_ERROR;
	}

	if (volt_error_counter > LUIGI_ERROR_THRESHOLD) {
		printk(KERN_ERR "battery driver voltage - %dmV\n", luigi_voltage);
		luigi_battery_error_flags |= VOLTAGE_ERROR;
		volt_error_counter = 0;
	}

	/*
	 * Check for the battery current
	 */
	err = luigi_battery_read_current(&luigi_battery_current);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;	
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Read the gasguage capacity
	 */
	err = luigi_battery_read_capacity(&luigi_battery_capacity);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/*
	 * Read the battery mAH
	 */
	err = luigi_battery_read_mAH(&luigi_battery_mAH);
	if (err) {
		batt_err_flag |= GENERAL_ERROR;
		goto out;
	}
	else
		i2c_error_counter = 0;

	/* Take these readings every 10 hours */
	if (luigi_lmd_counter == LUIGI_BATTERY_RELAXED_THRESH) {
		luigi_lmd_counter = 0;
		luigi_battery_read_lmd_cyc(&luigi_battery_lmd, &luigi_battery_cycl, &luigi_battery_cyct);
	}
	else {
		luigi_lmd_counter++;
	}

out:
	if (batt_err_flag) {
		i2c_error_counter++;
		if (i2c_error_counter == LUIGI_BATTERY_RETRY_THRESHOLD) {
			luigi_battery_error_flags |= GENERAL_ERROR;
			printk(KERN_ERR "Luigi battery: i2c read error, retry exceeded\n");
			i2c_error_counter = 0;
		}
	}
	else {
		luigi_battery_error_flags &= ~GENERAL_ERROR;
		i2c_error_counter = 0;
	}

	pr_debug("temp: %d, volt: %d, current: %d, capacity: %d%%, mAH: %d\n",
		luigi_temperature, luigi_voltage, luigi_battery_current,
		luigi_battery_capacity, luigi_battery_mAH);
out_done:
	if (batt_err_flag & GENERAL_ERROR)
		schedule_delayed_work(&battery_work, msecs_to_jiffies(LUIGI_BATTERY_INTERVAL_ERROR));
	else
		schedule_delayed_work(&battery_work, msecs_to_jiffies(LUIGI_BATTERY_INTERVAL));
	return;
}

static int luigi_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int luigi_remove(struct i2c_client *client);

static const struct i2c_device_id luigi_i2c_id[] =  {
        { "Luigi_Battery", 0 },
        { },
};
MODULE_DEVICE_TABLE(i2c, luigi_i2c_id);

static struct i2c_driver luigi_i2c_driver = {
	.driver = {
			.name = DRIVER_NAME,
		  },
	.probe = luigi_probe,
	.remove = luigi_remove,
	.id_table = luigi_i2c_id,
};

static unsigned short normal_i2c[] = { LUIGI_I2C_ADDRESS, I2C_CLIENT_END };
I2C_CLIENT_INSMOD;

int luigi_battery_suspend(void)
{
	if (luigi_battery_present) {
		battery_driver_stopped = 1;
		cancel_rearming_delayed_work(&battery_work);
		i2c_error_counter = 0;
	}

	return 0;
}
EXPORT_SYMBOL(luigi_battery_suspend);

int luigi_battery_resume(void)
{
	battery_driver_stopped = 0;

	return 0;
}
EXPORT_SYMBOL(luigi_battery_resume);

void luigi_battery_resume_stats(void)
{
	int batt_current = 0;
	int batt_voltage = 0;
	int batt_capacity = 0;

	if (luigi_battery_present && !mxc_i2c_suspended && !battery_driver_stopped && !luigi_battery_no_stats) {
		if (!luigi_battery_read_voltage(&batt_voltage)) {	
			luigi_voltage = batt_voltage;
		}

		if (!luigi_battery_read_capacity(&batt_capacity)) {
			luigi_battery_capacity = batt_capacity;
		}

		battery_suspend_curr_diags = 0;

		/*
		 * Take a reading now and dump into logs. Usually accurate suspend current
		 * if taken with a few seconds of resume.
		 */
		if (!luigi_battery_read_current(&batt_current)) {
			printk(KERN_INFO "battery: I def:sus:device_suspend_current=%dmA:\n", batt_current);
			last_suspend_current = batt_current;
		}
		else {
			printk(KERN_ERR "Battery suspend current could not be obtained\n");
			batt_current = 0;
		}
	}
	schedule_delayed_work(&battery_work, msecs_to_jiffies(LUIGI_BATTERY_INTERVAL));
}
EXPORT_SYMBOL(luigi_battery_resume_stats);
	
struct luigi_info {
        struct i2c_client *client;
};

static int luigi_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct luigi_info  *info;

        info = kzalloc(sizeof(*info), GFP_KERNEL);
        if (!info) {
                return -ENOMEM;
        }

        client->addr = LUIGI_I2C_ADDRESS;
        i2c_set_clientdata(client, info);
        info->client = client;
        luigi_battery_i2c_client = info->client;
        luigi_battery_i2c_client->addr = LUIGI_I2C_ADDRESS;

	if (luigi_battery_read_id(&luigi_battery_id) < 0)
		return -ENODEV;

	luigi_battery_present = 1;

	if (luigi_battery_sysdev_ctrl_init() < 0)
		printk(KERN_ERR "luigi battery: could not create sysfs entries\n");

        schedule_delayed_work(&battery_work, msecs_to_jiffies(LUIGI_BATTERY_INTERVAL_EARLY));

        return 0;
}

static int luigi_remove(struct i2c_client *client)
{
        struct luigi_info *info = i2c_get_clientdata(client);

        if (luigi_battery_present) {
                battery_driver_stopped = 1;
                cancel_rearming_delayed_work(&battery_work);
                luigi_battery_sysdev_ctrl_exit();
        }

        i2c_set_clientdata(client, info);

        return 0;
}

static int __init luigi_battery_init(void)
{
	int ret = 0;

	ret = i2c_add_driver(&luigi_i2c_driver);

	if (ret) {
		printk(KERN_ERR "Luigi battery: Could not add driver\n");
		return ret;
	}
	return 0;
}
	

static void __exit luigi_battery_exit(void)
{
	if (luigi_battery_present) {
		battery_driver_stopped = 1;
		cancel_rearming_delayed_work(&battery_work);
		luigi_battery_sysdev_ctrl_exit();
	}
	i2c_del_driver(&luigi_i2c_driver);
}

module_init(luigi_battery_init);
module_exit(luigi_battery_exit);

MODULE_DESCRIPTION("MX35 Luigi Battery Driver");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
