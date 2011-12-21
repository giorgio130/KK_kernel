/*
 * Copyright 2008-2009 Amazon.com All Rights Reserved.
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

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/nodemask.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/ata.h>
#include <linux/pmic_external.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <config/regulator.h>
#include <linux/reboot.h>
#include <linux/miscdevice.h>
#include <linux/pmic_rtc.h>
#include <linux/time.h>

#include <asm/memory.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <asm/mach/keypad.h>
#include <asm/arch/memory.h>
#include <asm/arch/gpio.h>
#include <asm/io.h>
#include <asm/arch/mmc.h>
#include <asm/arch/boot_globals.h>
#include <asm/arch/pmic_power.h>

#include "board-mx35_3stack.h"
#include "crm_regs.h"
#include "iomux.h"

#if defined(CONFIG_PROC_BOARDID)
#include "boardid.h"
#endif

/*!
 * @file mach-mx35/mx35_luigi.c
 *
 * @brief This file contains the board specific initialization routines.
 *
 * @ingroup MSL_MX35
 */

#define VSD_LSH 		18
#define SW4_HALT_MHMODE_LSH	12
#define SW4_HALT_MHMODE_VALUE	0
#define SW4_HALT_MHMODE_MASK	1
#define VUSB_LSH		0	/* VUSBIN in Register #50 */
#define VUSBEN_LSH		3	/* VUSBEN in Register #50 */
#define WDIRESET		12	/* Bit 12 in register 15 */

extern int mxc_init_devices(void);
extern void arch_reset(char);

unsigned int mx35_3stack_board_io;

/* Define a miscdevice for sending uevent for fat fs */
#define FATFS_MISCDEV_PORT_MINOR	160	/* /dev/fatfsdev */

static struct miscdevice fatfsdev_misc_device = {
	FATFS_MISCDEV_PORT_MINOR,
	"fatfsdev",
	NULL,
};

static int __init mxc_fatfsdev_init(void)
{
	if (misc_register (&fatfsdev_misc_device)) {
		printk (KERN_WARNING "fatfsdev: Couldn't register device\n");
		return -EBUSY;
	}

	return 0;
}
module_init(mxc_fatfsdev_init);

static void __exit mxc_fatfsdev_remove(void)
{
	misc_deregister(&fatfsdev_misc_device);
}
module_exit(mxc_fatfsdev_remove);

void mxc_send_fatfs_event(void)
{
	kobject_uevent_atomic(&fatfsdev_misc_device.this_device->kobj, KOBJ_ONLINE);
}
EXPORT_SYMBOL(mxc_send_fatfs_event);

static void mxc_nop_release(struct device *dev)
{
	/* Nothing */
}

static struct mxc_lcd_platform_data lcd_data = {
	.io_reg = "LCD"
};

static struct platform_device lcd_dev = {
	.name = "lcd_claa",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = (void *)&lcd_data,
		},
};

static void mxc_init_lcd(void)
{
	platform_device_register(&lcd_dev);
}

#if defined(CONFIG_FB_MXC_SYNC_PANEL) || defined(CONFIG_FB_MXC_SYNC_PANEL_MODULE)
/* mxc lcd driver */
static struct platform_device mxc_fb_device = {
	.name = "mxc_sdc_fb",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.coherent_dma_mask = 0xFFFFFFFF,
		},
};

static void mxc_init_fb(void)
{
	(void)platform_device_register(&mxc_fb_device);
}
#else
static inline void mxc_init_fb(void)
{
}
#endif

#if defined(CONFIG_BACKLIGHT_MXC)
static struct platform_device mxcbl_devices[] = {
#if defined(CONFIG_BACKLIGHT_MXC_IPU) || defined(CONFIG_BACKLIGHT_MXC_IPU_MODULE)
	{
	 .name = "mxc_ipu_bl",
	 .id = 0,
	 .dev = {
		 .platform_data = (void *)3,	/* DISP # for this backlight */
		 },
	 }
#endif
};

static inline void mxc_init_bl(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(mxcbl_devices); i++) {
		platform_device_register(&mxcbl_devices[i]);
	}
}
#else
static inline void mxc_init_bl(void)
{
}
#endif

