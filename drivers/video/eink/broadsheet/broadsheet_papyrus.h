/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_eeprom.h --
 *  eInk Papyrus PMIC defs
 *
 *      Copyright (C) 2005-2010 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _BROADSHEET_PAPYRUS_H
#define _BROADSHEET_PAPYRUS_H

#define PAPYRUS_REGISTER_TMST_VALUE    0x00  // Thremistor value read by ADC
#define PAPYRUS_REGISTER_ENABLE        0x01  // Enable/disable bits for regulators
#define PAPYRUS_REGISTER_VP_ADJUST     0x02  // Voltage settings for VPOS, VDDH
#define PAPYRUS_REGISTER_VN_ADJUST     0x03  // Voltage settings for VNEG, VEE
#define PAPYRUS_REGISTER_VCOM_ADJUST   0x04  // Voltage settings for VCOM
#define PAPYRUS_REGISTER_INT_ENABLE1   0x05  // Interrupt enable group1
#define PAPYRUS_REGISTER_INT_ENABLE2   0x06  // Interrupt enable group2
#define PAPYRUS_REGISTER_INT_STATUS1   0x07  // Interrupt status group1
#define PAPYRUS_REGISTER_INT_STATUS2   0x08  // Interrupt status group2
#define PAPYRUS_REGISTER_PWR_SEQ0      0x09  // Power up sequence
#define PAPYRUS_REGISTER_PWR_SEQ1      0x0A  // T0, T1 time set
#define PAPYRUS_REGISTER_PWR_SEQ2      0x0B  // T2, T3 time set
#define PAPYRUS_REGISTER_TMST_CONFIG   0x0C  // Thermistor config
#define PAPYRUS_REGISTER_TMST_OS       0x0D  // Thermistor hot temp set
#define PAPYRUS_REGISTER_TMST_HYST     0x0E  // Thermistor cool temp set
#define PAPYRUS_REGISTER_PG_STATUS     0x0F  // Power good status each rail
#define PAPYRUS_REGISTER_REVID         0x10  // Device revision ID information

#define PAPYRUS_READ_THERM             0x80  // Read thermistor
#define PAPYRUS_CONV_END               0x20  // A/D conversion finished
#define PAPYRUS_TEMP_DEFAULT           0x19  // 25C

extern void broadsheet_papyrus_create_proc_enteries(void);
extern void broadsheet_papyrus_remove_proc_enteries(void);

extern int  broadsheet_papyrus_read_register(char reg, char *value);
extern int  broadsheet_papyrus_write_register(char reg, char value);

extern s8   broadsheet_papyrus_read_temp(void);
extern void broadsheet_papyrus_set_vcom(u8 pmic_vcom);
extern bool broadsheet_papyrus_rails_are_up(void);

extern void broadsheet_papyrus_init(void);
extern void broadsheet_papyrus_exit(void);
extern bool broadsheet_papyrus_present(void);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
extern void gpio_papyrus_irq_configure(int enable);
extern void gpio_papyrus_pg_configure(int enable);
extern int  gpio_papyrus_get_irq(void);
extern int  gpio_papyrus_get_pg(void);
extern void gpio_papyrus_reset(void);

#define GPIO_PAPYRUS_IRQ_CONFIGURE(e) gpio_papyrus_irq_configure(e)
#define GPIO_PAPYRUS_PG_CONFIGURE(e)  gpio_papyrus_pg_configure(e)
#define GPIO_PAPYRUS_IRQ              gpio_papyrus_get_irq()
#define GPIO_PAPYRUS_PG               gpio_papyrus_get_pg()
#define RESET_PAPYRUS()               gpio_papyrus_reset()
#else
#define DO_NOTHING()                  do { } while (0)
#define GPIO_PAPYRUS_IRQ_CONFIGURE(e) DO_NOTHING()
#define GPIO_PAPYRUS_PG_CONFIGURE(e)  DO_NOTHING()
#define GPIO_PAPYRUS_IRQ              DO_NOTHING()
#define GPIO_PAPYRUS_PG               DO_NOTHING()
#define RESET_PAPYRUS()               DO_NOTHING()
#endif

#endif // _BROADSHEET_PAPYRUS_H
