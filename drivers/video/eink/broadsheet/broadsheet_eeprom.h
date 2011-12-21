/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_eeprom.h --
 *  eInk frame buffer device HAL broadsheet panel EEPROM defs
 *
 *      Copyright (C) 2009-2010 Amazon Technologies, Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _BROADSHEET_EEPROM_H
#define _BROADSHEET_EEPROM_H

#define EEPROM_BASE_PART_NUMBER 0x00
#define EEPROM_SIZE_PART_NUMBER 16

#define EEPROM_BASE_VCOM        0x10
#define EEPROM_SIZE_VCOM        5

#define EEPROM_BASE_WAVEFORM    0x20
#define EEPROM_SIZE_WAVEFORM    23

#define EEPROM_BASE_FPL         0x40
#define EEPROM_SIZE_FPL         3

#define EEPROM_BASE_BCD         0x50
#define EEPROM_SIZE_BCD         32

#define EEPROM_BASE             0x00
#define EEPROM_SIZE             256
#define EEPROM_LAST             (EEPROM_SIZE - 1)

#define FLASH_BASE              BS_PNL_ADDR

#define EEPROM_CHAR_UNKNOWN     '!'

extern void broadsheet_eeprom_create_proc_enteries(void);
extern void broadsheet_eeprom_remove_proc_enteries(void);

extern int  broadsheet_eeprom_read(u32 start_addr, u8 *buffer, int to_read);
extern bool broadsheet_supports_eeprom_read(void);

#endif // _BROADSHEET_EEPROM_H