#if defined(CONFIG_SDIO_UNIFI_FS) || defined(CONFIG_SDIO_UNIFI_FS_MODULE)
static void mxc_unifi_hardreset(void)
{
	pmic_gpio_set_bit_val(MCU_GPIO_REG_RESET_1, 1, 0);
	msleep(100);
	pmic_gpio_set_bit_val(MCU_GPIO_REG_RESET_1, 1, 1);
}

static void mxc_unifi_enable(int en)
{
	if (en) {
		pmic_gpio_set_bit_val(MCU_GPIO_REG_GPIO_CONTROL_1, 5, 1);
		msleep(10);
	} else
		pmic_gpio_set_bit_val(MCU_GPIO_REG_GPIO_CONTROL_1, 5, 0);
}

static struct mxc_unifi_platform_data unifi_data = {
	.hardreset = mxc_unifi_hardreset,
	.enable = mxc_unifi_enable,
	.reg_gpo1 = "GPO2",
	.reg_gpo2 = "GPO3",
	.reg_1v5_ana_bb = "PWGT1",
	.reg_vdd_vpa = "VAUDIO",
	.reg_1v5_dd = "SW1",
	.host_id = 1,
};

struct mxc_unifi_platform_data *get_unifi_plat_data(void)
{
	return &unifi_data;
}
#else
struct mxc_unifi_platform_data *get_unifi_plat_data(void)
{
	return NULL;
}
#endif

EXPORT_SYMBOL(get_unifi_plat_data);

static struct i2c_board_info mxc_i2c_board_info[] __initdata = {
	{
	 .type = "mc9sdz60",
	 .addr = 0x69,
	 },
	{
	 .type = "max8660",
	 .addr = 0x34,
	 },
	{
	 .type = "Luigi_Battery",
	 .addr = 0x55,
	},
};

static struct spi_board_info mxc_spi_board_info[] __initdata = {
	{
	 .modalias = "pmic_spi",
	 .irq = IOMUX_TO_IRQ(MX35_PIN_ATA_IORDY),
	 .max_speed_hz = 4000000,	/* max spi SCK clock speed in HZ */
	 .bus_num = 1,
	 .chip_select = 0,
	 },
};

#if defined(CONFIG_FEC) || defined(CONFIG_FEC_MODULE)
unsigned int expio_intr_fec;

EXPORT_SYMBOL(expio_intr_fec);
#endif

static struct mxc_mmc_platform_data mmc_data = {
	.ocr_mask = MMC_VDD_31_32 | MMC_VDD_32_33,
	.min_clk = 150000,
	.max_clk = 52000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "sdhc_clk",
	.power_mmc = "VGEN2",
};

/*
 * SD Slot - MMC #3
 */
static struct mxc_mmc_platform_data mmc_data_3 = {
        .ocr_mask = MMC_VDD_31_32 | MMC_VDD_32_33,
        .min_clk = 400000,
        .max_clk = 52000000,
        .card_inserted_state = 1,
        .status = sdhc_get_card_det_status,
        .wp_status = sdhc_write_protect,
        .clock_mmc = "sdhc_clk",
};

/*
 * WiFi
 */
static struct mxc_mmc_platform_data mmc_data_2 = {
	.ocr_mask = MMC_VDD_31_32 | MMC_VDD_32_33,
	.min_clk = 400000,
	.max_clk = 25000000,
	.card_inserted_state = 1,
	.status = sdhc_get_card_det_status,
	.wp_status = sdhc_write_protect,
	.clock_mmc = "sdhc_clk",
	.power_mmc = "SW1",
};

#define MMC_SDHC1_DETECT_IRQ	65	/* Fake Value */
#define MMC_SDHC2_DETECT_IRQ	66	/* Fake value */

#ifndef CONFIG_FEC
#define MMC_SDHC3_DETECT_IRQ    IOMUX_TO_IRQ(MX35_PIN_FEC_TX_EN)
#else
#define MMC_SDHC3_DETECT_IRQ	-1	/* Some random number now that FEC is in use */
#endif

