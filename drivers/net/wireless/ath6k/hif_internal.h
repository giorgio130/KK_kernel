//------------------------------------------------------------------------------
// <copyright file="hif_internal.h" company="Atheros">
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
// internal header file for hif layer
//
// Author(s): ="Atheros"
//==============================================================================
#include "a_config.h"
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "hif.h"

#define MANUFACTURER_ID_AR6001_BASE        0x100
#define MANUFACTURER_ID_AR6002_BASE        0x200
#define MANUFACTURER_ID_AR6003_BASE        0x300
#define FUNCTION_CLASS                     0x0
#define MANUFACTURER_CODE                  0x271

#define BUS_REQUEST_MAX_NUM                64

#define SDIO_CLOCK_FREQUENCY_DEFAULT       25000000
#define SDWLAN_ENABLE_DISABLE_TIMEOUT      20
#define FLAGS_CARD_ENAB                    0x02
#define FLAGS_CARD_IRQ_UNMSK               0x04

#define HIF_MBOX_BLOCK_SIZE                128
#define HIF_MBOX_BASE_ADDR                 0x800
#define HIF_MBOX_WIDTH                     0x800
#define HIF_MBOX0_BLOCK_SIZE               1
#define HIF_MBOX1_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX2_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE
#define HIF_MBOX3_BLOCK_SIZE               HIF_MBOX_BLOCK_SIZE

#define HIF_MBOX_START_ADDR(mbox)                        \
    HIF_MBOX_BASE_ADDR + mbox * HIF_MBOX_WIDTH

#define HIF_MBOX_END_ADDR(mbox)	                         \
    HIF_MBOX_START_ADDR(mbox) + HIF_MBOX_WIDTH - 1

typedef struct bus_request {
    struct bus_request *next;       /* link list of available requests */
    struct bus_request *inusenext;  /* link list of in use requests */
    struct compat_semaphore sem_req;
    A_UINT32 address;               /* request data */
    A_UCHAR *buffer;
    A_UINT32 length;
    A_UINT32 request;
    void *context;
    A_STATUS status;
} BUS_REQUEST;

struct hif_device {
    struct sdio_func *func;
    spinlock_t asynclock;
    struct task_struct* async_task;             /* task to handle async commands */
    struct compat_semaphore sem_async;                 /* wake up for async task */
    int    async_shutdown;                      /* stop the async task */
    struct completion async_completion;          /* thread completion */
    BUS_REQUEST   *asyncreq;                    /* request for async tasklet */
    BUS_REQUEST *taskreq;                       /*  async tasklet data */
    spinlock_t lock;
    BUS_REQUEST *s_busRequestFreeQueue;         /* free list */
    BUS_REQUEST busRequest[BUS_REQUEST_MAX_NUM]; /* available bus requests */
    void     *claimedContext;
    HTC_CALLBACKS htcCallbacks;
    A_UINT8     *dma_buffer;
};

#define HIF_DMA_BUFFER_SIZE (32 * 1024)
#define CMD53_FIXED_ADDRESS 1
#define CMD53_INCR_ADDRESS  2
