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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/spi/spi.h>

#include <asm/hardware.h>
#include <asm/arch/mmc.h>
#include <asm/arch/spba.h>
#include <asm/arch/sdma.h>

#include "iomux.h"
#include "sdma_script_code.h"
#include "sdma_script_code_v2.h"
#include "board-mx35_3stack.h"

void mxc_sdma_get_script_info(sdma_script_start_addrs * sdma_script_addr)
{
	if (cpu_is_mx35_rev(CHIP_REV_2_0) < 1) {
		sdma_script_addr->mxc_sdma_ap_2_ap_addr = ap_2_ap_ADDR;
		sdma_script_addr->mxc_sdma_ap_2_bp_addr = -1;
		sdma_script_addr->mxc_sdma_bp_2_ap_addr = -1;
		sdma_script_addr->mxc_sdma_loopback_on_dsp_side_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_interrupt_only_addr = -1;

		sdma_script_addr->mxc_sdma_firi_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_firi_2_mcu_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_firi_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_firi_addr = -1;

		sdma_script_addr->mxc_sdma_uart_2_per_addr = uart_2_per_ADDR;
		sdma_script_addr->mxc_sdma_uart_2_mcu_addr = uart_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_per_2_app_addr = per_2_app_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_app_addr = mcu_2_app_ADDR;

		sdma_script_addr->mxc_sdma_per_2_per_addr = p_2_p_ADDR;

		sdma_script_addr->mxc_sdma_uartsh_2_per_addr =
		    uartsh_2_per_ADDR;
		sdma_script_addr->mxc_sdma_uartsh_2_mcu_addr =
		    uartsh_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_per_2_shp_addr = per_2_shp_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_shp_addr = mcu_2_shp_ADDR;

		sdma_script_addr->mxc_sdma_ata_2_mcu_addr = ata_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_ata_addr = mcu_2_ata_ADDR;

		sdma_script_addr->mxc_sdma_app_2_per_addr = app_2_per_ADDR;
		sdma_script_addr->mxc_sdma_app_2_mcu_addr = app_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_shp_2_per_addr = shp_2_per_ADDR;
		sdma_script_addr->mxc_sdma_shp_2_mcu_addr = shp_2_mcu_ADDR;

		sdma_script_addr->mxc_sdma_mshc_2_mcu_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_mshc_addr = -1;

		sdma_script_addr->mxc_sdma_spdif_2_mcu_addr = spdif_2_mcu_ADDR;
		sdma_script_addr->mxc_sdma_mcu_2_spdif_addr = mcu_2_spdif_ADDR;

		sdma_script_addr->mxc_sdma_asrc_2_mcu_addr = asrc__mcu_ADDR;

		sdma_script_addr->mxc_sdma_dptc_dvfs_addr = -1;
		sdma_script_addr->mxc_sdma_ext_mem_2_ipu_addr =
		    ext_mem__ipu_ram_ADDR;
		sdma_script_addr->mxc_sdma_descrambler_addr = -1;

		sdma_script_addr->mxc_sdma_start_addr =
		    (unsigned short *)sdma_code;
		sdma_script_addr->mxc_sdma_ram_code_size = RAM_CODE_SIZE;
		sdma_script_addr->mxc_sdma_ram_code_start_addr =
		    RAM_CODE_START_ADDR;
	} else {
		sdma_script_addr->mxc_sdma_ap_2_ap_addr = ap_2_ap_ADDR_V2;
		sdma_script_addr->mxc_sdma_ap_2_bp_addr = -1;
		sdma_script_addr->mxc_sdma_bp_2_ap_addr = -1;
		sdma_script_addr->mxc_sdma_loopback_on_dsp_side_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_interrupt_only_addr = -1;

		sdma_script_addr->mxc_sdma_firi_2_per_addr = -1;
		sdma_script_addr->mxc_sdma_firi_2_mcu_addr = -1;
		sdma_script_addr->mxc_sdma_per_2_firi_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_firi_addr = -1;

		sdma_script_addr->mxc_sdma_uart_2_per_addr = uart_2_per_ADDR_V2;
		sdma_script_addr->mxc_sdma_uart_2_mcu_addr = uart_2_mcu_ADDR_V2;
		sdma_script_addr->mxc_sdma_per_2_app_addr = per_2_app_ADDR_V2;
		sdma_script_addr->mxc_sdma_mcu_2_app_addr = mcu_2_app_ADDR_V2;

		sdma_script_addr->mxc_sdma_per_2_per_addr = p_2_p_ADDR_V2;

		sdma_script_addr->mxc_sdma_uartsh_2_per_addr =
		    uartsh_2_per_ADDR_V2;
		sdma_script_addr->mxc_sdma_uartsh_2_mcu_addr =
		    uartsh_2_mcu_ADDR_V2;
		sdma_script_addr->mxc_sdma_per_2_shp_addr = per_2_shp_ADDR_V2;
		sdma_script_addr->mxc_sdma_mcu_2_shp_addr = mcu_2_shp_ADDR_V2;

		sdma_script_addr->mxc_sdma_ata_2_mcu_addr = ata_2_mcu_ADDR_V2;
		sdma_script_addr->mxc_sdma_mcu_2_ata_addr = mcu_2_ata_ADDR_V2;

		sdma_script_addr->mxc_sdma_app_2_per_addr = app_2_per_ADDR_V2;
		sdma_script_addr->mxc_sdma_app_2_mcu_addr = app_2_mcu_ADDR_V2;
		sdma_script_addr->mxc_sdma_shp_2_per_addr = shp_2_per_ADDR_V2;
		sdma_script_addr->mxc_sdma_shp_2_mcu_addr = shp_2_mcu_ADDR_V2;

		sdma_script_addr->mxc_sdma_mshc_2_mcu_addr = -1;
		sdma_script_addr->mxc_sdma_mcu_2_mshc_addr = -1;

		sdma_script_addr->mxc_sdma_spdif_2_mcu_addr =
		    spdif_2_mcu_ADDR_V2;
		sdma_script_addr->mxc_sdma_mcu_2_spdif_addr =
		    mcu_2_spdif_ADDR_V2;

		sdma_script_addr->mxc_sdma_asrc_2_mcu_addr = asrc__mcu_ADDR_V2;

		sdma_script_addr->mxc_sdma_dptc_dvfs_addr = -1;
		sdma_script_addr->mxc_sdma_ext_mem_2_ipu_addr =
		    ext_mem__ipu_ram_ADDR_V2;
		sdma_script_addr->mxc_sdma_descrambler_addr = -1;

		sdma_script_addr->mxc_sdma_start_addr =
		    (unsigned short *)sdma_code_v2;
		sdma_script_addr->mxc_sdma_ram_code_size = RAM_CODE_SIZE;
		sdma_script_addr->mxc_sdma_ram_code_start_addr =
		    RAM_CODE_START_ADDR_V2;
	}
}