/*!
 * Resource definition for the SDHC1
 */
static struct resource mxcsdhc1_resources[] = {
	[0] = {
	       .start = MMC_SDHC1_BASE_ADDR,
	       .end = MMC_SDHC1_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MXC_INT_MMC_SDHC1,
	       .end = MXC_INT_MMC_SDHC1,
	       .flags = IORESOURCE_IRQ,
	       },
	[2] = {
	       .start = MMC_SDHC1_DETECT_IRQ,
	       .end = MMC_SDHC1_DETECT_IRQ,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Device Definition for MXC SDHC1 */
static struct platform_device mxcsdhc1_device = {
	.name = "mxsdhci",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mmc_data,
		},
	.num_resources = ARRAY_SIZE(mxcsdhc1_resources),
	.resource = mxcsdhc1_resources,
};

static struct resource mxcsdhc3_resources[] = {
        [0] = {
               .start = MMC_SDHC3_BASE_ADDR,
               .end = MMC_SDHC3_BASE_ADDR + SZ_4K - 1,
               .flags = IORESOURCE_MEM,
               },
        [1] = {
               .start = MXC_INT_MMC_SDHC3,
               .end = MXC_INT_MMC_SDHC3,
               .flags = IORESOURCE_IRQ,
               },
        [2] = {
               .start = MMC_SDHC3_DETECT_IRQ,
               .end = MMC_SDHC3_DETECT_IRQ,
               .flags = IORESOURCE_IRQ,
               },
};

/*! Device Definition for MXC SDHC3 */
static struct platform_device mxcsdhc3_device = {
        .name = "mxsdhci",
        .id = 2,
        .dev = {
                .release = mxc_nop_release,
                .platform_data = &mmc_data_3,
                },
        .num_resources = ARRAY_SIZE(mxcsdhc3_resources),
        .resource = mxcsdhc3_resources,
};

static struct resource mxcsdhc2_resources[] = {
	[0] = {
		.start = MMC_SDHC2_BASE_ADDR,
		.end = MMC_SDHC2_BASE_ADDR + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
		},
	[1] = {
		.start = MXC_INT_MMC_SDHC2,
		.end = MXC_INT_MMC_SDHC2,
		.flags = IORESOURCE_IRQ,
		},
	[2] = {
		.start = MMC_SDHC2_DETECT_IRQ,
		.end = MMC_SDHC2_DETECT_IRQ,
		.flags = IORESOURCE_IRQ,
		},
};

static struct platform_device mxcsdhc4_device = {
	.name = "mxsdhci",
	.id = 1,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mmc_data_2,
		},
	.num_resources = ARRAY_SIZE(mxcsdhc2_resources),
	.resource = mxcsdhc2_resources,
};

static inline void mxc_init_mmc(void)
{
	(void)platform_device_register(&mxcsdhc1_device);
	(void)platform_device_register(&mxcsdhc4_device);
	(void)platform_device_register(&mxcsdhc3_device);
}

/*!
 * pmic board initialization code
 */
static int __init mxc_init_pmic(void)
{
	/*
	 * Charger settings
	 * 
	 * Set PLIM to 800mW and set PLIMDIS to 0
	 */
	/* PLIM: bits 15 and 16 */
	CHECK_ERROR(pmic_write_reg(REG_CHARGE, (0x1 << 15), (0x3 << 15)));

	/* PLIMDIS: bit 17 */
	CHECK_ERROR(pmic_write_reg(REG_CHARGE, (0 << 17), (1 << 17)));

	/* Turn off the VUSBIN that is responsible for leakage */
	CHECK_ERROR(pmic_write_reg(REG_USB1, (0 << VUSB_LSH), (1 << VUSB_LSH)));

	/* Turn off VUSBEN */
	CHECK_ERROR(pmic_write_reg(REG_USB1, (0 << VUSBEN_LSH), (1 << VUSBEN_LSH)));

	return 0;
}

late_initcall(mxc_init_pmic);

