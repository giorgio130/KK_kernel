/*
 * Copyright 2008-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2009 Amazon Technologies, Inc. All Rights Reserved.
 * Manish Lachwani (lachwani@lab126.com)
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file pmic/core/mc13892.c
 * @brief This file contains MC13892 specific PMIC code. This implementaion
 * may differ for each PMIC chip.
 *
 * @ingroup PMIC_CORE
 */

/*
 * Includes
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/pmic_external.h>
#include <linux/pmic_status.h>
#include <linux/pmic_light.h>
#include <linux/mfd/mc13892/core.h>

#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include "pmic.h"

/*
 * Defines
 */
#define MC13892_I2C_RETRY_TIMES 10
#define MXC_PMIC_FRAME_MASK		0x00FFFFFF
#define MXC_PMIC_MAX_REG_NUM		0x3F
#define MXC_PMIC_REG_NUM_SHIFT		0x19
#define MXC_PMIC_WRITE_BIT_SHIFT		31

static unsigned int events_enabled0;
static unsigned int events_enabled1;
static struct mxc_pmic pmic_drv_data;
#ifndef CONFIG_MXC_PMIC_I2C
struct i2c_client *mc13892_client;
#endif

#define BIT_CHG_CURR_LSH	3
#define BIT_CHG_CURR_WID	4
#define PMIC_ICHRG		0x6
#define VBUSVALIDS		0x08
#define CHGDETS         	0x40
#define IDFACTORYS		0x10
#define PMIC_ICHRG_OPEN 	0xf	/* Externally powered */
#define PMIC_ICHRG_DEFAULT	0x1	/* Default charging if not open or AC */

int pmic_i2c_24bit_read(struct i2c_client *client, unsigned int reg_num,
			unsigned int *value)
{
	unsigned char buf[3];
	int ret;
	int i;

	memset(buf, 0, 3);
	for (i = 0; i < MC13892_I2C_RETRY_TIMES; i++) {
		ret = i2c_smbus_read_i2c_block_data(client, reg_num, 3, buf);
		if (ret == 3)
			break;
		msleep(1);
	}

	if (ret == 3) {
		*value = buf[0] << 16 | buf[1] << 8 | buf[2];
		return ret;
	} else {
		pr_debug("24bit read error, ret = %d\n", ret);
		return -1;	/* return -1 on failure */
	}
}

int pmic_i2c_24bit_write(struct i2c_client *client,
			 unsigned int reg_num, unsigned int reg_val)
{
	char buf[3];
	int ret;
	int i;

	buf[0] = (reg_val >> 16) & 0xff;
	buf[1] = (reg_val >> 8) & 0xff;
	buf[2] = (reg_val) & 0xff;

	for (i = 0; i < MC13892_I2C_RETRY_TIMES; i++) {
		ret = i2c_smbus_write_i2c_block_data(client, reg_num, 3, buf);
		if (ret == 0)
			break;
		msleep(1);
	}

	return ret;
}

int pmic_read(int reg_num, unsigned int *reg_val)
{
	unsigned int frame = 0;
	int ret = 0;

	if (pmic_drv_data.spi != NULL) {
		if (reg_num > MXC_PMIC_MAX_REG_NUM)
			return PMIC_ERROR;

		frame |= reg_num << MXC_PMIC_REG_NUM_SHIFT;

		ret = spi_rw(pmic_drv_data.spi, (u8 *) &frame, 1);

		*reg_val = frame & MXC_PMIC_FRAME_MASK;
	} else {
		if (mc13892_client == NULL)
			return PMIC_ERROR;

		if (pmic_i2c_24bit_read(mc13892_client, reg_num, reg_val) == -1)
			return PMIC_ERROR;
	}

	return PMIC_SUCCESS;
}

int pmic_write(int reg_num, const unsigned int reg_val)
{
	unsigned int frame = 0;
	int ret = 0;

	if (pmic_drv_data.spi != NULL) {
		if (reg_num > MXC_PMIC_MAX_REG_NUM)
			return PMIC_ERROR;

		frame |= (1 << MXC_PMIC_WRITE_BIT_SHIFT);

		frame |= reg_num << MXC_PMIC_REG_NUM_SHIFT;

		frame |= reg_val & MXC_PMIC_FRAME_MASK;

		ret = spi_rw(pmic_drv_data.spi, (u8 *) &frame, 1);

		return ret;
	} else {
		if (mc13892_client == NULL)
			return PMIC_ERROR;

		return pmic_i2c_24bit_write(mc13892_client, reg_num, reg_val);
	}
}

void *pmic_alloc_data(struct device *dev)
{
	struct mc13892 *mc13892;

	mc13892 = kzalloc(sizeof(struct mc13892), GFP_KERNEL);
	if (mc13892 == NULL)
		return NULL;

	mc13892->dev = dev;

	return (void *)mc13892;
}

