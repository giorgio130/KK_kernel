//------------------------------------------------------------------------------
// <copyright file="addrs.h" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
// 
// The software source and binaries included in this development package are
// licensed, not sold. You, or your company, received the package under one
// or more license agreements. The rights granted to you are specifically
// listed in these license agreement(s). All other rights remain with Atheros
// Communications, Inc., its subsidiaries, or the respective owner including
// those listed on the included copyright notices.  Distribution of any
// portion of this package must be in strict compliance with the license
// agreement(s) terms.
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

#ifndef __ADDRS_H__
#define __ADDRS_H__

/* 
 * Special AR6002 Addresses that may be needed by special
 * applications (e.g. ART) on the Host as well as Target.
 */

#define AR6K_RAM_START 0x00500000
#define AR6K_RAM_ADDR(byte_offset) (AR6K_RAM_START+(byte_offset))
#define TARG_RAM_ADDRS(byte_offset) AR6K_RAM_ADDR(byte_offset)
#define TARG_RAM_OFFSET(vaddr) ((A_UINT32)(vaddr) & 0xfffff)
#define TARG_RAM_SZ (184*1024)

#define AR6K_ROM_START 0x004e0000
#define AR6K_ROM_ADDR(byte_offset) (AR6K_ROM_START+(byte_offset))
#define TARG_ROM_ADDRS(byte_offset) AR6K_ROM_ADDR(byte_offset)

/*
 * At this ROM address is a pointer to the start of the ROM DataSet Index.
 * If there are no ROM DataSets, there's a 0 at this address.
 */
#define ROM_DATASET_INDEX_ADDR          0x004f3ff8
#define ROM_MBIST_CKSUM_ADDR            0x004f3ffc

/*
 * The API A_BOARD_DATA_ADDR() is the proper way to get a read pointer to
 * board data.
 */

/* Size of Board Data, in bytes */
#define AR6002_BOARD_DATA_SZ 768
#define BOARD_DATA_SZ AR6002_BOARD_DATA_SZ

/*
 * Constants used by ASM code to access fields of host_interest_s,
 * which is at a fixed location in RAM.
 */
#define FLASH_IS_PRESENT_TARGADDR       0x0050040c
#endif /* __ADDRS_H__ */