/*!
 * Board specific fixup function. It is called by \b setup_arch() in
 * setup.c file very early on during kernel starts. It allows the user to
 * statically fill in the proper values for the passed-in parameters. None of
 * the parameters is used currently.
 *
 * @param  desc         pointer to \b struct \b machine_desc
 * @param  tags         pointer to \b struct \b tag
 * @param  cmdline      pointer to the command line
 * @param  mi           pointer to \b struct \b meminfo
 */
static void __init fixup_mxc_board(struct machine_desc *desc, struct tag *tags,
				   char **cmdline, struct meminfo *mi)
{
	mxc_cpu_init();

#ifdef CONFIG_DISCONTIGMEM
	do {
		int nid;
		mi->nr_banks = MXC_NUMNODES;
		for (nid = 0; nid < mi->nr_banks; nid++)
			SET_NODE(mi, nid);
	} while (0);
#endif
	mi->bank[0].start = PLFRM_MEM_BASE;
	mi->bank[0].size = PLFRM_MEM_SIZE;
	mi->nr_banks = 1;
}

#if defined(CONFIG_KEYBOARD_MXC) || defined(CONFIG_KEYBOARD_MXC_MODULE)

/*
 * Keypad keycodes for the keypads.
 * NOTE: for each keypad supported, add the same number of keys (64);
 *	 otherwise the mapping will NOT work!
 */
/*
 * IMPORTANT!  Change the following parameter when adding a new KB map
 * 		to the array below.
 */

#define NUMBER_OF_KB_MAPPINGS	2

static u16 keymapping[NUMBER_OF_KB_MAPPINGS][64] = {

/*
 * keymapping[0] = key mapping for Luigi3 boards
 * NOTE: 64 keys; even those unused
 * NOTE: The number keys are combined with the first
 *       row of letters (Q/1, W/2, etc).  We still need to define
 *       the number key codes in the matrix so that they are enabled
 *       for the input event driver.  Since we cannot define two key codes
 *       in one row/col entry, these 10 keycodes are defined in unused
 *       row/col entries.
 */
	{
		/* Row 0 */
		KEY_HOME, KEY_1, KEY_R, KEY_S, KEY_RESERVED, KEY_RESERVED,
		KEY_BACKSPACE, KEY_DOT, /* "123" Key */
		/* Row 1 */
		KEY_MENU, KEY_2, KEY_T, KEY_D, KEY_RESERVED, KEY_RESERVED,
		KEY_Z, KEY_RESERVED,
		/* Row 2 */
		KEY_BACK, KEY_3, KEY_Y, KEY_F, KEY_RESERVED, KEY_RESERVED,
		KEY_X, KEY_ENTER,
		/* Row 3 */
		KEY_PAGEUP /* Next-page left button */, KEY_4, KEY_U, KEY_G,
		KEY_5, KEY_RESERVED, KEY_C, KEY_LEFTSHIFT,
		/* Row 4 */
		KEY_F21 /* Next-page right button */, KEY_6, KEY_I, KEY_H,
		KEY_7, KEY_RESERVED, KEY_V, KEY_LEFTALT,
		/* Row 5 */
		KEY_F23 /* Prev-page left button */, KEY_Q, KEY_O, KEY_J,
		KEY_RESERVED, KEY_8, KEY_B, KEY_SPACE,
		/* Row 6 */
		KEY_PAGEDOWN /* Prev-page right button */, KEY_W, KEY_P, KEY_K,
		KEY_RESERVED, KEY_9, KEY_N, KEY_F20 /* Font Size */,
		/* Row 7 */
		KEY_0, KEY_E, KEY_A, KEY_L, KEY_RESERVED, KEY_RESERVED,
		KEY_M, KEY_RIGHTMETA /* SYM */,
	},

/*
 * keymapping[1] = key mapping for Shasta boards
 *
 */
	{
		/* Row 0 */
		KEY_MENU, KEY_1, KEY_R, KEY_S, KEY_RESERVED, KEY_RESERVED,
		KEY_BACKSPACE, KEY_DOT,
		/* Row 1 */
		KEY_HOME, KEY_2, KEY_T, KEY_D, KEY_RESERVED, KEY_RESERVED,
		KEY_Z, KEY_RESERVED,
		/* Row 2 */
		KEY_BACK, KEY_3, KEY_Y, KEY_F, KEY_RESERVED, KEY_RESERVED,
		KEY_X, KEY_ENTER,
		/* Row 3 */
		KEY_F21 /* Next-page right button */, KEY_4, KEY_U, KEY_G,
		KEY_5, KEY_RESERVED, KEY_C, KEY_LEFTSHIFT,
		/* Row 4 */
		KEY_PAGEUP /* Next-page left button */, KEY_6, KEY_I, KEY_H,
		KEY_7, KEY_RESERVED, KEY_V, KEY_LEFTALT,
		/* Row 5 */
		KEY_PAGEDOWN /* Prev-page right button */, KEY_Q, KEY_O, KEY_J,
		KEY_RESERVED, KEY_8, KEY_B, KEY_SPACE,
		/* Row 6 */
		KEY_F23 /* Prev-page left button */, KEY_W, KEY_P, KEY_K,
		KEY_RESERVED, KEY_9, KEY_N, KEY_F20 /* Font Size */,
		/* Row 7 */
		KEY_0, KEY_E, KEY_A, KEY_L, KEY_RESERVED, KEY_RESERVED,
		KEY_M, KEY_RIGHTMETA /* SYM */,
	},

/*
 * NOTE: If you add more keypad mappings, remember to increment
 *	NUMBER_OF_KB_MAPPINGS above
 */
};

