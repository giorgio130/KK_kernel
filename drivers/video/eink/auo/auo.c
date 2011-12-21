/*
 *  linux/drivers/video/eink/broadsheet/broadsheet.c
 *  -- eInk frame buffer device HAL AUO sw
 *
 *      Copyright (C) 2005-2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include <asm/arch/controller_common_display.c>
#include "auo.h"

//#define AUO_SOFTWARE_ROTATE 1
//#define AUO_SOFTWARE_NYBBLE_SWAP 1
#define AUO_POWER_MANAGEMENT_DOESNOT_WORK 1
#define AUO_USE_HW_DDMA_NO_MONITOR 1

static auo_display_mode_t auo_flashing_display_mode            = AUO_GRAY_REFRESH_FLASH;
static auo_display_mode_t auo_nonflashing_display_mode         = AUO_GRAY_REFRESH_NONFLASH;
static auo_auto_mode_t auo_auto_mode                           = AUO_SW_AUTO_MODE;
static int auo_bpp_display_mode                                = 0;
static auo_display_mode_t auo_override_mode                    = AUO_NONE;

static int auo_controller_orientation                          = EINKFB_ORIENT_PORTRAIT;
static bool auo_controller_orientation_flip                    = false;
static auo_controller_resolution_t auo_controller_resolution   = AUO_CONTROLLER_RESOLUTION_800_600;
static int auo_controller_bpp                                  = EINKFB_4BPP;
static auo_power_states auo_power_state                        = auo_power_state_init;
static dma_addr_t auo_phys_addr                                = 0;

static u16 auo_update_x                                        = 1;
static u16 auo_update_y                                        = 1;
static u16 auo_update_w                                        = 0;
static u16 auo_update_h                                        = 0;
DECLARE_MUTEX(auo_update_sem);

#ifdef AUO_SOFTWARE_NYBBLE_SWAP
static u8 auo_4bpp_nybble_swap_table_inverted[256] =
{
    0xFF, 0xEF, 0xDF, 0xCF, 0xBF, 0xAF, 0x9F, 0x8F, 0x7F, 0x6F, 0x5F, 0x4F, 0x3F, 0x2F, 0x1F, 0x0F,
    0xFE, 0xEE, 0xDE, 0xCE, 0xBE, 0xAE, 0x9E, 0x8E, 0x7E, 0x6E, 0x5E, 0x4E, 0x3E, 0x2E, 0x1E, 0x0E,
    0xFD, 0xED, 0xDD, 0xCD, 0xBD, 0xAD, 0x9D, 0x8D, 0x7D, 0x6D, 0x5D, 0x4D, 0x3D, 0x2D, 0x1D, 0x0D,
    0xFC, 0xEC, 0xDC, 0xCC, 0xBC, 0xAC, 0x9C, 0x8C, 0x7C, 0x6C, 0x5C, 0x4C, 0x3C, 0x2C, 0x1C, 0x0C,
    0xFB, 0xEB, 0xDB, 0xCB, 0xBB, 0xAB, 0x9B, 0x8B, 0x7B, 0x6B, 0x5B, 0x4B, 0x3B, 0x2B, 0x1B, 0x0B,
    0xFA, 0xEA, 0xDA, 0xCA, 0xBA, 0xAA, 0x9A, 0x8A, 0x7A, 0x6A, 0x5A, 0x4A, 0x3A, 0x2A, 0x1A, 0x0A,
    0xF9, 0xE9, 0xD9, 0xC9, 0xB9, 0xA9, 0x99, 0x89, 0x79, 0x69, 0x59, 0x49, 0x39, 0x29, 0x19, 0x09,
    0xF8, 0xE8, 0xD8, 0xC8, 0xB8, 0xA8, 0x98, 0x88, 0x78, 0x68, 0x58, 0x48, 0x38, 0x28, 0x18, 0x08,
    0xF7, 0xE7, 0xD7, 0xC7, 0xB7, 0xA7, 0x97, 0x87, 0x77, 0x67, 0x57, 0x47, 0x37, 0x27, 0x17, 0x07,
    0xF6, 0xE6, 0xD6, 0xC6, 0xB6, 0xA6, 0x96, 0x86, 0x76, 0x66, 0x56, 0x46, 0x36, 0x26, 0x16, 0x06,
    0xF5, 0xE5, 0xD5, 0xC5, 0xB5, 0xA5, 0x95, 0x85, 0x75, 0x65, 0x55, 0x45, 0x35, 0x25, 0x15, 0x05,
    0xF4, 0xE4, 0xD4, 0xC4, 0xB4, 0xA4, 0x94, 0x84, 0x74, 0x64, 0x54, 0x44, 0x34, 0x24, 0x14, 0x04,
    0xF3, 0xE3, 0xD3, 0xC3, 0xB3, 0xA3, 0x93, 0x83, 0x73, 0x63, 0x53, 0x43, 0x33, 0x23, 0x13, 0x03,
    0xF2, 0xE2, 0xD2, 0xC2, 0xB2, 0xA2, 0x92, 0x82, 0x72, 0x62, 0x52, 0x42, 0x32, 0x22, 0x12, 0x02,
    0xF1, 0xE1, 0xD1, 0xC1, 0xB1, 0xA1, 0x91, 0x81, 0x71, 0x61, 0x51, 0x41, 0x31, 0x21, 0x11, 0x01,
    0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90, 0x80, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10, 0x00,
};
#endif

#define UM02 AUO_02 // 1 bpp
#define UM04 AUO_04 // 2 bpp
#define UM16 AUO_16 // 4 bpp
static u8 auo_4bpp_auto_mode_table[256] =
{
    UM02, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM02,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM04, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM04,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM04, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM04,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16, UM16,
    UM02, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM04, UM16, UM16, UM16, UM16, UM02
};


// Benchmarking variables
//
static unsigned long auo_load_image_data_start;

#ifdef AUO_SOFTWARE_ROTATE
// right now it is fixed size, would want to make it
// dynamic to support 9 inch displays
u8 scratch_buf[600 * 800 / 2];
static u8 auo_nybble_read(u8 *data, int index) {
    int byte = (index / 2);
    int nybble_position = ((index % 2) ? 0 : 1);
    
    return (data[byte] >> (4 * (nybble_position)) & 0x0F);
}

static void auo_nybble_write(u8 *data, int index, u8 nybble) {
    int byte = (index / 2);
    int nybble_position = ((index % 2) ? 0 : 1);
    
    if (nybble_position) {
        data[byte] = 0;
    }

    data[byte] |= (nybble << (4 * nybble_position));
}

static void auo_software_rotate(u16 src_w, u16 src_h, u8 *src_data, u8 *dst_data) {
    int h, w, i = 0;
    u8 temp;

    for (w = 0; w < src_w; ++w) {
        for (h = src_h - 1; h >= 0; --h) {
            temp = auo_nybble_read(src_data, ((h * src_w) + w));
            auo_nybble_write(dst_data, i++, temp);
        }
        EINKFB_SCHEDULE_BLIT(w+1);
    }
}
#endif

#ifdef AUO_SOFTWARE_NYBBLE_SWAP
static void auo_nybble_swap(u8 *data, u16 w, u16 h) {
    int i = 0;
    // size is (w * h) / 2 because AUO is always in 4bpp mode
    for (i = 0; i < (w * h)/2; i++) {
        data[i] = auo_4bpp_nybble_swap_table_inverted[data[i]];
    }
}
#endif

void auo_clear_phys_addr(void) {
    auo_phys_addr = 0;
}

static void auo_set_phys_addr(bool area_update) {
    struct einkfb_info info;
    einkfb_get_info(&info);

    auo_clear_phys_addr();
    if (area_update) {
        auo_phys_addr = info.phys->addr + EINKFB_PHYS_BUF_OFFSET(info);
    } else {
        if ( EINKFB_RESTORE(info) ) {
            auo_phys_addr = info.phys->addr + EINKFB_PHYS_VFB_OFFSET(info);
        } else {
            auo_phys_addr = info.phys->addr;
        }
    }
}

dma_addr_t auo_get_phys_addr(void) {
    return auo_phys_addr;
}

static void auo_reset_coordinates(void) {
    auo_update_x = 1;
    auo_update_y = 1;
    auo_update_w = 0;
    auo_update_h = 0;
}

#define MIN(a,b) a < b ? a : b
#define MAX(a,b) a > b ? a : b
static void auo_update_coordinates(u16 x, u16 y, u16 w, u16 h) {
    if (0 == down_interruptible(&auo_update_sem)) {
        if (auo_update_w == 0 || auo_update_h == 0) {
            auo_update_x = x;
            auo_update_y = y;
            auo_update_w = w;
            auo_update_h = h;
        } else {
            u16 new_x1, new_y1, new_x2, new_y2;
        
            new_x1 = MIN(auo_update_x, x);
            new_y1 = MIN(auo_update_y, y);
            new_x2 = MAX((auo_update_x + auo_update_w), (x + w));
            new_y2 = MAX((auo_update_y + auo_update_h), (y + h));

            auo_update_x = new_x1;
            auo_update_y = new_y1;
            auo_update_w = (new_x2 - new_x1);
            auo_update_h = (new_y2 - new_y1);
        }
        up(&auo_update_sem);
    }
}

static bool auo_coordinates_ok(u16 x, u16 y, u16 w, u16 h) {
    bool result = true;
    // if x and y is less than 1
    if (x < 1 || y < 1) {
        result = false;
    }

    // if width or height is 0
    if (0 == w || 0 == h) {
        result = false;
    }
    
    return result;
}

static int auo_auto_mode_detect(u8 *data, u32 data_size) {
    int i = 0;
    int mode = 0;

    for (i = 0; mode != UM16 && i < data_size; ++i) {
        mode |= auo_4bpp_auto_mode_table[data[i]];
    }
    
    return mode;
}

static auo_display_mode_t auo_get_flashing_display_mode(int bpp_mode) {
    switch (bpp_mode) {
    case UM16:
        return AUO_GRAY_REFRESH_FLASH;
    case UM04:
    case UM02:
        return AUO_TEXT_MODE_FLASH;
    default:
        return AUO_GRAY_REFRESH_FLASH;
    }
}

static auo_display_mode_t auo_get_nonflashing_display_mode(int bpp_mode) {
    switch (bpp_mode) {
    case UM16:
        return AUO_GRAY_REFRESH_NONFLASH;
    case UM04:
        return AUO_TEXT_MODE_NONFLASH;
    case UM02:
        //return AUO_HIGH_SPEED_NONFLASH;
        return AUO_TEXT_MODE_NONFLASH;

    default:
        return AUO_GRAY_REFRESH_NONFLASH;
    }
}

static void auo_dma_image_data(u8 *data, u16 x, u16 y, u16 w, u16 h) {
    u16 mode = 0;

    // set what orientation the host is sending the data
    switch (auo_controller_orientation) {
    case EINKFB_ORIENT_PORTRAIT:
        einkfb_debug("DMA send portrait data\n");
        mode |= AUO_HPL_POTRAIT;
        //mode |= AUO_HPL_LANDSCAPE;        
        break;

    case EINKFB_ORIENT_LANDSCAPE:
        einkfb_debug("DMA send landscape data\n");
        mode |= AUO_HPL_LANDSCAPE;        
        //mode |= AUO_HPL_POTRAIT;
        break;

    default:
        mode |= AUO_HPL_POTRAIT;
    }

    // set where to put the image in the controller buffer
    mode |= AUO_FOREGROUND;

    // set the x location
    mode |= x;

    // get update semaphore so that we dont stomp 
    // on an update in progress
    if (0 == down_interruptible(&auo_update_sem)) {
        einkfb_debug("Going to start dma update: x:%d y:%d w:%d h:%d\n", x, y, w, h);
        // we are doing dma, controller should ignore hw ready
        controller_set_ignore_hw_ready(true);

        // start sending the dma data
        auo_mxc_wr_cmd(AUO_DMA_START, false);

        auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &mode);       // Display Mode and x start
        auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &y);          // y start
        auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &w);          // width
        auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &h);          // height

        auo_mxc_wr_dat(AUO_WR_DAT_DATA, (w * h)/(16/auo_controller_bpp), (u16 *)data);    

        // stop dma command
        auo_mxc_wr_cmd(AUO_DMA_STOP, false);

        einkfb_debug("Done dma update: x:%d y:%d w:%d h:%d\n", x, y, w, h);
        // stop ignoring the hw ready 
        controller_set_ignore_hw_ready(false);

        up(&auo_update_sem);
    }
}

static void auo_update_flashing(u16 x, u16 y, u16 w, u16 h) {
    u16 mode = 0;

    // set the flashing mode type
    mode = auo_flashing_display_mode;
    einkfb_debug("Starting flashing update: x:%d y:%d w:%d h:%d mode:0x%x\n", x, y, w, h, mode);

    // set the rotation
    if (auo_controller_orientation_flip) {
        einkfb_debug("Flipping data\n");
        mode |= AUO_FLIP_0;        
    } else {
        mode |= AUO_FLIP_180;
    }

    // send the display update command
    auo_mxc_wr_cmd(AUO_DISP_START, false);
    
    // send the args
    auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &mode);
    auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &x);
    auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &y);
    auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &w);
    auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &h);

    einkfb_debug("Done flashing update: x:%d y:%d w:%d h:%d\n", x, y, w, h);
}

static void auo_update_nonflashing(void) {
    u16 mode = 0;

    // set the flashing mode type
    mode = auo_nonflashing_display_mode;
    // set the rotation
    if (auo_controller_orientation_flip) {
        einkfb_debug("Flipping data\n");
        mode |= AUO_FLIP_0;
    } else {
        mode |= AUO_FLIP_180;
    }

    if (0 == down_interruptible(&auo_update_sem)) {
        if (auo_coordinates_ok(auo_update_x, auo_update_y, auo_update_w, auo_update_h)) {
            einkfb_debug("Starting non flashing update: x:%d y:%d w:%d h:%d mode:0x%x\n", 
                         auo_update_x, auo_update_y, auo_update_w, auo_update_h, auo_nonflashing_display_mode);
#ifdef AUO_USE_HW_DDMA_NO_MONITOR
            controller_set_ignore_hw_ready(true);
#endif
            // send the display update command
            auo_mxc_wr_cmd(AUO_DISP_START, false);
    
            // send the args
            auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &mode);
            auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &auo_update_x);
            auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &auo_update_y);
            auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &auo_update_w);
            auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, &auo_update_h);

            auo_reset_coordinates();

            if (AUO_SW_AUTO_MODE == auo_auto_mode) {
                auo_bpp_display_mode = 0;
                auo_nonflashing_display_mode = AUO_GRAY_REFRESH_NONFLASH;
            }
#ifdef AUO_USE_HW_DDMA_NO_MONITOR
            controller_set_ignore_hw_ready(false);
#endif
        }
        up(&auo_update_sem);
    } else {
        einkfb_print_info("Could not perform nonflashing update x: %d y: %d w: %d h: %d\n", 
                          auo_update_x, auo_update_y, auo_update_w, auo_update_h);
    }
}

static void auo_load_image_data(u8 *data, fx_type update_mode, bool area_update, u16 x, u16 y, u16 w, u16 h) {
    fx_type local_update_mode;
    struct einkfb_info info;
    auo_bpp_update_mode_t saved_override_mode = auo_override_mode;
    u16 auo_display_mode                      = auo_flashing_display_mode;
    bool flashing_update                      = true, 
         skip_buffer_display                  = false,
         skip_buffer_load                     = false;
#ifdef AUO_SOFTWARE_ROTATE
    u16 tmp;
#endif

    // get einkfb info
    //
    einkfb_get_info(&info);

    switch (update_mode) {
        
        // Just load up the hardware's buffer; don't display it
        //
        case fx_buffer_load:
            local_update_mode = fx_update_partial;
            skip_buffer_display = true;
        break;

        // Just display what's already in the hardware buffer
        //
        case fx_buffer_display_partial:
        case fx_buffer_display_full:
            skip_buffer_load = true;
        goto set_update_mode;

        // Regardless of what gets put into the hardware's buffer,
        // only update the black and white pixels,
        //
        case fx_update_fast:
            auo_override_mode = AUO_02;
        goto set_update_mode;

        // Regardless of what gets put into the hardware's buffer,
        // refresh all pixels as cleanly as possible.
        //
        case fx_update_slow:
            auo_override_mode = AUO_16;
        goto set_update_mode;
            
        set_update_mode:
        default:
            local_update_mode = area_update ? UPDATE_AREA_MODE(update_mode)
                                            : UPDATE_MODE(update_mode);
            flashing_update = UPDATE_AREA_FULL(local_update_mode);
            auo_display_mode = flashing_update ? auo_flashing_display_mode : auo_nonflashing_display_mode;
    }

    if (data && !skip_buffer_load) {
#ifdef AUO_SOFTWARE_ROTATE
        einkfb_debug("Before rotate; w: %d, h: %d, x: %d, y: %d\n",
                     w, h, x, y);
        if (0 != (h % 4)) {
            // cannot handle height not multiple of 4
            h = (h/4) * 4;
        }
        auo_software_rotate(w, h, data, scratch_buf);
        data = scratch_buf;
        // swap x and y
        tmp = y;
        y = x;
        x = (info.yres + 1) - (tmp + h - 1);
        // swap width and height without temp variable
        h = w ^ h;
        w = h ^ w;
        h = w ^ h;
    
        einkfb_debug("After rotate; w: %d, h: %d, x: %d, y: %d\n",
                     w, h, x, y);
#endif

#ifdef AUO_SOFTWARE_NYBBLE_SWAP
        // Nybble swap
        auo_nybble_swap(data, w, h);
#endif
    
        // for benchmark purposes
        auo_load_image_data_start = jiffies;
        
        // dma the image data
        auo_dma_image_data(data, x, y, w, h);
    }

    if (!skip_buffer_display) {
        // update the display
        if (flashing_update) {
            if (AUO_NONE != auo_override_mode) {
                auo_flashing_display_mode = auo_get_flashing_display_mode(auo_override_mode);
            } else {
                if (AUO_SW_AUTO_MODE == auo_auto_mode) {
                    // determine the mode
                    auo_flashing_display_mode = auo_get_flashing_display_mode(auo_auto_mode_detect(data, ((w * h) / (8 / auo_controller_bpp)) ));
                }
            }
            
            // for flashing update the display immediately
            auo_update_flashing(x, y, w, h);
        } else {
            if (AUO_NONE != auo_override_mode) {
                auo_nonflashing_display_mode = auo_get_nonflashing_display_mode(auo_override_mode);
            } else {
                if (AUO_SW_AUTO_MODE == auo_auto_mode) {
                    auo_bpp_display_mode |= auo_auto_mode_detect(data, ((w * h) / (8 / auo_controller_bpp)) );
                    auo_nonflashing_display_mode = auo_get_nonflashing_display_mode(auo_bpp_display_mode);
                }
            }

            // Update the coordinates
            auo_update_coordinates(x, y, w, h);

            // schedule the non flashing updates for later
#ifdef AUO_USE_HW_DDMA_NO_MONITOR
            auo_update_nonflashing();
#else
            controller_schedule_display_work(auo_update_nonflashing);
#endif
        }
    }

    // Restore the override mode
    //
    auo_override_mode = saved_override_mode;

    einkfb_debug("load image data time, display mode: 0x%x : %d\n", auo_display_mode, jiffies_to_msecs(jiffies - auo_load_image_data_start));
}

void auo_full_load_display(fx_type update_mode) {
    struct einkfb_info info;
    u8 *buffer;
    
    einkfb_get_info(&info);    
    buffer = info.start;
    auo_set_phys_addr(false);
    auo_load_image_data(buffer, update_mode, false, 1, 1, info.xres, info.yres);
}

void auo_area_load_display(u8 *data, fx_type update_mode, u16 x, u16 y, u16 w, u16 h) {
    auo_set_phys_addr(true);
    auo_load_image_data(data, update_mode, true, x, y, w, h);
}

bool auo_change_flashing_display_mode(auo_display_mode_t new_display_mode) {
    if (AUO_NO_AUTO_MODE == auo_auto_mode) {
        auo_flashing_display_mode = new_display_mode;
        return true;
    }

    return false;
}

auo_display_mode_t auo_read_flashing_display_mode(void) {
    return auo_flashing_display_mode;
}

bool auo_change_nonflashing_display_mode(auo_display_mode_t new_display_mode) {
    if (AUO_NO_AUTO_MODE == auo_auto_mode) {
        auo_nonflashing_display_mode = new_display_mode;
        return true;
    }

    return false;
}

auo_display_mode_t auo_read_nonflashing_display_mode(void) {
    return auo_nonflashing_display_mode;
}

bool auo_change_auto_mode(auo_auto_mode_t new_auto_mode) {
    switch (new_auto_mode) {
        case AUO_NO_AUTO_MODE:
        case AUO_SW_AUTO_MODE:
            // default to 4-bit gray level mode
            auo_flashing_display_mode = AUO_GRAY_REFRESH_FLASH;
            auo_nonflashing_display_mode = AUO_GRAY_REFRESH_NONFLASH;
            auo_auto_mode = new_auto_mode;
        return true;

        case AUO_HW_AUTO_MODE:
            // default to Auto mode update
            auo_flashing_display_mode = AUO_AUTO_FLASH;
            auo_nonflashing_display_mode = AUO_AUTO_NONFLASH;
            auo_auto_mode = new_auto_mode;
        return true;

        default:
            // Unrecognised mode
        return false;
    }
}

auo_auto_mode_t auo_read_auto_mode(void) {
    return auo_auto_mode;
}

auo_power_states auo_get_power_state(void) {
    return auo_power_state;
}

void auo_set_power_state(auo_power_states power_state) {
#ifdef AUO_POWER_MANAGEMENT_DOESNOT_WORK
    // do nothing 
    auo_power_state = power_state;
#else
    if (auo_power_state != power_state) {

        einkfb_print_info("Changing state %d -> %d\n", auo_power_state, power_state);
        
        switch (power_state) {
        // run
        //
        case auo_power_state_run:
            switch (auo_power_state) {
            // Initial state. Do nothing
            case auo_power_state_init:
                auo_power_state = auo_power_state_run;
                break;

            case auo_power_state_sleep:
                // TODO: sleep -> standby
            case auo_power_state_standby:
                // standby -> run
                auo_mxc_wr_cmd(AUO_WAKEUP, false);
                auo_power_state = auo_power_state_run;
                break;
            default:
                break;
            } // switch auo_power_state
            break;

        // standby
        //
        case auo_power_state_standby:
            switch (auo_power_state) {
            case auo_power_state_init:
                break;

            case auo_power_state_run:
                auo_mxc_wr_cmd(AUO_STANDBY, false);
                auo_power_state = auo_power_state_standby;
                break;

            case auo_power_state_sleep:
                // TODO sleep -> standby
                auo_power_state = auo_power_state_standby;
                break;
                
            default:
                break;
            } // switch auo_power_state
            break;

        // sleep
        //
        case auo_power_state_sleep:
            switch (auo_power_state) {
            case auo_power_state_init:
                break;

            case auo_power_state_run:
                // run -> standby
                auo_mxc_wr_cmd(AUO_STANDBY, false);
            case auo_power_state_sleep:
                // TODO: standby -> sleep
                auo_power_state = auo_power_state_sleep;
                break;
            default:
                break;
            } // switch auo_power_state
            break;

        default:
            break;

        }// switch power_state

    } // if auo_power_state != power_state
#endif
}

static void auo_send_init_command(bool refresh_display) {
    unsigned int controller_init = 0;
    //unsigned int internal_command = 0x0800;
    
    // set non user changeable configuration first
    controller_init |= AUO_SHL_REVERSE;
    controller_init |= AUO_UD_DEFAULT;
    
    // nybble swap and bpp
    switch (auo_controller_bpp) {
        default:
            einkfb_debug("Controller bpp is 4 bit revers\n");
            controller_init |= AUO_INPUT_ARRANGEMENT_4BIT_REVERSE;
            break;

        case EINKFB_4BPP:
            einkfb_debug("Controller bpp is 4 bit reverse\n");
            controller_init |= AUO_INPUT_ARRANGEMENT_4BIT_REVERSE;
            break;
            
        case EINKFB_8BPP:
            einkfb_debug("Controller bpp is 8 bit\n");
            controller_init |= AUO_INPUT_ARRANGEMENT_8BIT_DEFAULT;
            break;
    }

    // pixel polarity
    controller_init |= AUO_PIXEL_POLARITY_DEFAULT;

    // resolution
    controller_init |= auo_controller_resolution;
    einkfb_debug("Controller resolution : 0x%x\n", auo_controller_resolution);

    // send the init command to AUO now
    auo_mxc_wr_cmd(AUO_INIT_SET, false);
    auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, (u16 *)&controller_init);

    // Internal AUO command 
    //
    //auo_mxc_wr_cmd(0xf230, false);
    //auo_mxc_wr_dat(AUO_WR_DAT_ARGS, 1, (u16 *)&internal_command);

    // refresh the display
    //if (refresh_display) {
    //    auo_mxc_wr_cmd(AUO_DISP_RESET, false);
    //}
}

static void auo_set_resolution(int auo_size, int auo_bpp) {
    switch (auo_size) {
        default:
        case 6:
            //auo_controller_resolution = AUO_CONTROLLER_RESOLUTION_600_800;
            auo_controller_resolution = AUO_CONTROLLER_RESOLUTION_800_600;
            break;

        case 9:
            auo_controller_resolution = AUO_CONTROLLER_RESOLUTION_1024_768;
            break;
    }

    switch (auo_bpp) {
        default:
        case EINKFB_4BPP:
            auo_controller_bpp = EINKFB_4BPP;
            break;

        case EINKFB_8BPP:
            auo_controller_bpp = EINKFB_8BPP;
            break;
    }
}

static void auo_get_resolution(int auo_orientation, auo_resolution_t *res) {

    switch (auo_controller_resolution) {

        case AUO_CONTROLLER_RESOLUTION_800_600:
            res->x_hw = AUO_CONTROLLER_RESOLUTION_800_600_HSIZE;
            res->y_hw = AUO_CONTROLLER_RESOLUTION_800_600_VSIZE;

            res->x_sw = (EINKFB_ORIENT_PORTRAIT == auo_orientation) ? AUO_CONTROLLER_RESOLUTION_800_600_VSIZE : AUO_CONTROLLER_RESOLUTION_800_600_HSIZE;
            res->y_sw = (EINKFB_ORIENT_PORTRAIT == auo_orientation) ? AUO_CONTROLLER_RESOLUTION_800_600_HSIZE : AUO_CONTROLLER_RESOLUTION_800_600_VSIZE;
            break;

        case AUO_CONTROLLER_RESOLUTION_1024_768:
            res->x_hw = AUO_CONTROLLER_RESOLUTION_1024_768_HSIZE;
            res->y_hw = AUO_CONTROLLER_RESOLUTION_1024_768_VSIZE;
            
            res->x_sw = (EINKFB_ORIENT_PORTRAIT == auo_orientation) ? AUO_CONTROLLER_RESOLUTION_1024_768_VSIZE : AUO_CONTROLLER_RESOLUTION_1024_768_HSIZE;
            res->y_sw = (EINKFB_ORIENT_PORTRAIT == auo_orientation) ? AUO_CONTROLLER_RESOLUTION_1024_768_HSIZE : AUO_CONTROLLER_RESOLUTION_1024_768_VSIZE;
            break;

        default:
            res->x_hw = AUO_CONTROLLER_RESOLUTION_800_600_HSIZE;
            res->y_hw = AUO_CONTROLLER_RESOLUTION_800_600_VSIZE;

            res->x_sw = AUO_CONTROLLER_RESOLUTION_800_600_VSIZE;
            res->y_sw = AUO_CONTROLLER_RESOLUTION_800_600_HSIZE;
    }

    res->bpp  = auo_controller_bpp;
}

void auo_get_controller_orientation(int *auo_orientation, bool *flip) {
    *auo_orientation = auo_controller_orientation;
    *flip            = auo_controller_orientation_flip;
}

void auo_set_controller_orientation(int auo_orientation, bool flip, bool send_init) {
    
    if (auo_controller_orientation == auo_orientation &&
        auo_controller_orientation_flip == flip) {
        // skip because we are already there.
        return;
    }

    switch (auo_orientation) {
        case EINKFB_ORIENT_PORTRAIT:
            auo_controller_orientation = auo_orientation;
            break;

        case EINKFB_ORIENT_LANDSCAPE:
            auo_controller_orientation = auo_orientation;
            break;

        default:
            auo_controller_orientation = EINKFB_ORIENT_PORTRAIT;
            break;
    }

    auo_controller_orientation_flip = flip;

    if (send_init) {
        auo_send_init_command(false);
    }
}

static bool auo_hw_init_controller(auo_resolution_t *res) {
    bool result = false;

    // First get the IPU initilaized
    if (auo_mxc_init(res->x_sw, res->y_sw, res->bpp)) {
        // send auo init command
        auo_send_init_command(true);

        // set the power state
        auo_set_power_state(auo_power_state_run);

        result = true;
    }

    return result;
}

static void auo_hw_done_controller(void) {
    // set the power state to sleep
    auo_set_power_state(auo_power_state_sleep);
    auo_mxc_done();
}

bool auo_sw_init_controller(int auo_orientation, 
                            int auo_size, 
                            int auo_bpp, 
                            auo_resolution_t *res) {
    bool result = false;

    // set the orientation
    auo_set_controller_orientation(auo_orientation, false, false);

    // set resolution
    auo_set_resolution(auo_size, auo_bpp);

    // get resolution of auo display
    auo_get_resolution(auo_orientation, res);

    // init the hw
    if (auo_hw_init_controller(res)) {
        controller_start_display_wq("auo_display_wq");
        result = true;
    }

    return result;
}

void auo_sw_done_controller(void) {
    // deinitialize the hw
    auo_hw_done_controller();
    controller_stop_display_wq();
}
