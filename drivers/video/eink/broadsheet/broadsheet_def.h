/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_def.h --
 *  eInk frame buffer device HAL broadsheet defs
 *
 *      Copyright (C) 2005-2010 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _BROADSHEET_DEF_H
#define _BROADSHEET_DEF_H

// Miscellaneous Broadsheet Definitions
//
#define BS_DFMT_2BPP            0       // Load Image Data Formats (ld_img, dfmt)
#define BS_DFMT_2BPP_ISIS       1       //
#define BS_DFMT_4BPP            2       //
#define BS_DFMT_8BPP            3       //

#define BS_ROTMODE_0            0       // Rotation is counter clockwise:  0 -> landscape upside down
#define BS_ROTMODE_90           1       //                                 1 -> portrait
#define BS_ROTMODE_180          2       //                                 2 -> landscape
#define BS_ROTMODE_270          3       //                                 3 -> portrait  upside down

#define BS_UPD_MODE_REINIT      0xFFFF  // Resync panel and buffer with flashing update.

#define BS_UPD_MODES_00         0       // V100 modes:
#define BS_UPD_MODE_INIT        0       //  INIT (panel initialization)
#define BS_UPD_MODE_MU          1       //  MU   (monochrome update)
#define BS_UPD_MODE_GU          2       //  GU   (grayscale update)
#define BS_UPD_MODE_GC          3       //  GC   (grayscale clear)
#define BS_UPD_MODE_PU          4       //  PU   (pen update)

#define BS_UPD_MODES_01         1       // V110/V110A modes:
#define BS_UPD_MODES_02         2       //
//#define BS_UPD_MODE_INIT      0       //  INIT
#define BS_UPD_MODE_DU          1       //  DU   (direct update,    1bpp)
#define BS_UPD_MODE_GC16        2       //  GC16 (grayscale clear,  4bpp)
#define BS_UPD_MODE_GC4         3       //  GC4  (grayscale clear,  2bpp)

#define BS_UPD_MODES_03         3       // V220 50Hz/85Hz modes:
//#define BS_UPD_MODE_INIT      0       //  INIT
//#define BS_UPD_MODE_DU        1       //  DU
//#define BS_UPD_MODE_GC16      2       //  GC16
//#define BS_UPD_MODE_GC4       3       //  GC4
#define BS_UPD_MODE_AU          4       //  AU   (animation update, 1bpp)

#define BS_UPD_MODES_04         4       // V220 85Hz modes
//#define BS_UPD_MODE_INIT      0       //  INIT
//#define BS_UPD_MODE_DU        1       //  DU
//#define BS_UPD_MODE_GC16      2       //  GC16
#define BS_UPD_MODE_A2          3       //  AU

#define BS_PIX_CNFG_REG         0x015E  // Host Memory Access Pixel Swap Configuration
#define BS_PIX_DEFAULT          0       // 15, 14, 13, 12, ..., 03, 02, 01, 00
#define BS_PIX_REVERSE          1       // 00, 01, 02, 03, ..., 12, 13, 14, 15
#define BS_PIX_SWAPPED          2       // 07, 06, 05, 04, ..., 11, 10, 09, 08
#define BS_PIX_REVSWAP          3       // 08, 09, 10, 11, ..., 04, 05, 06, 07

#define BS_SDR_IMG_MSW_REG      0x0312  // Host Raw Memory Access Address bits 16-31
#define BS_SDR_IMG_LSW_REG      0x0310  // Host Raw Memory Access Address bits  0-15

#define BS_UPD_BUF_CFG_REG      0x0330  // Update Buffer Configuration Register
#define BS_LAST_WF_USED(v)      (((v) & 0x0F00) >> 8)

#define BS_AUTO_WF_REG_DU       0x03A0  // Auto Waveform Mode Configuration Register DU   [0, 15]
#define BS_AUTO_WF_REG_GC4      0x03A2  // Auto Waveform Mode Configuration Register GC4  [0, 5, 10, 15]
#define BS_AUTO_WF_REG_GC16     0x03A6  // Auto Waveform Mode Configuration Register GC16 [0..15]
#define BS_AUTO_WF_MODE_DU      ((15 << 12) | (3 << 4) | BS_UPD_MODE(BS_UPD_MODE_MU))
#define BS_AUTO_WF_MODE_GC4     ((15 << 12) | (3 << 4) | BS_UPD_MODE(BS_UPD_MODE_GU))
#define BS_AUTO_WF_MODE_GC16    ((15 << 12) | (3 << 4) | BS_UPD_MODE(BS_UPD_MODE_GC))

