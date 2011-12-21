//------------------------------------------------------------------------------
// <copyright file="wlan_defs.h" company="Atheros">
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
#ifndef __WLAN_DEFS_H__
#define __WLAN_DEFS_H__

/*
 * This file contains WLAN definitions that may be used across both
 * Host and Target software.  
 */

typedef enum {
    MODE_11A        = 0,   /* 11a Mode */
    MODE_11G        = 1,   /* 11b/g Mode */
    MODE_11B        = 2,   /* 11b Mode */
    MODE_11GONLY    = 3, /* 11g only Mode */
#ifdef SUPPORT_11N
    MODE_11A_HT20   = 4, /* 11a HT20 mode */
    MODE_11G_HT20   = 5, /* 11g HT20 mode */
    MODE_11A_HT40   = 6, /* 11a HT40 mode */
    MODE_11G_HT40   = 7, /* 11g HT40 mode */
    MODE_UNKNOWN    = 8,
    MODE_MAX        = 8
#else
    MODE_UNKNOWN    = 4,
    MODE_MAX        = 4
#endif
} WLAN_PHY_MODE;

typedef enum {
    WLAN_11A_CAPABILITY   = 1,
    WLAN_11G_CAPABILITY   = 2,
    WLAN_11AG_CAPABILITY  = 3,
}WLAN_CAPABILITY;

#endif /* __ATHDEFS_H__ */
