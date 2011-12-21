/*
 * Copyright 2005-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/fsl_devices.h>
#include <linux/delay.h>
#include <asm/arch/arc_otg.h>
#include <asm/arch/hardware.h>
#include <asm/io.h>
#include "usb.h"

/*
 * platform data structs
 * 	- Which one to use is determined by CONFIG options in usb.h
 * 	- operating_mode plugged at run time
 */
static struct fsl_usb2_platform_data __maybe_unused dr_utmi_config = {
	.name              = "DR",
	.platform_init     = usbotg_init,
	.platform_uninit   = usbotg_uninit,
	.phy_mode          = FSL_USB2_PHY_UTMI_WIDE,
	.power_budget      = 500, /* 500 mA max power */
	.gpio_usb_active   = gpio_usbotg_utmi_active,
	.gpio_usb_inactive = gpio_usbotg_utmi_inactive,
	.transceiver       = "utmi",
};


/*
 * resources
 */
static struct resource otg_resources[] = {
	[0] = {
		.start = (u32)(USB_OTGREGS_BASE),
		.end   = (u32)(USB_OTGREGS_BASE + 0x1ff),
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = MXC_INT_USB_OTG,
		.flags = IORESOURCE_IRQ,
	},
};


static u64 dr_udc_dmamask = ~(u32) 0;
static void dr_udc_release(struct device *dev)
{
}

static u64 dr_otg_dmamask = ~(u32) 0;
static void dr_otg_release(struct device *dev)
{
}

/*
 * platform device structs
 * 	dev.platform_data field plugged at run time
 */
static struct platform_device __maybe_unused dr_udc_device = {
	.name = "fsl-usb2-udc",
	.id   = -1,
	.dev  = {
		.release           = dr_udc_release,
		.dma_mask          = &dr_udc_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource      = otg_resources,
	.num_resources = ARRAY_SIZE(otg_resources),
};

static struct platform_device __maybe_unused dr_otg_device = {
	.name = "fsl-usb2-otg",
	.id = -1,
	.dev = {
		.release           = dr_otg_release,
		.dma_mask          = &dr_otg_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.resource      = otg_resources,
	.num_resources = ARRAY_SIZE(otg_resources),
};

struct usb_dr_dc {
        /* Capability register */
	u32 id;
	u32 res1[35];
	u32 sbuscfg;            /* sbuscfg ahb burst */
	u32 res11[27];
        u16 caplength;          /* Capability Register Length */
        u16 hciversion;         /* Host Controller Interface Version */
	u32 hcsparams;          /* Host Controller Structual Parameters */
	u32 hccparams;          /* Host Controller Capability Parameters */
	u32 res2[5];
	u32 dciversion;         /* Device Controller Interface Version */
	u32 dccparams;          /* Device Controller Capability Parameters */
	u32 res3[6];
        /* Operation register */
	u32 usbcmd;             /* USB Command Register */
	u32 usbsts;             /* USB Status Register */
	u32 usbintr;            /* USB Interrupt Enable Register */
	u32 frindex;            /* Frame Index Register */
	u32 res4;
	u32 deviceaddr;         /* Device Address */
	u32 endpointlistaddr;   /* Endpoint List Address Register */
	u32 res5;
	u32 burstsize;          /* Master Interface Data Burst Size Register */
	u32 txttfilltuning;     /* Transmit FIFO Tuning Controls Register */
	u32 res6[6];
	u32 configflag;         /* Configure Flag Register */
	u32 portsc1;            /* Port 1 Status and Control Register */
	u32 res7[7];
	u32 otgsc;              /* On-The-Go Status and Control */
	u32 usbmode;            /* USB Mode Register */
	u32 endptsetupstat;     /* Endpoint Setup Status Register */
	u32 endpointprime;      /* Endpoint Initialization Register */
	u32 endptflush;         /* Endpoint Flush Register */
	u32 endptstatus;        /* Endpoint Status Register */
	u32 endptcomplete;      /* Endpoint Complete Register */
	u32 endptctrl[8 * 2];   /* Endpoint Control Registers */
};

unsigned int usbdr_early_portsc1_read(void)
{
	volatile struct usb_dr_dc *dr_regs;
	int start = USB_OTGREGS_BASE;
	int end = USB_OTGREGS_BASE + 0x1ff;
	unsigned int portsc1;
	unsigned int temp;
	unsigned long timeout;

	if (!request_mem_region(start, (end - start + 1), "fsl-usb-dr"))
		return -1;

	dr_regs = ioremap(start, (end - start + 1));
	if (!dr_regs) {
		release_mem_region(start, (end - start + 1));
		return -1;
	}

	/* Stop and reset the usb controller */
        temp = readl(&dr_regs->usbcmd);
        temp &= ~0x1;
        writel(temp, &dr_regs->usbcmd);

        temp = readl(&dr_regs->usbcmd);
        temp |= 0x2;
        writel(temp, &dr_regs->usbcmd);

        /* Wait for reset to complete */
        timeout = jiffies + 1000;
        while (readl(&dr_regs->usbcmd) & 0x2) {
                if (time_after(jiffies, timeout)) {
                        printk("udc reset timeout! \n");
                        return -ETIMEDOUT;
                }
                cpu_relax();
        }

        /* Set the controller as device mode */
        temp = readl(&dr_regs->usbmode);
        temp &= ~0x3;        /* clear mode bits */
        temp |= 0x2;
        /* Disable Setup Lockout */
        temp |= 0x8;
        writel(temp, &dr_regs->usbmode);

	/* Config PHY interface */
        portsc1 = readl(&dr_regs->portsc1);
        portsc1 &= ~(0xc0000000 | 0x10000000 | 0x80) | 0x10000000 | 0x40;
        writel(portsc1, &dr_regs->portsc1);

	/* Set controller to Run */
	temp = readl(&dr_regs->usbcmd);
	temp |= 0x1;
	writel(temp, &dr_regs->usbcmd);

	mdelay(100);

	/* Get the line status bits */
	portsc1 = readl(&dr_regs->portsc1) & 0xc00;

	/* disable all INTR */
	writel(0, &dr_regs->usbintr);
	
	/* set controller to Stop */
	temp = readl(&dr_regs->usbcmd);
	temp &= ~0x1;
	writel(temp, &dr_regs->usbcmd);

	release_mem_region(start, (end - start + 1));
	return portsc1;
}
EXPORT_SYMBOL(usbdr_early_portsc1_read);

static int __init usb_dr_init(void)
{
	pr_debug("%s: \n", __func__);

	/* i.MX35 1.0 should work in INCR mode */
	if (cpu_is_mx35_rev(CHIP_REV_2_0) < 0) {
		PDATA->change_ahb_burst = 1;
		PDATA->ahb_burst_mode = 0;
	}

	dr_register_otg();
	dr_register_host(otg_resources, ARRAY_SIZE(otg_resources));
	dr_register_udc();

	return 0;
}

module_init(usb_dr_init);
