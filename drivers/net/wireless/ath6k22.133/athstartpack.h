//------------------------------------------------------------------------------
// <copyright file="athstartpack.h" company="Atheros">
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
// </copyright>
// 
// <summary>
// 	Wifi driver for AR6002
// </summary>
//
//------------------------------------------------------------------------------
//==============================================================================
// start compiler-specific structure packing
//
// Author(s): ="Atheros"
//==============================================================================
#ifdef VXWORKS
#endif /* VXWORKS */

#if defined(LINUX) || defined(__linux__)
#endif /* LINUX */

#ifdef QNX
#endif /* QNX */

#ifdef INTEGRITY
#include "integrity/athstartpack_integrity.h"
#endif /* INTEGRITY */

#ifdef NUCLEUS
#endif /* NUCLEUS */

#ifdef UNDER_NWIFI
#include "../os/windows/common/include/athstartpack_wince.h"
#endif

#ifdef ATHR_CE_LEGACY
#include "../os/wince/include/athstartpack_wince.h"
#endif /* WINCE */

