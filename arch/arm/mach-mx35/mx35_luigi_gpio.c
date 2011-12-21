/*
 * Copyright 2008 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright 2009-2010 Amazon.com
 * Manish Lachwani (lachwani@lab126.com)
 * Marcos Frid     (mfrid@lab126.com)
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*
 * MX35 Luigi GPIO definitions
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/irq.h>
#include <linux/pmic_external.h>
#include <asm/io.h>
#include <asm/hardware.h>
//#include <asm/arch/gpio.h>
#include <asm/arch/board_id.h>
#include <asm/arch/pmic_power.h>
#include <asm/arch/controller_common.h>
#include "board-mx35_3stack.h"
#include "iomux.h"
#include <linux/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/boardid.h>
extern void gpio_free(unsigned gpio);
/* use gpiolib dispatchers */
#define gpio_get_value		__gpio_get_value
#define gpio_set_value		__gpio_set_value
#define gpio_cansleep		__gpio_cansleep

#define gpio_to_irq(gpio)	(MXC_GPIO_IRQ_START + (gpio))
#define irq_to_gpio(irq)	((irq) - MXC_GPIO_IRQ_START)

#define mxc_get_gpio_datain(gpio) \
!gpio_get_value(IOMUX_TO_GPIO(gpio))
/*!
 * @file mach-mx35/mx35_3stack_gpio.c
 *
 * @brief This file contains all the GPIO setup functions for the board.
 *
 * @ingroup GPIO_MX35
 */

/*!
 * This system-wise GPIO function initializes the pins during system startup.
 * All the statically linked device drivers should put the proper GPIO
 * initialization code inside this function. It is called by \b fixup_mx31ads()
 * during system startup. This function is board specific.
 */
void mx35_luigi_gpio_init(void)
{
	/* config CS1 */
	mxc_request_iomux(MX35_PIN_CS1, MUX_CONFIG_FUNC);

}

void mx35_luigi_pullup_enable(iomux_pin_name_t pin)
{
	mxc_iomux_set_pad(pin, (PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD));
}

EXPORT_SYMBOL(mx35_luigi_pullup_enable);

void mx35_luigi_pullup_disable(iomux_pin_name_t pin)
{
	mxc_iomux_set_pad(pin, PAD_CTL_PKE_ENABLE);
}

EXPORT_SYMBOL(mx35_luigi_pullup_disable);

/*
 * Accessory port settings
 */

#define ACC_CHARGER_OK	MX35_PIN_MLB_SIG
#define ACC_DET_B	MX35_PIN_ATA_CS0
#define ACC_EN		MX35_PIN_GPIO1_0

void gpio_acc_active(void)
{
	unsigned int pad_val;

	mxc_request_iomux(ACC_CHARGER_OK, MUX_CONFIG_ALT5);
	mxc_request_iomux(ACC_DET_B, MUX_CONFIG_ALT5);
	mxc_request_iomux(ACC_EN, MUX_CONFIG_FUNC);

	pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_100K_PU |
		PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
		PAD_CTL_HYS_SCHMITZ |
		PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V;
	
	mxc_iomux_set_pad(ACC_CHARGER_OK, pad_val);

	pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_100K_PU |
		PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
		PAD_CTL_HYS_SCHMITZ |
		PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V;

	mxc_iomux_set_pad(ACC_DET_B, pad_val);
	mxc_iomux_set_pad(ACC_EN, pad_val);

	gpio_direction_output(IOMUX_TO_GPIO(ACC_CHARGER_OK), 1);
	gpio_direction_output(IOMUX_TO_GPIO(ACC_DET_B), 1);
	mxc_iomux_set_input(MUX_IN_GPIO2_IN_6, INPUT_CTL_PATH1);
	gpio_direction_output(IOMUX_TO_GPIO(ACC_EN), 0);
	mxc_iomux_set_input(MUX_IN_GPIO3_IN_5, INPUT_CTL_PATH1);
}
EXPORT_SYMBOL(gpio_acc_active);

int gpio_acc_get_irq(void)
{
	return IOMUX_TO_IRQ(ACC_DET_B);
}
EXPORT_SYMBOL(gpio_acc_get_irq);

int gpio_acc_detected(void)
{
	return mxc_get_gpio_datain(ACC_DET_B);
}
EXPORT_SYMBOL(gpio_acc_detected);

int gpio_acc_power_ok(void)
{
	return mxc_get_gpio_datain(ACC_CHARGER_OK);
}
EXPORT_SYMBOL(gpio_acc_power_ok);

int gpio_acc_power_irq(void)
{
	return IOMUX_TO_IRQ(ACC_CHARGER_OK);
}
EXPORT_SYMBOL(gpio_acc_power_irq);

void gpio_acc_charger_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(ACC_EN), enable);
}
EXPORT_SYMBOL(gpio_acc_charger_enable);

void gpio_acc_inactive(void)
{
	mxc_free_iomux(ACC_CHARGER_OK, MUX_CONFIG_ALT5);
	mxc_free_iomux(ACC_DET_B, MUX_CONFIG_ALT5);
	mxc_free_iomux(ACC_EN, MUX_CONFIG_ALT5);
}
EXPORT_SYMBOL(gpio_acc_inactive);

void watchdog_reset_config(void)
{
	mxc_request_iomux(MX35_PIN_WATCHDOG_RST, MUX_CONFIG_FUNC);
}
EXPORT_SYMBOL(watchdog_reset_config);

void watchdog_pin_gpio(void)
{
	int pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_100K_PU |
			PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
			PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V |
			PAD_CTL_HYS_SCHMITZ;

	mxc_request_iomux(MX35_PIN_WATCHDOG_RST, MUX_CONFIG_ALT5);
	mxc_iomux_set_pad(MX35_PIN_WATCHDOG_RST, pad_val);
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_WATCHDOG_RST), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_WATCHDOG_RST), 0);
}
EXPORT_SYMBOL(watchdog_pin_gpio);

void gpio_usbotg_oc_disable(int enable)
{
	if (!enable) {
		mxc_free_iomux(MX35_PIN_USBOTG_OC, MUX_CONFIG_FUNC);
	}
	else {
		mxc_request_iomux(MX35_PIN_USBOTG_OC, MUX_CONFIG_FUNC);
		mxc_iomux_set_pad(MX35_PIN_USBOTG_OC, PAD_CTL_100K_PD);
	}
}
EXPORT_SYMBOL(gpio_usbotg_oc_disable);

/*!
 * Configure the papyrus IRQ line
 */
void gpio_papyrus_irq_configure(int enable)
{
	int pad_val = PAD_CTL_PUE_PUD | PAD_CTL_ODE_CMOS | PAD_CTL_DRV_NORMAL |
			PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V | PAD_CTL_HYS_CMOS |
			PAD_CTL_PKE_NONE | PAD_CTL_100K_PU;

#ifdef CONFIG_FEC
	return;
#endif

	if (enable) {
		mxc_request_iomux(MX35_PIN_FEC_MDC, MUX_CONFIG_ALT5);
		mxc_iomux_set_pad(MX35_PIN_FEC_MDC, pad_val);
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_FEC_MDC), 1);
		mxc_iomux_set_input(MUX_IN_GPIO3_IN_13, INPUT_CTL_PATH1);
	}
	else {
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_FEC_MDC), 0);
		mxc_free_iomux(MX35_PIN_FEC_MDC, MUX_CONFIG_ALT5);
	}
}
EXPORT_SYMBOL(gpio_papyrus_irq_configure);

int gpio_papyrus_get_irq(void)
{
#ifdef CONFIG_FEC
	return -1;
#else
	return IOMUX_TO_IRQ(MX35_PIN_FEC_MDC);
#endif
}
EXPORT_SYMBOL(gpio_papyrus_get_irq);	

/*!
 * Configure the papyrus power good line
 */
void gpio_papyrus_pg_configure(int enable)
{
	int pad_val = PAD_CTL_PUE_PUD | PAD_CTL_ODE_CMOS | PAD_CTL_DRV_NORMAL |
			PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V | PAD_CTL_HYS_CMOS |
			PAD_CTL_PKE_NONE | PAD_CTL_100K_PU;

	if (enable) {
		mxc_request_iomux(MX35_PIN_STXFS5, MUX_CONFIG_ALT5);
		mxc_iomux_set_pad(MX35_PIN_STXFS5, pad_val);
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_STXFS5), 1);
		mxc_iomux_set_input(MUX_IN_GPIO1_IN_3, INPUT_CTL_PATH0);
	}
	else {
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_STXFS5), 0);
		mxc_free_iomux(MX35_PIN_STXFS5, MUX_CONFIG_GPIO);
	}
}
EXPORT_SYMBOL(gpio_papyrus_pg_configure);

int gpio_papyrus_get_pg(void)
{
	return IOMUX_TO_IRQ(MX35_PIN_STXFS5);
}
EXPORT_SYMBOL(gpio_papyrus_get_pg);

/*!
 * Configure papyrus power state
 */
static int papyrus_enable_state = 0;
 
static int gpio_papryus_needs_legacy_polarity(void)
{
	return ( (IS_SHASTA() && IS_EVT() && (1 == GET_BOARD_HW_VERSION())) ||
			(IS_LUIGI() && (3 == GET_BOARD_HW_VERSION())) );
}

