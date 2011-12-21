//------------------------------------------------------------------------------
// <copyright file="a_config.h" company="Atheros">
//    Copyright (c) 2004-2008 Atheros Corporation.  All rights reserved.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
// 
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
// implied. See the License for the specific language governing
// rights and limitations under the License.
// 
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// This file contains software configuration options that enables
// specific software "features"
//
// Author(s): ="Atheros"
//==============================================================================
#ifndef _A_CONFIG_H_
#define _A_CONFIG_H_

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/config_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/config_wince.h"
#endif

#if defined(__linux__) && !defined(LINUX_EMULATION)
#include "config_linux.h"
#endif

#ifdef REXOS
#include "../os/rexos/include/common/config_rexos.h"
#endif

#endif
