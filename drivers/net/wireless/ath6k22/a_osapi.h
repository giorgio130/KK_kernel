//------------------------------------------------------------------------------
// <copyright file="a_osapi.h" company="Atheros">
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
// This file contains the definitions of the basic atheros data types.
// It is used to map the data types in atheros files to a platform specific
// type.
//
// Author(s): ="Atheros"
//==============================================================================
#ifndef _A_OSAPI_H_
#define _A_OSAPI_H_

#if defined(__linux__) && !defined(LINUX_EMULATION)
#include "osapi_linux.h"
#endif

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/osapi_wince.h"
#include "../os/windows/common/include/netbuf.h"
#if defined __cplusplus || defined __STDC__
extern "C"
#endif
A_UINT32 a_copy_from_user(void *to, const void *from, A_UINT32 n);
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/osapi_wince.h"
#include "../os/wince/include/netbuf.h"
#if defined __cplusplus || defined __STDC__
extern "C"
#endif
A_UINT32 a_copy_from_user(void *to, const void *from, A_UINT32 n);
#endif

#ifdef REXOS
#include "../os/rexos/include/common/osapi_rexos.h"
#endif

#endif /* _OSAPI_H_ */