static void mxc_nop_release(struct device *dev)
{
	/* Nothing */
}

#if defined(CONFIG_RTC_MXC) || defined(CONFIG_RTC_MXC_MODULE)
static struct resource rtc_resources[] = {
	{
	 .start = RTC_BASE_ADDR,
	 .end = RTC_BASE_ADDR + 0x30,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MXC_INT_RTC,
	 .flags = IORESOURCE_IRQ,
	 },
};
static struct platform_device mxc_rtc_device = {
	.name = "mxc_rtc",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(rtc_resources),
	.resource = rtc_resources,
};
static void mxc_init_rtc(void)
{
	(void)platform_device_register(&mxc_rtc_device);
}
#else
static inline void mxc_init_rtc(void)
{
}
#endif

#if defined(CONFIG_MXC_MC9SDZ60_RTC) || defined(CONFIG_MXC_MC9SDZ60_RTC_MODULE)
static struct resource pmic_rtc_resources[] = {
	{
	 .start = MXC_PSEUDO_IRQ_RTC,
	 .flags = IORESOURCE_IRQ,
	 },
};
static struct platform_device pmic_rtc_device = {
	.name = "pmic_rtc",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(pmic_rtc_resources),
	.resource = pmic_rtc_resources,
};
static void pmic_init_rtc(void)
{
	platform_device_register(&pmic_rtc_device);
}
#else
static void pmic_init_rtc(void)
{
}
#endif

#if defined(CONFIG_MXC_WATCHDOG) || defined(CONFIG_MXC_WATCHDOG_MODULE)
static struct resource wdt_resources[] = {
	{
	 .start = WDOG1_BASE_ADDR,
	 .end = WDOG1_BASE_ADDR + 0x30,
	 .flags = IORESOURCE_MEM,
	 },
};

static struct platform_device mxc_wdt_device = {
	.name = "mxc_wdt",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(wdt_resources),
	.resource = wdt_resources,
};

static void mxc_init_wdt(void)
{
	(void)platform_device_register(&mxc_wdt_device);
}
#else
static inline void mxc_init_wdt(void)
{
}
#endif