void gpio_papyrus_configure(int enable)
{
#ifdef CONFIG_FEC
	return;
#endif

	if (papyrus_enable_state != enable) {
		int pad_val;
		
		if (enable) {
			pad_val = PAD_CTL_PUE_PUD | PAD_CTL_ODE_CMOS | PAD_CTL_DRV_NORMAL | 
				PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V | PAD_CTL_HYS_CMOS | 
				PAD_CTL_PKE_NONE | PAD_CTL_100K_PD;
			
			mxc_request_iomux(MX35_PIN_FEC_MDIO, MUX_CONFIG_ALT5);
			mxc_iomux_set_pad(MX35_PIN_FEC_MDIO, pad_val);
			gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_FEC_MDIO), 0);
			mxc_iomux_set_input(MUX_IN_GPIO3_IN_14, INPUT_CTL_PATH1);
			
			if (gpio_papryus_needs_legacy_polarity())
				gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_FEC_MDIO), 0);
			else
				gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_FEC_MDIO), 1);
		}
		else {
			pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PU; 
			
			if (gpio_papryus_needs_legacy_polarity())
				gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_FEC_MDIO), 1);
			else
				gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_FEC_MDIO), 0);
			
			gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_FEC_MDIO), 1);
			mxc_iomux_set_pad(MX35_PIN_FEC_MDIO, pad_val);
			mxc_free_iomux(MX35_PIN_FEC_MDIO, MUX_CONFIG_ALT5);
		}
	
		papyrus_enable_state = enable;
	}
}
EXPORT_SYMBOL(gpio_papyrus_configure);

void gpio_papyrus_reset(void)
{
	gpio_papyrus_configure(1);
	mdelay(20);
	gpio_papyrus_configure(0);
	mdelay(20);
	gpio_papyrus_configure(1);
}
EXPORT_SYMBOL(gpio_papyrus_reset);

static int charger_led_state = 0;

/*
 * Shasta boards have a GPIO to control the charger led 
 */
void gpio_chrgled_iomux(void)
{
	mxc_request_iomux(MX35_PIN_CSI_D12, MUX_CONFIG_ALT5);
}
EXPORT_SYMBOL(gpio_chrgled_iomux);

static int gpio_chrgled_needs_legacy_polarity(void)
{
	return ( (IS_SHASTA() && IS_EVT() && (1 == GET_BOARD_HW_VERSION())) ||
			(IS_LUIGI() && (3 == GET_BOARD_HW_VERSION())) );
}

void gpio_chrgled_active(int enable)
{
	int pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V |
			PAD_CTL_HYS_CMOS | PAD_CTL_PKE_NONE | PAD_CTL_100K_PU;

	if (charger_led_state != enable) {
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_CSI_D12), 0);
		if (gpio_chrgled_needs_legacy_polarity())
			gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSI_D12), enable);
		else
			gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSI_D12), !enable);
		mxc_iomux_set_pad(MX35_PIN_CSI_D12, pad_val);
		charger_led_state = enable;
	}
}
EXPORT_SYMBOL(gpio_chrgled_active);

void gpio_chrgled_inactive(void)
{
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSI_D12), 0);
}
EXPORT_SYMBOL(gpio_chrgled_inactive);

void gpio_fec_active(void)
{
        mxc_request_iomux(MX35_PIN_FEC_TX_CLK, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RX_CLK, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RX_DV, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_COL, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RDATA0, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_TDATA0, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_TX_EN, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_MDC, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_MDIO, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_TX_ERR, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RX_ERR, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_CRS, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RDATA1, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_TDATA1, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RDATA2, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_TDATA2, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_RDATA3, MUX_CONFIG_FUNC);
        mxc_request_iomux(MX35_PIN_FEC_TDATA3, MUX_CONFIG_FUNC);

#define FEC_PAD_CTL_COMMON (PAD_CTL_PUE_PUD| \
                        PAD_CTL_ODE_CMOS|PAD_CTL_DRV_NORMAL|PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V)
        mxc_iomux_set_pad(MX35_PIN_FEC_TX_CLK, FEC_PAD_CTL_COMMON |
                          PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE |
                          PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RX_CLK,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RX_DV,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_COL,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RDATA0,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_TDATA0,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_TX_EN,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_MDC,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_MDIO,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_22K_PU);
        mxc_iomux_set_pad(MX35_PIN_FEC_TX_ERR,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RX_ERR,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_CRS,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RDATA1,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_TDATA1,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RDATA2,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_TDATA2,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_RDATA3,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_SCHMITZ |
                          PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PD);
        mxc_iomux_set_pad(MX35_PIN_FEC_TDATA3,
                          FEC_PAD_CTL_COMMON | PAD_CTL_HYS_CMOS |
                          PAD_CTL_PKE_NONE | PAD_CTL_100K_PD);
#undef FEC_PAD_CTL_COMMON
}
EXPORT_SYMBOL(gpio_fec_active);

/*!
 * Setup GPIO for a UART port to be active
 *
 * @param  port         a UART port
 * @param  no_irda      indicates if the port is used for SIR
 */
void gpio_uart_active(int port, int no_irda)
{
	/*
	 * Configure the IOMUX control registers for the UART signals
	 */
	switch (port) {
		/* UART 1 IOMUX Configs */
	case 0:
		mxc_request_iomux(MX35_PIN_RXD1, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_TXD1, MUX_CONFIG_FUNC);
		break;
		/* UART 2 IOMUX Configs */
	case 1:
		mxc_request_iomux(MX35_PIN_TXD2, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_RXD2, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_RTS2, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_CTS2, MUX_CONFIG_FUNC);
		mxc_iomux_set_pad(MX35_PIN_RXD2,
				  PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE |
				  PAD_CTL_PUE_PUD | PAD_CTL_100K_PU);
		mxc_iomux_set_pad(MX35_PIN_TXD2,
				  PAD_CTL_PUE_PUD | PAD_CTL_100K_PD);
		mxc_iomux_set_pad(MX35_PIN_RTS2,
				  PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE |
				  PAD_CTL_PUE_PUD | PAD_CTL_100K_PU);
		mxc_iomux_set_pad(MX35_PIN_CTS2,
				  PAD_CTL_PUE_PUD | PAD_CTL_100K_PD);
		break;
		/* UART 3 IOMUX Configs */
	case 2:
#ifdef CONFIG_FEC	
		return;
#endif

		mxc_request_iomux(MX35_PIN_FEC_TX_CLK, MUX_CONFIG_ALT2);
		mxc_request_iomux(MX35_PIN_FEC_RX_CLK, MUX_CONFIG_ALT2);
		mxc_request_iomux(MX35_PIN_FEC_COL, MUX_CONFIG_ALT2);
		mxc_request_iomux(MX35_PIN_FEC_RX_DV, MUX_CONFIG_ALT2);

		mxc_iomux_set_input(MUX_IN_UART3_UART_RTS_B, INPUT_CTL_PATH2);
		mxc_iomux_set_input(MUX_IN_UART3_UART_RXD_MUX, INPUT_CTL_PATH3);
		break;
	default:
		break;
	}

}

EXPORT_SYMBOL(gpio_uart_active);

/*!
 * Setup GPIO for a UART port to be inactive
 *
 * @param  port         a UART port
 * @param  no_irda      indicates if the port is used for SIR
 */
void gpio_uart_inactive(int port, int no_irda)
{
	switch (port) {
	case 0:
		break;
	case 1:
		mxc_free_iomux(MX35_PIN_RXD2, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_TXD2, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_RTS2, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_CTS2, MUX_CONFIG_GPIO);
		break;
	case 2:
		gpio_request(IOMUX_TO_GPIO(MX35_PIN_FEC_TX_CLK), NULL);
		gpio_request(IOMUX_TO_GPIO(MX35_PIN_FEC_RX_CLK), NULL);
		gpio_request(IOMUX_TO_GPIO(MX35_PIN_FEC_COL), NULL);
		gpio_request(IOMUX_TO_GPIO(MX35_PIN_FEC_RX_DV), NULL);

		mxc_free_iomux(MX35_PIN_FEC_TX_CLK, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_FEC_RX_CLK, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_FEC_COL, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_FEC_RX_DV, MUX_CONFIG_GPIO);

		mxc_iomux_set_input(MUX_IN_UART3_UART_RTS_B, INPUT_CTL_PATH0);
		mxc_iomux_set_input(MUX_IN_UART3_UART_RXD_MUX, INPUT_CTL_PATH0);
		break;
	default:
		break;
	}
}

EXPORT_SYMBOL(gpio_uart_inactive);

/*!
 * Add a test point to track down suspend/resume issues
 */
void gpio_test_pm(void)
{
	mxc_request_iomux(MX35_PIN_CSI_VSYNC, MUX_CONFIG_ALT5);

	/* Configure as an output */
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_CSI_VSYNC), 0);

	/* Default value is 0 */
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSI_VSYNC), 0);
}
EXPORT_SYMBOL(gpio_test_pm);

void gpio_pm_set_value(int val)
{
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_CSI_VSYNC), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSI_VSYNC), val);
}
EXPORT_SYMBOL(gpio_pm_set_value);

/*!
 * Configure the IOMUX GPR register to receive shared SDMA UART events
 *
 * @param  port         a UART port
 */
void config_uartdma_event(int port)
{
}

EXPORT_SYMBOL(config_uartdma_event);

/*!
 * Setup GPIO for an I2C device to be active
 *
 * @param  i2c_num         an I2C device
 */
