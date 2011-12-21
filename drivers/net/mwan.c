/*
 * mwan.c  --  Mario WAN hardware control driver
 *
 * Copyright 2005-2009 Lab126, Inc.  All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/uaccess.h>
#include <net/mwan.h>
#include <asm/arch/board_id.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
#include <linux/device.h>
#include <linux/pmic_external.h>
#else
#include <asm/arch/pmic_external.h>
#endif
#include <asm/arch/pmic_power.h>

// DTP module hardware-specific settings
#undef USE_DTP_SLEEP_MODE
#undef USE_DTP_RFE_PULLUP


#undef DEBUG

#ifdef DEBUG
#define log_debug(format, arg...) printk("mwan: D %s:" format, __func__, ## arg)
#else
#define log_debug(format, arg...)
#endif

#define log_info(format, arg...) printk("mwan: I %s:" format, __func__, ## arg)
#define log_err(format, arg...) printk("mwan: E %s:" format, __func__, ## arg)

#define VERSION			"1.2.0"

#define PROC_WAN		"wan"
#define PROC_WAN_POWER		"power"
#define PROC_WAN_ENABLE		"enable"
#define PROC_WAN_TYPE		"type"
#define PROC_WAN_USB		"usb"

static struct proc_dir_entry *proc_wan_parent;
static struct proc_dir_entry *proc_wan_power;
static struct proc_dir_entry *proc_wan_enable;
static struct proc_dir_entry *proc_wan_type;
static struct proc_dir_entry *proc_wan_usb;

static wan_status_t wan_status = WAN_OFF;
static int modem_type = MODEM_TYPE_UNKNOWN;

static int wan_rf_enable_state = 0;
static int wan_usb_enable_state = 0;
static int wan_on_off_state = 0;


#define WAN_STRING_CLASS	"wan"
#define WAN_STRING_DEV		"mwan"

static struct file_operations mwan_ops = {
	.owner = THIS_MODULE,
	.ioctl = NULL,		// (add ioctl support, if desired)
};


static struct class *wan_class = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
static struct device *wan_dev = NULL;
#else
static struct class_device *wan_dev = NULL;
#endif
static int wan_major = 0;

#define TPH_TYPE_NONE		0
#define TPH_TYPE_RINGSMS	1
#define TPH_TYPE_USBWAKE	2
static time_t tph_last_seconds = 0;
static int tph_last_type = TPH_TYPE_NONE;

// standard network deregistration time definition:
//   2s -- maximum time required between the start of power down and that of IMSI detach
//   5s -- maximum time required for the IMSI detach
//   5s -- maximum time required between the IMSI detach and power down finish (time
//         required to stop tasks, etc.)
//   3s -- recommended safety margin
#define NETWORK_DEREG_TIME	((12 + 3) * 1000)

// for the DTP module, we use an optimized path for performing deregistration
#define NETWORK_DEREG_TIME_OPT	(3 * 1000)

// minimum allowed delay between TPH notifications
#define WAKE_EVENT_INTERVAL	10


static time_t modem_off_seconds = -1;

#define DTP_ON_DELAY_SEC	5	// DTP WAN power-up boot delay
#define DTP_OFF_DELAY_SEC	3	// DTP WAN power-down supercap discharge delay


static inline int
get_wan_on_off(
	void)
{
	return wan_on_off_state;
}


static void
set_wan_on_off(
	int enable)
{
	extern void gpio_wan_power(int);

	enable = enable != 0;	// (ensure that "enable" is a boolean)

	if (modem_type == MODEM_TYPE_AD_DTP) {
		if (!enable) {
			modem_off_seconds = CURRENT_TIME_SEC.tv_sec;

		} else if (modem_off_seconds > 0) {
			long wait_seconds = (modem_off_seconds + DTP_OFF_DELAY_SEC) - CURRENT_TIME_SEC.tv_sec;

			if (wait_seconds < 0) {
				modem_off_seconds = wait_seconds = 0;

			} else if (wait_seconds > 0) {
				if (wait_seconds > DTP_OFF_DELAY_SEC) {
					wait_seconds = DTP_OFF_DELAY_SEC;
				}

				log_info("wpd:wait=%ld:modem power on delay\n", wait_seconds);

				ssleep(wait_seconds);
			}
		}
	}

	log_debug("pow:enable=%d:setting WAN hardware power state\n", enable);

	gpio_wan_power(enable);

	wan_on_off_state = enable;
}


static inline int
get_wan_rf_enable(
	void)
{
	return wan_rf_enable_state;
}


static void
set_wan_rf_enable(
	int enable)
{
	extern void gpio_wan_rf_enable(int);

	if (enable != get_wan_rf_enable()) {
		log_debug("swe:enable=%d:setting WAN RF enable state\n", enable);

		gpio_wan_rf_enable(enable);

		wan_rf_enable_state = enable;
	}
}


static inline int
get_wan_usb_enable(
	void)
{
	return wan_usb_enable_state;
}


static void
set_wan_usb_enable(
	int enable)
{
	extern void gpio_wan_usb_enable(int);

	if (enable != get_wan_usb_enable()) {
		log_debug("swu:enable=%d:setting WAN USB enable state\n", enable);

		gpio_wan_usb_enable(enable);

		wan_usb_enable_state = enable;
	}
}


static int
set_wan_power(
	wan_status_t new_status)
{
	wan_status_t check_status = new_status == WAN_OFF_KILL ? WAN_OFF : new_status;

	if (check_status == wan_status) {
		return 0;
	}

	// ignore any spurious WAKE line events during module power processing
	tph_last_seconds = CURRENT_TIME_SEC.tv_sec;

	switch (new_status) {

		case WAN_ON :
#ifdef USE_DTP_SLEEP_MODE
			if (get_wan_on_off() == 0 || modem_type != MODEM_TYPE_AD_DTP) {
#endif
				// bring up WAN_ON_OFF
				set_wan_on_off(1);
#ifdef USE_DTP_SLEEP_MODE
			}
#endif

			if (modem_type == MODEM_TYPE_AD_DTP || modem_type == MODEM_TYPE_UNKNOWN) {
				// pause following power-on before bringing up the RF_ENABLE line (use 1s per DTP spec)
				ssleep(1);

			} else {
				// pause for 100ms for all other modem types
				msleep(100);
			}

			// bring up WAN_RF_ENABLE
			set_wan_rf_enable(1);

			if (modem_type == MODEM_TYPE_AD_DTP || modem_type == MODEM_TYPE_UNKNOWN) {
				// (if modem is unknown, it could be a DTP, so must delay as well)
				ssleep(DTP_ON_DELAY_SEC);
			}
			break;

		default :
			log_err("req_err:request=%d:unknown power request\n", new_status);

			// (fall through)

		case WAN_OFF :
		case WAN_OFF_KILL :
			// bring down WAN_RF_ENABLE
			set_wan_rf_enable(0);

			if (new_status != WAN_OFF_KILL) {
				// wait the necessary deregistration interval
				msleep(modem_type == MODEM_TYPE_AD_DTP ? NETWORK_DEREG_TIME_OPT : NETWORK_DEREG_TIME);
			}

#ifdef USE_DTP_SLEEP_MODE
			if (new_status == WAN_OFF_KILL || modem_type != MODEM_TYPE_AD_DTP) {
#endif
				// bring down WAN_ON_OFF
				set_wan_on_off(0);
#ifdef USE_DTP_SLEEP_MODE
			}
#endif

			new_status = WAN_OFF;
			break;

	}

	wan_status = new_status;

	wan_set_power_status(wan_status);

	return 0;
}


static void
init_modem_type(
	int type)
{
	if (modem_type != type) {
		log_info("smt:type=%d:setting modem type\n", type);

		modem_type = type;
	}
}


int
wan_get_modem_type(
	void)
{
	return modem_type;
}

EXPORT_SYMBOL(wan_get_modem_type);


static int
proc_power_read(
	char *page,
	char **start,
	off_t off,
	int count,
	int *eof,
	void *data)
{
	*eof = 1;

	return sprintf(page, "%d\n", wan_status == WAN_ON ? 1 : 0);
}


static int
proc_power_write(
	struct file *file,
	const char __user *buf,
	unsigned long count,
	void *data)
{
	char lbuf[16];
	unsigned char op;

	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, 1)) {
		return -EFAULT;
	}

	op = lbuf[0];
	if (op >= '0' && op <= '9') {
		wan_status_t new_wan_status = (wan_status_t)(op - '0'), prev_wan_status = wan_status;

		switch (new_wan_status) {

			case WAN_OFF :
			case WAN_ON :
				// perform normal on/off power handling
				if ((new_wan_status == WAN_ON && prev_wan_status == WAN_OFF) ||
				    (new_wan_status == WAN_OFF && prev_wan_status == WAN_ON)) {
					set_wan_power(new_wan_status);
				}
				break;


			case WAN_OFF_KILL :
				set_wan_power(new_wan_status);
				break;

			default :
				log_err("req_err:request=%d:unknown power request\n", new_wan_status);
				break;

		}

	} else {
		log_err("req_err:request='%c':unknown power request\n", op);
	}

	return count;
}


static int
proc_enable_read(
	char *page,
	char **start,
	off_t off,
	int count,
	int *eof,
	void *data)
{
	*eof = 1;

	return sprintf(page, "%d\n", get_wan_rf_enable());
}


static int
proc_enable_write(
	struct file *file,
	const char __user *buf,
	unsigned long count,
	void *data)
{
	char lbuf[16];

	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, 1)) {
		return -EFAULT;
	}

	set_wan_rf_enable(lbuf[0] != '0');

	return count;
}


static int
proc_type_read(
	char *page,
	char **start,
	off_t off,
	int count,
	int *eof,
	void *data)
{
	*eof = 1;

	return sprintf(page, "%d\n", modem_type);
}


static int
proc_type_write(
	struct file *file,
	const char __user *buf,
	unsigned long count,
	void *data)
{
	char lbuf[16], ch;

	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, 1)) {
		return -EFAULT;
	}

	ch = lbuf[0];
	if (ch >= '0' && ch <= '9') {
		init_modem_type(ch - '0');

	} else {
		log_err("type_err:type=%c:invalid type\n", ch);

	}

	return count;
}


static int
proc_usb_read(
	char *page,
	char **start,
	off_t off,
	int count,
	int *eof,
	void *data)
{
	*eof = 1;

	return sprintf(page, "%d\n", get_wan_usb_enable());
}


static int
proc_usb_write(
	struct file *file,
	const char __user *buf,
	unsigned long count,
	void *data)
{
	char lbuf[16];

	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, 1)) {
		return -EFAULT;
	}

	set_wan_usb_enable(lbuf[0] != '0');

	return count;
}


static wan_usb_wake_callback_t usb_wake_callback = NULL;

void
wan_set_usb_wake_callback(
	wan_usb_wake_callback_t wake_fn)
{
	usb_wake_callback = wake_fn;
}
EXPORT_SYMBOL(wan_set_usb_wake_callback);


static void
wan_tph_notify(
	void)
{
	kobject_uevent(&wan_dev->kobj, KOBJ_CHANGE);
	log_info("tph::tph event occurred; notifying system of TPH\n");
}

#define NUM_ITER_TPH	2 /* 70 ms threshold = 2 * 35 ms */
#define ITER_DELAY_TPH	35
#define SENSE_ON2B_MASK	(1 << 4)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
#define HOST_WAKE_SENSE REG_INT_SENSE1
#else
#define HOST_WAKE_SENSE REG_INTERRUPT_SENSE_1
#endif