#define BS_PWR_PIN_CONF_REG     0x0232
#define BS_PWR_COM_BYPASS_CTL   8
#define BS_PWR_COM_BYPASS_VAL   0

#define BS_PWR_PIN_VCOM_ON      ((1 << BS_PWR_COM_BYPASS_CTL) | (1 << BS_PWR_COM_BYPASS_VAL))
#define BS_PWR_PIN_VCOM_AUTO    0

#define BS_PWR_PIN_INIT         0x1E00

#define BS_VCOM_ALWAYS_ON()     bs_cmd_bypass_vcom_enable(1)
#define BS_VCOM_CONTROL()       bs_cmd_bypass_vcom_enable(0)

#define BS_TEMP_DEV_SELECT_REG  0x0320
#define BS_TEMP_DEV_INT         0x0000
#define BS_TEMP_DEV_EXT         0x0001

#define BS_TEMP_VALUE_REG       0x0322

#define BS_REV_CODE_REG         0x0000  // Revision Code Register
#define BS_PRD_CODE_REG         0x0002  // Product  Code Register

#define BS_REV_CODE_UNKNOWN     0xFFFF  // Revision Code Not Read Yet
#define BS_PRD_CODE_UNKNOWN     0xFFFF  // Product  Code Not Read Yet

#define BS_PRD_CODE             0x0047  // All Broadsheets
#define BS_REV_ASIC_B00         0x0000  // S1D13521 B00
#define BS_REV_ASIC_B01         0x0100  // S1D13521 B01
#define BS_REV_ASIC_B02         0x0200  // S1D13521 B02

#define BS_PRD_CODE_ISIS        0x004D  // All ISISes

#define BS_BROADSHEET()         (!BS_ISIS())
#define BS_ISIS()               bs_isis()
#define BS_ASIC()               bs_asic()
#define BS_FPGA()               bs_fpga()

#define BS_CMD_Q_ITERATE_ALL     (-1)   // For use with bs_iterate_cmd_queue() &...
#define BS_CMD_Q_DEBUG           5      // ...broadsheet_get_recent_commands().

#define BS_BOOTSTRAPPED()       (true == broadsheet_get_bootstrap_state())
#define BS_STILL_READY()        (true == broadsheet_get_ready_state())
#define BS_UPD_REPAIR()         (true == broadsheet_get_upd_repair_state())

#define BS_ROUNDUP32(h)         ((((unsigned long)(h)) + 31) & 0xFFFFFFE0)

#define BS_RDY_TIMEOUT          CONTROLLER_COMMON_RDY_TIMEOUT
#define BS_SFM_TIMEOUT          (HZ * 2)
#define BS_PLL_TIMEOUT          (HZ * 1)
#define BS_TMP_TIMEOUT          (HZ * 1)
#define BS_CMD_TIMEOUT          (HZ / 2)

#define FULL_BRINGUP_CONTROLLER FULL_BRINGUP
#define DONT_BRINGUP_CONTROLLER DONT_BRINGUP

#define FULL_BRINGUP_PANEL      FULL_BRINGUP
#define DONT_BRINGUP_PANEL      DONT_BRINGUP

#define UPD_DATA_RESTORE        true
#define UPD_DATA_NORMAL         false

#define broadsheet_pixels(b, p) (BS_ISIS() ? (p) : ~(p))

#define BS_HW_INIT()            (bs_pmic_init() && bs_hw_init())
#define BS_HW_DONE()            bs_hw_done(); bs_pmic_done()

#define BS_HAS_PMIC()           (bs_pmic_present == bs_pmic_get_status())

#define TEMPERATURE_REFRESHED   true
#define TEMPERATURE_CACHED      false