void gpio_i2c_active(int i2c_num)
{
	unsigned int pad_val = PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE |
				PAD_CTL_PUE_PUD | PAD_CTL_ODE_OpenDrain;

	switch (i2c_num) {
	case 0:
		mxc_request_iomux(MX35_PIN_I2C1_CLK, MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_I2C1_DAT, MUX_CONFIG_SION);

		mxc_iomux_set_pad(MX35_PIN_I2C1_CLK, pad_val);
		mxc_iomux_set_pad(MX35_PIN_I2C1_DAT, pad_val);
		break;
	case 1:
		mxc_request_iomux(MX35_PIN_I2C2_CLK, MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_I2C2_DAT, MUX_CONFIG_SION);

		mxc_iomux_set_pad(MX35_PIN_I2C2_CLK, pad_val);
		mxc_iomux_set_pad(MX35_PIN_I2C2_DAT, pad_val);

		break;
	case 2:
		mxc_request_iomux(MX35_PIN_TX3_RX2, MUX_CONFIG_ALT1);
		mxc_request_iomux(MX35_PIN_TX2_RX3, MUX_CONFIG_ALT1);
		mxc_iomux_set_pad(MX35_PIN_TX3_RX2, pad_val);
		mxc_iomux_set_pad(MX35_PIN_TX2_RX3, pad_val);
		break;
	default:
		break;
	}

#undef PAD_CONFIG

}

EXPORT_SYMBOL(gpio_i2c_active);

/*!
 * Setup GPIO for an I2C device to be inactive
 *
 * @param  i2c_num         an I2C device
 */
void gpio_i2c_inactive(int i2c_num)
{
	switch (i2c_num) {
	case 0:
		break;
	case 1:
		break;
	case 2:
		mxc_request_iomux(MX35_PIN_TX3_RX2, MUX_CONFIG_GPIO);
		mxc_request_iomux(MX35_PIN_TX2_RX3, MUX_CONFIG_GPIO);
		break;
	default:
		break;
	}
}

EXPORT_SYMBOL(gpio_i2c_inactive);

/*!
 * Setup GPIO for a CSPI device to be active
 *
 * @param  cspi_mod         an CSPI device
 */
void gpio_spi_active(int cspi_mod)
{
	switch (cspi_mod) {
	case 0:
		/* SPI1 */
		mxc_request_iomux(MX35_PIN_CSPI1_MOSI, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_CSPI1_MISO, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_CSPI1_SS0, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_CSPI1_SCLK, MUX_CONFIG_FUNC);
		mxc_request_iomux(MX35_PIN_CSPI1_SPI_RDY, MUX_CONFIG_FUNC);

		mxc_iomux_set_pad(MX35_PIN_CSPI1_MOSI,
				  PAD_CTL_DRV_1_8V | PAD_CTL_HYS_SCHMITZ |
				  PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD |
				  PAD_CTL_100K_PD | PAD_CTL_DRV_NORMAL);
		mxc_iomux_set_pad(MX35_PIN_CSPI1_MISO,
				  PAD_CTL_DRV_1_8V | PAD_CTL_HYS_SCHMITZ |
				  PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD |
				  PAD_CTL_100K_PD | PAD_CTL_DRV_NORMAL);
		mxc_iomux_set_pad(MX35_PIN_CSPI1_SS0,
				  PAD_CTL_DRV_1_8V | PAD_CTL_HYS_SCHMITZ |
				  PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD |
				  PAD_CTL_100K_PU | PAD_CTL_ODE_CMOS |
				  PAD_CTL_DRV_NORMAL);
		mxc_iomux_set_pad(MX35_PIN_CSPI1_SCLK,
				  PAD_CTL_DRV_1_8V | PAD_CTL_HYS_SCHMITZ |
				  PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD |
				  PAD_CTL_100K_PD | PAD_CTL_DRV_NORMAL);
		mxc_iomux_set_pad(MX35_PIN_CSPI1_SPI_RDY,
				  PAD_CTL_DRV_1_8V | PAD_CTL_HYS_SCHMITZ |
				  PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD |
				  PAD_CTL_100K_PU | PAD_CTL_DRV_NORMAL);
		break;
	case 1:
		/* SPI2 */
		break;
	default:
		break;
	}
}

EXPORT_SYMBOL(gpio_spi_active);

/*!
 * Setup GPIO for a CSPI device to be inactive
 *
 * @param  cspi_mod         a CSPI device
 */
void gpio_spi_inactive(int cspi_mod)
{
	switch (cspi_mod) {
	case 0:
		/* SPI1 */
		mxc_free_iomux(MX35_PIN_CSPI1_MOSI, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_CSPI1_MISO, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_CSPI1_SS0, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_CSPI1_SCLK, MUX_CONFIG_GPIO);
		mxc_free_iomux(MX35_PIN_CSPI1_SPI_RDY, MUX_CONFIG_GPIO);
		break;
	case 1:
		/* SPI2 */
		break;
	default:
		break;
	}
}

EXPORT_SYMBOL(gpio_spi_inactive);

#define LCD_DATA_pad_enable	PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD | \
				PAD_CTL_DRV_1_8V | PAD_CTL_100K_PU | \
				PAD_CTL_SRE_FAST | PAD_CTL_DRV_HIGH
#define LCD_DATA_pad_disable	PAD_CTL_PKE_ENABLE

/*!
 * Setup GPIO for LCD to be inactive
 */
#define CLEAR_LCD_PIN(pin, func) \
	mxc_iomux_set_pad(pin, LCD_DATA_pad_disable); \
	mxc_free_iomux(pin, func);

void gpio_lcd_inactive(void)
{
	CLEAR_LCD_PIN(MX35_PIN_LD0,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD1,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD2,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD3,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD4,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD5,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD6,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD7,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD8,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD9,  MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD10, MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD11, MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD12, MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD13, MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD14, MUX_CONFIG_FUNC);
	CLEAR_LCD_PIN(MX35_PIN_LD15, MUX_CONFIG_FUNC);

	CLEAR_LCD_PIN(MX35_PIN_LD20, MUX_CONFIG_ALT1); /* CHIP SELECT */
	CLEAR_LCD_PIN(MX35_PIN_LD21, MUX_CONFIG_ALT1); /* PAR_RS */
	CLEAR_LCD_PIN(MX35_PIN_LD22, MUX_CONFIG_ALT1); /* WRITE */
	CLEAR_LCD_PIN(MX35_PIN_LD23, MUX_CONFIG_ALT1); /* READ */
 }

EXPORT_SYMBOL(gpio_lcd_inactive);

/*!
 * Setup GPIO for LCD to be active
 */
#define SET_LCD_PIN(pin, func) \
	err = mxc_request_iomux(pin, func); \
	if (err) { \
	   goto disable_lcd_lines; \
	} \
	mxc_iomux_set_pad(pin, LCD_DATA_pad_enable); \

int gpio_lcd_active(void)
{
	int err;

	SET_LCD_PIN(MX35_PIN_LD0,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD1,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD2,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD3,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD4,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD5,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD6,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD7,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD8,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD9,  MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD10, MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD11, MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD12, MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD13, MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD14, MUX_CONFIG_FUNC);
	SET_LCD_PIN(MX35_PIN_LD15, MUX_CONFIG_FUNC);

	return err;

disable_lcd_lines:
	gpio_lcd_inactive();
	return err;
}

EXPORT_SYMBOL(gpio_lcd_active);

/*!
 * Setup pins for SLCD to be active
 */
void slcd_gpio_config(void)
{
	mxc_request_iomux(MX35_PIN_LD20, MUX_CONFIG_ALT1); /* CHIP SELECT */
	mxc_request_iomux(MX35_PIN_LD21, MUX_CONFIG_ALT1); /* PAR_RS */
	mxc_request_iomux(MX35_PIN_LD22, MUX_CONFIG_ALT1); /* WRITE */
	mxc_request_iomux(MX35_PIN_LD23, MUX_CONFIG_ALT1); /* READ */
}

EXPORT_SYMBOL(slcd_gpio_config);

/*!
 * Setup pins for Broadsheet/ISIS.
 */
#define CONTROLLER_HIRQ_LINE		MX35_PIN_LD19
#define CONTROLLER_HRST_LINE		MX35_PIN_LD17
#define CONTROLLER_HRDY_LINE		MX35_PIN_LD18
#define CONTROLLER_SUPPLY_FAULT		MX35_PIN_STXFS5
#define CONTROLLER_RESET_VAL		0
#define CONTROLLER_NON_RESET_VAL	1
#define WR_GPIO_LINE(addr, val)		gpio_set_value(IOMUX_TO_GPIO(addr), val);
#define CONTROLLER_HIRQ_IRQ		IOMUX_TO_IRQ(CONTROLLER_HIRQ_LINE)
#define CONTROLLER_PAD_EN		PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD | \
					PAD_CTL_DRV_1_8V | PAD_CTL_100K_PU | \
					PAD_CTL_HYS_SCHMITZ
#define CONTROLLER_PAD_DIS		PAD_CTL_PKE_ENABLE

