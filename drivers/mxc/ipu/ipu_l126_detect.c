/*
 *  linux/drivers/mxc/ipu/ipu_l126_detect.c --
 *  Lab126 specific controller early detection
 *
 *      Copyright (C) 2005-2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/einkfb.h>
#include <asm/arch/controller_common.c>
#include <asm/arch/controller_common_mxc.c>
#include <asm/arch/ipu_regs.h>
#include "../../video/eink/broadsheet/broadsheet_def.h"
#include "../../video/eink/auo/auo_def.h"

/* Controller detect routines should be of the following type */
typedef int (*ipu_controller_detect_t)(void);

/* Forward declarations for all controller detect routines */
static int ipu_test_broadsheet(void);
static int ipu_test_auo(void);

/* Enum for controller type */
enum ipu_controller_t {
    IPU_CONTROLLER_UNINIT   = -1,
    IPU_CONTROLLER_UNKNOWN  =  0,
    IPU_CONTROLLER_BS       =  1,
    IPU_CONTROLLER_AUO      =  2,
    /* Add more controllers here */
    IPU_CONTROLLER_TOTAL        ,
};

/* Structure which stores all the controllers to check for */
static struct {
    char *controller_name;
    ipu_controller_detect_t controller_detect;
} ipu_controllers[] = {
    [IPU_CONTROLLER_UNKNOWN] = {"emu",        NULL},  /* emu controller is the default
                                                         and should always be the at the
                                                         zero location */
    [IPU_CONTROLLER_BS]      = {"broadsheet", ipu_test_broadsheet},
    [IPU_CONTROLLER_AUO]     = {"auo",        ipu_test_auo},
    /* add more controllers here */
};

/* Variable which stores which controller is detected */
static enum ipu_controller_t controller_type  = IPU_CONTROLLER_UNINIT;

/* All controller detection routines here */
static int ipu_test_broadsheet(void) {
    u16                     product_id    = 0;
    int                     result        = EINKFB_FAILURE;

    controller_properties_t broad_props;

    BROADSHEET_CONFIG_CONTROLLER_PROPS(broad_props, NULL, NULL);

    if (controller_hw_init(false, &broad_props)) {
        // Read the product id

        // send the ipu read register command
        result = controller_wr_cmd(bs_cmd_RD_REG, false);
        // send the register to read
        if (EINKFB_SUCCESS == result) {
            u16 reg = BS_PRD_CODE_REG;
            result = controller_wr_dat(CONTROLLER_COMMON_WR_DAT_ARGS, 1, &reg);
        }
        // read product code
        if (EINKFB_SUCCESS == result) {
            result = controller_rd_one(&product_id);
        }

        if (EINKFB_SUCCESS == result && 
            (BS_PRD_CODE_ISIS == product_id || BS_PRD_CODE == product_id)) {
            // it's an isis / broadsheet
            result = EINKFB_SUCCESS;
        } else {
            result = EINKFB_FAILURE;
        }
    }
    
    // uninit the controller and ipu
    controller_hw_done();

    return result;
}

static int ipu_test_auo(void) {
    u16                     product_id    = 0;
    u16                     temp          = 0;
    int                     result        = EINKFB_FAILURE;
    controller_properties_t auo_props;
    AUO_CONFIG_CONTROLLER_PROPS(auo_props, 
                                AUO_SCREEN_WIDTH, 
                                AUO_SCREEN_HEIGHT, 
                                AUO_SCREEN_BPP, 
                                NULL, 
                                NULL);

    if (controller_hw_init(false, &auo_props)) {
        // Read the product id

        // send the AUO read version cmd
        result = controller_wr_cmd(AUO_READ_VERSION, false);

        // read the 1st parameter temp (ignore)
        if (EINKFB_SUCCESS == result) {
            result = controller_rd_one(&temp);
        }

        // read the 2nd parameter EPD type (ignore)
        if (EINKFB_SUCCESS == result) {
            result = controller_rd_one(&temp);
        }

        // read the 3rd parameter Panel/Product type (save)
        if (EINKFB_SUCCESS == result) {
            result = controller_rd_one(&product_id);
        }

        // read the 4th parameter LUT version (ignore)
        if (EINKFB_SUCCESS == result) {
            result = controller_rd_one(&temp);
        }

        if (EINKFB_SUCCESS == result && 
            (0 == product_id || AUO_PRD_CODE_2 == product_id || AUO_PRD_CODE_3 == product_id)) {
            // it's an auo
            result = EINKFB_SUCCESS;
        } else {
            result = EINKFB_FAILURE;
        }
    }

    // uninit the controller and ipu
    controller_hw_done();
    
    return result;
}
/* End of controller detection routines */

static int ipu_controller_type(void) {
    if (IPU_CONTROLLER_UNINIT == controller_type) {
        // controller type is not detected yet
        // do it.
        int i;

        // Go through all the controllers
        for (i = 1; i < IPU_CONTROLLER_TOTAL; ++i) {
            if (ipu_controllers[i].controller_detect &&
                EINKFB_SUCCESS == ipu_controllers[i].controller_detect()) {
                break;
            }
        }

        if (i == IPU_CONTROLLER_TOTAL) {
            // set to emu controller
            i = IPU_CONTROLLER_UNKNOWN;
        }

        // If we could not find any controller i will be == 0, which is emu 
        // else i will be some number which denotes the controller detected.
        controller_type = i;
        einkfb_print_info("Detected %s controller\n", ipu_controllers[i].controller_name);       
    }

    return controller_type;
}

// /sys/devices/platform/mxc_ipu/eink_controller_type (read)
//
static ssize_t show_eink_controller_type(struct device *dev, struct device_attribute *attr, char *buf) {
    return ( sprintf(buf, "%d\n", ipu_controller_type()) );
}
static DEVICE_ATTR(eink_controller_type, S_IRUGO, show_eink_controller_type, NULL);

int _ipu_l126_detect_init(struct platform_device *dev) {
    int unused;
    // create sysfs entry
    unused = device_create_file(&dev->dev, &dev_attr_eink_controller_type);

    return EINKFB_SUCCESS;
}

void _ipu_l126_detect_done(struct platform_device *dev) {
    device_remove_file(&dev->dev, &dev_attr_eink_controller_type);
}