#if defined(CONFIG_MXC_IPU) || defined(CONFIG_MXC_IPU_MODULE)
static struct mxc_ipu_config mxc_ipu_data = {
	.rev = 2,
};

static struct resource ipu_resources[] = {
	{
	 .start = IPU_CTRL_BASE_ADDR,
	 .end = IPU_CTRL_BASE_ADDR + SZ_4K,
	 .flags = IORESOURCE_MEM,
	 },
	{
	 .start = MXC_INT_IPU_SYN,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 .start = MXC_INT_IPU_ERR,
	 .flags = IORESOURCE_IRQ,
	 },
};

static struct platform_device mxc_ipu_device = {
	.name = "mxc_ipu",
	.id = -1,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxc_ipu_data,
		},
	.num_resources = ARRAY_SIZE(ipu_resources),
	.resource = ipu_resources,
};

static void mxc_init_ipu(void)
{
	platform_device_register(&mxc_ipu_device);
}
#else
static inline void mxc_init_ipu(void)
{
}
#endif

/* SPI controller and device data */
#if defined(CONFIG_SPI_MXC) || defined(CONFIG_SPI_MXC_MODULE)

#ifdef CONFIG_SPI_MXC_SELECT1
/*!
 * Resource definition for the CSPI1
 */
static struct resource mxcspi1_resources[] = {
	[0] = {
	       .start = CSPI1_BASE_ADDR,
	       .end = CSPI1_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MXC_INT_CSPI1,
	       .end = MXC_INT_CSPI1,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC CSPI1 */
static struct mxc_spi_master mxcspi1_data = {
	.maxchipselect = 4,
	.spi_version = 7,
};

/*! Device Definition for MXC CSPI1 */
static struct platform_device mxcspi1_device = {
	.name = "mxc_spi",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxcspi1_data,
		},
	.num_resources = ARRAY_SIZE(mxcspi1_resources),
	.resource = mxcspi1_resources,
};

#endif				/* CONFIG_SPI_MXC_SELECT1 */

#ifdef CONFIG_SPI_MXC_SELECT2
/*!
 * Resource definition for the CSPI2
 */
static struct resource mxcspi2_resources[] = {
	[0] = {
	       .start = CSPI2_BASE_ADDR,
	       .end = CSPI2_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MXC_INT_CSPI2,
	       .end = MXC_INT_CSPI2,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC CSPI2 */
static struct mxc_spi_master mxcspi2_data = {
	.maxchipselect = 4,
	.spi_version = 7,
};

/*! Device Definition for MXC CSPI2 */
static struct platform_device mxcspi2_device = {
	.name = "mxc_spi",
	.id = 1,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxcspi2_data,
		},
	.num_resources = ARRAY_SIZE(mxcspi2_resources),
	.resource = mxcspi2_resources,
};
#endif				/* CONFIG_SPI_MXC_SELECT2 */

static inline void mxc_init_spi(void)
{
	/* SPBA configuration for CSPI2 - MCU is set */
	spba_take_ownership(SPBA_CSPI2, SPBA_MASTER_A);
#ifdef CONFIG_SPI_MXC_SELECT1
	if (platform_device_register(&mxcspi1_device) < 0)
		printk(KERN_ERR "Error: Registering the SPI Controller_1\n");
#endif				/* CONFIG_SPI_MXC_SELECT1 */
#ifdef CONFIG_SPI_MXC_SELECT2
	if (platform_device_register(&mxcspi2_device) < 0)
		printk(KERN_ERR "Error: Registering the SPI Controller_2\n");
#endif				/* CONFIG_SPI_MXC_SELECT2 */
}
#else
static inline void mxc_init_spi(void)
{
}
#endif

/* I2C controller and device data */
#if defined(CONFIG_I2C_MXC) || defined(CONFIG_I2C_MXC_MODULE)

#ifdef CONFIG_I2C_MXC_SELECT1
/*!
 * Resource definition for the I2C1
 */
static struct resource mxci2c1_resources[] = {
	[0] = {
	       .start = I2C_BASE_ADDR,
	       .end = I2C_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MXC_INT_I2C,
	       .end = MXC_INT_I2C,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC I2C */
static struct mxc_i2c_platform_data mxci2c1_data = {
#ifdef CONFIG_I2C_MXC_FAST_MODE_BUS_0
	.i2c_clk = 400000,
#else
	.i2c_clk = 100000,
#endif
};
#endif

/*!
 * Resource definition for the I2C2
 */
static struct resource mxci2c2_resources[] = {
	[0] = {
	       .start = I2C2_BASE_ADDR,
	       .end = I2C2_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MXC_INT_I2C2,
	       .end = MXC_INT_I2C2,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC I2C */
static struct mxc_i2c_platform_data mxci2c2_data = {
#ifdef CONFIG_I2C_MXC_FAST_MODE_BUS_1
	.i2c_clk = 400000,
#else
	.i2c_clk = 100000,
#endif
};

#ifdef CONFIG_I2C_MXC_SELECT3
/*!
 * Resource definition for the I2C3
 */
static struct resource mxci2c3_resources[] = {
	[0] = {
	       .start = I2C3_BASE_ADDR,
	       .end = I2C3_BASE_ADDR + SZ_4K - 1,
	       .flags = IORESOURCE_MEM,
	       },
	[1] = {
	       .start = MXC_INT_I2C3,
	       .end = MXC_INT_I2C3,
	       .flags = IORESOURCE_IRQ,
	       },
};

/*! Platform Data for MXC I2C */
static struct mxc_i2c_platform_data mxci2c3_data = {
#ifdef CONFIG_I2C_MXC_FAST_MODE_BUS_2
	.i2c_clk = 400000,
#else
	.i2c_clk = 100000,
#endif
};
#endif

/*! Device Definition for MXC I2C1 */
static struct platform_device mxci2c_devices[] = {
#ifdef CONFIG_I2C_MXC_SELECT1
	{
	 .name = "mxc_i2c",
	 .id = 0,
	 .dev = {
		 .release = mxc_nop_release,
		 .platform_data = &mxci2c1_data,
		 },
	 .num_resources = ARRAY_SIZE(mxci2c1_resources),
	 .resource = mxci2c1_resources,},
#endif
	{
	 .name = "mxc_i2c",
	 .id = 1,
	 .dev = {
		 .release = mxc_nop_release,
		 .platform_data = &mxci2c2_data,
		 },
	 .num_resources = ARRAY_SIZE(mxci2c2_resources),
	 .resource = mxci2c2_resources,},
#ifdef CONFIG_I2C_MXC_SELECT3
	{
	 .name = "mxc_i2c",
	 .id = 2,
	 .dev = {
		 .release = mxc_nop_release,
		 .platform_data = &mxci2c3_data,
		 },
	 .num_resources = ARRAY_SIZE(mxci2c3_resources),
	 .resource = mxci2c3_resources,},
#endif
};

static inline void mxc_init_i2c(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mxci2c_devices); i++) {
		if (platform_device_register(&mxci2c_devices[i]) < 0)
			dev_err(&mxci2c_devices[i].dev,
				"Unable to register I2C device\n");
	}
}
#else
static inline void mxc_init_i2c(void)
{
}
#endif

struct mxc_gpio_port mxc_gpio_ports[GPIO_PORT_NUM] = {
	{
	 .num = 0,
	 .base = IO_ADDRESS(GPIO1_BASE_ADDR),
	 .irq = MXC_INT_GPIO1,
	 .virtual_irq_start = MXC_GPIO_INT_BASE,
	 },
	{
	 .num = 1,
	 .base = IO_ADDRESS(GPIO2_BASE_ADDR),
	 .irq = MXC_INT_GPIO2,
	 .virtual_irq_start = MXC_GPIO_INT_BASE + GPIO_NUM_PIN,
	 },
	{
	 .num = 2,
	 .base = IO_ADDRESS(GPIO3_BASE_ADDR),
	 .irq = MXC_INT_GPIO3,
	 .virtual_irq_start = MXC_GPIO_INT_BASE + GPIO_NUM_PIN * 2,
	 },
};

static struct platform_device mxc_dma_device = {
	.name = "mxc_dma",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
};

static inline void mxc_init_dma(void)
{
	(void)platform_device_register(&mxc_dma_device);
}

static struct resource spdif_resources[] = {
	{
	 .start = SPDIF_BASE_ADDR,
	 .end = SPDIF_BASE_ADDR + 0x50,
	 .flags = IORESOURCE_MEM,
	 },
};

static struct mxc_spdif_platform_data mxc_spdif_data = {
	.spdif_tx = 1,
	.spdif_rx = 1,
	.spdif_clk_44100 = 3,	/* spdif_ext_clk source for 44.1KHz */
	.spdif_clk_48000 = 0,	/* audio osc source */
	.spdif_clkid = 0,
	.spdif_clk = NULL,	/* spdif bus clk */
};

static struct platform_device mxc_alsa_spdif_device = {
	.name = "mxc_alsa_spdif",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxc_spdif_data,
		},
	.num_resources = ARRAY_SIZE(spdif_resources),
	.resource = spdif_resources,
};

static inline void mxc_init_spdif(void)
{
	mxc_spdif_data.spdif_clk = clk_get(NULL, "spdif_ipg_clk");
	clk_put(mxc_spdif_data.spdif_clk);
	mxc_spdif_data.spdif_core_clk = clk_get(NULL, "spdif_clk");
	clk_put(mxc_spdif_data.spdif_core_clk);
	platform_device_register(&mxc_alsa_spdif_device);
}

static struct resource asrc_resources[] = {
	{
	 .start = ASRC_BASE_ADDR,
	 .end = ASRC_BASE_ADDR + 0x9C,
	 .flags = IORESOURCE_MEM,
	 },
};

static struct mxc_asrc_platform_data mxc_asrc_data;

static struct platform_device mxc_asrc_device = {
	.name = "mxc_asrc",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		.platform_data = &mxc_asrc_data,
		},
	.num_resources = ARRAY_SIZE(asrc_resources),
	.resource = asrc_resources,
};

/*!
 * Currently not called. If the Luigi Audio design decides to use
 * ASRC, then a call to this will be added in the __init fn
 */
static inline void mxc_init_asrc(void)
{
	if (cpu_is_mx35_rev(CHIP_REV_2_0) < 1)
		mxc_asrc_data.channel_bits = 3;
	else
		mxc_asrc_data.channel_bits = 4;

	mxc_asrc_data.asrc_core_clk = clk_get(NULL, "asrc_clk");
	clk_put(mxc_asrc_data.asrc_core_clk);
	mxc_asrc_data.asrc_audio_clk = clk_get(NULL, "asrc_audio_clk");
	clk_set_rate(mxc_asrc_data.asrc_audio_clk, 768000);
	clk_put(mxc_asrc_data.asrc_audio_clk);
	platform_device_register(&mxc_asrc_device);
}

static void mxc_luigi_init_audio(void)
{
	struct clk *cko_clk;
	struct clk *ckih_clk;

	cko_clk = clk_get(NULL, "cko1_clk");
	ckih_clk = clk_get(NULL, "ckih");

	clk_set_parent(cko_clk, ckih_clk);
	clk_enable(cko_clk);
}

#if defined(CONFIG_MXC_PWM)
static struct resource pwm_resources[] = {
	{
	.start = PWM_BASE_ADDR,
	.end = PWM_BASE_ADDR + 0x14,
	.flags = IORESOURCE_MEM,
	},
	{
	.start = MXC_INT_PWM,
	.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device mxc_pwm_device = {
	.name = "mxc_pwm",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(pwm_resources),
	.resource = pwm_resources,
};

static void mxc_init_pwm(void)
{
	printk(KERN_INFO "mxc_pwm_device registered\n");
	if (platform_device_register(&mxc_pwm_device) < 0)
		printk(KERN_ERR "registration of mxc_pwm device failed\n");
}

#else

static void mxc_init_pwm(void)
{

}

#endif

#if defined(CONFIG_MXC_IIM) || defined(CONFIG_MXC_IIM_MODULE)
static struct resource mxc_iim_resources[] = {
	{
	.start = IIM_BASE_ADDR,
	.end = IIM_BASE_ADDR + SZ_4K - 1,
	.flags = IORESOURCE_MEM,
	},
};

static struct platform_device mxc_iim_device = {
	.name = "mxc_iim",
	.id = 0,
	.dev = {
		.release = mxc_nop_release,
		},
	.num_resources = ARRAY_SIZE(mxc_iim_resources),
	.resource = mxc_iim_resources
};

static inline void mxc_init_iim(void)
{
	if (platform_device_register(&mxc_iim_device) < 0)
		dev_err(&mxc_iim_device.dev,
			"Unable to register mxc iim device\n");
}
#else
static inline void mxc_init_iim(void)
{
}
#endif

int __init mxc_init_devices(void)
{
	mxc_init_wdt();
	mxc_init_ipu();
	mxc_init_spi();
	mxc_init_i2c();
	pmic_init_rtc();
	mxc_init_rtc();
	mxc_init_dma();
	mxc_luigi_init_audio();	
	mxc_init_pwm();
	mxc_init_iim();

	/* SPBA configuration for SSI2 - SDMA and MCU are set */
	spba_take_ownership(SPBA_SSI2, SPBA_MASTER_C | SPBA_MASTER_A);
	return 0;
}
