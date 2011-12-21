/*
 *  linux/drivers/video/eink/controller_common/controller_common.c --
 *  eInk common controller operations
 *
 *      Copyright (C) 2005-2010 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _EINKFB_HAL_UTIL
#include <asm/arch/controller_common.h>
#define FORCE_INTERRUPTIBLE()       false
#else
#define FORCE_INTERRUPTIBLE()       einkfb_get_force_interruptible()
#endif

#define UNINTERRUPTIBLE             false
#define INTERRUPTIBLE               true

#define INTERRUPTIBLE_TIMEOUT_COUNT 3

static int einkfb_schedule_timeout_guts(unsigned long hardware_timeout, einkfb_hardware_ready_t hardware_ready, void *data, bool interruptible)
{
    unsigned long start_time = jiffies, stop_time = start_time + hardware_timeout,
        timeout = CONTROLLER_COMMON_TIMEOUT_MIN;
    int result = EINKFB_SUCCESS;

    // Ask the hardware whether it's ready or not.  And, if it's not ready, start yielding
    // the CPU for CONTROLLER_COMMON_TIMEOUT_MIN jiffies, increasing the yield time up to
    // CONTROLLER_COMMON_TIMEOUT_MAX jiffies.  Time out after the requested number of
    // of jiffies has occurred.
    //
    while ( !(*hardware_ready)(data) && time_before_eq(jiffies, stop_time) )
    {
        timeout = min(timeout++, CONTROLLER_COMMON_TIMEOUT_MAX);
        
        if ( interruptible )
            schedule_timeout_interruptible(timeout);
        else
            schedule_timeout(timeout);
    }

    if ( time_after(jiffies, stop_time) )
    {
       einkfb_print_crit("Timed out waiting for the hardware to become ready!\n");
       result = EINKFB_FAILURE;
    }
    else
    {
        // For debugging purposes, dump the time it took for the hardware to
        // become ready if it was more than CONTROLLER_COMMON_TIMEOUT_MAX.
        //
        stop_time = jiffies - start_time;
        
        if ( CONTROLLER_COMMON_TIMEOUT_MAX < stop_time )
            einkfb_debug("Timeout time = %ld\n", stop_time);
    }

    return ( result );    
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
static int einkfb_schedule_timeout_uninterruptible(unsigned long hardware_timeout, einkfb_hardware_ready_t hardware_ready, void *data)
{
    return ( einkfb_schedule_timeout_guts(hardware_timeout, hardware_ready, data, UNINTERRUPTIBLE) );
}
#else
static int einkfb_schedule_timeout_uninterruptible(unsigned long hardware_timeout, einkfb_hardware_ready_t hardware_ready, void *data)
{
    int timeout_count = 0, result = EINKFB_FAILURE;
    
    // In the uninterruptible case, we try to be more CPU friendly by first doing things
    // in an interruptible way.
    //
    while ( (INTERRUPTIBLE_TIMEOUT_COUNT > timeout_count++) && (EINKFB_FAILURE == result) )
        result = einkfb_schedule_timeout_guts(hardware_timeout, hardware_ready, data, INTERRUPTIBLE);
    
    // It's possible that we haven't been able to access the hardware enough times by doing
    // things in an interruptible way.  So, try again once more in an uninterruptible way.
    //
    if ( EINKFB_FAILURE == result )
        result = einkfb_schedule_timeout_guts(hardware_timeout, hardware_ready, data, UNINTERRUPTIBLE);

    return ( result );
}
#endif

int einkfb_schedule_timeout(unsigned long hardware_timeout, einkfb_hardware_ready_t hardware_ready, void *data, bool interruptible)
{
    int result = EINKFB_SUCCESS;
    
    if ( hardware_timeout && hardware_ready )
    {
        if ( FORCE_INTERRUPTIBLE() || interruptible )
            result = einkfb_schedule_timeout_guts(hardware_timeout, hardware_ready, data, INTERRUPTIBLE);
        else
            result = einkfb_schedule_timeout_uninterruptible(hardware_timeout, hardware_ready, data);
    }
    else
    {
        // Yield the CPU with schedule.
        //
        einkfb_debug("Yielding CPU with schedule.\n");
        schedule();
    }
    
    return ( result );
}

#ifdef _EINKFB_HAL_UTIL
EXPORT_SYMBOL(einkfb_schedule_timeout);
#endif