static struct resource mxc_kpp_resources[] = {
	[0] = {
		.start = MXC_INT_KPP,
		.end = MXC_INT_KPP,
		.flags = IORESOURCE_IRQ,
	      }
};

static struct keypad_data evb_8_by_8_keypad = {
	.rowmax = 8,
	.colmax = 8,
	.irq = MXC_INT_KPP,
	.learning = 0,
	.delay = 2,
	.matrix = (u16 *)keymapping,
};

/* mxc keypad driver */
static struct platform_device mxc_keypad_device = {
	.name = "mxc_keypad",
	.id = 0,
	.num_resources = ARRAY_SIZE(mxc_kpp_resources),
	.resource = mxc_kpp_resources,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &evb_8_by_8_keypad,
		},
};

static void mxc_init_keypad(void)
{
	(void)platform_device_register(&mxc_keypad_device);
}
#else
static inline void mxc_init_keypad(void)
{

}
#endif

static void __init luigi_mxc_init_irq(void)
{
	reserve_bootmem(BOOT_GLOBALS_BASE, BOOT_GLOBALS_SIZE, BOOTMEM_DEFAULT);
	reserve_bootmem(OOPS_SAVE_BASE, OOPS_SAVE_SIZE, BOOTMEM_DEFAULT);
	
	mxc_init_irq();
}

extern void gpio_papyrus_reset(void);
extern void gpio_i2c_active(int i2c_num);
extern void gpio_chrgled_iomux(void);
extern void gpio_chrgled_active(int enable);
extern void wan_sysdev_ctrl_init(void);
extern void gpio_test_pm(void);

static int __init mx35_luigi_sysfs_init(void)
{
	int err = 0;

	wan_sysdev_ctrl_init();

	return err;
}
late_initcall(mx35_luigi_sysfs_init);

static void mx35_pmic_power_off(void);
static void mx35_configure_rtc(int atlas_3_1);
static void mx35_rtc_pmic_off(void);

/*
 * Power off for the MX35
 */
void mx35_power_off(void)
{
	unsigned int reg = 0;

	pmic_read_reg(REG_IDENTIFICATION, &reg, 0xffffff);

	/* Check if Atlas Rev 3.1 */
	if (reg & 0x1)
		mx35_configure_rtc(1);
	else
		mx35_configure_rtc(0);

	mx35_pmic_power_off();
}
EXPORT_SYMBOL(mx35_power_off);

extern void watchdog_pin_gpio(void);