int controller_common_gpio_config(irq_handler_t controller_irq_handler, char *controller_irq_handler_name)
{
	int result = CONTROLLER_COMMON_GPIO_INIT_SUCCESS;
	
	gpio_lcd_active();
	slcd_gpio_config();
	
	if ( controller_irq_handler )
	{
		// Set up IRQ for for Broadsheet HIRQ line.
		//
		disable_irq(CONTROLLER_HIRQ_IRQ);
		set_irq_type(CONTROLLER_HIRQ_IRQ, IRQF_TRIGGER_RISING);
		
		if ( request_irq(CONTROLLER_HIRQ_IRQ, controller_irq_handler, 0, controller_irq_handler_name, NULL) )
			result = CONTROLLER_COMMON_HIRQ_RQST_FAILURE;
		else
		{
			if ( mxc_request_iomux(CONTROLLER_HIRQ_LINE, MUX_CONFIG_GPIO) )
				result = CONTROLLER_COMMON_HIRQ_INIT_FAILURE;
			else
			{
				// Set HIRQ pin as input.
				//
				gpio_direction_output(IOMUX_TO_GPIO(CONTROLLER_HIRQ_LINE), 1);
				mxc_iomux_set_pad(CONTROLLER_HIRQ_LINE, CONTROLLER_PAD_EN);
			}
		}
	}
	
	if ( CONTROLLER_COMMON_GPIO_INIT_SUCCESS == result )
	{
		if ( mxc_request_iomux(CONTROLLER_HRST_LINE, MUX_CONFIG_GPIO) )
			result = CONTROLLER_COMMON_HRST_INIT_FAILURE;
		else
		{
			// Set HRST pin as an output and initialize it to zero (it's active LOW).
			//
			gpio_direction_output(IOMUX_TO_GPIO(CONTROLLER_HRST_LINE), 0);
			mxc_iomux_set_pad(CONTROLLER_HRST_LINE, CONTROLLER_PAD_EN);
			gpio_set_value(IOMUX_TO_GPIO(CONTROLLER_HRST_LINE), 0);
		}
	}
	
	if ( CONTROLLER_COMMON_GPIO_INIT_SUCCESS == result )
	{
		if ( mxc_request_iomux(CONTROLLER_HRDY_LINE, MUX_CONFIG_GPIO) )
			result = CONTROLLER_COMMON_HRDY_INIT_FAILURE;
		else
		{
			// Set HRDY pin as an input.
			//
			gpio_direction_output(IOMUX_TO_GPIO(CONTROLLER_HRDY_LINE), 1);
			mxc_iomux_set_pad(CONTROLLER_HRDY_LINE, CONTROLLER_PAD_EN);
		}
	}
	
	return ( result );
}

void controller_common_gpio_disable(int disable_bs_irq)
{
	if ( disable_bs_irq )
	{
		disable_irq(CONTROLLER_HIRQ_IRQ);
		free_irq(CONTROLLER_HIRQ_IRQ, NULL);
		
		mxc_iomux_set_pad(CONTROLLER_HIRQ_LINE, CONTROLLER_PAD_DIS);
		mxc_free_iomux(CONTROLLER_HIRQ_LINE, MUX_CONFIG_GPIO);
	}
	
	mxc_iomux_set_pad(CONTROLLER_HRST_LINE, CONTROLLER_PAD_DIS);
	mxc_free_iomux(CONTROLLER_HRST_LINE, MUX_CONFIG_GPIO);

	mxc_iomux_set_pad(CONTROLLER_HRDY_LINE, CONTROLLER_PAD_DIS);
	mxc_free_iomux(CONTROLLER_HRDY_LINE, MUX_CONFIG_GPIO);
}

void controller_common_reset(void)
{
	WR_GPIO_LINE(CONTROLLER_HRST_LINE, CONTROLLER_RESET_VAL);	// Assert RST.
	mdelay(100);	// Pause 100 ms during reset.
	WR_GPIO_LINE(CONTROLLER_HRST_LINE, CONTROLLER_NON_RESET_VAL);	// Clear RST.
	mdelay(400);	// Pause 400 ms to allow Broasheet time to come up.
}

int controller_common_ready(void)
{
	return ( mxc_get_gpio_datain(CONTROLLER_HRDY_LINE) );
}

EXPORT_SYMBOL(controller_common_gpio_config);
EXPORT_SYMBOL(controller_common_gpio_disable);
EXPORT_SYMBOL(controller_common_reset);
EXPORT_SYMBOL(controller_common_ready);

/*!
 * Setup pin for touchscreen
 */
void gpio_tsc_active(void)
{
	unsigned int pad_val = PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU;
	mxc_request_iomux(MX35_PIN_CAPTURE, MUX_CONFIG_GPIO);
	mxc_iomux_set_pad(MX35_PIN_CAPTURE, pad_val);
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_CAPTURE), 1);
}

/*!
 * Release pin for touchscreen
 */
void gpio_tsc_inactive(void)
{
	mxc_free_iomux(MX35_PIN_CAPTURE, MUX_CONFIG_GPIO);
}

void gpio_iomux_32khz(int enable)
{
	if (enable) {
		mxc_request_iomux(MX35_PIN_CAPTURE, MUX_CONFIG_ALT4);
		mxc_iomux_set_pad(MX35_PIN_CAPTURE,
				PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE |
				PAD_CTL_100K_PU | PAD_CTL_PUE_PUD);
		mxc_iomux_set_input(MUX_IN_CCM_32K_MUXED, INPUT_CTL_PATH0);
	}
	else {
		mxc_free_iomux(MX35_PIN_CAPTURE, MUX_CONFIG_ALT4);
	}
}

/*
 * GPIO definitions for the LUIGI fiveway device
 */

#define FIVEWAY_up_gpio        MX35_PIN_ATA_DATA14
#define FIVEWAY_down_gpio      MX35_PIN_ATA_DATA15
#define FIVEWAY_left_gpio      MX35_PIN_TX5_RX0
#define FIVEWAY_right_gpio     MX35_PIN_ATA_BUFF_EN
#define FIVEWAY_select_gpio    MX35_PIN_ATA_DMARQ
#define FIVEWAY_pad_enable     PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD | \
                               PAD_CTL_DRV_1_8V | PAD_CTL_100K_PU
#define FIVEWAY_pad_disable    PAD_CTL_PKE_ENABLE


void gpio_fiveway_inactive(void)
{
	mxc_iomux_set_pad(FIVEWAY_up_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_down_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_left_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_right_gpio, FIVEWAY_pad_disable);
	mxc_iomux_set_pad(FIVEWAY_select_gpio, FIVEWAY_pad_disable);
	gpio_free(IOMUX_TO_GPIO(FIVEWAY_up_gpio));
	gpio_free(IOMUX_TO_GPIO(FIVEWAY_down_gpio));
	gpio_free(IOMUX_TO_GPIO(FIVEWAY_left_gpio));
	gpio_free(IOMUX_TO_GPIO(FIVEWAY_right_gpio));
	gpio_free(IOMUX_TO_GPIO(FIVEWAY_select_gpio));
}
EXPORT_SYMBOL(gpio_fiveway_inactive);


#define SET_FIVEWAY_GPIO(gpio, select, path) \
	err = mxc_request_iomux(gpio, MUX_CONFIG_GPIO); \
	if (err) { \
	   goto disable_fiveway_gpios; \
	} \
	gpio_direction_output(IOMUX_TO_GPIO(gpio), 1); \
	mxc_iomux_set_pad(gpio, FIVEWAY_pad_enable); \
	mxc_iomux_set_input(select, path);

int gpio_fiveway_active(void)
{
	int err;

	SET_FIVEWAY_GPIO(FIVEWAY_up_gpio, MUX_IN_GPIO2_IN_27,
	                 INPUT_CTL_PATH1);
	SET_FIVEWAY_GPIO(FIVEWAY_down_gpio, MUX_IN_GPIO2_IN_28,
	                 INPUT_CTL_PATH1);
	SET_FIVEWAY_GPIO(FIVEWAY_left_gpio, MUX_IN_GPIO1_IN_10,
	                 INPUT_CTL_PATH0);
	SET_FIVEWAY_GPIO(FIVEWAY_right_gpio, MUX_IN_GPIO2_IN_30,
	                 INPUT_CTL_PATH1);
	SET_FIVEWAY_GPIO(FIVEWAY_select_gpio, MUX_IN_GPIO2_IN_31,
	                 INPUT_CTL_PATH1);

	return err;

disable_fiveway_gpios:
	gpio_fiveway_inactive();
	return err;
}
EXPORT_SYMBOL(gpio_fiveway_active);


int fiveway_datain(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	   case 0: gpio = FIVEWAY_up_gpio;
	   break;
	   case 1: gpio = FIVEWAY_down_gpio;
	   break;
	   case 2: gpio = FIVEWAY_left_gpio;
	   break;
	   case 3: gpio = FIVEWAY_right_gpio;
	   break;
	   case 4: gpio = FIVEWAY_select_gpio;
	   break;
	   default: return err;
	}
	return mxc_get_gpio_datain(gpio);
}
EXPORT_SYMBOL(fiveway_datain);


int fiveway_get_irq(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	   case 0: gpio = FIVEWAY_up_gpio;
	   break;
	   case 1: gpio = FIVEWAY_down_gpio;
	   break;
	   case 2: gpio = FIVEWAY_left_gpio;
	   break;
	   case 3: gpio = FIVEWAY_right_gpio;
	   break;
	   case 4: gpio = FIVEWAY_select_gpio;
	   break;
	   default: return err;
	}
	return IOMUX_TO_IRQ(gpio);
}
EXPORT_SYMBOL(fiveway_get_irq);


/*
 * GPIO definitions for the LUIGI Volume keys
 */

#define VOLUME_UP_gpio        MX35_PIN_SCKR
#define VOLUME_DOWN_gpio      MX35_PIN_FSR

#define VOLUME_pad_enable     PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD | \
                              PAD_CTL_DRV_1_8V | PAD_CTL_100K_PU | \
                              PAD_CTL_HYS_SCHMITZ
#define VOLUME_pad_disable    PAD_CTL_PKE_ENABLE


void gpio_volume_inactive(void)
{
	mxc_iomux_set_pad(VOLUME_UP_gpio, VOLUME_pad_disable);
	mxc_iomux_set_pad(VOLUME_DOWN_gpio, VOLUME_pad_disable);
	gpio_free(IOMUX_TO_GPIO(VOLUME_UP_gpio));
	gpio_free(IOMUX_TO_GPIO(VOLUME_DOWN_gpio));
}
EXPORT_SYMBOL(gpio_volume_inactive);