static void
wan_tph_event_handler(
	void)
{
	unsigned long tph_cur_seconds;
	int long_delta;

	tph_cur_seconds = CURRENT_TIME_SEC.tv_sec;

	long_delta = (tph_last_seconds <= tph_cur_seconds) ?
		(tph_cur_seconds - tph_last_seconds > WAKE_EVENT_INTERVAL) :
		1;

	tph_last_seconds = tph_cur_seconds;

	if (modem_type == MODEM_TYPE_AD_DTP) {
		int i;
		unsigned int sense;
		if (usb_wake_callback == NULL && long_delta) {
			wan_tph_notify();
			tph_last_type = TPH_TYPE_RINGSMS;
		} else {
			/* determine if long or short pulse */
			for (i = 0; i < NUM_ITER_TPH; i++) {
				pmic_read_reg(HOST_WAKE_SENSE, &sense, SENSE_ON2B_MASK);
				if (sense & SENSE_ON2B_MASK) {
					break;
				}
				msleep(ITER_DELAY_TPH);
			}

			if (i >= NUM_ITER_TPH) {
				/* SMS TPH: immediately following USB wake ok */
				if (long_delta || tph_last_type == TPH_TYPE_USBWAKE) {
					wan_tph_notify();
				}
				tph_last_type = TPH_TYPE_RINGSMS;
			} else {
				/* USB wake: multiples not allowed */
				if (long_delta) {
					(*usb_wake_callback)();
					log_info("uwake::usb wake sent to ehci-hcd\n");
				}
				tph_last_type = TPH_TYPE_USBWAKE;
			}
		}
	} else {
		// other modules may generate extraneous TPH events; filter out these extra events
		if (long_delta) {
			wan_tph_notify();
		}
		tph_last_type = TPH_TYPE_RINGSMS;
	}
}

