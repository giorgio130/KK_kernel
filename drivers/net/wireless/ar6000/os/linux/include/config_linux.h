/*
 * Copyright (c) 2004-2007 Atheros Communications Inc.
 * All rights reserved.
 *
 * 
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
 *
 */

#ifndef _CONFIG_LINUX_H_
#define _CONFIG_LINUX_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Host-side GPIO support is optional.
 * If run-time access to GPIO pins is not required, then
 * this should be changed to #undef.
 */
#define CONFIG_HOST_GPIO_SUPPORT

/*
 * Host side Test Command support
 */
#define CONFIG_HOST_TCMD_SUPPORT

#define USE_4BYTE_REGISTER_ACCESS

/* Host-side support for Target-side profiling */
#undef CONFIG_TARGET_PROFILE_SUPPORT

/* Force AR6002 REV1 hardware to recognize Host */
#undef CONFIG_AR6002_REV1_FORCE_HOST

#ifdef __cplusplus
}
#endif

#endif