#define SET_VOLUME_GPIO(gpio, select, path) \
	err = mxc_request_iomux(gpio, MUX_CONFIG_GPIO); \
	if (err) { \
	   goto disable_volume_gpios; \
	} \
	gpio_direction_output(IOMUX_TO_GPIO(gpio), 1); \
	mxc_iomux_set_pad(gpio, VOLUME_pad_enable); \
	if (select != (iomux_input_select_t)NULL) { \
		mxc_iomux_set_input(select, path); \
	}

int gpio_volume_active(void)
{
	int err;

	SET_VOLUME_GPIO(VOLUME_UP_gpio, MUX_IN_GPIO1_IN_4, INPUT_CTL_PATH1);
	SET_VOLUME_GPIO(VOLUME_DOWN_gpio, MUX_IN_GPIO1_IN_5, INPUT_CTL_PATH1);

	return err;

disable_volume_gpios:
	gpio_volume_inactive();
	return err;
}
EXPORT_SYMBOL(gpio_volume_active);


int volume_datain(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	   case 0: gpio = VOLUME_UP_gpio;
	   break;
	   case 1: gpio = VOLUME_DOWN_gpio;
	   break;
	   default: return err;
	}
	return mxc_get_gpio_datain(gpio);
}
EXPORT_SYMBOL(volume_datain);


int volume_get_irq(int line_direction)
{
	iomux_pin_name_t gpio;
	int err = -1;

	switch (line_direction) {
	   case 0: gpio = VOLUME_UP_gpio;
	   break;
	   case 1: gpio = VOLUME_DOWN_gpio;
	   break;
	   default: return err;
	}
	return IOMUX_TO_IRQ(gpio);
}
EXPORT_SYMBOL(volume_get_irq);


/*!
 * Luigi keypad pin definitions
 */

#define KB_PIN_COL0	MX35_PIN_TX2_RX3
#define KB_PIN_COL1	MX35_PIN_TX1
#define KB_PIN_COL2	MX35_PIN_TX0
#define KB_PIN_COL3	MX35_PIN_HCKT
#define KB_PIN_COL6	MX35_PIN_RTS1
#define KB_PIN_COL7	MX35_PIN_CTS1
#define KB_PIN_ROW0	MX35_PIN_TX4_RX1
#define KB_PIN_ROW1	MX35_PIN_TX3_RX2
#define KB_PIN_ROW2	MX35_PIN_SCKT
#define KB_PIN_ROW3	MX35_PIN_FST
#define KB_PIN_ROW4	MX35_PIN_RXD2
#define KB_PIN_ROW5	MX35_PIN_TXD2
#define KB_PIN_ROW6	MX35_PIN_RTS2
#define KB_PIN_ROW7	MX35_PIN_CTS2
#define KB_PAD_ENABLE   PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD | \
                        PAD_CTL_DRV_1_8V | PAD_CTL_100K_PU
#define KB_PAD_DISABLE  PAD_CTL_PKE_ENABLE


/*!
 * Release Keypad pins
 */
void gpio_keypad_inactive(void)
{
	mxc_iomux_set_pad(KB_PIN_ROW7, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW6, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW5, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW4, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW3, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW2, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW1, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_ROW0, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_COL7, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_COL6, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_COL3, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_COL2, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_COL1, KB_PAD_DISABLE);
	mxc_iomux_set_pad(KB_PIN_COL0, KB_PAD_DISABLE);
	mxc_free_iomux(KB_PIN_ROW7, MUX_CONFIG_ALT4);
	mxc_free_iomux(KB_PIN_ROW6, MUX_CONFIG_ALT4);
	mxc_free_iomux(KB_PIN_ROW5, MUX_CONFIG_ALT4);
	mxc_free_iomux(KB_PIN_ROW4, MUX_CONFIG_ALT4);
	mxc_free_iomux(KB_PIN_ROW3, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_ROW2, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_ROW1, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_ROW0, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_COL7, MUX_CONFIG_ALT4);
	mxc_free_iomux(KB_PIN_COL6, MUX_CONFIG_ALT4);
	mxc_free_iomux(KB_PIN_COL3, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_COL2, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_COL1, MUX_CONFIG_ALT7);
	mxc_free_iomux(KB_PIN_COL0, MUX_CONFIG_ALT7);
}

EXPORT_SYMBOL(gpio_keypad_inactive);


#define SET_KB_FUNC(gpio, config, select, path) \
	err = mxc_request_iomux(gpio, config); \
	if (err) { \
	   goto disable_kb_gpios; \
	} \
	mxc_iomux_set_pad(gpio, KB_PAD_ENABLE); \
	mxc_iomux_set_input(select, path);

/*!
 * Setup Keypad pins for their special function
 *
 */
int gpio_keypad_active(void)
{
	int err;

	SET_KB_FUNC(KB_PIN_COL0, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_COL_0, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_COL1, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_COL_1, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_COL2, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_COL_2, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_COL3, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_COL_3, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_COL6, MUX_CONFIG_ALT4,
	            MUX_IN_KPP_COL_6, INPUT_CTL_PATH0);
	SET_KB_FUNC(KB_PIN_COL7, MUX_CONFIG_ALT4,
	            MUX_IN_KPP_COL_7, INPUT_CTL_PATH0);
	SET_KB_FUNC(KB_PIN_ROW0, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_ROW_0, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_ROW1, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_ROW_1, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_ROW2, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_ROW_2, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_ROW3, MUX_CONFIG_ALT7,
	            MUX_IN_KPP_ROW_3, INPUT_CTL_PATH1);
	SET_KB_FUNC(KB_PIN_ROW4, MUX_CONFIG_ALT4,
	            MUX_IN_KPP_ROW_4, INPUT_CTL_PATH0);
	SET_KB_FUNC(KB_PIN_ROW5, MUX_CONFIG_ALT4,
	            MUX_IN_KPP_ROW_5, INPUT_CTL_PATH0);
	SET_KB_FUNC(KB_PIN_ROW6, MUX_CONFIG_ALT4,
	            MUX_IN_KPP_ROW_6, INPUT_CTL_PATH0);
	SET_KB_FUNC(KB_PIN_ROW7, MUX_CONFIG_ALT4,
	            MUX_IN_KPP_ROW_7, INPUT_CTL_PATH0);

	return err;

disable_kb_gpios:
	gpio_keypad_inactive();
	return err;
}

EXPORT_SYMBOL(gpio_keypad_active);

/* 
 * Get the SDIO DATA1 IRQ
 */
int gpio_sdio_irq(void)
{
	return IOMUX_TO_IRQ(MX35_PIN_SD2_DATA1);
}
EXPORT_SYMBOL(gpio_sdio_irq);

/*
 * Configure SDIO Data1 line to be an IRQ
 */
void gpio_sdio_data1(int enable)
{
	int pad_val = 0;

	if (enable) {
		mxc_request_iomux(MX35_PIN_SD2_DATA1, MUX_CONFIG_ALT5);
		mxc_iomux_set_input(MUX_IN_GPIO2_IN_3, INPUT_CTL_PATH1);
		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_100K_PU | PAD_CTL_PUE_PUD |
				PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_SCHMITZ |
				PAD_CTL_SRE_SLOW | PAD_CTL_DRV_1_8V;
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA1, pad_val);
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_SD2_DATA1), 1);
	}
	else {
		mxc_free_iomux(MX35_PIN_SD2_DATA1, MUX_CONFIG_ALT5);
	}
}
EXPORT_SYMBOL(gpio_sdio_data1);

/*!
 * Setup GPIO for SDHC to be active
 *
 * @param module SDHC module number
 */
void gpio_sdhc_active(int module)
{
	unsigned int pad_val;

	switch (module) {
	case 0:
		mxc_request_iomux(MX35_PIN_SD1_CLK, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD1_CMD, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD1_DATA0, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD1_DATA1, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD1_DATA2, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD1_DATA3, MUX_CONFIG_FUNC | MUX_CONFIG_SION);

#ifndef CONFIG_FEC
		/* DAT4 - DAT7 Data lines for MMC 8-bit bus width */
		mxc_request_iomux(MX35_PIN_FEC_TX_CLK, MUX_CONFIG_ALT1);
		mxc_request_iomux(MX35_PIN_FEC_RX_CLK, MUX_CONFIG_ALT1);
		mxc_request_iomux(MX35_PIN_FEC_RX_DV, MUX_CONFIG_ALT1);
		mxc_request_iomux(MX35_PIN_FEC_COL, MUX_CONFIG_ALT1);
#endif
		/* Configure the pads on SD1_CLK */
		mxc_iomux_set_pad(MX35_PIN_SD1_CLK, PAD_CTL_DRV_HIGH | PAD_CTL_47K_PU |
				PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_CMOS | PAD_CTL_ODE_CMOS |
				PAD_CTL_PUE_PUD | PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V);

		pad_val = PAD_CTL_DRV_MAX | PAD_CTL_47K_PU | 
				PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
				PAD_CTL_HYS_SCHMITZ |
				PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

		mxc_iomux_set_pad(MX35_PIN_SD1_CMD, pad_val);
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA0, pad_val);
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA1, pad_val);
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA2, pad_val);

