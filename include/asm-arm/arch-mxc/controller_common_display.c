/*
 *  linux/drivers/video/eink/broadsheet/broadsheet.c
 *  -- eInk common controller display functions
 *
 *      Copyright (C) 2005-2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <asm/arch/controller_common.h>

// workqueue to perform the display task
static struct workqueue_struct *controller_display_wq = NULL;

// declare the work to perform
static void controller_display_work_function(struct work_struct *unused);
DECLARE_WORK(controller_display_work, controller_display_work_function);
DECLARE_MUTEX(controller_display_sem);
static bool controller_work_scheduled = false;

// the display task for the controller
static controller_display_task_t controller_display_task = NULL;

bool controller_start_display_wq(const char *wq_name) {
    controller_display_wq = create_workqueue(wq_name);
    return controller_display_wq != NULL;
}

void controller_stop_display_wq(void) {
    struct workqueue_struct *wq = NULL;

    if (controller_display_wq == NULL) {
        return;
    }

    wq = controller_display_wq;
    controller_display_wq = NULL;
    destroy_workqueue(wq);
}

bool controller_schedule_display_work(controller_display_task_t task) {
    bool result = false;

    if (0 == down_interruptible(&controller_display_sem)) {
        controller_display_task = task;
        if (!controller_work_scheduled &&
            queue_work(controller_display_wq, &controller_display_work)) {
            controller_work_scheduled = true;
        }

        result = true;
        up(&controller_display_sem);
    }

    return result;
}

static void controller_display_work_function(struct work_struct *unused) {
    // wait for the controller to be ready
    if (EINKFB_SUCCESS == controller_wait_for_ready()) {
        // controller is ready, now perform the display task
        if (controller_display_task) {
            controller_display_task();
        } else {
            einkfb_print_info("No controller display task found!\n");
        }

        if (0 == down_interruptible(&controller_display_sem)) {
            controller_work_scheduled = false;
            up(&controller_display_sem);
        }
    } else {
        einkfb_print_info("Failed to perform display task! Controller not ready\n");
    }
}