#define BS_TEMP_MIN             0
#define BS_TEMP_MAX             50
#define BS_TEMP_MIN_IDEAL       15
#define BS_TEMP_MAX_IDEAL       32
#define BS_TEMP_INVALID         (-1)
#define BS_TEMP_STUCK           (-10)

#define BS_MIN_F                32  // BS_TEMP_MIN is in C
#define BS_MAX_F                122 // BS_TEMP_MAX is in C

#define BS_GET_TEMP_FRESH()     bs_pmic_get_temperature(TEMPERATURE_REFRESHED)
#define BS_GET_TEMP_CACHED()    bs_pmic_get_temperature(TEMPERATURE_CACHED)

#define BS_SFM_PREFLIGHT_ISIS   true
#define BS_SFM_PREFLIGHT_BS     false

#define BS_SFM_PREFLIGHT_PD()   bs_sfm_preflight(BS_SFM_PREFLIGHT_ISIS)
#define BS_SFM_PREFLIGHT()      bs_sfm_preflight(BS_SFM_PREFLIGHT_BS)

#define BS_PANEL_DATA_WHICH()   panel_data
#define BS_PANEL_DATA_FLASH()   (bs_panel_data_flash == BS_PANEL_DATA_WHICH())

#define BS_INIT_SDRV_CFG        (100 | (1 << 8) | (1 << 9))
#define BS_INIT_GDRV_CFG        0x2
#define BS_INIT_LUTIDXFMT       (4 | (1 << 7))
#define BS_INIT_PIX_INVRT       (1 << 4)
#define BS_INIT_AUTO_WF         (1 << 6)

// Broadsheet  800x600, 6.0-inch Panel Support (AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs60_init/bs60_init.h)
//
#define BS60_INIT_HSIZE         800
#define BS60_INIT_VSIZE         600
#define BS60_MM_800             121
#define BS60_MM_600             91

// 50.09Hz
//
#define BS60_INIT_FSLEN_26_50   4
#define BS60_INIT_FBLEN_26_50   4
#define BS60_INIT_FELEN_26_50   12
#define BS60_INIT_LSLEN_26_50   10
#define BS60_INIT_LBLEN_26_50   4
#define BS60_INIT_LELEN_26_50   84
#define BS60_INIT_PXCKD_26_50   6

// 85.06Hz
//
#define BS60_INIT_FSLEN_26_85   4
#define BS60_INIT_FBLEN_26_85   4
#define BS60_INIT_FELEN_26_85   6
#define BS60_INIT_LSLEN_26_85   4
#define BS60_INIT_LBLEN_26_85   20
#define BS60_INIT_LELEN_26_85   24
#define BS60_INIT_PXCKD_26_85   4

// 50.08Hz
//
#define BS60_INIT_FSLEN_24_50   4
#define BS60_INIT_FBLEN_24_50   4
#define BS60_INIT_FELEN_24_50   10
#define BS60_INIT_LSLEN_24_50   10
#define BS60_INIT_LBLEN_24_50   10
#define BS60_INIT_LELEN_24_50   56
#define BS60_INIT_PXCKD_24_50   6

// 85.00HZ
//
#define BS60_INIT_FSLEN_24_85   4
#define BS60_INIT_FBLEN_24_85   4
#define BS60_INIT_FELEN_24_85   8
#define BS60_INIT_LSLEN_24_85   8
#define BS60_INIT_LBLEN_24_85   8
#define BS60_INIT_LELEN_24_85   71
#define BS60_INIT_PXCKD_24_85   3

// Broadsheet 1200x825, 9.7-inch Panel Support (AM300_MMC_IMAGE_X03b/source/broadsheet_soft/bs97_init/bs97_init.h)
//
#define BS97_INIT_HSIZE         1200
#define BS97_INIT_VSIZE         825
#define BS97_INIT_VSLOP         1

#define BS97_HARD_VSIZE         (BS97_INIT_VSIZE + BS97_INIT_VSLOP)
#define BS97_SOFT_VSIZE         824

#define BS97_MM_1200            203
#define BS97_MM_825             139

