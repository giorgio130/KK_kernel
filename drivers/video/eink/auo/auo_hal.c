/*
 *  linux/drivers/video/eink/auo/auo_hal.c
 *  -- eInk frame buffer device HAL AUO/SiPix
 *
 *      Copyright (C) 2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include "auo.h"

// Default to Turing's resolution:  600x800@4bpp.
//
#define XRES_6               AUO_CONTROLLER_RESOLUTION_800_600_HSIZE
#define YRES_6               AUO_CONTROLLER_RESOLUTION_800_600_VSIZE
#define XRES_9               AUO_CONTROLLER_RESOLUTION_1024_768_HSIZE
#define YRES_9               AUO_CONTROLLER_RESOLUTION_1024_768_VSIZE
#define BPP                  EINKFB_4BPP
#define AUO_SIZE             BPP_SIZE((XRES_6*YRES_6), BPP)
#define AUO_BYTE_ALIGNMENT_X 2
#define AUO_BYTE_ALIGNMENT_Y 4

static struct fb_var_screeninfo auo_var =
{
    .xres                    = XRES_6,
    .yres                    = YRES_6,
    .xres_virtual            = XRES_6,
    .yres_virtual            = YRES_6,
    .bits_per_pixel          = BPP,
    .grayscale               = 1,
    .activate                = FB_ACTIVATE_TEST,
    .height                  = -1,
    .width                   = -1,
};

static struct fb_fix_screeninfo auo_fix =
{
    .id                      = EINKFB_NAME,
    .smem_len                = AUO_SIZE,
    .type                    = FB_TYPE_PACKED_PIXELS,
    .visual                  = FB_VISUAL_STATIC_PSEUDOCOLOR,
    .xpanstep                = 0,
    .ypanstep                = 0,
    .ywrapstep               = 0,
    .line_length             = BPP_SIZE(XRES_6, BPP),
    .accel                   = FB_ACCEL_NONE,
};

static unsigned long auo_bpp = BPP;
static int auo_orientation   = EINKFB_ORIENT_PORTRAIT;
static int auo_size          = 0;

#define EINKFB_PROC_AUO_CMD_WRITE            "auo_cmd_write"
#define EINKFB_PROC_AUO_CMD_READ             "auo_cmd_read"

static struct proc_dir_entry *einkfb_proc_auo_cmd_write          = NULL;
static struct proc_dir_entry *einkfb_proc_auo_cmd_read           = NULL;

#ifdef MODULE
module_param_named(auo_bpp, auo_bpp, long, S_IRUGO);
MODULE_PARM_DESC(auo_bpp, "1, 2, 4, or 8");

module_param_named(auo_orientation, auo_orientation, int, S_IRUGO);
MODULE_PARM_DESC(auo_orientation, "0 (portrait) or 1 (landscape)");

module_param_named(auo_size, auo_size, int, S_IRUGO);
MODULE_PARM_DESC(auo_size, "0 (default, 6-inch), 6 (6-inch), or 9 (9.7-inch)");
#endif // MODULE

#if PRAGMAS
    #pragma mark Procfs enteries
    #pragma mark -
#endif

static int write_auo_cmd(char *buf, unsigned long count, int unused) {
    int result = count;
    int cmd, argument;
    
    switch ( sscanf(buf, "%x %x", &cmd, &argument)) {
    case 1:
        // only cmd
        auo_mxc_wr_cmd(cmd, false);
        einkfb_print_info("Sent command : 0x%x\n", cmd);
        break;

    case 2:
        // cmd and argument
        auo_mxc_wr_cmd(cmd, false);
        auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, (u16 *)(&argument));
        einkfb_print_info("Sent command : 0x%x and argument : 0x%x\n", cmd, argument);
        break;

    default:
        // seems like more than one argument, can't handle now
        result = -EINVAL;
        einkfb_print_info("Unrecognized command and argument\n");
        break;
    }

    return result;
}

static int auo_cmd_write(struct file *file, const char __user *buf, unsigned long count, void *data) {
    return ( EINKFB_PROC_SYSFS_RW_LOCK((char *)buf, NULL, count, 0, NULL, 0, write_auo_cmd) );
}

static int read_auo_cmd(char *buf, unsigned long count, int unused) {
    int ret = count;
    int cmd, i, read_arg = 0, result = EINKFB_FAILURE; 
    u16 temp = 0;

    sscanf(buf, "%x %d", &cmd, &read_arg);

    // send the command to read
    result = auo_mxc_wr_cmd(cmd, false);

    if (EINKFB_SUCCESS == result) {
        // we were able to read the data
        einkfb_print_info("we were able to send data\n");

        // read the data
        for (i = 0; i < read_arg; ++i) {
            result = auo_mxc_rd_one(&temp);
            if (result == EINKFB_SUCCESS) {
                einkfb_print_info("Sent command : 0x%4x, read data : 0x%4x\n", cmd, temp);
            } else {
                einkfb_print_info("Failed to read data\n");
            }
        }
    } else {
        einkfb_print_info("we were not able to send data\n");
    }
    
    //einkfb_print_info("Sent command : 0x%4x, read data : 0x%4x\n", cmd, read_arg);

    return ret;
}

static int auo_cmd_read(struct file *file, const char __user *buf, unsigned long count, void *data) {
    return ( EINKFB_PROC_SYSFS_RW_LOCK((char *)buf, NULL, count, 0, NULL, 0, read_auo_cmd) );
}

#if PRAGMAS
    #pragma mark Sysfs enteries
    #pragma mark -
#endif

// /sys/devices/platform/eink_fb.0/auo_flashing_update_mode (read/write)
//
static ssize_t show_auo_flashing_update_mode(FB_DSHOW_PARAMS) {
    char *curr = buf;

    curr += sprintf(curr, "AUO Flashing update mode: 0x%x\n", auo_read_flashing_display_mode());
    curr += sprintf(curr, "\n");

    return curr - buf;
}

static ssize_t store_auo_flashing_update_mode(FB_DSTOR_PARAMS) {
    auo_display_mode_t value;
    if (sscanf(buf, "%x", &value) <= 0) {
        einkfb_debug("Could not store the AUO Flashing update mode\n");
        return -EINVAL;
    }
    
    if (!auo_change_flashing_display_mode(value)) {
        einkfb_debug("Could not store the AUO Flashing update mode\n");
        return -EINVAL;
    }

    return count;
}

// /sys/devices/platform/eink_fb.0/auo_nonflashing_update_mode (read/write)
//
static ssize_t show_auo_nonflashing_update_mode(FB_DSHOW_PARAMS) {
    char *curr = buf;

    curr += sprintf(curr, "AUO Non-Flashing update mode: 0x%x\n", auo_read_nonflashing_display_mode());
    curr += sprintf(curr, "\n");

    return curr - buf;
}

static ssize_t store_auo_nonflashing_update_mode(FB_DSTOR_PARAMS) {
    auo_display_mode_t value;
    if (sscanf(buf, "%x", &value) <= 0) {
        einkfb_debug("Could not store the AUO Non-Flashing update mode\n");
        return -EINVAL;
    }
    
    if (!auo_change_nonflashing_display_mode(value)) {
        einkfb_debug("Could not store the AUO Non-Flashing update mode\n");
        return -EINVAL;
    }

    return count;
}

// /sys/device/platform/eink_fb.0/auo_auto_mode (read/write)
//
static ssize_t show_auo_auto_mode(FB_DSHOW_PARAMS) {
    char *curr = buf;

    curr += sprintf(curr, "AUO Auto update mode: 0x%x\n", auo_read_auto_mode());
    curr += sprintf(curr, "\n");

    return curr - buf;
}

static ssize_t store_auo_auto_mode(FB_DSTOR_PARAMS) {
    int value;
    if (sscanf(buf, "%d", &value) <= 0) {
        einkfb_debug("Could not store the AUO Auto update mode\n");
        return -EINVAL;
    }

    if (!auo_change_auto_mode((auo_auto_mode_t)value)) {
        einkfb_debug("Could not store the AUO Auto update mode\n");
        return -EINVAL;
    }

    return count;
}

#if PRAGMAS
    #pragma mark HAL module operations
    #pragma mark -
#endif

static bool auo_sw_init(struct fb_var_screeninfo *var, struct fb_fix_screeninfo *fix)
{
    auo_resolution_t res;
    bool result = EINKFB_FAILURE;

    einkfb_print_info("AUO SW INIT called\n");
    if (auo_sw_init_controller(auo_orientation, auo_size, auo_bpp, &res)) {
    
        auo_bpp = res.bpp;

        auo_var.bits_per_pixel = res.bpp;
        auo_fix.smem_len = BPP_SIZE((res.x_sw*res.y_sw), res.bpp);
        auo_fix.line_length = BPP_SIZE(res.x_sw, res.bpp);
        auo_var.xres = auo_var.xres_virtual = res.x_sw;
        auo_var.yres = auo_var.yres_virtual = res.y_sw;

        *var = auo_var;
        *fix = auo_fix;

        result = EINKFB_SUCCESS;
    }
    
    return ( EINKFB_SUCCESS );
}

static void auo_sw_done(void) {
    auo_sw_done_controller();
}

static unsigned long auo_byte_alignment_x(void) {
    return ( AUO_BYTE_ALIGNMENT_X );
}

static unsigned long auo_byte_alignment_y(void) {
    return ( AUO_BYTE_ALIGNMENT_Y );
}

static bool auo_set_display_orientation(orientation_t orientation) {
    bool flip   = false;
    
    switch ( orientation ) {
        case orientation_portrait:
            auo_orientation = EINKFB_ORIENT_PORTRAIT;
            flip = false;
        break;
        case orientation_portrait_upside_down:
            auo_orientation = EINK_ORIENT_PORTRAIT;
            flip = true;    
        break;
        
        case orientation_landscape:
            auo_orientation = EINK_ORIENT_LANDSCAPE;
            flip = false;
        break;
        case orientation_landscape_upside_down:
            auo_orientation = EINK_ORIENT_LANDSCAPE;
            flip = true;
        break;
        default:
            auo_orientation = EINKFB_ORIENT_PORTRAIT;
            flip = false;
    }

    auo_set_controller_orientation(auo_orientation, flip, true);
    
    return true;
}

static orientation_t auo_get_display_orientation(void) {
    orientation_t orientation;
    bool flip = false;
    
    auo_get_controller_orientation(&auo_orientation, &flip);

    switch (auo_orientation) {
        case EINKFB_ORIENT_PORTRAIT:
            if (flip) {
                orientation = orientation_portrait_upside_down;
            } else {
                orientation = orientation_portrait;
            }
        break;

        case EINKFB_ORIENT_LANDSCAPE:
            if (flip) {
                orientation = orientation_landscape_upside_down;
            } else {
                orientation = orientation_landscape;
            }
        break;

        default:
            orientation = orientation_portrait;
    }
    
    return ( orientation );
}

static bool auo_hw_init(struct fb_info *info, bool full) {
    // Do nothing. HW init handled already.
    return true;
}

static void auo_hw_done(bool full) {
    // Do nothing.
}

static void auo_update_display(fx_type update_mode) {
    unsigned long start = jiffies;
    auo_full_load_display(update_mode);
    einkfb_debug("Full update : %d\n", jiffies_to_msecs(jiffies - start));
}

static void auo_update_area(update_area_t *update_area) {
    unsigned long start = jiffies;
    //auo_update_display(update_area->which_fx);
    int xstart  = update_area->x1 + 1, xend = update_area->x2 + 1,
        ystart  = update_area->y1 + 1, yend = update_area->y2 + 1,
        xres    = xend - xstart,
        yres    = yend - ystart;

    auo_area_load_display(update_area->buffer, 
                          update_area->which_fx, 
                          xstart, 
                          ystart, 
                          xres, 
                          yres);
    einkfb_debug("Area update : %d\n", jiffies_to_msecs(jiffies - start));
}

static void auo_set_power_level(einkfb_power_level power_level) {
    auo_power_states power_state;

    switch (power_level) {
    case einkfb_power_level_on:
        power_state = auo_power_state_run;
        goto set_power_state;
        
    case einkfb_power_level_standby:
        power_state = auo_power_state_standby;
        goto set_power_state;

    case einkfb_power_level_off:
        // auo_blank_screen();
    case einkfb_power_level_sleep:
        power_state = auo_power_state_sleep;
        
    set_power_state:
        auo_set_power_state(power_state);
        break;

        // Blank screen in eInk HAL's standby mode.
    case einkfb_power_level_blank:
        // auo_blank_screen();
        auo_set_power_state(auo_power_state_standby);
        break;

    default:
        break;
    }
}

static einkfb_power_level auo_get_power_level(void) {
    einkfb_power_level power_level;
    
    switch(auo_get_power_state()) {
    case auo_power_state_run:
        power_level = einkfb_power_level_on;
        break;
    case auo_power_state_standby:
        power_level = einkfb_power_level_standby;
        break;
    case auo_power_state_sleep:
        power_level = einkfb_power_level_off;
        break;
    default:
        power_level = einkfb_power_level_init;
        break;
    }

    return power_level;
}

/* AUO related entries */
static DEVICE_ATTR(auo_flashing_update_mode, DEVICE_MODE_RW, show_auo_flashing_update_mode, store_auo_flashing_update_mode);
static DEVICE_ATTR(auo_nonflashing_update_mode, DEVICE_MODE_RW, show_auo_nonflashing_update_mode,  store_auo_nonflashing_update_mode);
static DEVICE_ATTR(auo_auto_mode, DEVICE_MODE_RW, show_auo_auto_mode, store_auo_auto_mode);