#ifndef CONFIG_FEC
		/* DAT4 to DAT7 IOMUX setting */
		mxc_iomux_set_pad(MX35_PIN_FEC_TX_CLK, pad_val);
		mxc_iomux_set_pad(MX35_PIN_FEC_RX_CLK, pad_val);
		mxc_iomux_set_pad(MX35_PIN_FEC_RX_DV, pad_val);
		mxc_iomux_set_pad(MX35_PIN_FEC_COL, pad_val);

		mxc_iomux_set_input(MUX_IN_ESDHC1_DAT4_IN, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC1_DAT5_IN, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC1_DAT6_IN, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC1_DAT7_IN, INPUT_CTL_PATH1);
#endif

		pad_val = PAD_CTL_DRV_MAX | PAD_CTL_100K_PU |
				PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
				PAD_CTL_HYS_SCHMITZ |
				PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

		mxc_iomux_set_pad(MX35_PIN_SD1_DATA3, pad_val);

		mdelay(100); /* Setting time */

                break;
	case 1:
		/*
		 * ESDHC2 - Atheros WiFi
		 */
		mxc_request_iomux(MX35_PIN_SD2_CLK, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD2_CMD, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD2_DATA0, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD2_DATA1, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD2_DATA2, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_SD2_DATA3, MUX_CONFIG_FUNC | MUX_CONFIG_SION);

		/* Configure the SD2_CLK */
		mxc_iomux_set_pad(MX35_PIN_SD2_CLK, PAD_CTL_DRV_HIGH | PAD_CTL_47K_PU |
				PAD_CTL_PKE_ENABLE | PAD_CTL_HYS_CMOS | PAD_CTL_ODE_CMOS |
				PAD_CTL_PUE_PUD | PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V);

		pad_val = PAD_CTL_DRV_MAX | PAD_CTL_47K_PU | 
				PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
				PAD_CTL_HYS_SCHMITZ |
				PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

		mxc_iomux_set_pad(MX35_PIN_SD2_CMD, pad_val);
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA0, pad_val);
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA1, pad_val);
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA2, pad_val);

		pad_val = PAD_CTL_DRV_MAX | PAD_CTL_100K_PU |
				PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
				PAD_CTL_HYS_SCHMITZ |
				PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

		mxc_iomux_set_pad(MX35_PIN_SD2_DATA3, pad_val);

		mdelay(100); /* Setting time */

                break;
	case 2:
#ifdef CONFIG_FEC
		return;
#endif

		/*
		 * ESDHC3
		 */
		mxc_request_iomux(MX35_PIN_ATA_DATA3, MUX_CONFIG_ALT1 | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_ATA_DATA4, MUX_CONFIG_ALT1 | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_ATA_DIOR, MUX_CONFIG_ALT1 | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_ATA_DIOW, MUX_CONFIG_ALT1 | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_ATA_DMACK, MUX_CONFIG_ALT1 | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_ATA_RESET_B, MUX_CONFIG_ALT1 | MUX_CONFIG_SION);
		mxc_request_iomux(MX35_PIN_FEC_TDATA0, MUX_CONFIG_ALT5);
		mxc_request_iomux(MX35_PIN_FEC_TX_EN, MUX_CONFIG_ALT5);

		mxc_iomux_set_input(MUX_IN_ESDHC3_CARD_CLK_IN, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC3_CMD_IN, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC3_DAT0, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC3_DAT1, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC3_DAT2, INPUT_CTL_PATH1);
		mxc_iomux_set_input(MUX_IN_ESDHC3_DAT3, INPUT_CTL_PATH1);

		/*
		 * Card Detect
		 */
		pad_val = PAD_CTL_100K_PU;
		mxc_iomux_set_pad(MX35_PIN_FEC_TX_EN, pad_val);
		mxc_iomux_set_input(MUX_IN_GPIO3_IN_12, INPUT_CTL_PATH1);

		/* WP */
		mxc_iomux_set_pad(MX35_PIN_FEC_TDATA0, pad_val);
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_FEC_TDATA0), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_FEC_TDATA0), 1);

		pad_val = PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
		    PAD_CTL_HYS_SCHMITZ | PAD_CTL_DRV_MAX |
		    PAD_CTL_47K_PU | PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

		mxc_iomux_set_pad(MX35_PIN_ATA_RESET_B, pad_val);

		pad_val = PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
                    PAD_CTL_HYS_SCHMITZ | PAD_CTL_DRV_MAX |
                    PAD_CTL_47K_PU | PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

		mxc_iomux_set_pad(MX35_PIN_ATA_DATA4, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DIOR, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DIOW, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DMACK, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA3, pad_val);

		mdelay(100); /* Setting time */

		break;	
	default:
		break;
	}

}

EXPORT_SYMBOL(gpio_sdhc_active);


/* 
 * WiFi Power Configure
 */
static void gpio_wifi_power(int enable)
{
	unsigned int pad_val;

	if (enable == 1) {
		mxc_request_iomux(MX35_PIN_ATA_DATA5, MUX_CONFIG_ALT5);
		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW |
			PAD_CTL_DRV_1_8V | PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
			PAD_CTL_100K_PU | PAD_CTL_ODE_OpenDrain;
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA5, pad_val);
	}
	else {
		mxc_free_iomux(MX35_PIN_ATA_DATA5, MUX_CONFIG_ALT5);
		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW;
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA5, pad_val);
	}
}

/* Hook to reset the WiFi line */
void gpio_wifi_do_reset(void)
{
	/* Set the direction to output */
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 0);

	/* Pull the line low */
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 0);

	mdelay(10);

	/* Pull the line high */
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 1);
	
	mdelay(10);
}
EXPORT_SYMBOL(gpio_wifi_do_reset);

/*
 * WiFi Reset Configure
 */
static void gpio_wifi_reset(int enable)
{
	unsigned int pad_val;

	if (enable == 1) {
		mxc_request_iomux(MX35_PIN_ATA_DATA7, MUX_CONFIG_ALT5);
		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW |
			PAD_CTL_DRV_1_8V | PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
			PAD_CTL_100K_PU | PAD_CTL_ODE_OpenDrain;
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA7, pad_val);
	}
	else {
		mxc_free_iomux(MX35_PIN_ATA_DATA7, MUX_CONFIG_ALT5);
		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW;
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA7, pad_val);
	}
}

void gpio_wifi_enable(int enable)
{
	if (enable == 1) {
		gpio_wifi_power(1);
		msleep(10); /* Settling time */
		gpio_wifi_reset(1);

		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA5), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA5), 1);
		mdelay(10); /* Settling time */
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 1);
	} else {
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA5), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA5), 0);
		mdelay(10); /* Settling time */
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA7), 0);
		gpio_wifi_power(0);
		msleep(10); /* Settling time */
		gpio_wifi_reset(0);
	}

}
EXPORT_SYMBOL(gpio_wifi_enable);

void gpio_wifi_power_enable(int enable)
{
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA5), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA5), enable);
}
EXPORT_SYMBOL(gpio_wifi_power_enable);

static int wan_gpios_configured = 0;

void gpio_wan_configure_gpios (void)
{
	unsigned int pad_val;
	mxc_iomux_set_input(MUX_IN_GPIO2_IN_14, INPUT_CTL_PATH1);

	/* set up wan power pad, gpio direction, and initial state */
	mxc_request_iomux(MX35_PIN_ATA_DATA1, MUX_CONFIG_ALT5);
	pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW |
		PAD_CTL_DRV_1_8V | PAD_CTL_PKE_NONE |
		PAD_CTL_ODE_CMOS; 
	mxc_iomux_set_pad(MX35_PIN_ATA_DATA1, pad_val);
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA1), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA1), 0);

	/* set up wan rf enable pad, gpio direction, and initial state */
	mxc_request_iomux(MX35_PIN_CSPI1_SS1, MUX_CONFIG_ALT5);
	pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW |
		PAD_CTL_DRV_1_8V | PAD_CTL_PKE_NONE |
		PAD_CTL_ODE_OpenDrain;
	mxc_iomux_set_pad(MX35_PIN_CSPI1_SS1, pad_val);
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_CSPI1_SS1), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSPI1_SS1), 0);

	/* set up wan usb enable pad, gpio direction, and initial state */
	mxc_request_iomux(MX35_PIN_GPIO2_0, MUX_CONFIG_FUNC);
	pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW |
		PAD_CTL_DRV_1_8V | PAD_CTL_PKE_NONE |
		PAD_CTL_ODE_CMOS;
	mxc_iomux_set_pad(MX35_PIN_GPIO2_0, pad_val);
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_GPIO2_0), 0);
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_GPIO2_0), 0);

	wan_gpios_configured = 1;
}

void gpio_wan_power(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA1), enable ? 1 : 0);
}

EXPORT_SYMBOL(gpio_wan_power);

void gpio_wan_rf_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_CSPI1_SS1), enable ? 1 : 0);
}

EXPORT_SYMBOL(gpio_wan_rf_enable);

void gpio_wan_usb_enable(int enable)
{
	gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_GPIO2_0), enable ? 1 : 0);
}

EXPORT_SYMBOL(gpio_wan_usb_enable);

void gpio_wan_experiment_enable(int enable)
{
	if (!wan_gpios_configured) {
		gpio_wan_configure_gpios();
	}
	gpio_wan_power(enable);
	gpio_wan_rf_enable(enable);
	gpio_wan_usb_enable(enable);
}

EXPORT_SYMBOL(gpio_wan_experiment_enable);

void gpio_wan_exit(void *tph_event_callback)
{
	/* clear the PMIC event handler */
	pmic_power_event_unsub(PWR_IT_ONOFD2I, tph_event_callback);
}

EXPORT_SYMBOL(gpio_wan_exit);