// 50.03Hz
//
#define BS97_INIT_FSLEN_26_50   2
#define BS97_INIT_FBLEN_26_50   4
#define BS97_INIT_FELEN_26_50   4
#define BS97_INIT_LSLEN_26_50   4
#define BS97_INIT_LBLEN_26_50   10
#define BS97_INIT_LELEN_26_50   74
#define BS97_INIT_PXCKD_26_50   3

// 81.72Hz
//
#define BS97_INIT_FSLEN_26_85   1
#define BS97_INIT_FBLEN_26_85   4
#define BS97_INIT_FELEN_26_85   4
#define BS97_INIT_LSLEN_26_85   2
#define BS97_INIT_LBLEN_26_85   1
#define BS97_INIT_LELEN_26_85  14
#define BS97_INIT_PXCKD_26_85   2

// 49.89Hz
//
#define BS97_INIT_FSLEN_24_50   2
#define BS97_INIT_FBLEN_24_50   4
#define BS97_INIT_FELEN_24_50   2
#define BS97_INIT_LSLEN_24_50   4
#define BS97_INIT_LBLEN_24_50   20
#define BS97_INIT_LELEN_24_50   35
#define BS97_INIT_PXCKD_24_50   3

// 85.03Hz
//
#define BS97_INIT_FSLEN_24_85   4
#define BS97_INIT_FBLEN_24_85   4
#define BS97_INIT_FELEN_24_85   5
#define BS97_INIT_LSLEN_24_85   4
#define BS97_INIT_LBLEN_24_85   36
#define BS97_INIT_LELEN_24_85   80
#define BS97_INIT_PXCKD_24_85   1

// 59.94Hz
//
#define BS97_INIT_FSLEN_26_60   4
#define BS97_INIT_FBLEN_26_60   4
#define BS97_INIT_FELEN_26_60   4
#define BS97_INIT_LSLEN_26_60   4
#define BS97_INIT_LBLEN_26_60   1
#define BS97_INIT_LELEN_26_60   18
#define BS97_INIT_PXCKD_26_60   3

// 60.04Hz
//
#define BS97_INIT_FSLEN_24_60   4
#define BS97_INIT_FBLEN_24_60   4
#define BS97_INIT_FELEN_24_60   4
#define BS97_INIT_LSLEN_24_60   4
#define BS97_INIT_LBLEN_24_60   1
#define BS97_INIT_LELEN_24_60   92
#define BS97_INIT_PXCKD_24_60   2

// Broadsheet 1600x1200, 9.7-inch Panel Support
//
#define BS99_INIT_HSIZE         1600
#define BS99_INIT_VSIZE         1200
#define BS99_MM_1600            197
#define BS99_MM_1200            148

// 50.13Hz
//
#define BS99_INIT_FSLEN_26_50   4
#define BS99_INIT_FBLEN_26_50   4
#define BS99_INIT_FELEN_26_50   4
#define BS99_INIT_LSLEN_26_50   30
#define BS99_INIT_LBLEN_26_50   10
#define BS99_INIT_LELEN_26_50   94
#define BS99_INIT_PXCKD_26_50   1

// 50.01Hz
//
#define BS99_INIT_FSLEN_24_50   4
#define BS99_INIT_FBLEN_24_50   4
#define BS99_INIT_FELEN_24_50   4
#define BS99_INIT_LSLEN_24_50   10
#define BS99_INIT_LBLEN_24_50   10
#define BS99_INIT_LELEN_24_50   74
#define BS99_INIT_PXCKD_24_50   1

#define BS_WFM_ADDR             0x00886     // See AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs60_init/run_bs60_init.sh.
#define BS_CMD_ADDR             0x00000     // Base of flash holds the commands (0x00000...(BS_WFM_ADDR - 1)).
#define BS_PNL_ADDR             0x30000     // Start of panel data.
#define BS_TST_ADDR_128K        0x1E000     // Test area (last 8K of 128K).
#define BS_TST_ADDR_256K        0x3E000     // Test area (last 8K of 256K).

#define BS_WFM_ADDR_ISIS        0x000AFC80  // ISIS waveform SDRAM location (0x00000000...((800x600)/2) * 3)).
#define BS_WFM_ADDR_FLASH       0           // ISIS waveform in flash instead..
#define BS_WFM_ADDR_SDRAM       1           // ...of in SDRAM.