static int
wan_init(
	void)
{
	extern void gpio_wan_init(void *);
	extern void gpio_wan_exit(void *);
	int ret;

	gpio_wan_init(wan_tph_event_handler);

	wan_major = register_chrdev(0, WAN_STRING_DEV, &mwan_ops);
	if (wan_major < 0) {
		ret = wan_major;
		log_err("dev_err:device=" WAN_STRING_DEV ",err=%d:could not register device\n", ret);
		goto exit1;
	}

	wan_class = class_create(THIS_MODULE, WAN_STRING_CLASS);
	if (IS_ERR(wan_class)) {
		ret = PTR_ERR(wan_class);
		log_err("class_err:err=%d:could not create class\n", ret);
		goto exit2;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
	wan_dev = device_create_drvdata(wan_class, NULL, MKDEV(wan_major, 0), NULL, WAN_STRING_DEV);
#else
	wan_dev = class_device_create(wan_class, NULL, MKDEV(wan_major, 0), NULL, WAN_STRING_DEV);
#endif
	if (IS_ERR(wan_dev)) {
		ret = PTR_ERR(wan_dev);
		log_err("dev_err:err=%d:could not create class device\n", ret);
		goto exit3;
	}

	wan_set_power_status(WAN_OFF);

	ret = 0;
	goto exit0;

exit3:
	class_destroy(wan_class);
	wan_class = NULL;

exit2:
	unregister_chrdev(wan_major, WAN_STRING_DEV);

exit1:
	gpio_wan_exit(wan_tph_event_handler);

exit0:
	return ret;
}


static void
wan_exit(
	void)
{
	extern void gpio_wan_exit(void *);

	wan_set_power_status(WAN_INVALID);

	gpio_wan_exit(wan_tph_event_handler);

	if (wan_dev != NULL) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
		device_destroy(wan_class, MKDEV(wan_major, 0));
#else
		class_device_destroy(wan_class, MKDEV(wan_major, 0));
#endif
		wan_dev = NULL;
		class_destroy(wan_class);
		unregister_chrdev(wan_major, WAN_STRING_DEV);
	}
}