void gpio_wan_init(void *tph_event_callback)
{
	int error = 0;

	/* set up the GPIO lines */
	gpio_wan_configure_gpios();

	/* set up the PMIC event configuration for the "ON2B" line */
	pmic_power_set_conf_button(BT_ON2B, 0, 0);
	error = pmic_power_event_sub(PWR_IT_ONOFD2I, tph_event_callback);
	if (error < 0)
		printk(KERN_ERR "Could not subscribe to the WAN TPH handler : %d\n", error);
}

EXPORT_SYMBOL(gpio_wan_init);

static ssize_t wan_enable_store(struct sys_device *dev, const char *buf,
				size_t size)
{
	if (strstr(buf, "1") != NULL)
		gpio_wan_experiment_enable(1);
	else if (strstr(buf, "0") != NULL)
		gpio_wan_experiment_enable(0);

	return size;
}

static SYSDEV_ATTR(wan_enable, 0644, NULL, wan_enable_store);

static struct sysdev_class wan_sysclass = {
	.name	= "wan",	
};

static struct sys_device wan_device = {
	.id = 0,
	.cls = &wan_sysclass,
};

int wan_sysdev_ctrl_init(void)
{
	int err;

	err = sysdev_class_register(&wan_sysclass);
	if (!err)
		err = sysdev_register(&wan_device);
	if (!err)
		err = sysdev_create_file(&wan_device, &attr_wan_enable);

	return err;
}
EXPORT_SYMBOL(wan_sysdev_ctrl_init);

void wan_sysdev_ctrl_exit(void)
{
	sysdev_remove_file(&wan_device, &attr_wan_enable);
	sysdev_unregister(&wan_device);
	sysdev_class_unregister(&wan_sysclass);
}
EXPORT_SYMBOL(wan_sysdev_ctrl_exit);


/*!
 * Setup GPIO for SDHC1 to be inactive
 *
 * @param module SDHC module number
 */
void gpio_sdhc_inactive(int module)
{
	unsigned int pad_val;

	switch (module) {
	case 0:
		mxc_free_iomux(MX35_PIN_SD1_CLK, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_free_iomux(MX35_PIN_SD1_CMD, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_free_iomux(MX35_PIN_SD1_DATA0, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_SD1_DATA1, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_SD1_DATA2, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_SD1_DATA3, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_FEC_TX_CLK, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_FEC_RX_CLK, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_FEC_RX_DV, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_FEC_COL, MUX_CONFIG_ALT1);

		mxc_iomux_set_pad(MX35_PIN_SD1_CLK,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_SD1_CMD,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA0,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA1,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA2,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_SD1_DATA3,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));

		mxc_iomux_set_pad(MX35_PIN_FEC_TX_CLK,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_FEC_RX_CLK,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_FEC_RX_DV,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		mxc_iomux_set_pad(MX35_PIN_FEC_COL,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW | PAD_CTL_100K_PD));
		break;
	case 1:
		mxc_free_iomux(MX35_PIN_SD2_CLK, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_free_iomux(MX35_PIN_SD2_CMD, MUX_CONFIG_FUNC | MUX_CONFIG_SION);
		mxc_free_iomux(MX35_PIN_SD2_DATA0, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_SD2_DATA1, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_SD2_DATA2, MUX_CONFIG_FUNC);
		mxc_free_iomux(MX35_PIN_SD2_DATA3, MUX_CONFIG_FUNC);

		mxc_iomux_set_pad(MX35_PIN_SD2_CLK,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW));
		mxc_iomux_set_pad(MX35_PIN_SD2_CMD,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW));
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA0,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW));
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA1,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW));
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA2,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW));
		mxc_iomux_set_pad(MX35_PIN_SD2_DATA3,
					(PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW));
		break;
	case 2:
		/*
		 * ESDHC3
		 */
		mxc_free_iomux(MX35_PIN_ATA_DATA3, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_ATA_DATA4, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_ATA_DIOR, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_ATA_DIOW, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_ATA_DMACK, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_ATA_RESET_B, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_FEC_TDATA0, MUX_CONFIG_ALT5);
		mxc_free_iomux(MX35_PIN_FEC_TX_EN, MUX_CONFIG_ALT5);

		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW;

		mxc_iomux_set_pad(MX35_PIN_FEC_TX_EN, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_RESET_B, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA4, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DIOR, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DIOW, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DMACK, pad_val);
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA3, pad_val);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(gpio_sdhc_inactive);

/*
 * Probe for the card. If present the GPIO data would be set.
 */
unsigned int sdhc_get_card_det_status(struct device *dev)
{
	if (to_platform_device(dev)->id == 0)
		return 0;
	else {
#ifdef CONFIG_FEC
		return -1;
#else
		return mxc_get_gpio_datain(MX35_PIN_FEC_TX_EN);
#endif
	}
}

EXPORT_SYMBOL(sdhc_get_card_det_status);

/*!
 * Get pin value to detect write protection
 */
int sdhc_write_protect(struct device *dev)
{
	unsigned int rc = 0;

	return rc;
}

EXPORT_SYMBOL(sdhc_write_protect);

/*!
 * This function configures the IOMux block for PMIC standard operations.
 *
 */
void gpio_pmic_active(void)
{
	unsigned int pad_val;

	/*
	 * PMIC IRQ
	 */
	mxc_request_iomux(MX35_PIN_ATA_IORDY, MUX_CONFIG_ALT5);
	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_IORDY), 1);
	pad_val = PAD_CTL_100K_PD;
	mxc_iomux_set_pad(MX35_PIN_ATA_IORDY, pad_val);

	mxc_iomux_set_input(MUX_IN_GPIO2_IN_12, INPUT_CTL_PATH1);	
}

/*
 *  USB Host2
 */
int gpio_usbh2_active(void)
{
	unsigned int pad_val = PAD_CTL_DRV_MAX | PAD_CTL_100K_PU |
				PAD_CTL_PUE_PUD | PAD_CTL_PKE_ENABLE |
				PAD_CTL_HYS_SCHMITZ |
				PAD_CTL_SRE_FAST | PAD_CTL_DRV_1_8V;

	mxc_iomux_set_input(MUX_IN_USB_UH2_USB_OC, INPUT_CTL_PATH2);
#ifndef CONFIG_FEC
	mxc_iomux_set_pad(MX35_PIN_FEC_RDATA1, pad_val);
#endif
	return 0;
}

EXPORT_SYMBOL(gpio_usbh2_active);

void gpio_usbh2_inactive(void)
{
	/*
	 * Do Nothing
	 */
}

EXPORT_SYMBOL(gpio_usbh2_inactive);

/*
 *  USB OTG UTMI
 */
int gpio_usbotg_utmi_active(void)
{
	/*
	 * Do Nothing as the Gadget is driven by Atlas	
	 */
	return 0;
}

EXPORT_SYMBOL(gpio_usbotg_utmi_active);

void gpio_usbotg_utmi_inactive(void)
{
	/*
	 * Do nothing as the gadget is driven by Atlas
	 */
}

EXPORT_SYMBOL(gpio_usbotg_utmi_inactive);

void gpio_sensor_active(void)
{
	/*CSI D6 */
	mxc_request_iomux(MX35_PIN_TX1, MUX_CONFIG_ALT6);
	/*CSI D7 */
	mxc_request_iomux(MX35_PIN_TX0, MUX_CONFIG_ALT6);
	mxc_request_iomux(MX35_PIN_CSI_D8, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D9, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D10, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D11, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D12, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D13, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D14, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_D15, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_HSYNC, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_MCLK, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_PIXCLK, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_CSI_VSYNC, MUX_CONFIG_FUNC);
}

EXPORT_SYMBOL(gpio_sensor_active);

void gpio_sensor_inactive(void)
{
	mxc_request_iomux(MX35_PIN_TX1, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_TX0, MUX_CONFIG_FUNC);
}

EXPORT_SYMBOL(gpio_sensor_inactive);

/*!
 * Setup GPIO for spdif tx/rx to be active
 */
void gpio_spdif_active(void)
{
	/* SPDIF OUT */
	mxc_request_iomux(MX35_PIN_STXD5, MUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX35_PIN_STXD5, PAD_CTL_PKE_NONE | PAD_CTL_PUE_PUD);
	/* SPDIF IN */
	mxc_request_iomux(MX35_PIN_SRXD5, MUX_CONFIG_ALT1);
	mxc_iomux_set_pad(MX35_PIN_SRXD5, PAD_CTL_PKE_ENABLE
			  | PAD_CTL_100K_PU | PAD_CTL_HYS_SCHMITZ);
	/* SPDIF ext clock */
	mxc_request_iomux(MX35_PIN_SCK5, MUX_CONFIG_ALT1);
}

EXPORT_SYMBOL(gpio_spdif_active);

/*!
 * Setup GPIO for spdif tx/rx to be inactive
 */
void gpio_spdif_inactive(void)
{
	/* SPDIF OUT */
	mxc_free_iomux(MX35_PIN_STXD5, MUX_CONFIG_GPIO);
	/* SPDIF IN */
	mxc_free_iomux(MX35_PIN_SRXD5, MUX_CONFIG_GPIO);
	/* SPDIF ext clock */
	mxc_free_iomux(MX35_PIN_SCK5, MUX_CONFIG_GPIO);
}

EXPORT_SYMBOL(gpio_spdif_inactive);

/*!
 * This function activates DAM ports 3 to enable
 * audio I/O.
 */