static void mx35_configure_rtc(int atlas_3_1)
{
	struct timeval pmic_time;

	pmic_write_reg(REG_INT_STATUS1, 0, 0x2);
	pmic_write_reg(REG_INT_MASK1, 0, 0x2);
	pmic_rtc_get_time(&pmic_time);
	if (atlas_3_1)
		pmic_time.tv_sec += 2;
	else
		pmic_time.tv_sec += 10;

	pmic_rtc_set_time_alarm(&pmic_time);
}

#define PMIC_BUTTON_DEBOUNCE_VALUE	0x3
#define PMIC_BUTTON_DEBOUNCE_MASK	0x3

static void mxc_configure_pb_debounce(void)
{
	/* Configure debounce time for power button 1 */
	pmic_write_reg(REG_POWER_CTL2, (PMIC_BUTTON_DEBOUNCE_VALUE << 4), (PMIC_BUTTON_DEBOUNCE_MASK << 4));

	/* Configure debounce time for power button 2 */
	pmic_write_reg(REG_POWER_CTL2, (PMIC_BUTTON_DEBOUNCE_VALUE << 6), (PMIC_BUTTON_DEBOUNCE_MASK << 6));

	/* Configure debounce time for power button 3 */
	pmic_write_reg(REG_POWER_CTL2, (PMIC_BUTTON_DEBOUNCE_VALUE << 8), (PMIC_BUTTON_DEBOUNCE_MASK << 8));

	pmic_write_reg(REG_POWER_CTL2, (0 << 3), (1 << 3));
	pmic_write_reg(REG_POWER_CTL2, (0 << 1), (1 << 1));
	pmic_write_reg(REG_POWER_CTL2, (0 << 2), (1 << 2));
	pmic_write_reg(23, (1 << 24), (1 << 24));
}

/*!
 * The power off is different for the Atlas Rev 3.1
 */
static void mx35_pmic_power_off(void)
{
	unsigned int reg = 0;
	unsigned long flags;

	mxc_configure_pb_debounce();

	pmic_read_reg(REG_IDENTIFICATION, &reg, 0xffffffff);

	/* Check if Atlas Rev 3.1 */
	if (reg & 0x1) {
		/* Set VUSBIN */
		pmic_write_reg(REG_USB1, (0 << VUSB_LSH), (1 << VUSB_LSH));

		/* Clear WDIRESET */
		pmic_write_reg(REG_POWER_CTL2, (0 << WDIRESET), (1 << WDIRESET));

		pmic_write_reg(REG_MEM_A, (1 << 2), (1 << 2));

		/* Configure WATCHDOG PIN as GPIO and pull it low */
		watchdog_pin_gpio();
	}
	else  {
		/* Turn off VSD */
		pmic_write_reg(REG_MODE_1, (0 << VSD_LSH), (1 << VSD_LSH));

		/* Turn off SW4 in halt/poweroff */
		pmic_write_reg(REG_SW_5, (SW4_HALT_MHMODE_VALUE << SW4_HALT_MHMODE_LSH),
				(SW4_HALT_MHMODE_MASK << SW4_HALT_MHMODE_LSH));

		/* Clear out bit #2 in MEMA */
		pmic_write_reg(REG_MEM_A, (0 << 2), (1 << 2));

		/* Turn on bit #2 */
		pmic_write_reg(REG_MEM_A, (1 << 2), (1 << 2));

		/*
		 * This puts Atlas in USEROFF power cut mode
		 */
		pmic_power_off(); 
	}

	/* Spin */
	local_irq_save(flags);

	while (1) {
		/* do nothing */
	}
}

void mxc_kernel_uptime(void)
{
	struct timespec uptime;

	/* Record the kernel boot time, now that this is the last thing */
	do_posix_clock_monotonic_gettime(&uptime);
	monotonic_to_bootbased(&uptime);

	printk("%lu.%02lu seconds:\n", (unsigned long) uptime.tv_sec,
			(uptime.tv_nsec / (NSEC_PER_SEC / 100)));
}
EXPORT_SYMBOL(mxc_kernel_uptime);