static int __init
mwan_init(
	void)
{
	int ret = 0;

	log_info("init:mario WAN hardware driver " VERSION "\n");

	// create the "/proc/wan" parent directory
	proc_wan_parent = create_proc_entry(PROC_WAN, S_IFDIR | S_IRUGO | S_IXUGO, NULL);
	if (proc_wan_parent != NULL) {

		// create the "/proc/wan/power" entry
		proc_wan_power = create_proc_entry(PROC_WAN_POWER, S_IWUSR | S_IRUGO, proc_wan_parent);
		if (proc_wan_power != NULL) {
			proc_wan_power->data = NULL;
			proc_wan_power->read_proc = proc_power_read;
			proc_wan_power->write_proc = proc_power_write;
		}

		// create the "/proc/wan/enable" entry
		proc_wan_enable = create_proc_entry(PROC_WAN_ENABLE, S_IWUSR | S_IRUGO, proc_wan_parent);
		if (proc_wan_enable != NULL) {
			proc_wan_enable->data = NULL;
			proc_wan_enable->read_proc = proc_enable_read;
			proc_wan_enable->write_proc = proc_enable_write;
		}

		// create the "/proc/wan/type" entry
		proc_wan_type = create_proc_entry(PROC_WAN_TYPE, S_IWUSR | S_IRUGO, proc_wan_parent);
		if (proc_wan_type != NULL) {
			proc_wan_type->data = NULL;
			proc_wan_type->read_proc = proc_type_read;
			proc_wan_type->write_proc = proc_type_write;
		}

		// create the "/proc/wan/usb" entry
		proc_wan_usb = create_proc_entry(PROC_WAN_USB, S_IWUSR | S_IRUGO, proc_wan_parent);
		if (proc_wan_usb != NULL) {
			proc_wan_usb->data = NULL;
			proc_wan_usb->read_proc = proc_usb_read;
			proc_wan_usb->write_proc = proc_usb_write;
		}

	} else {
		ret = -1;

	}

	if (ret == 0) {
		wan_init();
	}

	return ret;
}


static void __exit
mwan_exit(
	void)
{
	if (proc_wan_parent != NULL) {
		remove_proc_entry(PROC_WAN_USB, proc_wan_parent);
		remove_proc_entry(PROC_WAN_TYPE, proc_wan_parent);
		remove_proc_entry(PROC_WAN_ENABLE, proc_wan_parent);
		remove_proc_entry(PROC_WAN_POWER, proc_wan_parent);
		remove_proc_entry(PROC_WAN, NULL);

		proc_wan_usb = proc_wan_type = proc_wan_enable = proc_wan_power = proc_wan_parent = NULL;
	}

	wan_exit();
}


module_init(mwan_init);
module_exit(mwan_exit);

MODULE_DESCRIPTION("Mario WAN hardware driver");
MODULE_AUTHOR("Lab126");
MODULE_LICENSE("GPL");
MODULE_VERSION(VERSION);

