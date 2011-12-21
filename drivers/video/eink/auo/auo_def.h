/*
 *  linux/drivers/video/eink/broadsheet/broadsheet.h --
 *  eInk frame buffer device AUO defs
 *
 *      Copyright (C) 2005-2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _AUO_DEF_H_
#define _AUO_DEF_H_

#define AUO_CONTROLLER_RESOLUTION_800_600_HSIZE      800
#define AUO_CONTROLLER_RESOLUTION_800_600_VSIZE      600
#define AUO_CONTROLLER_RESOLUTION_1024_768_HSIZE     1024
#define AUO_CONTROLLER_RESOLUTION_1024_768_VSIZE     768

#define AUO_DISPLAY_NUMBER                           DISP0
#define AUO_PIXEL_FORMAT                             IPU_PIX_FMT_RGB565
#define AUO_SCREEN_HEIGHT                            AUO_CONTROLLER_RESOLUTION_1024_768_VSIZE
#define AUO_SCREEN_WIDTH                             AUO_CONTROLLER_RESOLUTION_1024_768_HSIZE
#define AUO_SCREEN_BPP                               4
#define AUO_READY_TIMEOUT                            (HZ * 5)

// AUO Product codes
#define AUO_PRD_CODE_1                               0x1800  // Z060S    / U-cronix IC
#define AUO_PRD_CODE_2                               0x1801  // A060SE02 / AUO-K7900
#define AUO_PRD_CODE_3                               0x2400  // A090XE01 / AUO-K7900
#define AUO_PRD_CODE_4                               0x0800  // A020AE01 / AUO-K0200
#define AUO_PRD_CODE_5                               0x0840  // A021AE01 / AUO-K0200

// Select which R/W timing parameters to use (slow or fast); these
// settings affect the Freescale IPU Asynchronous Parallel System 80
// Interface (Type 2) timings.
// The slow timing parameters are meant to be used for bring-up
// and debugging the functionality.  The fast timings are up to the
// Auo specifications for the 16-bit Host Interface Timing, and
// are meant to maximize performance.
//
#undef SLOW_RW_TIMING

#ifdef SLOW_RW_TIMING // Slow timing for bring-up or debugging
#    define AUO_HSP_CLK_PER         0x00080008L
#    define AUO_READ_CYCLE_TIME     1900  // nsec
#    define AUO_READ_UP_TIME         170  // nsec
#    define AUO_READ_DOWN_TIME      1040  // nsec
#    define AUO_READ_LATCH_TIME     1900  // nsec
#    define AUO_PIXEL_CLK        5000000
#    define AUO_WRITE_CYCLE_TIME    1230  // nsec
#    define AUO_WRITE_UP_TIME        170  // nsec
#    define AUO_WRITE_DOWN_TIME      680  // nsec
#else // Fast timing, according to Auo specs 
// NOTE: these timings assume a Auo System Clock at the 
//       maximum speed of 33MHz (15.15 nsec period) 
#    define AUO_HSP_CLK_PER          0x00080008L
#    define AUO_READ_CYCLE_TIME      250//1500//110  // nsec 
#    define AUO_READ_UP_TIME           1//1//16//1  // nsec 
#    define AUO_READ_DOWN_TIME       125//750//80//100  // nsec 
#    define AUO_READ_LATCH_TIME      250//1500//150//110  // nsec 
#    define AUO_PIXEL_CLK        5000000 
#    define AUO_WRITE_CYCLE_TIME     250//1500//83  // nsec 
#    define AUO_WRITE_UP_TIME          1//1  // nsec 
#    define AUO_WRITE_DOWN_TIME      125//750//72  // nsec 
#endif

#define AUO_CONFIG_CONTROLLER_PROPS(props, width, height, bpp, get_dma_addr, done_dma_addr) \
        props.controller_disp = AUO_DISPLAY_NUMBER;                                         \
        props.screen_width = width;                                                         \
        props.screen_height = height;                                                       \
        props.pixel_fmt = AUO_PIXEL_FORMAT;                                                 \
        props.screen_stride = BPP_SIZE(width, bpp);                                         \
        props.read_cycle_time = AUO_READ_CYCLE_TIME;                                        \
        props.read_up_time = AUO_READ_UP_TIME;                                              \
        props.read_down_time = AUO_READ_DOWN_TIME;                                          \
        props.read_latch_time = AUO_READ_LATCH_TIME;                                        \
        props.write_cycle_time = AUO_WRITE_CYCLE_TIME;                                      \
        props.write_up_time = AUO_WRITE_UP_TIME;                                            \
        props.write_down_time = AUO_WRITE_DOWN_TIME;                                        \
        props.pixel_clk = AUO_PIXEL_CLK;                                                    \
        props.hsp_clk_per = AUO_HSP_CLK_PER;                                                \
        props.get_dma_phys_addr = get_dma_addr;                                             \
        props.done_dma_phys_addr = done_dma_addr;                              

typedef u16 auo_mxc_cmd;

#define AUO_DAT         CONTROLLER_COMMON_DAT
#define AUO_CMD         CONTROLLER_COMMON_CMD
#define AUO_RD          CONTROLLER_COMMON_RD
#define AUO_WR          CONTROLLER_COMMON_WR
#define AUO_WR_DAT_DATA CONTROLLER_COMMON_WR_DAT_DATA
#define AUO_WR_DAT_ARGS CONTROLLER_COMMON_WR_DAT_ARGS

enum auo_command_t {
    AUO_INIT_SET              = 0x0000,
    AUO_STANDBY               = 0x0001,
    AUO_WAKEUP                = 0x0002,
    AUO_SOFTWARE_RESET        = 0x0003,
    
    AUO_DMA_START             = 0x1001,
    AUO_DMA_STOP              = 0x1002,
    AUO_LUT_START             = 0x1003,
    AUO_DISP_REFRESH          = 0x1004,
    AUO_DISP_RESET            = 0x1005,
    AUO_CURSOR_DMA_START      = 0x1007,
    AUO_CURSOR_DMA_STOP       = AUO_DMA_STOP,
    AUO_PIP_DMA_START         = 0x1008,
    AUO_PIP_DMA_STOP          = AUO_DMA_STOP,
    AUO_DISP_START            = 0x1009,
    AUO_DISP_CURSOR_START     = 0x100A,
    AUO_DISP_PIP_START        = 0x100B,
    
    AUO_READ_VERSION          = 0x4000,
    AUO_READ_STATUS           = 0x4001,
    AUO_DATA_READ             = 0x4002,
    AUO_LUT_READ              = 0x4003,

    AUO_LUMINANCE_BALANCE     = 0x5001,
    
    AUO_AGING_MODE            = 0x6000,
    AUO_AGING_MODE_EXIT       = 0x6001,
    
    AUO_PLL_CONTROL           = 0x7000,
};
typedef enum auo_command_t auo_command_t;

enum auo_auto_mode_t {
    AUO_NO_AUTO_MODE,
    AUO_SW_AUTO_MODE,
    AUO_HW_AUTO_MODE,
};
typedef enum auo_auto_mode_t auo_auto_mode_t;

enum auo_bpp_update_mode_t {
    AUO_NONE                  = 0xffff,
    AUO_02                    = 0x1,
    AUO_04                    = 0x3,
    AUO_16                    = 0x7,
};
typedef enum auo_bpp_update_mode_t auo_bpp_update_mode_t;

enum auo_display_mode_t {
    // Flashing modes
    AUO_GRAY_REFRESH_FLASH    = 0x0000, // 4bpp
    AUO_TEXT_MODE_FLASH       = 0x3000, // 2bpp
    //AUO_HIGH_SPEED_FLASH    = 0xb000, // 1bpp
    AUO_HANDWRITING_ERASE     = 0xd000, // 1bpp
    AUO_AUTO_FLASH            = 0xf000, // Auto

    // Non-Flashing modes
    AUO_GRAY_REFRESH_NONFLASH = 0x1000, // 4bpp
    AUO_TEXT_MODE_NONFLASH    = 0x2000, // 2bpp
    AUO_HIGH_SPEED_NONFLASH   = 0x4000, // 1bpp
    AUO_HANDWRITING_WRITE     = 0x5000, // 1bpp
    AUO_AUTO_NONFLASH         = 0x7000, // Auto
};
typedef enum auo_display_mode_t auo_display_mode_t;

enum auo_shl_t {
    //                 bit 0
    AUO_SHL_DEFAULT  = 0x0001,
    AUO_SHL_REVERSE  = 0x0000,
};

enum auo_ud_t {
    //                bit 1
    AUO_UD_DEFAULT  = 0x0000,
    AUO_UD_REVERSE  = 0x0002,
};
typedef enum auo_ud_t auo_ud_t;

enum auo_controller_resolution_t {
    //                                    bit5-2
    AUO_CONTROLLER_RESOLUTION_600_800   = 0x0020,
    AUO_CONTROLLER_RESOLUTION_768_1024  = 0x0024,
    AUO_CONTROLLER_RESOLUTION_600_1024  = 0x000c,
    AUO_CONTROLLER_RESOLUTION_825_1200  = 0x0010,
    AUO_CONTROLLER_RESOLUTION_1024_1280 = 0x0014,
    AUO_CONTROLLER_RESOLUTION_1200_1600 = 0x0018,
    AUO_CONTROLLER_RESOLUTION_800_600   = 0x0000,
    AUO_CONTROLLER_RESOLUTION_1024_768  = 0x0024,
    AUO_CONTROLLER_RESOLUTION_1024_600  = 0x002c,
    AUO_CONTROLLER_RESOLUTION_1200_825  = 0x0030,
    AUO_CONTROLLER_RESOLUTION_1280_1024 = 0x0034,
    AUO_CONTROLLER_RESOLUTION_1600_1200 = 0x0038,
};
typedef enum auo_controller_resolution_t auo_controller_resolution_t;

enum auo_input_data_arrangement_t {
    //                                    bit6-8
    AUO_INPUT_ARRANGEMENT_4BIT_DEFAULT  = 0x0000,
    AUO_INPUT_ARRANGEMENT_4BIT_REVERSE  = 0x0040,
    AUO_INPUT_ARRANGEMENT_8BIT_DEFAULT  = 0x0080,
    AUO_INPUT_ARRANGEMENT_8BIT_REVERSE  = 0x00c0,
};
typedef enum auo_input_data_arrangement_t auo_input_data_arrangement_t;

enum auo_pixel_polarity_t {
    //                            bit9
    AUO_PIXEL_POLARITY_DEFAULT  = 0x0200, // 0x0 - black 0xf - white
    AUO_PIXEL_POLARITY_REVERSE  = 0x0000, // 0x0 - white 0xf - black
};
typedef enum auo_pixel_polarity_t auo_pixel_polarity_t;

enum auo_foreground_background_t {
    //                  bit 12
    AUO_FOREGROUND    = 0x0000,
    AUO_BACKGROUND    = 0x1000,
};
typedef enum auo_foreground_background_t auo_foreground_background_t;

enum auo_hpl_t {
    //                     bit 13
    AUO_HPL_LANDSCAPE    = 0x0000,
    AUO_HPL_POTRAIT      = 0x2000,
};
typedef enum auo_hpl_t auo_hpl_t;

enum auo_flip_t {
    //               bit 11
    AUO_FLIP_0     = 0x0800,
    AUO_FLIP_180   = 0x0000,
};
typedef enum auo_flip_t auo_flip_t;

enum auo_controller_orientation_t {
    //                                 bit11-10
    AUO_CONTROLLER_ORIENTATION_ZERO  = 0x0000,
    AUO_CONTROLLER_ORIENTATION_90    = 0x0400,
    AUO_CONTROLLER_ORIENTATION_180   = 0x0800,
    AUO_CONTROLLER_ORIENTATION_270   = 0x0c00,
};
typedef enum auo_controller_orientation_t auo_controller_orientation_t;

enum auo_power_states {
    auo_power_state_init,
    auo_power_state_run,
    auo_power_state_standby,
    auo_power_state_sleep
};
typedef enum auo_power_states auo_power_states;

#endif // _AUO_DEF_H_