#define BS_IB_ADDR_ISIS         0x000EFC80  // Allows for a 256K waveform.

#define BS_INIT_DISPLAY_FAST    0x74736166  // Bring the panel up without going through cycle-back-to-white process.
#define BS_INIT_DISPLAY_SLOW	0x776F6C73  // Bring the panel up by manually cycling it back to white.
#define BS_INIT_DISPLAY_ASIS    0x73697361  // Leave whatever is on the panel alone.

#define BS_INIT_DISPLAY_READ    true        // Read whatever was in the boot globals for display initialization purposes.
#define BS_INIT_DISPLAY_DONT    false       // Don't.

#define BS_DISPLAY_ASIS()       (BS_INIT_DISPLAY_ASIS == get_broadsheet_init_display_flag())

// Broadsheet interrupt registers constants
#define BS_INTR_RAW_STATUS_REG         0x0240
#define BS_INTR_MASKED_STATUS_REG      0x0242
#define BS_INTR_CTL_REG                0x0244
#define BS_DE_INTR_RAW_STATUS_REG      0x033A
#define BS_DE_INTR_MASKED_STATUS_REG   0x033C
#define BS_DE_INTR_ENABLE_REG          0x033E
#define BS_ALL_IRQS                    0x01FB
#define BS_SDRAM_INIT_IRQ              0x01
#define BS_DISPLAY_ENGINE_IRQ          0x01 << 1
#define BS_GPIO_IRQ                    0x01 << 3
#define BS_3_WIRE_IRQ                  0x01 << 4
#define BS_PWR_MGMT_IRQ                0x01 << 5
#define BS_THERMAL_SENSOR_IRQ          0x01 << 6
#define BS_SDRAM_REFRESH_IRQ           0x01 << 7
#define BS_HOST_MEM_FIFO_IRQ           0x01 << 8
#define BS_DE_ALL_IRQS                 0x3FFF
#define BS_DE_OP_TRIG_DONE_IRQ         0x01
#define BS_DE_UPD_BUFF_RFSH_DONE_IRQ   0x01 <<  1
#define BS_DE_DISP1_FC_COMPLT_IRQ      0x01 <<  2
#define BS_DE_ONE_LUT_COMPLT_IRQ       0x01 <<  3
#define BS_DE_UPD_BUFF_CHG_IRQ         0x01 <<  4
#define BS_DE_ALL_FRMS_COMPLT_IRQ      0x01 <<  5
#define BS_DE_FIFO_UNDERFLOW_IRQ       0x01 <<  6
#define BS_DE_LUT_BSY_CONFLICT_IRQ     0x01 <<  7
#define BS_DE_OP_TRIG_ERR_IRQ          0x01 <<  8
#define BS_DE_LUT_REQ_ERR_IRQ          0x01 <<  9
#define BS_DE_TEMP_OUT_OF_RNG_IRQ      0x01 << 10
#define BS_DE_ENTRY_CNT_ERR_IRQ        0x01 << 11
#define BS_DE_FLASH_CHKSM_ERR_IRQ      0x01 << 12
#define BS_DE_BUF_UPD_INCOMPLETE_IRQ   0x01 << 13

// Broadsheet mxc constants
#define BROADSHEET_GPIO_INIT_SUCCESS    0
#define BROADSHEET_HIRQ_RQST_FAILURE    1
#define BROADSHEET_HIRQ_INIT_FAILURE    2
#define BROADSHEET_HRST_INIT_FAILURE    3
#define BROADSHEET_HRDY_INIT_FAILURE    4

#define DISABLE_BS_IRQ                  1
#define NO_BS_IRQ                       0

#define BROADSHEET_DISPLAY_NUMBER       DISP0
#define BROADSHEET_PIXEL_FORMAT         IPU_PIX_FMT_RGB565
#define BROADSHEET_SCREEN_HEIGHT        BS97_HARD_VSIZE
#define BROADSHEET_SCREEN_WIDTH         BS97_INIT_HSIZE
#define BROADSHEET_SCREEN_BPP           8
#define BROADSHEET_DMA_MIN_SIZE         128
#define BROADSHEET_DMA_MIN_FAIL         3