static void mx35_check_wdog_rst(void)
{
	printk(KERN_INFO "kernel: W perf:kernel:kernel_loaded=");
	mxc_kernel_uptime();

	return;
}

/*!
 * Check for the OOPS on bootup
 */
void *oops_start;
static int __init mxc_check_oops(void)
{
	char oops_buffer[OOPS_SAVE_SIZE];
	oops_start = __arm_ioremap(OOPS_SAVE_BASE, OOPS_SAVE_SIZE, 0);

	memcpy((void *)oops_buffer, oops_start, OOPS_SAVE_SIZE);

	if ( (oops_buffer[0] == 'O') && (oops_buffer[1] == 'O') &&
		(oops_buffer[2] == 'P') && (oops_buffer[3] == 'S') ) {
			printk("Kernel Crash Start\n");
			printk("%s", oops_buffer);
			printk("\nKernel Crash End\n");
	}
	else {
		mx35_check_wdog_rst();
	}
	
	memset(oops_start, 0, OOPS_SAVE_SIZE);
	return 0;
}
late_initcall(mxc_check_oops);

extern void watchdog_reset_config(void);
extern void doze_disable(void);

/*!
 * This function clears out the RTC specific registers and masks out the
 * RTC alarm interrupt. When a device is halted, it is not expected to 
 * wake on an RTC expiry. Hence, these interrupt sources have been cleared.
 * Also, the RTC alarm time and day registers are also cleared.
 */
static void mx35_rtc_pmic_off(void)
{
	unsigned int reg = 0;

	doze_disable();

        reg = __raw_readl(MXC_CCM_CGR0);
        reg |= (MXC_CCM_CGR0_ESDHC1_MASK | MXC_CCM_CGR0_ESDHC2_MASK |
                MXC_CCM_CGR0_ESDHC3_MASK | MXC_CCM_CGR0_CSPI1_MASK |
                MXC_CCM_CGR0_CSPI2_MASK);
        __raw_writel(reg, MXC_CCM_CGR0);

	/* Clear out the RTC time interrupt */
	pmic_write_reg(REG_INT_STATUS1, 0, 0x2);

	/* Mask the RTC alarm interrupt */
	pmic_write_reg(REG_INT_MASK1, 0x3, 0x3);

	/* Zero out the RTC alarm day */
	pmic_write_reg(REG_RTC_ALARM, 0x0, 0xffffffff);

	/* Zero out the RTC alarm time */
	pmic_write_reg(REG_RTC_DAY_ALARM, 0x0, 0xffffffff);

	/* Now invoke power_off */
	mx35_pmic_power_off();
}

/*!
 * Board specific initialization.
 */
static void __init mxc_board_init(void)
{
	pm_power_off = mx35_rtc_pmic_off;

	mxc_cpu_common_init();
#if defined(CONFIG_PROC_BOARDID)
	mx35_init_boardid();
#endif
	mxc_clocks_init();
	early_console_setup(saved_command_line);
	mxc_gpio_init();
	mxc_init_devices();
	mx35_luigi_gpio_init();
	mxc_init_keypad();
	mxc_init_lcd();

	watchdog_reset_config();

	i2c_register_board_info(0, mxc_i2c_board_info,
				ARRAY_SIZE(mxc_i2c_board_info));

	spi_register_board_info(mxc_spi_board_info,
				ARRAY_SIZE(mxc_spi_board_info));
	mxc_init_mmc();
	gpio_i2c_active(0);
	gpio_i2c_active(1);

	/* Turn on support for the GPIO that controls the charger led */
	gpio_chrgled_iomux();
	gpio_chrgled_active(1);

	/* Reset papyrus */
	gpio_papyrus_reset();

	/* Initialize the PM test point */
	gpio_test_pm();
}

#define PLL_PCTL_REG(brmo, pd, mfd, mfi, mfn)		\
		(((brmo) << 31) + (((pd) - 1) << 26) + (((mfd) - 1) << 16) + \
		((mfi)  << 10) + mfn)

