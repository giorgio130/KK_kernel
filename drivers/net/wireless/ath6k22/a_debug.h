//------------------------------------------------------------------------------
// <copyright file="a_debug.h" company="Atheros">
//    Copyright (c) 2004-2008 Atheros Corporation.  All rights reserved.
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
#ifndef _A_DEBUG_H_
#define _A_DEBUG_H_

#include "a_types.h"
#include "a_osapi.h"

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/debug_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/debug_wince.h"
#endif

#ifndef UNDER_CE
#define DBG_INFO        0x00000001
#define DBG_ERROR       0x00000002
#define DBG_WARNING     0x00000004
#define DBG_SDIO        0x00000008
#define DBG_HIF         0x00000010
#define DBG_HTC         0x00000020
#define DBG_WMI         0x00000040
#define DBG_WMI2        0x00000080
#define DBG_DRIVER      0x00000100

#define DBG_DEFAULTS    (DBG_ERROR|DBG_WARNING)
#endif

#if defined(__linux__) && !defined(LINUX_EMULATION)
#include "debug_linux.h"
#endif

#ifdef REXOS
#include "../os/rexos/include/common/debug_rexos.h"
#endif

#endif