void gpio_activate_audio_ports(void)
{
	unsigned int pad_val;

	mxc_request_iomux(MX35_PIN_STXD4, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_SRXD4, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_SCK4, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_STXFS4, MUX_CONFIG_FUNC);

	/* Audio GPIO for headset detect */
	mxc_request_iomux(MX35_PIN_CSI_PIXCLK, MUX_CONFIG_ALT5);

	pad_val = PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU |
	    PAD_CTL_PUE_PUD | PAD_CTL_DRV_1_8V;

	mxc_iomux_set_pad(MX35_PIN_STXD4, pad_val);
	mxc_iomux_set_pad(MX35_PIN_SRXD4, pad_val);
	mxc_iomux_set_pad(MX35_PIN_SCK4, pad_val);
	mxc_iomux_set_pad(MX35_PIN_STXFS4, pad_val);
	mxc_iomux_set_pad(MX35_PIN_CSI_PIXCLK, pad_val);

	gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_CSI_PIXCLK), 1);
}

EXPORT_SYMBOL(gpio_activate_audio_ports);

int gpio_headset_irq(void)
{
	return IOMUX_TO_IRQ(MX35_PIN_CSI_PIXCLK);
}

EXPORT_SYMBOL(gpio_headset_irq);

int gpio_headset_status(void)
{
	return mxc_get_gpio_datain(MX35_PIN_CSI_PIXCLK);
}

EXPORT_SYMBOL(gpio_headset_status);

/*!
 * This function activates ESAI ports to enable
 * surround sound I/O
 */
void gpio_activate_esai_ports(void)
{
	unsigned int pad_val;
	/* ESAI TX - WM8580 */
	mxc_request_iomux(MX35_PIN_HCKT, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_SCKT, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_FST, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_TX0, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_TX1, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_TX2_RX3, MUX_CONFIG_FUNC);

	/* ESAI RX - AK5702 */
	/*mxc_request_iomux(MX35_PIN_HCKR, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_SCKR, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_FSR, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_TX3_RX2, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_TX4_RX1, MUX_CONFIG_FUNC);*/

	pad_val = PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU |
	    PAD_CTL_PUE_PUD;
	/* ESAI TX - WM8580 */
	mxc_iomux_set_pad(MX35_PIN_SCKT, pad_val);
	mxc_iomux_set_pad(MX35_PIN_FST, pad_val);
	mxc_iomux_set_pad(MX35_PIN_TX0, pad_val);
	mxc_iomux_set_pad(MX35_PIN_TX1, pad_val);
	mxc_iomux_set_pad(MX35_PIN_TX2_RX3, pad_val);

	/* ESAI RX - AK5702 */
	/*mxc_iomux_set_pad(MX35_PIN_SCKR, pad_val);
	mxc_iomux_set_pad(MX35_PIN_FSR, pad_val);
	mxc_iomux_set_pad(MX35_PIN_TX3_RX2, pad_val);
	mxc_iomux_set_pad(MX35_PIN_TX4_RX1, pad_val);*/

	pad_val =
	    PAD_CTL_DRV_HIGH | PAD_CTL_PKE_ENABLE | PAD_CTL_100K_PU |
	    PAD_CTL_PUE_PUD;

	/* ESAI TX - WM8580 */
	mxc_iomux_set_pad(MX35_PIN_HCKT, pad_val);
	/* ESAI RX - AK5702 */
	/*mxc_iomux_set_pad(MX35_PIN_HCKR, pad_val);*/
}

EXPORT_SYMBOL(gpio_activate_esai_ports);

/*!
 * This function deactivates ESAI ports to disable
 * surround sound I/O
 */
void gpio_deactivate_esai_ports(void)
{

	mxc_free_iomux(MX35_PIN_HCKT, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_SCKT, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_FST, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_TX0, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_TX1, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_TX2_RX3, MUX_CONFIG_GPIO);
	/*mxc_free_iomux(MX35_PIN_HCKR, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_SCKR, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_FSR, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_TX3_RX2, MUX_CONFIG_GPIO);
	mxc_free_iomux(MX35_PIN_TX4_RX1, MUX_CONFIG_GPIO);*/
}

EXPORT_SYMBOL(gpio_deactivate_esai_ports);

/*!
 * The MLB gpio configuration routine
 */
void gpio_mlb_active(void)
{
	mxc_request_iomux(MX35_PIN_MLB_CLK, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_MLB_SIG, MUX_CONFIG_FUNC);
	mxc_request_iomux(MX35_PIN_MLB_DAT, MUX_CONFIG_FUNC);
}

EXPORT_SYMBOL(gpio_mlb_active);

void gpio_mlb_inactive(void)
{
	mxc_free_iomux(MX35_PIN_MLB_CLK, MUX_CONFIG_FUNC);
	mxc_free_iomux(MX35_PIN_MLB_SIG, MUX_CONFIG_FUNC);
	mxc_free_iomux(MX35_PIN_MLB_DAT, MUX_CONFIG_FUNC);
}

EXPORT_SYMBOL(gpio_mlb_inactive);

void gpio_can_active(int id)
{
	int pad;

	switch (id) {
	case 0:
		pad = PAD_CTL_HYS_SCHMITZ | PAD_CTL_PKE_ENABLE | \
		    PAD_CTL_PUE_PUD | PAD_CTL_100K_PU | PAD_CTL_ODE_OpenDrain;
		mxc_request_iomux(MX35_PIN_I2C2_CLK, MUX_CONFIG_ALT1);
		mxc_request_iomux(MX35_PIN_I2C2_DAT, MUX_CONFIG_ALT1);
		mxc_iomux_set_pad(MX35_PIN_I2C2_CLK, pad);
		mxc_iomux_set_pad(MX35_PIN_I2C2_DAT, pad);
		mxc_iomux_set_input(MUX_IN_CAN1_CANRX, INPUT_CTL_PATH0);
		break;
	case 1:
		pad = PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PUD | PAD_CTL_100K_PU;
		mxc_request_iomux(MX35_PIN_FEC_MDC, MUX_CONFIG_ALT1);
		mxc_request_iomux(MX35_PIN_FEC_MDIO, MUX_CONFIG_ALT1);
		mxc_iomux_set_pad(MX35_PIN_FEC_MDC, pad);
		mxc_iomux_set_pad(MX35_PIN_FEC_MDIO, pad);
		mxc_iomux_set_input(MUX_IN_CAN2_CANRX, INPUT_CTL_PATH2);
		break;
	default:
		printk(KERN_ERR "NO such device\n");
	}
}

void gpio_can_inactive(int id)
{
	switch (id) {
	case 0:
		mxc_free_iomux(MX35_PIN_I2C2_CLK, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_I2C2_DAT, MUX_CONFIG_ALT1);
		mxc_iomux_set_input(MUX_IN_CAN1_CANRX, INPUT_CTL_PATH0);
		break;
	case 1:
		mxc_free_iomux(MX35_PIN_FEC_MDC, MUX_CONFIG_ALT1);
		mxc_free_iomux(MX35_PIN_FEC_MDIO, MUX_CONFIG_ALT1);
		mxc_iomux_set_input(MUX_IN_CAN2_CANRX, INPUT_CTL_PATH0);
		break;
	default:
		printk(KERN_ERR "NO such device\n");
	}
}

static int usbh1_gpio_state = 0;		/* Currently disable */
static int usbh1_gpio_configured = 0;		/* Configure the GPIO */

static void gpio_usbh1_configure(void)
{
	mxc_request_iomux(MX35_PIN_ATA_DATA1, MUX_CONFIG_ALT5);
}

static void gpio_usbh1_power_enable(int enable)
{
	int pad_val = 0;

	if (enable == usbh1_gpio_state)
		return;

	if (enable) {
		if (!usbh1_gpio_configured) {
			usbh1_gpio_configured = 1;
			gpio_usbh1_configure();
		}

		pad_val = PAD_CTL_DRV_NORMAL | PAD_CTL_SRE_SLOW |
				PAD_CTL_DRV_1_8V | PAD_CTL_PKE_NONE |
				PAD_CTL_ODE_CMOS;
		mxc_iomux_set_pad(MX35_PIN_ATA_DATA1, pad_val);
		gpio_direction_output(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA1), 0);
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA1), 1);
		usbh1_gpio_state = 1;
	}
	else {
		gpio_set_value(IOMUX_TO_GPIO(MX35_PIN_ATA_DATA1), 0);
		usbh1_gpio_state = 0;
	}
}

static ssize_t usbh1_gpio_store(struct sys_device *dev, const char *buf,
					size_t size)
{
	if (strstr(buf, "1") != NULL)
		gpio_usbh1_power_enable(1);
	else if (strstr(buf, "0") != NULL)
		gpio_usbh1_power_enable(0);

	return size;
}

static SYSDEV_ATTR(usbh1_gpio_enable, 0644, NULL, usbh1_gpio_store);

static struct sysdev_class usbh1_gpio_sysclass = {
	.name = "usbh1_gpio",
};

static struct sys_device usbh1_gpio_device = {
	.id = 0,
	.cls = &usbh1_gpio_sysclass,
};

static int __init usbh1_gpio_sysdev_ctrl_init(void)
{
	int err;

	err = sysdev_class_register(&usbh1_gpio_sysclass);
	if (!err)
		err = sysdev_register(&usbh1_gpio_device);
	if (!err)
		err = sysdev_create_file(&usbh1_gpio_device, &attr_usbh1_gpio_enable);

	return err;
}
module_init(usbh1_gpio_sysdev_ctrl_init);

static void __exit usbh1_gpio_sysdev_ctrl_exit(void)
{
	sysdev_remove_file(&usbh1_gpio_device, &attr_usbh1_gpio_enable);
	sysdev_unregister(&usbh1_gpio_device);
	sysdev_class_unregister(&usbh1_gpio_sysclass);
}
module_exit(usbh1_gpio_sysdev_ctrl_exit);