#define BROADSHEET_DMA_SIZE(s)          BROADSHEET_DMA_MIN_SIZE,     \
                                        (s)/BROADSHEET_DMA_MIN_SIZE, \
                                        BROADSHEET_DMA_MIN_SIZE

#define BROADSHEET_DMA_FAILED()         (BROADSHEET_DMA_MIN_FAIL < dma_failed)

// Select which R/W timing parameters to use (slow or fast); these
// settings affect the Freescale IPU Asynchronous Parallel System 80
// Interface (Type 2) timings.
// The slow timing parameters are meant to be used for bring-up
// and debugging the functionality.  The fast timings are up to the
// Broadsheet specifications for the 16-bit Host Interface Timing, and
// are meant to maximize performance.
//
#undef SLOW_RW_TIMING

#ifdef SLOW_RW_TIMING // Slow timing for bring-up or debugging
#    define BROADSHEET_HSP_CLK_PER  0x00100010L
#    define BROADSHEET_READ_CYCLE_TIME     1900  // nsec
#    define BROADSHEET_READ_UP_TIME         170  // nsec
#    define BROADSHEET_READ_DOWN_TIME      1040  // nsec
#    define BROADSHEET_READ_LATCH_TIME     1900  // nsec
#    define BROADSHEET_PIXEL_CLK        5000000
#    define BROADSHEET_WRITE_CYCLE_TIME    1230  // nsec
#    define BROADSHEET_WRITE_UP_TIME        170  // nsec
#    define BROADSHEET_WRITE_DOWN_TIME      680  // nsec
#else // Fast timing, according to Broadsheet specs 
// NOTE: these timings assume a Broadsheet System Clock at the 
//       maximum speed of 66MHz (15.15 nsec period) 
#    define BROADSHEET_HSP_CLK_PER  0x00100010L
#    define BROADSHEET_READ_CYCLE_TIME      110  // nsec 
#    define BROADSHEET_READ_UP_TIME           1  // nsec 
#    define BROADSHEET_READ_DOWN_TIME       100  // nsec 
#    define BROADSHEET_READ_LATCH_TIME      110  // nsec 
#    define BROADSHEET_PIXEL_CLK        5000000 
#    define BROADSHEET_WRITE_CYCLE_TIME      83  // nsec 
#    define BROADSHEET_WRITE_UP_TIME          1  // nsec 
#    define BROADSHEET_WRITE_DOWN_TIME       72  // nsec 
#endif

#define BROADSHEET_CONFIG_CONTROLLER_PROPS(props, get_dma_addr, done_dma_addr)        \
        props.controller_disp = BROADSHEET_DISPLAY_NUMBER;                            \
        props.screen_width = BROADSHEET_SCREEN_WIDTH;                                 \
        props.screen_height = BROADSHEET_SCREEN_HEIGHT;                               \
        props.pixel_fmt = BROADSHEET_PIXEL_FORMAT;                                    \
        props.screen_stride = BPP_SIZE(BROADSHEET_SCREEN_WIDTH,                       \
                                       BROADSHEET_SCREEN_BPP);                        \
        props.read_cycle_time = BROADSHEET_READ_CYCLE_TIME;                           \
        props.read_up_time = BROADSHEET_READ_UP_TIME;                                 \
        props.read_down_time = BROADSHEET_READ_DOWN_TIME;                             \
        props.read_latch_time = BROADSHEET_READ_LATCH_TIME;                           \
        props.write_cycle_time = BROADSHEET_WRITE_CYCLE_TIME;                         \
        props.write_up_time = BROADSHEET_WRITE_UP_TIME;                               \
        props.write_down_time = BROADSHEET_WRITE_DOWN_TIME;                           \
        props.pixel_clk = BROADSHEET_PIXEL_CLK;                                       \
        props.hsp_clk_per = BROADSHEET_HSP_CLK_PER;                                   \
        props.get_dma_phys_addr = get_dma_addr;                                       \
        props.done_dma_phys_addr = done_dma_addr;                              

