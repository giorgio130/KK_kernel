//------------------------------------------------------------------------------
// <copyright file="common_drv.h" company="Atheros">
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
#ifndef COMMON_DRV_H_
#define COMMON_DRV_H_

#include "hif.h"
#include "htc_packet.h"

/* structure that is the state information for the default credit distribution callback
 * drivers should instantiate (zero-init as well) this structure in their driver instance
 * and pass it as a context to the HTC credit distribution functions */
typedef struct _COMMON_CREDIT_STATE_INFO {
    int TotalAvailableCredits;      /* total credits in the system at startup */
    int CurrentFreeCredits;         /* credits available in the pool that have not been
                                       given out to endpoints */
    HTC_ENDPOINT_CREDIT_DIST *pLowestPriEpDist;  /* pointer to the lowest priority endpoint dist struct */
} COMMON_CREDIT_STATE_INFO;


/* HTC TX packet tagging definitions */
#define AR6K_CONTROL_PKT_TAG    HTC_TX_PACKET_TAG_USER_DEFINED
#define AR6K_DATA_PKT_TAG       (AR6K_CONTROL_PKT_TAG + 1)

#define AR6002_VERSION_REV1     0x20000086
#define AR6002_VERSION_REV2     0x20000188

#ifdef __cplusplus
extern "C" {
#endif

/* OS-independent APIs */
A_STATUS ar6000_setup_credit_dist(HTC_HANDLE HTCHandle, COMMON_CREDIT_STATE_INFO *pCredInfo);

A_STATUS ar6000_ReadRegDiag(HIF_DEVICE *hifDevice, A_UINT32 *address, A_UINT32 *data);

A_STATUS ar6000_WriteRegDiag(HIF_DEVICE *hifDevice, A_UINT32 *address, A_UINT32 *data);

A_STATUS ar6000_ReadDataDiag(HIF_DEVICE *hifDevice, A_UINT32 address,  A_UCHAR *data, A_UINT32 length);

A_STATUS ar6000_reset_device(HIF_DEVICE *hifDevice, A_UINT32 TargetType, A_BOOL waitForCompletion);

void ar6000_dump_target_assert_info(HIF_DEVICE *hifDevice, A_UINT32 TargetType);

A_STATUS ar6000_reset_device_skipflash(HIF_DEVICE *hifDevice);

A_STATUS ar6000_set_htc_params(HIF_DEVICE *hifDevice,
                               A_UINT32    TargetType,
                               A_UINT32    MboxIsrYieldValue,
                               A_UINT8     HtcControlBuffers);

A_STATUS ar6000_prepare_target(HIF_DEVICE *hifDevice,
                               A_UINT32    TargetType,
                               A_UINT32    TargetVersion);

#ifdef __cplusplus
}
#endif

#endif /*COMMON_DRV_H_*/