static void auo_create_proc_entries(void) {
    struct einkfb_info info; einkfb_get_info(&info);

    // Create AUO-specific proc enteries.
    einkfb_proc_auo_cmd_write = einkfb_create_proc_entry(EINKFB_PROC_AUO_CMD_WRITE, EINKFB_PROC_CHILD_W,
                                                    NULL, auo_cmd_write);

    einkfb_proc_auo_cmd_read = einkfb_create_proc_entry(EINKFB_PROC_AUO_CMD_READ, EINKFB_PROC_CHILD_W,
                                                    NULL, auo_cmd_read);

    // Create sys enteries
    FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_auo_flashing_update_mode);
    FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_auo_nonflashing_update_mode);
    FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_auo_auto_mode);
}

static void auo_remove_proc_entries(void) {
    struct einkfb_info info; einkfb_get_info(&info);
    
    einkfb_remove_proc_entry(EINKFB_PROC_AUO_CMD_WRITE, einkfb_proc_auo_cmd_write);
    einkfb_remove_proc_entry(EINKFB_PROC_AUO_CMD_READ, einkfb_proc_auo_cmd_read);

    device_remove_file(&info.dev->dev, &dev_attr_auo_flashing_update_mode);
    device_remove_file(&info.dev->dev, &dev_attr_auo_nonflashing_update_mode);
    device_remove_file(&info.dev->dev, &dev_attr_auo_auto_mode);
}