// Broadsheet Primitives Support
//
#define BS_RD_DAT_ONE           CONTROLLER_COMMON_RD_DAT_ONE
#define BS_WR_DAT_ONE           CONTROLLER_COMMON_WR_DAT_ONE

#define BS_WR_DAT_DATA          CONTROLLER_COMMON_WR_DAT_DATA
#define BS_WR_DAT_ARGS          CONTROLLER_COMMON_WR_DAT_ARGS

// Broadsheet Host Interface Commands per Epson/eInk S1D13521 Hardware
// Fuctional Specification (Rev. 0.06)
//
#define BS_CMD_ARGS_MAX         CONTROLLER_COMMON_CMD_ARGS_MAX

#define BS_CMD_BST_SDR_WR_REG   0x0154  // Host Memory Access Port Register
#define BS_CMD_ARGS_BST_SDR     4

#define BS_CMD_LD_IMG_WR_REG    0x0154  // Host Memory Access Port Register
#define BS_CMD_ARGS_WR_REG_SUB  1

#define BS_CMD_UPD_ARGS         1
#define BS_CMD_UPD_AREA_ARGS    5

enum bs_cmd
{
    // System Commands
    //
    bs_cmd_INIT_CMD_SET         = 0x0000,   BS_CMD_ARGS_INIT_CMD_SET    = 3,
    bs_cmd_INIT_PLL_STBY        = 0x0001,   BS_CMD_ARGS_INIT_PLL_STBY   = 3,
    bs_cmd_RUN_SYS              = 0x0002,
    bs_cmd_STBY                 = 0x0004,
    bs_cmd_SLP                  = 0x0005,
    bs_cmd_INIT_SYS_RUN         = 0x0006,
    bs_cmd_INIT_SYS_STBY        = 0x0007,
    bs_cmd_INIT_SDRAM           = 0x0008,   BS_CMD_ARGS_INIT_SDRAM      = 4,
    bs_cmd_INIT_DSPE_CFG        = 0x0009,   BS_CMD_ARGS_DSPE_CFG        = 5,
    bs_cmd_INIT_DSPE_TMG        = 0x000A,   BS_CMD_ARGS_DSPE_TMG        = 5,
    bs_cmd_SET_ROTMODE          = 0x000B,   BS_CMD_ARGS_SET_ROTMODE     = 1,
    bs_cmd_INIT_WAVEFORMDEV     = 0x000C,   BS_CMD_ARGS_INIT_WFDEV      = 1,

    // Register and Memory Access Commands
    //
    bs_cmd_RD_REG               = 0x0010,   BS_CMD_ARGS_RD_REG          = 1,
    bs_cmd_WR_REG               = 0x0011,   BS_CMD_ARGS_WR_REG          = 2,
    bs_cmd_RD_SFM               = 0x0012,
    bs_cmd_WR_SFM               = 0x0013,   BS_CMD_ARGS_WR_SFM          = 1,
    bs_cmd_END_SFM              = 0x0014,
    
    // Burst Access Commands
    //
    bs_cmd_BST_RD_SDR           = 0x001C,   BS_CMD_ARGS_BST_RD_SDR      = BS_CMD_ARGS_BST_SDR,
    bs_cmd_BST_WR_SDR           = 0x001D,   BS_CMD_ARGS_BST_WR_SDR      = BS_CMD_ARGS_BST_SDR,
    bs_cmd_BST_END              = 0x001E,

    // Image Loading Commands
    //
    bs_cmd_LD_IMG               = 0x0020,   BS_CMD_ARGS_LD_IMG          = 1,
    bs_cmd_LD_IMG_AREA          = 0x0022,   BS_CMD_ARGS_LD_IMG_AREA     = 5,
    bs_cmd_LD_IMG_END           = 0x0023,
    bs_cmd_LD_IMG_WAIT          = 0x0024,
    bs_cmd_LD_IMG_SETADR        = 0x0025,   BS_CMD_ARGS_LD_IMG_SETADR   = 2,
    bs_cmd_LD_IMG_DSPEADR       = 0x0026,
    