/*!
 * This function initializes the SPI device parameters for this PMIC.
 *
 * @param    spi	the SPI slave device(PMIC)
 *
 * @return   None
 */
int pmic_spi_setup(struct spi_device *spi)
{
	/* Setup the SPI slave i.e.PMIC */
	pmic_drv_data.spi = spi;

	spi->mode = SPI_MODE_2 | SPI_CS_HIGH;
	spi->bits_per_word = 32;

	return spi_setup(spi);
}

static void pmic_set_ichrg(unsigned short curr)
{
	unsigned int mask;
	unsigned int value;

	printk(KERN_INFO "Setting ichrg to %d\n", curr);

	/* Turn on CHRGLED */
	pmic_write_reg(REG_CHARGE, (1 << 18), (1 << 18));
	/* Turn on V & I programming */
	pmic_write_reg(REG_CHARGE, (1 << 23), (1 << 23));
	/* Turn off CHGAUTOB */
	pmic_write_reg(REG_CHARGE, (1 << 21), (1 << 21));
	/* Turn off TRICKLE CHARGE */
	pmic_write_reg(REG_CHARGE, (0 << 7), (1 << 7));

	/* Set the ichrg */
	value = BITFVAL(BIT_CHG_CURR, curr);
	mask = BITFMASK(BIT_CHG_CURR);
	pmic_write_reg(REG_CHARGE, value, mask);

	/* Restart charging */
	pmic_write_reg(REG_CHARGE, (1 << 20), (1 << 20));
}
		
#ifdef CONFIG_DEBUG_FS

/*-------------------------------------------------------------------------
	Debug filesystem
-------------------------------------------------------------------------*/
#include <linux/debugfs.h>
#include <linux/seq_file.h>

static struct dentry *debugfs_root;
static struct dentry *debugfs_state;

#define MC13892_MAX_REGS	63

extern pmic_version_t mxc_pmic_version;

static int mc13892_regs_show(struct seq_file *s, void *p)
{
	int t = 0, i = 0;
	int reg_value = 0;
	pmic_get_revision(&mxc_pmic_version);

	t += seq_printf(s, "MC13892 Version: %d\n\n", mxc_pmic_version.revision);

	for (i = 0; i <= MC13892_MAX_REGS; i++) {
		pmic_read_reg(i, &reg_value, 0xffffffff);
		t += seq_printf(s, "Reg# %d - 0x%x\n", i, reg_value);
	}

	return 0;
}

static int mc13892_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, mc13892_regs_show, inode->i_private);
}

static const struct file_operations mc13892_regs_fops = {
	.owner		= THIS_MODULE,
	.open		= mc13892_regs_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

static int mc13892_debugfs_init(void)
{
	struct dentry *root, *state;
	int ret = -1;
	
	root = debugfs_create_dir("MC13892", NULL);
	if (IS_ERR(root) || !root)
		goto err_root;

	state = debugfs_create_file("mc13892_registers", 0400, root, (void *)0,
			&mc13892_regs_fops);	

	if (!state)
		goto err_state;
	
	debugfs_root = root;
	debugfs_state = state;

	return 0;
err_state:
	debugfs_remove(root);
err_root:
	printk(KERN_ERR "mc13892_regs: debugfs is not available\n");
	return ret;
}

static void mc13892_cleanup_debugfs(void)
{
	debugfs_remove(debugfs_state);
	debugfs_remove(debugfs_root);
	debugfs_state = NULL;
	debugfs_root = NULL;
}

#endif

/*!
 * Check if a charger is connected or not
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

extern unsigned int usbdr_early_portsc1_read(void);

static void pmic_enable_green_led(void)
{
	mc13892_bklit_set_current(LIT_GREEN, 0x7);
	mc13892_bklit_set_ramp(LIT_GREEN, 0);
	mc13892_bklit_set_dutycycle(LIT_GREEN, 0x3f);
}

int luigi_button_green_led = 0;
EXPORT_SYMBOL(luigi_button_green_led);

int pmic_init_registers(void)
{
	int sense_0 = 0, idfacs = 0;

	CHECK_ERROR(pmic_write(REG_INT_MASK0, 0xFFFFFF));
	CHECK_ERROR(pmic_write(REG_INT_MASK0, 0xFFFFFF));
	CHECK_ERROR(pmic_write(REG_INT_STATUS0, 0xFFFFFF));
	CHECK_ERROR(pmic_write(REG_INT_STATUS1, 0xFFFFFF));
	/* disable auto charge */
	if (machine_is_mx51_3ds())
		CHECK_ERROR(pmic_write(REG_CHARGE, 0xB40003));

	/* Turn on support for VSD */
	CHECK_ERROR(pmic_write_reg(REG_MODE_1, 7 << 18, 7 << 18));

	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);
	if (sense_0 & IDFACTORYS)
		idfacs = 1;

	/*
	 * Set the ichrg depending on whether booting off the battery
	 * or not. In factory mode, the board is booted using USB and 
	 * there is no battery. In such a scenario, setting the ichrg to 
	 * 0x3 does not make sense. This is a case where the board is externally
	 * powered and ichrg should be 0xf
	 */
	if (idfacs) {
		pmic_set_ichrg(PMIC_ICHRG_OPEN);
	}
	else {
		int portsc1 = usbdr_early_portsc1_read();
		printk(KERN_INFO "usbdr_early_portsc1_read: %d\n", portsc1);

		if (pmic_connected_charger() && (portsc1 == 0xc00))
			pmic_set_ichrg(PMIC_ICHRG);	
		else {
			if (!pmic_connected_charger()) {
				luigi_button_green_led = 1;
				pmic_enable_green_led();
			}
			else 
				pmic_set_ichrg(PMIC_ICHRG_DEFAULT);
		}
	}

