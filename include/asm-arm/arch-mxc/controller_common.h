/*
 *  linux/include/asm-arm/arch-mxc/common_controller.h --
 *  eInk common controller definitions
 *
 *      Copyright (C) 2005-2010 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _CONTROLLER_COMMON_H_
#define _CONTROLLER_COMMON_H_

#include <asm/arch/ipu.h>
#include <linux/types.h>
#include <linux/version.h>
#include <llog.h>

#define PRAGMAS                 0

#define EINKFB_NAME             "eink_fb"

#define EINKFB_SUCCESS          false
#define EINKFB_FAILURE          true

#define EINKFB_MEMCPY_MIN       ((824 * 1200)/4)

#define einkfb_schedule_timeout_interruptible(t, r, d)                  \
    einkfb_schedule_timeout(t, r, d, true)

#define EINKFB_SCHEDULE_TIMEOUT_DATA(t, r, d)                           \
    einkfb_schedule_timeout(t, r, d, false)

#define EINKFB_SCHEDULE_TIMEOUT(t, r)                                   \
    einkfb_schedule_timeout(t, r, NULL, false)

#define EINKFB_SCHEDULE_TIMEOUT_INTERRUPTIBLE(t, r)                     \
    einkfb_schedule_timeout_interruptible(t, r, NULL)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
#define EINKFB_SCHEDULE()                                               \
    do { } while (0)
#else
#define EINKFB_SCHEDULE()                                               \
    EINKFB_SCHEDULE_TIMEOUT(0, 0)
#endif

#define EINKFB_SCHEDULE_BLIT(b)                                         \
    if ( 0 == ((b) % EINKFB_MEMCPY_MIN) )                               \
        EINKFB_SCHEDULE()

#define EINKFB_LOGGING_MASK(m)  (m & einkfb_logging)
#define EINKFB_DEBUG_FULL       (einkfb_logging_debug | einkfb_logging_debug_full)
#define EINKFB_DEBUG()          EINKFB_LOGGING_MASK(EINKFB_DEBUG_FULL)
#define EINKFB_PERF()           EINKFB_LOGGING_MASK(einkfb_logging_perf)

#define einkfb_print(i, f, ...)                                         \
    printk(EINKFB_NAME ": " i " %s:%s:" f,                              \
        __FUNCTION__, _LSUBCOMP_DEFAULT, ##__VA_ARGS__)

#define EINKFB_PRINT(f, ...)                                            \
    einkfb_print(LLOG_MSG_ID_INFO, f, ##__VA_ARGS__)

#define einkfb_print_crit(f, ...)                                       \
    if (EINKFB_LOGGING_MASK(einkfb_logging_crit))                       \
        einkfb_print(LLOG_MSG_ID_CRIT, f, ##__VA_ARGS__)

#define einkfb_print_error(f, ...)                                      \
    if (EINKFB_LOGGING_MASK(einkfb_logging_error))                      \
        einkfb_print(LLOG_MSG_ID_ERR, f, ##__VA_ARGS__)

#define einkfb_print_warn(f, ...)                                       \
        einkfb_print(LLOG_MSG_ID_WARN, f, ##__VA_ARGS__)

#define einkfb_print_info(f, ...)                                       \
    if (EINKFB_LOGGING_MASK(einkfb_logging_info))                       \
        einkfb_print(LLOG_MSG_ID_INFO, f, ##__VA_ARGS__)

#define einkfb_print_perf(f, ...)                                       \
    if (EINKFB_LOGGING_MASK(einkfb_logging_perf))                       \
        einkfb_print(LLOG_MSG_ID_PERF, f, ##__VA_ARGS__)

#define EINKFB_PRINT_PERF_REL(i, t, n)                                  \
    einkfb_print_perf("id=%s,time=%ld,type=relative:%s\n", i, t, n)
    
#define EINKFB_PRINT_PERF_ABS(i, t, n)                                  \
    einkfb_print_perf("id=%s,time=%ld,type=absolute:%s\n", i, t, n)

#define einkfb_print_debug(f, ...)                                      \
    einkfb_print(LLOG_MSG_ID_DEBUG, f,  ##__VA_ARGS__)

#define einkfb_debug_full(f, ...)                                       \
    if (EINKFB_LOGGING_MASK(einkfb_logging_debug_full))                 \
        einkfb_print_debug(f, ##__VA_ARGS__)

#define einkfb_debug(f, ...)                                            \
    if (EINKFB_LOGGING_MASK(EINKFB_DEBUG_FULL))                         \
        einkfb_print_debug(f, ##__VA_ARGS__)

#define einkfb_debug_lock(f, ...)                                       \
    if (EINKFB_LOGGING_MASK(einkfb_logging_debug_lock))                 \
        einkfb_print_debug(f, ##__VA_ARGS__)

#define einkfb_debug_power(f, ...)                                      \
    if (EINKFB_LOGGING_MASK(einkfb_logging_debug_power))                \
        einkfb_print_debug(f, ##__VA_ARGS__)

#define einkfb_debug_ioctl(f, ...)                                      \
    if (EINKFB_LOGGING_MASK(einkfb_logging_debug_ioctl))                \
        einkfb_print_debug(f, ##__VA_ARGS__)

#define einkfb_debug_poll(f, ...)                                       \
    if (EINKFB_LOGGING_MASK(einkfb_logging_debug_poll))                 \
        einkfb_print_debug(f, ##__VA_ARGS__)

enum einkfb_logging_level
{
    einkfb_logging_crit         = LLOG_LEVEL_CRIT,    // Crash - can't go on as is.
    einkfb_logging_error        = LLOG_LEVEL_ERROR,   // Environment not right.
    einkfb_logging_warn         = LLOG_LEVEL_WARN,    // Oops, some bug.
    einkfb_logging_info         = LLOG_LEVEL_INFO,    // FYI.
    einkfb_logging_perf         = LLOG_LEVEL_PERF,    // Performance.

    einkfb_logging_debug        = LLOG_LEVEL_DEBUG0,  // Miscellaneous general debugging.
    einkfb_logging_debug_lock   = LLOG_LEVEL_DEBUG1,  // Mutex lock/unlock debugging.
    einkfb_logging_debug_power  = LLOG_LEVEL_DEBUG2,  // Power Management debugging.
    einkfb_logging_debug_ioctl  = LLOG_LEVEL_DEBUG3,  // Debugging for ioctls.
    einkfb_logging_debug_poll   = LLOG_LEVEL_DEBUG4,  // Debugging for polls.
    
    einkfb_logging_debug_full   = LLOG_LEVEL_DEBUG9,  // Full, general debugging.
    
    einkfb_logging_none         = 0x00000000,
    einkfb_logging_all          = LLOG_LEVEL_MASK_ALL,
    
    einkfb_logging_default      = _LLOG_LEVEL_DEFAULT,
    einkfb_debugging_default    = einkfb_logging_default | einkfb_logging_debug | einkfb_logging_debug_ioctl
};
typedef enum einkfb_logging_level einkfb_logging_level;

extern unsigned long einkfb_logging;

typedef bool (*einkfb_hardware_ready_t)(void *data);

extern int einkfb_schedule_timeout(unsigned long hardware_timeout, einkfb_hardware_ready_t hardware_ready,
    void *data, bool interruptible);

// Controller-specific Definitions and Functions
//
#define CONTROLLER_COMMON_NAME                 EINKFB_NAME

#define CONTROLLER_COMMON_GPIO_INIT_SUCCESS    0
#define CONTROLLER_COMMON_HIRQ_RQST_FAILURE    1
#define CONTROLLER_COMMON_HIRQ_INIT_FAILURE    2
#define CONTROLLER_COMMON_HRST_INIT_FAILURE    3
#define CONTROLLER_COMMON_HRDY_INIT_FAILURE    4

#define CONTROLLER_COMMON_RD_DAT_ONE           1       // One 16-bit read.
#define CONTROLLER_COMMON_WR_DAT_ONE           1       // One 16-bit write.
#define CONTROLLER_COMMON_WR_DAT_DATA          true    // For use with...
#define CONTROLLER_COMMON_WR_DAT_ARGS          false   // ...controller_wr_dat().
#define CONTROLLER_COMMON_CMD_ARGS_MAX         5

#define CONTROLLER_COMMON_RDY_TIMEOUT          (HZ * 5)
#define CONTROLLER_COMMON_DMA_TIMEOUT          (HZ * 1)
#define CONTROLLER_COMMON_TIMEOUT_MIN          1UL
#define CONTROLLER_COMMON_TIMEOUT_MAX          10UL
#define CONTROLLER_COMMON_DMA_MIN_SIZE         128
#define CONTROLLER_COMMON_DMA_MIN_FAIL         3

#define CONTROLLER_COMMON_DMA_SIZE(s)          CONTROLLER_COMMON_DMA_MIN_SIZE,     \
                                               (s)/CONTROLLER_COMMON_DMA_MIN_SIZE, \
                                               CONTROLLER_COMMON_DMA_MIN_SIZE

#define CONTROLLER_COMMON_DMA_FAILED()         (CONTROLLER_COMMON_DMA_MIN_FAIL < dma_failed)

#define CONTROLLER_COMMON_CMD                  true
#define CONTROLLER_COMMON_DAT                  false
#define CONTROLLER_COMMON_WR                   true
#define CONTROLLER_COMMON_RD                   false

typedef dma_addr_t (*controller_common_get_dma_addr_t)(void);  
typedef void (*controller_common_done_dma_addr_t)(void); 
typedef void (*controller_common_controller_display_task_t)(void);
typedef void (*controller_display_task_t)(void);

struct controller_properties_t
{
    display_port_t         controller_disp;
    uint16_t               screen_width;
    uint16_t               screen_height;
    uint32_t               pixel_fmt;
    uint32_t               screen_stride;
    uint32_t               read_cycle_time;
    uint32_t               read_up_time;
    uint32_t               read_down_time;
    uint32_t               read_latch_time;
    uint32_t               write_cycle_time;
    uint32_t               write_up_time;
    uint32_t               write_down_time;
    uint32_t               pixel_clk;
    uint32_t               hsp_clk_per;
    
    // Optional
    //
    controller_common_get_dma_addr_t
                           get_dma_phys_addr; 
    controller_common_done_dma_addr_t
                           done_dma_phys_addr;
};
typedef struct controller_properties_t controller_properties_t;

bool controller_ignore_hw_ready(void);
void controller_set_ignore_hw_ready(bool value);
bool controller_force_hw_not_ready(void);
void controller_set_force_hw_not_ready(bool value);

int  controller_wait_for_ready(void);
int  controller_wr_cmd(uint32_t cmd, bool poll);
int  controller_io_buf(u32 data_size, u16 *data, bool which);
bool controller_wr_one_ready(void);
void controller_wr_one(u16 data);
int  controller_wr_dat(bool which, u32 data_size, u16 *data);
int  controller_rd_one(u16 *data);
int  controller_rd_dat(u32 data_size, u16 *data);

bool controller_hw_init(bool use_dma, controller_properties_t *prop);
void controller_hw_done(void);

bool controller_start_display_wq(const char *wq_name);
void controller_stop_display_wq(void);
bool controller_schedule_display_work(controller_display_task_t task);

extern int  controller_common_gpio_config(irq_handler_t broadsheet_irq_handler, char *broadsheet_irq_handler_name);
extern void controller_common_gpio_disable(int disable_bs_irq);
extern void controller_common_reset(void);
extern int  controller_common_ready(void);

#endif // _CONTROLLER_COMMON_H_