    // Polling Commands
    //
    bs_cmd_WAIT_DSPE_TRG        = 0x0028,
    bs_cmd_WAIT_DSPE_FREND      = 0x0029,
    bs_cmd_WAIT_DSPE_LUTFREE    = 0x002A,
    bs_cmd_WAIT_DSPE_MLUTFREE   = 0x002B,   BS_CMD_ARGS_DSPE_MLUTFREE   = 1,
    
    // Waveform Update Commands
    //
    bs_cmd_RD_WFM_INFO          = 0x0030,   BS_CMD_ARGS_RD_WFM_INFO     = 2,
    bs_cmd_UPD_INIT             = 0x0032,
    bs_cmd_UPD_FULL             = 0x0033,   BS_CMD_ARGS_UPD_FULL        = BS_CMD_UPD_ARGS,
    bs_cmd_UPD_FULL_AREA        = 0x0034,   BS_CMD_ARGS_UPD_FULL_AREA   = BS_CMD_UPD_AREA_ARGS,
    bs_cmd_UPD_PART             = 0x0035,   BS_CMD_ARGS_UPD_PART        = BS_CMD_UPD_ARGS,
    bs_cmd_UPD_PART_AREA        = 0x0036,   BS_CMD_ARGS_UPD_PART_AREA   = BS_CMD_UPD_AREA_ARGS,
    bs_cmd_UPD_GDRV_CLR         = 0x0037,
    bs_cmd_UPD_SET_IMGADR       = 0x0038,   BS_CMD_ARGS_UPD_SET_IMGADR  = 2
};
typedef enum bs_cmd bs_cmd;

enum bs_flash_select
{
    bs_flash_waveform,
    bs_flash_commands,
    bs_flash_test
};
typedef enum bs_flash_select bs_flash_select;

enum bs_power_states
{
    bs_power_state_off_screen_clear,     // einkfb_power_level_off
    bs_power_state_off,                  // einkfb_power_level_sleep
 
    bs_power_state_run,                  // einkfb_power_level_on
    bs_power_state_standby,
    bs_power_state_sleep,                // einkfb_power_level_standby
    
    bs_power_state_init
};
typedef enum bs_power_states bs_power_states;

enum bs_pmic_status
{
    bs_pmic_present,                     // pmic present 
    bs_pmic_absent,                      // pmic absent
    
    bs_pmic_not_initialized              // pmic initialized (presence/absence unknown)
};
typedef enum bs_pmic_status bs_pmic_status;

enum bs_pmic_power_states
{
    bs_pmic_power_state_uninitialized,   // uninitialized power state
    
    bs_pmic_power_state_active,          // bs_power_state_run
    bs_pmic_power_state_standby,         // bs_power_state_sleep
    bs_pmic_power_state_sleep            // bs_power_state_off
};
typedef enum bs_pmic_power_states bs_pmic_power_states;

enum bs_preflight_failure
{
    bs_preflight_failure_hw   = 1 << 0,  // Hardware isn't responding at all.
    bs_preflight_failure_id   = 1 << 1,  // ID bits aren't right.
    bs_preflight_failure_bus  = 1 << 2,  // Bits on bus aren't toggling correctly.
    bs_preflight_failure_cmd  = 1 << 3,  // Commands area of flash isn't valid.
    bs_preflight_failure_wf   = 1 << 4,  // Waveform area of flash isn't valid.
    bs_preflight_failure_flid = 1 << 5,  // Flash id isn't recognized.
    bs_preflight_failure_hrdy = 1 << 6,  // HRDY signal isn't responding.
    
    bs_preflight_failure_none = 0
};
typedef enum bs_preflight_failure bs_preflight_failure;

enum bs_panel_data
{
    bs_panel_data_none,                  // No panel data.
    
    bs_panel_data_eeprom,                // Panel data is in an EEPROM (i2c).
    bs_panel_data_flash                  // Panel data is in Flash (SPI).
};
typedef enum bs_panel_data bs_panel_data;

enum bs_cmd_type
{
    bs_cmd_type_write,
    bs_cmd_type_read
};
typedef enum bs_cmd_type bs_cmd_type;

#endif // _BROADSHEET_DEF_H