#ifdef CONFIG_DEBUG_FS
	mc13892_debugfs_init();
#endif
	return PMIC_SUCCESS;
}

void pmic_uninit_registers(void)
{
#ifdef CONFIG_DEBUG_FS
	mc13892_cleanup_debugfs();
#endif
}

extern int mxc_spi_suspended;
extern atomic_t mxc_device_suspended;

unsigned int pmic_get_active_events(unsigned int *active_events)
{
	unsigned int count = 0;
	unsigned int status0, status1;
	unsigned int cause0 = 0, cause1 = 0;
	int bit_set;

	while (mxc_spi_suspended)
		yield();

	pmic_read(REG_INT_STATUS0, &status0);
	pmic_read(REG_INT_STATUS1, &status1);
	pmic_write(REG_INT_STATUS0, status0);
	pmic_write(REG_INT_STATUS1, status1);
	status0 &= events_enabled0;
	status1 &= events_enabled1;

	/* If the device is suspended, log the wakeup cause */
	if (atomic_read(&mxc_device_suspended)) {	
		cause0 = status0;
		cause1 = status1;

		if (cause1 & 0x2) {
			printk(KERN_INFO "kernel: I pmic:wakeup:source=RTC:\n");
		}

		if (cause1 & 0x8) {
			printk(KERN_INFO "kernel: I pmic:wakeup:source=Power Button:\n");
		}

		if (cause1 & 0x10) {
			printk(KERN_INFO "kernel: I pmic:wakeup:source=TPH:\n");
		}

		if (cause0 & 0x40) {
			printk(KERN_INFO "kernel: I pmic:wakeup:source=Charger Plug:\n");
		}

		if (cause0 & 0x4000) {
			printk(KERN_INFO "kernel: I pmic:wakeup:source=Lobath:\n");
		}

		/* Reset the flag for the next suspend */
		atomic_set(&mxc_device_suspended, 0);
	}

	while (status0) {
		bit_set = ffs(status0) - 1;
		*(active_events + count) = bit_set;
		count++;
		status0 ^= (1 << bit_set);
	}
	while (status1) {
		bit_set = ffs(status1) - 1;
		*(active_events + count) = bit_set + 24;
		count++;
		status1 ^= (1 << bit_set);
	}

	return count;
}

#define EVENT_MASK_0			0x387fff
#define EVENT_MASK_1			0x1177fb

int pmic_event_unmask(type_event event)
{
	unsigned int event_mask = 0;
	unsigned int mask_reg = 0;
	unsigned int event_bit = 0;
	int ret;

	if (event < EVENT_1HZI) {
		mask_reg = REG_INT_MASK0;
		event_mask = EVENT_MASK_0;
		event_bit = (1 << event);
		events_enabled0 |= event_bit;
	} else {
		event -= 24;
		mask_reg = REG_INT_MASK1;
		event_mask = EVENT_MASK_1;
		event_bit = (1 << event);
		events_enabled1 |= event_bit;
	}

	if ((event_bit & event_mask) == 0) {
		pr_debug("Error: unmasking a reserved/unused event\n");
		return PMIC_ERROR;
	}

	ret = pmic_write_reg(mask_reg, 0, event_bit);

	pr_debug("Enable Event : %d\n", event);

	return ret;
}