/* For 24MHz input clock */
#define PLL_665MHZ		PLL_PCTL_REG(1, 1, 48, 13, 41)
#define PLL_532MHZ		PLL_PCTL_REG(0, 0, 2, 2, 10)
#define PLL_399MHZ		PLL_PCTL_REG(0, 1, 16, 8, 5)

/* working point(wp): 0,1 - 133MHz; 2,3 - 266MHz; 4,5 - 399MHz;*/
/* auto input clock table */
static struct cpu_wp cpu_wp_auto[] = {
	{
	 .pll_reg = PLL_399MHZ,
	 .pll_rate = 399000000,
	 .cpu_rate = 133000000,
	 .pdr0_reg = (0x2 << MXC_CCM_PDR0_AUTO_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_399MHZ,
	 .pll_rate = 399000000,
	 .cpu_rate = 133000000,
	 .pdr0_reg = (0x6 << MXC_CCM_PDR0_AUTO_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_399MHZ,
	 .pll_rate = 399000000,
	 .cpu_rate = 266000000,
	 .pdr0_reg = (0x1 << MXC_CCM_PDR0_AUTO_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_399MHZ,
	 .pll_rate = 399000000,
	 .cpu_rate = 266000000,
	 .pdr0_reg = (0x5 << MXC_CCM_PDR0_AUTO_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_399MHZ,
	 .pll_rate = 399000000,
	 .cpu_rate = 399000000,
	 .pdr0_reg = (0x0 << MXC_CCM_PDR0_AUTO_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_399MHZ,
	 .pll_rate = 399000000,
	 .cpu_rate = 399000000,
	 .pdr0_reg = (0x6 << MXC_CCM_PDR0_AUTO_MUX_DIV_OFFSET),},
};

/* consumer input clock table */
static struct cpu_wp cpu_wp_con[] = {
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 512000000,
	 .cpu_rate = 128000000,
	 .pdr0_reg = (0x6 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 512000000,
	 .cpu_rate = 128000000,
	 .pdr0_reg = (0xE << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 512000000,
	 .cpu_rate = 256000000,
	 .pdr0_reg = (0x2 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 512000000,
	 .cpu_rate = 256000000,
	 .pdr0_reg = (0xA << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 532000000,
	 .cpu_rate = 399000000,
	 .pdr0_reg = (0x1 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 532000000,
	 .cpu_rate = 399000000,
	 .pdr0_reg = (0x9 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 512000000,
	 .cpu_rate = 512000000,
	 .pdr0_reg = (0x0 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_532MHZ,
	 .pll_rate = 512000000,
	 .cpu_rate = 512000000,
	 .pdr0_reg = (0x8 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
	{
	 .pll_reg = PLL_665MHZ,
	 .pll_rate = 665000000,
	 .cpu_rate = 665000000,
	 .pdr0_reg = (0x7 << MXC_CCM_PDR0_CON_MUX_DIV_OFFSET),},
};

struct cpu_wp *get_cpu_wp(int *wp)
{
	if (cpu_is_mx35_rev(CHIP_REV_2_0) >= 1) {
		*wp = 9;
		return cpu_wp_con;
	} else {
		if (__raw_readl(MXC_CCM_PDR0) & MXC_CCM_PDR0_AUTO_CON) {
			*wp = 9;
			return cpu_wp_con;
		} else {
			*wp = 6;
			return cpu_wp_auto;
		}
	}
}

/*
 * The following uses standard kernel macros define in arch.h in order to
 * initialize __mach_desc_MX35_LUIGI data structure.
 */
/* *INDENT-OFF* */
MACHINE_START(MX35_LUIGI, "Amazon MX35 Luigi Board")
	/* Maintainer: Manish Lachwani (lachwani@lab126.com) */
	.phys_io = AIPS1_BASE_ADDR,
	.io_pg_offst = ((AIPS1_BASE_ADDR_VIRT) >> 18) & 0xfffc,
	.boot_params = PHYS_OFFSET + 0x100,
	.fixup = fixup_mxc_board,
	.map_io = mxc_map_io,
	.init_irq = luigi_mxc_init_irq,
	.init_machine = mxc_board_init,
	.timer = &mxc_timer,
MACHINE_END