bool auo_needs_dma(void) {
    return false;
}

static einkfb_hal_ops_t auo_hal_ops =
{
    .hal_sw_init                 = auo_sw_init,
    .hal_sw_done                 = auo_sw_done,
    
    .hal_hw_init                 = auo_hw_init,
    .hal_hw_done                 = auo_hw_done,

    .hal_create_proc_entries     = auo_create_proc_entries,
    .hal_remove_proc_entries     = auo_remove_proc_entries,

    .hal_update_display          = auo_update_display,
    .hal_update_area             = auo_update_area,

    .hal_set_power_level         = auo_set_power_level,
    .hal_get_power_level         = auo_get_power_level,

    .hal_byte_alignment_x        = auo_byte_alignment_x,
    .hal_byte_alignment_y        = auo_byte_alignment_y,
    
    .hal_set_display_orientation = auo_set_display_orientation,
    .hal_get_display_orientation = auo_get_display_orientation,

    .hal_needs_dma_buffer        = auo_needs_dma
};

static int auo_hal_init(void) {
    einkfb_print_info("AUO HAL INIT called\n");
    return ( einkfb_hal_ops_init(&auo_hal_ops) );
}

module_init(auo_hal_init);

#ifdef MODULE
static void auo_hal_exit(void) {
    einkfb_hal_ops_done();
}

module_exit(auo_hal_exit);

MODULE_AUTHOR("Lab126");
MODULE_LICENSE("GPL");
#endif // MODULE