int pmic_event_mask(type_event event)
{
	unsigned int event_mask = 0;
	unsigned int mask_reg = 0;
	unsigned int event_bit = 0;
	int ret;

	if (event < EVENT_1HZI) {
		mask_reg = REG_INT_MASK0;
		event_mask = EVENT_MASK_0;
		event_bit = (1 << event);
		events_enabled0 &= ~event_bit;
	} else {
		event -= 24;
		mask_reg = REG_INT_MASK1;
		event_mask = EVENT_MASK_1;
		event_bit = (1 << event);
		events_enabled1 &= ~event_bit;
	}

	if ((event_bit & event_mask) == 0) {
		pr_debug("Error: masking a reserved/unused event\n");
		return PMIC_ERROR;
	}

	ret = pmic_write_reg(mask_reg, event_bit, event_bit);

	pr_debug("Disable Event : %d\n", event);

	return ret;
}

/*!
 * This function returns the PMIC version in system.
 *
 * @param 	ver	pointer to the pmic_version_t structure
 *
 * @return       This function returns PMIC version.
 */
void pmic_get_revision(pmic_version_t *ver)
{
	int rev_id = 0;
	int rev1 = 0;
	int rev2 = 0;
	int finid = 0;
	int icid = 0;

	ver->id = PMIC_MC13892;
	pmic_read(REG_IDENTIFICATION, &rev_id);

	rev1 = (rev_id & 0x018) >> 3;
	rev2 = (rev_id & 0x007);
	icid = (rev_id & 0x01C0) >> 6;
	finid = (rev_id & 0x01E00) >> 9;

	ver->revision = ((rev1 * 10) + rev2);
	printk(KERN_INFO "mc13892 Rev %d.%d FinVer %x detected\n", rev1,
	       rev2, finid);

	pmic_write_reg(REG_MODE_1, 0 << 18, 7 << 18);
	mdelay(100);
	pmic_write_reg(REG_MODE_1, 7 << 18, 7 << 18);
	mdelay(10);
	pmic_write_reg(REG_SETTING_1, 7 << 6, 7 << 6);
}

void mc13892_power_off(void)
{
	unsigned int value;

	pmic_read_reg(REG_POWER_CTL0, &value, 0xffffff);

	value |= 0x000008;

	pmic_write_reg(REG_POWER_CTL0, value, 0xffffff);
}

/*!
 * This function checks one sensor of PMIC.
 *
 * @param	sensor_bits	structure of all sensor bits.
 *
 * @return	This function returns PMIC_SUCCESS on SUCCESS, error on FAILURE.
 */
PMIC_STATUS pmic_get_sensors(t_sensor_bits * sensor_bits)
{
	int sense_0 = 0;
	int sense_1 = 0;

	memset(sensor_bits, 0, sizeof(t_sensor_bits));

	pmic_read_reg(REG_INT_SENSE0, &sense_0, 0xffffff);
	pmic_read_reg(REG_INT_SENSE1, &sense_1, 0xffffff);

	sensor_bits->sense_vbusvs = (sense_0 & (1 << 3)) ? true : false;
	sensor_bits->sense_idfacs = (sense_0 & (1 << 4)) ? true : false;
	sensor_bits->sense_usbovs = (sense_0 & (1 << 5)) ? true : false;
	sensor_bits->sense_chgdets = (sense_0 & (1 << 6)) ? true : false;
	sensor_bits->sense_chgrevs = (sense_0 & (1 << 8)) ? true : false;
	sensor_bits->sense_chgrshorts = (sense_0 & (1 << 9)) ? true : false;
	sensor_bits->sense_cccvs = (sense_0 & (1 << 10)) ? true : false;
	sensor_bits->sense_chgcurrs = (sense_0 & (1 << 11)) ? true : false;
	sensor_bits->sense_bpons = (sense_0 & (1 << 12)) ? true : false;
	sensor_bits->sense_lobatls = (sense_0 & (1 << 13)) ? true : false;
	sensor_bits->sense_lobaths = (sense_0 & (1 << 14)) ? true : false;
	sensor_bits->sense_idfloats = (sense_0 & (1 << 19)) ? true : false;
	sensor_bits->sense_idgnds = (sense_0 & (1 << 20)) ? true : false;
	sensor_bits->sense_se1s = (sense_0 & (1 << 21)) ? true : false;

	sensor_bits->sense_pwron3s = (sense_1 & (1 << 2)) ? true : false;
	sensor_bits->sense_pwron1s = (sense_1 & (1 << 3)) ? true : false;
	sensor_bits->sense_pwron2s = (sense_1 & (1 << 4)) ? true : false;
	sensor_bits->sense_thwarnls = (sense_1 & (1 << 12)) ? true : false;
	sensor_bits->sense_thwarnhs = (sense_1 & (1 << 13)) ? true : false;
	sensor_bits->sense_clks = (sense_1 & (1 << 14)) ? true : false;
	sensor_bits->sense_lbps = (sense_1 & (1 << 15)) ? true : false;
	
	return PMIC_SUCCESS;
}
EXPORT_SYMBOL(pmic_get_sensors);
	
