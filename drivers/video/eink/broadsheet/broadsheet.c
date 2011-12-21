/*
 *  linux/drivers/video/eink/broadsheet/broadsheet.c
 *  -- eInk frame buffer device HAL broadsheet sw
 *
 *      Copyright (C) 2005-2010 Amazon Technologies, Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include "broadsheet.h"

#if PRAGMAS
    #pragma mark Definitions/Globals
#endif

#define INIT_PWR_SAVE_MODE      0x0000

#define INIT_PLL_CFG_0_FPGA     0x0000
#define INIT_PLL_CFG_1_FPGA     0x0000
#define INIT_PLL_CFG_0_ASIC     0x0004
#define INIT_PLL_CFG_1_ASIC     0x5949
#define INIT_PLL_CFG_2          0x0040
#define INIT_CLK_CFG            0x0000

#define INIT_SPI_FLASH_ACC_MODE 0       // access mode select
#define INIT_SPI_FLASH_RDC_MODE 0       // read command select
#define INIT_SPI_FLASH_CLK_DIV  3       // clock divider
#define INIT_SPI_FLASH_CLK_PHS  0       // clock phase select
#define INIT_SPI_FLASH_CLK_POL  0       // clock polarity select
#define INIT_SPI_FLASH_ENB      1       // enable
#define INIT_SPI_FLASH_CTL              \
    ((INIT_SPI_FLASH_ACC_MODE << 7) |   \
     (INIT_SPI_FLASH_RDC_MODE << 6) |   \
     (INIT_SPI_FLASH_CLK_DIV  << 3) |   \
     (INIT_SPI_FLASH_CLK_PHS  << 2) |   \
     (INIT_SPI_FLASH_CLK_POL  << 1) |   \
     (INIT_SPI_FLASH_ENB      << 0))

#define INIT_SPI_FLASH_CS_ENB   1
#define INIT_SPI_FLASH_CSC      INIT_SPI_FLASH_CS_ENB

#define SFM_READ_COMMAND        0
#define SFM_CLOCK_DIVIDE        3
#define SFM_CLOCK_PHASE         0
#define SFM_CLOCK_POLARITY      0

#define BS_SFM_WREN             0x06
#define BS_SFM_WRDI             0x04
#define BS_SFM_RDSR             0x05
#define BS_SFM_READ             0x03
#define BS_SFM_PP               0x02
#define BS_SFM_SE               0xD8
#define BS_SFM_RES              0xAB
#define BS_SFM_ESIG_M25P10      0x10    // 128K
#define BS_SFM_ESIG_M25P20      0x11    // 256K (MX25L2005 returns the same esig)

#define BS_SFM_SECTOR_COUNT     4
#define BS_SFM_SECTOR_SIZE_128K (32*1024)
#define BS_SFM_SECTOR_SIZE_256K (64*1024)
#define BS_SFM_SIZE_128K        (BS_SFM_SECTOR_SIZE_128K * BS_SFM_SECTOR_COUNT)
#define BS_SFM_SIZE_256K        (BS_SFM_SECTOR_SIZE_256K * BS_SFM_SECTOR_COUNT)
#define BS_SFM_PAGE_SIZE        256
#define BS_SFM_PAGE_COUNT_128K  (BS_SFM_SECTOR_SIZE_128K/BS_SFM_PAGE_SIZE)
#define BS_SFM_PAGE_COUNT_256K  (BS_SFM_SECTOR_SIZE_256K/BS_SFM_PAGE_SIZE)

#define BS_PANEL_ID_ISIS_MARIO  "V110_035_60_M01"
#define BS_PANEL_ID_ISIS_LUIGI  "????_???_??_???"
#define BS_PANEL_BCD_SIZE       33
#define BS_PANEL_ID_SIZE        16
#define BS_VCOM_SIZE            6
#define BS_WAVEFORM_FILE_PATH   EINKFB_RW_DIR"%s"
#define BS_WAVEFORM_FILE_ISIS   "isis.wbf"
#define BS_WAVEFORM_FILE_SIZE   256
#define BS_WAVEFORM_BUFFER_SIZE BS_SFM_SIZE_256K
#define BS_WAVEFORM_BUFFER      &sd[0]
#define BS_FPL_INFO_FILE        EINKFB_RW_DIR"fpl_info"
#define BS_FPL_INFO_SIZE        16
#define BS_FPL_INFO_FORMAT      "%02X %02X"
#define BS_FPL_INFO_COUNT       2

#define BS_WF_BASE              BS_WFM_ADDR
#define BS_PD_BASE              BS_PNL_ADDR
#define BS_WF_SIZE              (BS_PD_BASE - BS_WF_BASE)

#define BS_SDR_SIZE             (16*1024*1024)
#define BS_SDR_SIZE_ISIS        ( 2*1024*1024)

#define BS_NUM_CMD_QUEUE_ELEMS  101 // n+1 -> n cmds + 1 for empty
#define BS_RECENT_CMDS_SIZE     (32*1024)

static int bs_sfm_sector_size = BS_SFM_SECTOR_SIZE_128K;
static int bs_sfm_size = BS_SFM_SIZE_128K;
static int bs_sfm_page_count = BS_SFM_PAGE_COUNT_128K;
static int bs_tst_addr = BS_TST_ADDR_128K;
static char sd[BS_SFM_SIZE_256K];
static char *rd = &sd[0];
static int sfm_cd;

static char bs_panel_bcd[BS_PANEL_BCD_SIZE] = { 0 };
static char bs_panel_id[BS_PANEL_ID_SIZE] = { 0 };
static char bs_vcom_str[BS_VCOM_SIZE] = { 0 };
static int  bs_vcom = 0;

static char bs_vcom_override_str[BS_VCOM_SIZE] = { 0 };
static int  bs_vcom_override = 0;

static bool bs_vcom_diags = false;

static u8 *bs_waveform_buffer = NULL;
static int bs_waveform_size = 0;

static u16 bs_prd_code = BS_REV_CODE_UNKNOWN;
static u16 bs_rev_code = BS_PRD_CODE_UNKNOWN;
static u16 bs_upd_mode = BS_UPD_MODE_INIT;
static char *bs_upd_mode_string = NULL;
static int bs_sdr_size = BS_SDR_SIZE;

static int wfm_fvsn = 0;
static int wfm_luts = 0;
static int wfm_mc   = 0;
static int wfm_trc  = 0;
static int wfm_eb   = 0;
static int wfm_sb   = 0;
static int wfm_wmta = 0;

static bool bs_clip_temp = true;

static int bs_fpl_size = 0;
static int bs_fpl_rate = 0;
static int bs_hsize = 0;
static int bs_vsize = 0;

static bs_cmd_queue_elem_t bs_cmd_queue[BS_NUM_CMD_QUEUE_ELEMS];
static bs_cmd_queue_elem_t bs_cmd_elem;

static int bs_cmd_queue_entry = 0;
static int bs_cmd_queue_exit  = 0;

static char bs_recent_commands_page[BS_RECENT_CMDS_SIZE];
static bool bs_pll_steady = false;
static bool bs_ready = false;
static bool bs_hw_up = false;

static bs_preflight_failure preflight_failure = bs_preflight_failure_none;
bs_panel_data panel_data = bs_panel_data_none;
static int bs_bootstrap = 0;

static bool bs_upd_repair_skipped = false;
static int  bs_upd_repair_count = 0;
static int  bs_upd_repair_mode;
static u16  bs_upd_repair_x1;
static u16  bs_upd_repair_y1;
static u16  bs_upd_repair_x2;
static u16  bs_upd_repair_y2;

#ifdef MODULE
module_param_named(bs_bootstrap, bs_bootstrap, int, S_IRUGO);
MODULE_PARM_DESC(bs_bootstrap, "non-zero to bootstrap");
#endif // MODULE

static bs_power_states bs_power_state = bs_power_state_init;
static dma_addr_t bs_phys_addr = 0;
static u8 *bs_ld_fb = NULL;

static u16 bs_upd_modes_00[] =
{
    BS_UPD_MODE_INIT,
    BS_UPD_MODE_MU,
    BS_UPD_MODE_GU,
    BS_UPD_MODE_GC,
    BS_UPD_MODE_PU
};

static char *bs_upd_mode_00_names[] =
{
    "INIT",
    "MU",
    "GU",
    "GC",
    "PU",
};

static u16 bs_upd_modes_01_2bpp[] =
{
    BS_UPD_MODE_INIT,
    BS_UPD_MODE_DU,
    BS_UPD_MODE_GC4,
    BS_UPD_MODE_GC4,
    BS_UPD_MODE_DU
};

static u16 bs_upd_modes_01_4bpp[] =
{
    BS_UPD_MODE_INIT,
    BS_UPD_MODE_DU,
    BS_UPD_MODE_GC4,
    BS_UPD_MODE_GC16,
    BS_UPD_MODE_DU
};

static char *bs_upd_mode_01_names[] =
{
    "INIT",
    "DU",
    "GC16",
    "GC4",
};

static u16 bs_upd_modes_03_2bpp[] =
{
    BS_UPD_MODE_INIT,
    BS_UPD_MODE_DU,
    BS_UPD_MODE_GC4,
    BS_UPD_MODE_GC4,
    BS_UPD_MODE_AU
};

static u16 bs_upd_modes_03_4bpp[] =
{
    BS_UPD_MODE_INIT,
    BS_UPD_MODE_DU,
    BS_UPD_MODE_GC4,
    BS_UPD_MODE_GC16,
    BS_UPD_MODE_AU
};

static char *bs_upd_mode_03_names[] =
{
    "INIT",
    "DU",
    "GC16",
    "GC4",
    "AU"
};

static u16 bs_upd_modes_04[] =
{
    BS_UPD_MODE_INIT,
    BS_UPD_MODE_DU,
    BS_UPD_MODE_GC16,
    BS_UPD_MODE_GC16,
    BS_UPD_MODE_A2
};

static char *bs_upd_mode_04_names[] =
{
    "INIT",
    "DU",
    "GC16",
    "AU"
};

static unsigned char bs_upd_mode_version = BS_UPD_MODES_00;
static char **bs_upd_mode_names = NULL;
static u16 *bs_upd_modes = NULL;

static u16 bs_regs[] =
{
    // System Configuration Registers
    //
    0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000A,

    // Clock Configuration Registers
    //
    0x0010, 0x0012, 0x0014, 0x0016, 0x0018, 0x001A,

    // Component Configuration
    //
    0x0020,

    // Memory Controller Configuration
    //
    0x0100, 0x0102, 0x0104, 0x0106, 0x0108, 0x010A,
    0x010C,

    // Host Interface Memory Access Configuration
    //
    0x0140, 0x0142, 0x0144, 0x0146, 0x0148, 0x014A,
    0x014C, 0x014E, 0x0150, 0x0152, 0x0154, 0x0156,

    // Misc. Configuration Registers
    //
    0x015E,

    // SPI Flash Memory Interface
    //
    0x0200, 0x0202, 0x0204, 0x0206, 0x0208,

    // I2C Thermal Sensor Interface Registers
    //
    0x0210, 0x0212, 0x0214, 0x0216,

    // 3-Wire Chip Interface Registers
    //
    0x0220, 0x0222, 0x0224, 0x0226,

    // Power Pin Control Configuration
    //
    0x0230, 0x0232, 0x0234, 0x0236, 0x0238,

    // Interrupt Configuration Register
    //
    0x0240, 0x0242, 0x0244,

    // GPIO Control Registers
    //
    0x0250, 0x0252, 0x0254, 0x0256,

    // Waveform Read Control Register (ISIS)
    //
    0x260,

    // Command RAM Controller Registers
    //
    0x0290, 0x0292, 0x0294,

    // Command Sequence Controller Registers
    //
    0x02A0, 0x02A2,

    // Display Engine:  Display Timing Configuration
    //
    0x0300, 0x0302, 0x0304, 0x0306, 0x0308, 0x30A,

    // Display Engine:  Driver Configurations
    //
    0x030C, 0x30E,

    // Display Engine:  Memory Region Configuration Registers
    //
    0x0310, 0x0312, 0x0314, 0x0316,

    // Display Engine:  Component Control
    //
    0x0320, 0x0322, 0x0324, 0x0326, 0x0328, 0x032A,
    0x032C, 0x032E,

    // Display Engine:  Control/Trigger Registers
    //
    0x0330, 0x0332, 0x0334,

    // Display Engine:  Update Buffer Status Registers
    //
    0x0336, 0x0338,

    // Display Engine:  Interrupt Registers
    //
    0x033A, 0x033C, 0x033E,

    // Display Engine:  Partial Update Configuration Registers
    //
    0x0340, 0x0342, 0x0344, 0x0346, 0x0348, 0x034A,
    0x034C, 0x034E,

    // Display Engine:  Waveform Registers (Broadsheet = 0x035x, ISIS = 0x039x)
    //
    0x0350, 0x0352, 0x0354, 0x0356, 0x0358, 0x035A,
    0x0390, 0x0392, 0x0394, 0x0396, 0x0398, 0x039A,
    
    // Display Engine:  Driver Advance Configuration Registers (ISIS)
    //
    0x0360, 0x362,    

    // Display Engine:  Auto Waveform Mode Configuration Registers
    //
    0x03A0, 0x03A2, 0x03A4, 0x03A6
};

static bs_cmd bs_poll_cmds[] =
{
    bs_cmd_INIT_CMD_SET,
    bs_cmd_INIT_PLL_STBY,
    bs_cmd_RUN_SYS,
    bs_cmd_STBY,
    bs_cmd_SLP,
    bs_cmd_INIT_SYS_RUN,
    bs_cmd_INIT_SYS_STBY,
    bs_cmd_INIT_SDRAM,
    bs_cmd_INIT_DSPE_CFG,
    bs_cmd_INIT_DSPE_TMG,
    bs_cmd_SET_ROTMODE,
    bs_cmd_INIT_WAVEFORMDEV,

    bs_cmd_BST_RD_SDR,
    bs_cmd_BST_WR_SDR,
    bs_cmd_BST_END,

    bs_cmd_RD_WFM_INFO,
    bs_cmd_UPD_GDRV_CLR
};

static unsigned long bs_set_ld_img_start;
static unsigned long bs_ld_img_start;
static unsigned long bs_upd_data_start;

static unsigned long bs_image_start_time;
static unsigned long bs_image_processing_time;
static unsigned long bs_image_loading_time;
static unsigned long bs_image_display_time;
static unsigned long bs_image_stop_time;

#define BS_IMAGE_TIMING_START   0
#define BS_IMAGE_TIMING_PROC    1
#define BS_IMAGE_TIMING_LOAD    2
#define BS_IMAGE_TIMING_DISP    3
#define BS_IMAGE_TIMING_STOP    4
#define BS_NUM_IMAGE_TIMINGS    (BS_IMAGE_TIMING_STOP + 1)
static unsigned long bs_image_timings[BS_NUM_IMAGE_TIMINGS];

static u8 bs_4bpp_nybble_swap_table_inverted[256] =
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

#define UM02   0x00
#define UM04   0x04
#define UM16   0x06

static u8 bs_4bpp_upd_mode_table[256] =
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

#define BS_INIT_UPD_MODE()          UM02
#define BS_TEST_UPD_MODE(m, b)      ((EINKFB_2BPP == (b)) ? (UM04 == (m)) : (UM16 == (m)))
#define BS_FIND_UPD_MODE(m, p, b)   (BS_TEST_UPD_MODE(m, b) ? (m) : ((m) | bs_4bpp_upd_mode_table[(p)]))
#define BS_DONE_UPD_MODE(m)                         \
    ((UM02 == (m)) ? BS_UPD_MODE(BS_UPD_MODE_MU) :  \
     (UM04 == (m)) ? BS_UPD_MODE(BS_UPD_MODE_GU) :  BS_UPD_MODE(BS_UPD_MODE_GC))

// 4bpp -> 1bpp (inverted and nybble swapped)
//
static u8 bs_posterize_table_4bpp[256] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 00..0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 10..1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 20..2F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 30..3F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 40..4F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 50..5F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 60..6F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, // 70..7F
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80..8F
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 90..9F
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // A0..AF
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // B0..BF
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // C0..CF
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // D0..DF
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // E0..EF
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // F0..FF
};

// 8bpp -> 1bpp (inverted and nybble swapped)
//
static u8 bs_posterize_table_8bpp[256] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 00..0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 10..1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 20..2F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 30..3F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 40..4F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 50..5F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 60..6F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 70..7F
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80..8F
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 90..9F
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // A0..AF
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // B0..BF
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // C0..CF
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // D0..DF
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // E0..EF
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // F0..FF
};

static u8 isis_commands[] =
{
    0x00, 0x00, 0x03, 0x03, 0x43, 0x02, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00,
    0x00, 0xe0, 0x18, 0x00, 0x46, 0x00, 0x69, 0x00, 0x00, 0xe0, 0x00, 0xe0,
    0xda, 0xa0, 0xe6, 0xa0, 0xf0, 0x20, 0xf8, 0x20, 0x00, 0xe0, 0x00, 0xe0,
    0x00, 0xe0, 0x00, 0x20, 0x00, 0x40, 0x00, 0xe0, 0x00, 0xe0, 0xfa, 0x00,
    0x02, 0x81, 0x0c, 0x41, 0x10, 0x41, 0x14, 0x81, 0x1c, 0x41, 0x20, 0x21,
    0x00, 0xe0, 0x22, 0x81, 0x4d, 0x81, 0x78, 0x01, 0x00, 0xe0, 0x88, 0x21,
    0x00, 0xe0, 0xc4, 0xa1, 0xe2, 0x01, 0xf1, 0x01, 0xf4, 0x41, 0xfc, 0x01,
    0x00, 0xe0, 0x00, 0x02, 0x03, 0x02, 0x06, 0x02, 0x09, 0x22, 0x00, 0xe0,
    0x00, 0xe0, 0x00, 0xe0, 0x00, 0xe0, 0x13, 0x42, 0x00, 0xe0, 0x1c, 0x02,
    0x1e, 0x22, 0x22, 0xa2, 0x2e, 0x22, 0x32, 0xa2, 0x3e, 0x02, 0x40, 0x42,
    0x00, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0xe0,
    0x00, 0xe0, 0x06, 0x70, 0x01, 0x00, 0x16, 0x70, 0x01, 0x00, 0x0a, 0x10,
    0x01, 0x00, 0x01, 0x00, 0x16, 0x70, 0x00, 0x00, 0x06, 0x70, 0x00, 0x00,
    0x30, 0x32, 0x30, 0x92, 0x01, 0x00, 0x03, 0x00, 0x04, 0x71, 0x02, 0x00,
    0x0a, 0x10, 0x00, 0x00, 0x00, 0x42, 0x0a, 0x30, 0x0a, 0x98, 0x00, 0x04,
    0x00, 0x0c, 0x0a, 0x10, 0x00, 0x00, 0xdc, 0xc0, 0x0a, 0x00, 0x00, 0x08,
    0x00, 0x0c, 0x2e, 0x00, 0x0a, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x30, 0x00,
    0x04, 0x71, 0x01, 0x00, 0x0a, 0x10, 0x00, 0x02, 0x00, 0x02, 0x0a, 0x30,
    0x0a, 0x90, 0x00, 0x08, 0x00, 0x0c, 0x16, 0x70, 0x00, 0x00, 0x06, 0x78,
    0x01, 0x00, 0x06, 0x70, 0x01, 0x00, 0x16, 0x70, 0x01, 0x00, 0x0a, 0x10,
    0x01, 0x00, 0x01, 0x00, 0x16, 0x70, 0x00, 0x00, 0x06, 0x70, 0x00, 0x00,
    0x30, 0x32, 0x30, 0x92, 0x01, 0x00, 0x03, 0x00, 0x0a, 0x10, 0x00, 0x00,
    0x00, 0x40, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x0a, 0x10,
    0x00, 0x00, 0xdc, 0xc0, 0x0a, 0x00, 0x00, 0x08, 0x00, 0x0c, 0x51, 0x00,
    0x0a, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x66, 0x00, 0x06, 0x70, 0x00, 0x00,
    0x30, 0x32, 0x30, 0x92, 0x02, 0x00, 0x03, 0x00, 0x04, 0x71, 0x01, 0x00,
    0x0a, 0x10, 0x00, 0x02, 0x00, 0x42, 0x0a, 0x30, 0x0a, 0x90, 0x00, 0x0c,
    0x00, 0x0c, 0x06, 0x70, 0x01, 0x00, 0x16, 0x70, 0x01, 0x00, 0x16, 0x70,
    0x03, 0x00, 0x0a, 0x18, 0x00, 0x00, 0x01, 0x00, 0x06, 0x70, 0x01, 0x00,
    0x10, 0x70, 0x03, 0x00, 0x12, 0x70, 0x49, 0x59, 0x14, 0x70, 0x40, 0x00,
    0x16, 0x70, 0x01, 0x00, 0x0a, 0x10, 0x01, 0x00, 0x01, 0x00, 0x16, 0x70,
    0x00, 0x00, 0x06, 0x70, 0x00, 0x00, 0x00, 0x71, 0xf1, 0x71, 0x06, 0x71,
    0x75, 0x01, 0x08, 0x71, 0x88, 0x00, 0x0a, 0x71, 0x00, 0x00, 0x18, 0x70,
    0x05, 0x00, 0x00, 0x73, 0x58, 0x02, 0x02, 0x73, 0x04, 0x00, 0x04, 0x73,
    0x04, 0x0a, 0x06, 0x73, 0x20, 0x03, 0x08, 0x73, 0x0a, 0x00, 0x0a, 0x73,
    0x04, 0x6e, 0x0c, 0x73, 0x64, 0x03, 0x0e, 0x73, 0x02, 0x00, 0x60, 0x73,
    0x03, 0x00, 0x62, 0x73, 0x01, 0x00, 0x64, 0x73, 0x00, 0x00, 0x66, 0x73,
    0x00, 0x00, 0x68, 0x73, 0x00, 0x00, 0x70, 0x73, 0x00, 0x00, 0x72, 0x73,
    0x00, 0x00, 0x74, 0x73, 0x00, 0x00, 0x76, 0x73, 0x00, 0x00, 0x78, 0x73,
    0x00, 0x00, 0x7a, 0x73, 0x00, 0x00, 0x34, 0x72, 0x1b, 0x00, 0x36, 0x72,
    0x08, 0x00, 0x38, 0x72, 0x00, 0x00, 0x30, 0x72, 0x01, 0x00, 0x30, 0x12,
    0x00, 0x06, 0x00, 0x06, 0x02, 0x71, 0x01, 0x00, 0x10, 0x73, 0x80, 0xfc,
    0x12, 0x73, 0x0c, 0x00, 0x14, 0x73, 0x00, 0x00, 0x16, 0x73, 0x00, 0x00,
    0x2c, 0x73, 0x00, 0x04, 0x30, 0x73, 0x84, 0x00, 0x1a, 0x70, 0x0a, 0x00,
    0x0a, 0x10, 0x02, 0x00, 0x02, 0x40, 0x04, 0x72, 0x99, 0x00, 0x0a, 0x30,
    0x0a, 0x90, 0x00, 0x04, 0x00, 0x0c, 0x50, 0x72, 0x3f, 0x00, 0x52, 0x72,
    0x00, 0x00, 0xc0, 0x73, 0x11, 0x01, 0xc2, 0x73, 0x02, 0x00, 0xc4, 0x73,
    0x02, 0x00, 0xca, 0x7b, 0x00, 0x00, 0x06, 0x63, 0x01, 0x00, 0x00, 0x63,
    0x02, 0x00, 0x0c, 0x63, 0x03, 0x00, 0x0e, 0x63, 0x04, 0x00, 0x30, 0x63,
    0x05, 0x00, 0x2c, 0x7b, 0x00, 0x04, 0x02, 0x63, 0x01, 0x00, 0x04, 0x63,
    0x02, 0x00, 0x08, 0x63, 0x03, 0x00, 0x0a, 0x63, 0x04, 0x00, 0x18, 0x68,
    0x05, 0x00, 0x2c, 0x33, 0x2c, 0x83, 0x01, 0x00, 0x00, 0x03, 0x40, 0x31,
    0x40, 0x89, 0x01, 0x00, 0x00, 0x03, 0x60, 0x6a, 0x01, 0x00, 0xc0, 0x73,
    0x11, 0x01, 0xc2, 0x73, 0x02, 0x00, 0xc4, 0x73, 0x02, 0x00, 0xca, 0x7b,
    0x00, 0x00, 0xc0, 0xa3, 0x01, 0x00, 0x01, 0x00, 0xfe, 0xff, 0xc2, 0x63,
    0x02, 0x00, 0xc4, 0x63, 0x03, 0x00, 0xca, 0x6b, 0x04, 0x00, 0x18, 0x63,
    0x01, 0x00, 0x1a, 0x6b, 0x02, 0x00, 0xc6, 0x63, 0x01, 0x00, 0xc8, 0x6b,
    0x02, 0x00, 0xd0, 0x63, 0x01, 0x00, 0xd2, 0x63, 0x02, 0x00, 0xd4, 0x63,
    0x03, 0x00, 0xda, 0x6b, 0x04, 0x00, 0xd6, 0x63, 0x01, 0x00, 0xd8, 0x6b,
    0x02, 0x00, 0xdc, 0x6b, 0x01, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x10,
    0x29, 0x01, 0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x42, 0x71, 0x02, 0x00,
    0x40, 0x11, 0x00, 0x00, 0x00, 0x30, 0x40, 0x31, 0x40, 0x91, 0x05, 0x00,
    0x07, 0x00, 0x44, 0x61, 0x01, 0x00, 0x46, 0x61, 0x02, 0x00, 0x48, 0x61,
    0x03, 0x00, 0x4a, 0x61, 0x04, 0x00, 0x48, 0x01, 0x00, 0x00, 0xff, 0xff,
    0x42, 0x01, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x46, 0x01, 0x4a, 0x01,
    0x00, 0x00, 0xff, 0xff, 0x4b, 0x01, 0x42, 0x71, 0x01, 0x00, 0x40, 0x19,
    0x00, 0x20, 0x00, 0x20, 0x42, 0x79, 0x02, 0x00, 0x40, 0x01, 0x00, 0x00,
    0x00, 0x10, 0x54, 0x01, 0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x42, 0x71,
    0x02, 0x00, 0x40, 0x11, 0x00, 0x00, 0x00, 0x30, 0x40, 0x31, 0x40, 0x91,
    0x01, 0x00, 0x07, 0x00, 0x44, 0x61, 0x01, 0x00, 0x46, 0x61, 0x02, 0x00,
    0x48, 0x61, 0x03, 0x00, 0x4a, 0x61, 0x04, 0x00, 0x48, 0x01, 0x00, 0x00,
    0xff, 0xff, 0x6d, 0x01, 0x48, 0x01, 0x00, 0x00, 0x00, 0x00, 0x71, 0x01,
    0x4a, 0x01, 0x00, 0x00, 0xff, 0xff, 0x76, 0x01, 0x42, 0x71, 0x01, 0x00,
    0x40, 0x19, 0x00, 0x20, 0x00, 0x20, 0x42, 0x79, 0x02, 0x00, 0x40, 0x01,
    0x00, 0x00, 0x00, 0x10, 0x7f, 0x01, 0x40, 0x11, 0x00, 0x00, 0x00, 0x10,
    0x42, 0x71, 0x02, 0x00, 0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x40, 0x31,
    0x40, 0x99, 0x00, 0x80, 0x00, 0x80, 0x40, 0x01, 0x00, 0x00, 0x00, 0x10,
    0x8f, 0x01, 0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x42, 0x71, 0x02, 0x00,
    0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x40, 0x31, 0x40, 0x91, 0x00, 0x00,
    0x07, 0x00, 0x40, 0x31, 0x40, 0x81, 0x01, 0x00, 0x78, 0x04, 0x4c, 0x71,
    0x00, 0x00, 0x4e, 0x71, 0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x04,
    0xad, 0x01, 0xc2, 0x33, 0x50, 0x91, 0x00, 0x00, 0x00, 0x00, 0xc4, 0x33,
    0x52, 0x91, 0x00, 0x00, 0x00, 0x00, 0xc2, 0xf1, 0x40, 0x01, 0x00, 0x01,
    0x00, 0x01, 0xba, 0x01, 0x06, 0x33, 0x50, 0x91, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x33, 0x52, 0x91, 0x00, 0x00, 0x00, 0x00, 0xc2, 0xf1, 0x06, 0x33,
    0x52, 0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x50, 0x91, 0x00, 0x00,
    0x00, 0x00, 0x42, 0x79, 0x01, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x10,
    0xcd, 0x01, 0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x42, 0x71, 0x02, 0x00,
    0x40, 0x11, 0x00, 0x00, 0x00, 0x10, 0x40, 0x31, 0x40, 0x91, 0x00, 0x00,
    0x07, 0x00, 0x40, 0x31, 0x40, 0x81, 0x01, 0x00, 0x78, 0x04, 0x4c, 0x61,
    0x02, 0x00, 0x4e, 0x61, 0x03, 0x00, 0x50, 0x61, 0x04, 0x00, 0x52, 0x61,
    0x05, 0x00, 0x42, 0x79, 0x01, 0x00, 0x40, 0x11, 0x00, 0x00, 0x00, 0x40,
    0x40, 0x01, 0x00, 0x20, 0x00, 0x20, 0xec, 0x01, 0x40, 0x11, 0x00, 0x00,
    0x00, 0x10, 0x42, 0x71, 0x02, 0x00, 0x40, 0x19, 0x00, 0x00, 0x00, 0x10,
    0x40, 0x19, 0x00, 0x00, 0x00, 0x10, 0x40, 0x31, 0x40, 0x91, 0x80, 0x00,
    0x80, 0x00, 0x44, 0x61, 0x01, 0x00, 0x46, 0x69, 0x02, 0x00, 0x40, 0x31,
    0x40, 0x99, 0x00, 0x00, 0x80, 0x00, 0x38, 0x1b, 0x00, 0x00, 0x01, 0x00,
    0x38, 0x1b, 0x00, 0x00, 0x08, 0x00, 0x38, 0x1b, 0x20, 0x00, 0x20, 0x00,
    0x2e, 0x63, 0x01, 0x00, 0x2e, 0x03, 0x00, 0x00, 0xff, 0xff, 0x12, 0x02,
    0x38, 0x1b, 0x40, 0x00, 0x40, 0x00, 0x2e, 0x3b, 0x90, 0x63, 0x01, 0x00,
    0x92, 0x63, 0x02, 0x00, 0x34, 0x73, 0x01, 0x00, 0x38, 0x1b, 0x00, 0x00,
    0x01, 0x00, 0x34, 0x7b, 0x05, 0x00, 0x34, 0xab, 0x01, 0x00, 0x07, 0x00,
    0xf0, 0x4f, 0x80, 0x63, 0x02, 0x00, 0x82, 0x63, 0x03, 0x00, 0x84, 0x63,
    0x04, 0x00, 0x86, 0x63, 0x05, 0x00, 0x34, 0xab, 0x01, 0x00, 0x07, 0x20,
    0xf0, 0x4f, 0x34, 0xab, 0x01, 0x00, 0x09, 0x00, 0xf0, 0x4f, 0x80, 0x63,
    0x02, 0x00, 0x82, 0x63, 0x03, 0x00, 0x84, 0x63, 0x04, 0x00, 0x86, 0x63,
    0x05, 0x00, 0x34, 0xab, 0x01, 0x00, 0x09, 0x20, 0xf0, 0x4f, 0x34, 0x7b,
    0x0b, 0x00, 0x10, 0x63, 0x01, 0x00, 0x12, 0x6b, 0x02, 0x00, 0x65, 0xeb,
    0x00, 0x00
};

#define SIZEOF_ISIS_COMMANDS 1298

static u8 isis_waveform[] =
{
    0xb1, 0x08, 0xa8, 0xf3, 0x84, 0x06, 0x00, 0x00, 0xbc, 0x01, 0x00, 0x00,
    0x00, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x0b, 0x3c, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x3c, 0x3a, 0x00, 0x00, 0x01,
    0x00, 0x03, 0x00, 0x00, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x39,
    0x00, 0x23, 0x23, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x4a, 0x00,
    0x00, 0x4a, 0x4e, 0x00, 0x00, 0x4e, 0x52, 0x00, 0x00, 0x52, 0x56, 0x00,
    0x00, 0x56, 0x5a, 0x00, 0x00, 0x5a, 0x94, 0x00, 0x00, 0x94, 0x6e, 0x01,
    0x00, 0x6f, 0xf9, 0x03, 0x00, 0xfc, 0x55, 0xff, 0x55, 0xff, 0x55, 0xff,
    0x55, 0xff, 0x55, 0x7f, 0x00, 0xff, 0x00, 0xbf, 0xaa, 0xff, 0xaa, 0xff,
    0xaa, 0xff, 0xaa, 0xff, 0xaa, 0x7f, 0x00, 0xff, 0x00, 0xff, 0x55, 0xff,
    0x55, 0xff, 0x55, 0xff, 0x55, 0xff, 0x55, 0x7f, 0x00, 0xff, 0x00, 0xbf,
    0xaa, 0xff, 0xaa, 0xff, 0xaa, 0xff, 0xaa, 0xff, 0xaa, 0x7f, 0x00, 0xff,
    0x00, 0x3f, 0xff, 0x99, 0x00, 0x3b, 0xfc, 0x44, 0x00, 0x01, 0xfc, 0x00,
    0x3c, 0xfc, 0x44, 0x45, 0x41, 0x10, 0x00, 0x00, 0x00, 0x40, 0xfc, 0x00,
    0x37, 0xfc, 0x46, 0x55, 0x45, 0x11, 0x00, 0x00, 0x00, 0x50, 0xfc, 0x00,
    0x37, 0xfc, 0x5a, 0x55, 0x45, 0x11, 0x00, 0x00, 0x00, 0x55, 0xfc, 0x00,
    0x37, 0x5a, 0x00, 0x55, 0x01, 0x15, 0x00, 0x00, 0x01, 0xfc, 0x40, 0x55,
    0xfc, 0x00, 0x37, 0xaa, 0x00, 0x55, 0x01, 0x15, 0x00, 0x00, 0x01, 0x55,
    0x01, 0x00, 0x37, 0xaa, 0x00, 0x55, 0x01, 0xfc, 0x15, 0x00, 0x40, 0x55,
    0x55, 0xfc, 0x00, 0x37, 0xfc, 0xaa, 0x5a, 0x55, 0x15, 0x00, 0x50, 0x55,
    0x55, 0xfc, 0x00, 0x37, 0xaa, 0x01, 0xfc, 0x56, 0x15, 0x00, 0x55, 0x55,
    0x55, 0xfc, 0x00, 0x37, 0xaa, 0x01, 0xfc, 0x9a, 0x15, 0x00, 0x55, 0x55,
    0x55, 0xfc, 0x00, 0x37, 0xaa, 0x02, 0xfc, 0x26, 0x00, 0xfc, 0x55, 0x02,
    0x00, 0x37, 0xaa, 0x02, 0xfc, 0x2a, 0x40, 0xfc, 0x55, 0x02, 0x00, 0x37,
    0xaa, 0x02, 0xfc, 0x2a, 0x40, 0xfc, 0x55, 0x02, 0x00, 0x37, 0xaa, 0x02,
    0xfc, 0x2a, 0x40, 0xfc, 0x55, 0x02, 0x00, 0x37, 0xaa, 0x02, 0xfc, 0x2a,
    0x50, 0xfc, 0x55, 0x02, 0x00, 0x37, 0xaa, 0x02, 0xfc, 0x2a, 0x54, 0xfc,
    0x55, 0x02, 0x00, 0x37, 0xaa, 0x02, 0xfc, 0x2a, 0x54, 0xfc, 0x55, 0x02,
    0x00, 0x37, 0xaa, 0x02, 0xfc, 0x2a, 0x54, 0xfc, 0x55, 0x02, 0x00, 0x37,
    0xaa, 0x02, 0xfc, 0x2a, 0x54, 0xfc, 0x55, 0x02, 0x00, 0x37, 0xaa, 0x02,
    0x2a, 0x00, 0x00, 0x3f, 0xff, 0x60, 0x00, 0x01, 0x10, 0x00, 0x00, 0x3a,
    0x10, 0x00, 0x00, 0x01, 0xfc, 0x04, 0x10, 0xfc, 0x00, 0x3a, 0xfc, 0x14,
    0x00, 0x41, 0x04, 0x10, 0x01, 0xfc, 0x00, 0x26, 0x40, 0x00, 0x00, 0x10,
    0xfc, 0x01, 0x15, 0x00, 0x41, 0x04, 0x50, 0x11, 0xfc, 0x00, 0x12, 0x05,
    0x00, 0x00, 0x12, 0x50, 0x00, 0x00, 0x10, 0xfc, 0x51, 0x15, 0x00, 0x46,
    0x04, 0x55, 0x15, 0xfc, 0x00, 0x11, 0xfc, 0x40, 0x05, 0xfc, 0x00, 0x12,
    0x55, 0x00, 0x00, 0x10, 0x55, 0x01, 0xfc, 0x00, 0x56, 0x05, 0x55, 0x15,
    0xfc, 0x00, 0x11, 0xfc, 0x55, 0x05, 0xfc, 0x00, 0x10, 0xfc, 0x01, 0x40,
    0x55, 0xfc, 0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0xfc, 0x05, 0x56, 0x15,
    0x55, 0x15, 0xfc, 0x00, 0x10, 0xfc, 0x40, 0x55, 0x05, 0xfc, 0x00, 0x10,
    0x01, 0x00, 0x55, 0x01, 0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0xfc, 0x15,
    0x56, 0x55, 0x55, 0x15, 0xfc, 0x00, 0x0f, 0xfc, 0x04, 0x50, 0x55, 0x05,
    0xfc, 0x00, 0x10, 0x41, 0x00, 0x55, 0x01, 0x00, 0x0f, 0x44, 0x00, 0x55,
    0x01, 0xfc, 0x15, 0x56, 0x55, 0x55, 0x15, 0xfc, 0x00, 0x0f, 0x04, 0x00,
    0x55, 0x01, 0x05, 0x00, 0x00, 0x0f, 0xfc, 0x40, 0x51, 0x55, 0x55, 0xfc,
    0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0xaa, 0x00, 0x55, 0x01, 0x15, 0x00,
    0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0x05, 0x00, 0x00, 0x0f, 0x40, 0x00,
    0x55, 0x02, 0x00, 0x0f, 0xfc, 0x50, 0x55, 0x45, 0x55, 0xaa, 0x55, 0x55,
    0x15, 0xfc, 0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0x15, 0x00, 0x00, 0x0f,
    0x40, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0x50, 0x54, 0x41, 0x55, 0xaa,
    0x55, 0x55, 0x15, 0xfc, 0x00, 0x0f, 0x4a, 0x00, 0x55, 0x02, 0x00, 0x0f,
    0x50, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0x4a, 0x00, 0x40, 0x55, 0xaa,
    0x59, 0x65, 0x15, 0xfc, 0x00, 0x0f, 0x4a, 0x00, 0x55, 0x02, 0x00, 0x0f,
    0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0x0a, 0x00, 0x00, 0x01, 0xfc, 0x55,
    0xaa, 0x5a, 0x65, 0x15, 0xfc, 0x00, 0x0f, 0x4a, 0x00, 0x55, 0x02, 0x00,
    0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0x2a, 0x00, 0x00, 0x01, 0xfc,
    0x50, 0xaa, 0x5a, 0xa9, 0x16, 0xfc, 0x00, 0x0f, 0x5a, 0x00, 0x55, 0x02,
    0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0xaa, 0x02, 0x00,
    0x40, 0xaa, 0x6a, 0xaa, 0x1a, 0xfc, 0x00, 0x0f, 0xaa, 0x02, 0x5a, 0x00,
    0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0xaa, 0x2a, 0x00,
    0x40, 0xaa, 0xaa, 0xaa, 0x6a, 0xfc, 0x00, 0x0f, 0xaa, 0x02, 0x5a, 0x00,
    0x00, 0x0f, 0xfc, 0xaa, 0x56, 0x55, 0x55, 0xfc, 0x00, 0x0f, 0xaa, 0x01,
    0xfc, 0x2a, 0x40, 0xfc, 0xaa, 0x02, 0x6a, 0x00, 0x00, 0x0f, 0xaa, 0x02,
    0x5a, 0x00, 0x00, 0x0f, 0xfc, 0xaa, 0x56, 0x55, 0x55, 0xfc, 0x00, 0x0f,
    0xaa, 0x01, 0xfc, 0x2a, 0x40, 0xfc, 0xaa, 0x02, 0x6a, 0x00, 0x00, 0x0f,
    0xaa, 0x02, 0x5a, 0x00, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x02,
    0x00, 0x00, 0xaa, 0x02, 0x6a, 0x00, 0x00, 0x0f, 0xaa, 0x02, 0x5a, 0x00,
    0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x02, 0xfc, 0x8a, 0xa9, 0xaa,
    0xaa, 0x6a, 0xfc, 0x00, 0x0f, 0xaa, 0x02, 0x6a, 0x00, 0x00, 0x0f, 0xaa,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x59, 0x00, 0xaa, 0x01, 0x6a, 0x00, 0x00,
    0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0xfc,
    0x55, 0xa9, 0x56, 0x65, 0xfc, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00,
    0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f, 0xaa,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00,
    0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa,
    0x03, 0x55, 0x03, 0x00, 0x0f, 0x55, 0x02, 0xa5, 0x00, 0x00, 0x0f, 0xaa,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f, 0x55, 0x03, 0x00,
    0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f, 0x55,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x13, 0x55, 0x03, 0x00, 0x0f, 0x55,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x13, 0x55, 0x03, 0x00, 0x0f, 0x55,
    0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x13, 0x55, 0x03, 0x00, 0x0f, 0x55,
    0x03, 0x00, 0x0f, 0x55, 0x03, 0x00, 0x13, 0x55, 0x03, 0x00, 0x0f, 0xaa,
    0x03, 0x00, 0x0f, 0x55, 0x03, 0x00, 0x53, 0xff, 0x23, 0x00, 0x01, 0x10,
    0x00, 0x00, 0x3a, 0x10, 0x00, 0x00, 0x01, 0xfc, 0x04, 0x10, 0xfc, 0x00,
    0x3a, 0xfc, 0x14, 0x00, 0x41, 0x04, 0x10, 0x01, 0xfc, 0x00, 0x26, 0x40,
    0x00, 0x00, 0x10, 0xfc, 0x01, 0x15, 0x00, 0x41, 0x04, 0x50, 0x11, 0xfc,
    0x00, 0x12, 0x05, 0x00, 0x00, 0x12, 0x50, 0x00, 0x00, 0x10, 0xfc, 0x51,
    0x15, 0x00, 0x46, 0x04, 0x55, 0x15, 0xfc, 0x00, 0x11, 0xfc, 0x40, 0x05,
    0xfc, 0x00, 0x12, 0x55, 0x00, 0x00, 0x10, 0x55, 0x01, 0xfc, 0x00, 0x56,
    0x05, 0x55, 0x15, 0xfc, 0x00, 0x11, 0xfc, 0x55, 0x05, 0xfc, 0x00, 0x10,
    0xfc, 0x01, 0x40, 0x55, 0xfc, 0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0xfc,
    0x05, 0x56, 0x15, 0x55, 0x15, 0xfc, 0x00, 0x10, 0xfc, 0x40, 0x55, 0x05,
    0xfc, 0x00, 0x10, 0x01, 0x00, 0x55, 0x01, 0x00, 0x0f, 0x04, 0x00, 0x55,
    0x01, 0xfc, 0x15, 0x56, 0x55, 0x55, 0x15, 0xfc, 0x00, 0x0f, 0xfc, 0x04,
    0x50, 0x55, 0x05, 0xfc, 0x00, 0x10, 0x41, 0x00, 0x55, 0x01, 0x00, 0x0f,
    0x44, 0x00, 0x55, 0x01, 0xfc, 0x15, 0x56, 0x55, 0x55, 0x15, 0xfc, 0x00,
    0x0f, 0x04, 0x00, 0x55, 0x01, 0x05, 0x00, 0x00, 0x0f, 0xfc, 0x40, 0x51,
    0x55, 0x55, 0xfc, 0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0xaa, 0x00, 0x55,
    0x01, 0x15, 0x00, 0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0x05, 0x00, 0x00,
    0x0f, 0x40, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0x50, 0x55, 0x45, 0x55,
    0xaa, 0x55, 0x55, 0x15, 0xfc, 0x00, 0x0f, 0x04, 0x00, 0x55, 0x01, 0x15,
    0x00, 0x00, 0x0f, 0x40, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0x50, 0x54,
    0x41, 0x55, 0xaa, 0x55, 0x55, 0x15, 0xfc, 0x00, 0x0f, 0x4a, 0x00, 0x55,
    0x02, 0x00, 0x0f, 0x50, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc, 0x4a, 0x00,
    0x40, 0x55, 0xaa, 0x59, 0x65, 0x15, 0xfc, 0x00, 0x0f, 0x4a, 0x00, 0x55,
    0x02, 0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0x0a, 0x00, 0x00,
    0x01, 0xfc, 0x55, 0xaa, 0x5a, 0x65, 0x15, 0xfc, 0x00, 0x0f, 0x4a, 0x00,
    0x55, 0x02, 0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0x2a, 0x00,
    0x00, 0x01, 0xfc, 0x50, 0xaa, 0x5a, 0xa9, 0x16, 0xfc, 0x00, 0x0f, 0x5a,
    0x00, 0x55, 0x02, 0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc,
    0xaa, 0x02, 0x00, 0x40, 0xaa, 0x6a, 0xaa, 0x1a, 0xfc, 0x00, 0x0f, 0xaa,
    0x02, 0x5a, 0x00, 0x00, 0x0f, 0x54, 0x00, 0x55, 0x02, 0x00, 0x0f, 0xfc,
    0xaa, 0x2a, 0x00, 0x40, 0xaa, 0xaa, 0xaa, 0x6a, 0xfc, 0x00, 0x0f, 0xaa,
    0x02, 0x5a, 0x00, 0x00, 0x0f, 0xfc, 0xaa, 0x56, 0x55, 0x55, 0xfc, 0x00,
    0x0f, 0xaa, 0x01, 0xfc, 0x2a, 0x40, 0xfc, 0xaa, 0x02, 0x6a, 0x00, 0x00,
    0x0f, 0xaa, 0x02, 0x5a, 0x00, 0x00, 0x0f, 0xfc, 0xaa, 0x56, 0x55, 0x55,
    0xfc, 0x00, 0x0f, 0xaa, 0x01, 0xfc, 0x2a, 0x40, 0xfc, 0xaa, 0x02, 0x6a,
    0x00, 0x00, 0x0f, 0xaa, 0x02, 0x5a, 0x00, 0x00, 0x0f, 0xaa, 0x03, 0x00,
    0x0f, 0xaa, 0x02, 0x00, 0x00, 0xaa, 0x02, 0x6a, 0x00, 0x00, 0x0f, 0xaa,
    0x02, 0x5a, 0x00, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x02, 0xfc,
    0x8a, 0xa9, 0xaa, 0xaa, 0x6a, 0xfc, 0x00, 0x0f, 0xaa, 0x02, 0x6a, 0x00,
    0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x59, 0x00, 0xaa, 0x01,
    0x6a, 0x00, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f,
    0xaa, 0x03, 0xfc, 0x55, 0xa9, 0x56, 0x65, 0xfc, 0x00, 0x0f, 0xaa, 0x03,
    0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f,
    0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03,
    0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03,
    0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f,
    0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03,
    0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f, 0x55, 0x02, 0xa5, 0x00,
    0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03, 0x00, 0x0f,
    0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x55, 0x03,
    0x00, 0x0f, 0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x13, 0x55, 0x03,
    0x00, 0x0f, 0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x13, 0x55, 0x03,
    0x00, 0x0f, 0x55, 0x03, 0x00, 0x0f, 0xaa, 0x03, 0x00, 0x13, 0x55, 0x03,
    0x00, 0x0f, 0x55, 0x03, 0x00, 0x0f, 0x55, 0x03, 0x00, 0x13, 0x55, 0x03,
    0x00, 0x0f, 0xaa, 0x03, 0x00, 0x0f, 0x55, 0x03, 0x00, 0x53, 0xff, 0x23
};

#define SIZEOF_ISIS_WAVEFORM 1668

static char *panel_skus[] = 
{
    // 6.0-inch panels
    //
    "ED060SC4!!!!!!!!", "ED060SC4H1!!!!!!", "ED060SC4H2!!!!!!", "ED060SC5!!!!!!!!", "ED060SC5H1!!!!!!", "ED060SC5H2!!!!!!",
    "ED060SC7!!!!!!!!", "ED060SC7H1!!!!!!", "ED060SC7H2!!!!!!", "ED060SC5C1!!!!!!", "ED060SC7C1!!!!!!", "ED060SC7T1!!!!!!",
    "LB060S03-RD01!!!", "LB060S03-RD02!!!",
    
    // 9.7-inch panels
    //
    "ED097OC1!!!!!!!!", "ED097OC1H1!!!!!!", "ED097OC1H2!!!!!!", "EF097OC3!!!!!!!!", "EF097OC3H1!!!!!!", "EF097OC3H2!!!!!!",
    "EF097OC4!!!!!!!!", "EF097OC4H1!!!!!!", "EF097OC4H2!!!!!!", "ED097OC4!!!!!!!!", "ED097OC4H1!!!!!!", "ED097OC4H2!!!!!!",
    
    // EOL
    //
    ""
};

static char *panel_mmms[] = 
{
    // 6.0-inch panels
    //
    "M01", "M03", "M04", "M06", "M0B", "M0C",
    "M12", "M13", "M14", "M23", "M24", "M3D",
    "MA1", "MA2",
    
    // 9.7-inch panels
    //
    "M01", "M0D", "M05", "M0E", "M0F", "M10",
    "M15", "M16", "M17", "M20", "M21", "M22",
    
    // EOL
    //
    ""
};

#define ISIS_PANEL_MMM_DEFAULT "M01"

static char *power_state_strings[] =
{
    "init",     // bs_power_state_init

    "clear",    // bs_power_state_off_screen_clear
    "off",      // bs_power_state_off
    "run",      // bs_power_state_run 
    "standby",  // bs_power_state_standby
    "sleep"     // bs_power_state_sleep
};

static bool bs_isis(void);
static bool bs_asic(void);
static bool bs_fpga(void);

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet SW Primitives
    #pragma mark -
#endif

#define BS_UPD_MODE(u) (bs_upd_modes ? bs_upd_modes[(u)] : (u))

static void bs_set_upd_modes(void)
{
    // Don't re-read the waveform to set up the modes again unless we have to.
    //
    if ( !bs_upd_modes )
    {
        broadsheet_waveform_t waveform;
        struct einkfb_info info;
        
        broadsheet_get_waveform_version(&waveform);
        einkfb_get_info(&info);

        switch ( waveform.mode_version )
        {
            case BS_UPD_MODES_00:
            default:
                bs_upd_mode_version = BS_UPD_MODES_00;
                bs_upd_mode_names = bs_upd_mode_00_names;
                bs_upd_modes = bs_upd_modes_00;
            break;
            
            case BS_UPD_MODES_01:
            case BS_UPD_MODES_02:
                bs_upd_mode_version = BS_UPD_MODES_01;
                bs_upd_mode_names = bs_upd_mode_01_names;
                bs_upd_modes = (EINKFB_2BPP == info.bpp) ? bs_upd_modes_01_2bpp : bs_upd_modes_01_4bpp;
            break;
            
            case BS_UPD_MODES_03:
                bs_upd_mode_version = BS_UPD_MODES_03;
                bs_upd_mode_names = bs_upd_mode_03_names;
                bs_upd_modes = (EINKFB_2BPP == info.bpp) ? bs_upd_modes_03_2bpp : bs_upd_modes_03_4bpp;
            break;
            
            case BS_UPD_MODES_04:
                bs_upd_mode_version = BS_UPD_MODES_04;
                bs_upd_mode_names = bs_upd_mode_04_names;
                bs_upd_modes = bs_upd_modes_04;
            break;
        }
    }
}

static void bs_debug_upd_mode(u16 upd_mode)
{
    if ( EINKFB_DEBUG() || EINKFB_PERF() )
    {
        char *upd_mode_string = NULL;

        if ( bs_upd_mode_names )
            upd_mode_string = bs_upd_mode_names[upd_mode];

        if ( upd_mode_string )
            einkfb_debug("upd_mode = %s\n", upd_mode_string);

        bs_upd_mode_string = upd_mode_string ? upd_mode_string : "??";
    }
}

static u16 bs_upd_mode_max(u16 a, u16 b)
{
    u16 max = BS_UPD_MODE(BS_UPD_MODE_GC);

    switch ( bs_upd_mode_version )
    {
        case BS_UPD_MODES_00:
            switch ( a )
            {
                case BS_UPD_MODE_MU:
                    max = b;
                 break;
    
                case BS_UPD_MODE_PU:
                    max = ((BS_UPD_MODE_GU == b) || (BS_UPD_MODE_GC == b)) ? b : a;
                break;
    
                case BS_UPD_MODE_GU:
                    max = (BS_UPD_MODE_GC == b) ? b : a;
                break;
    
                case BS_UPD_MODE_GC:
                    max = a;
                break;
            }
        break;
        
        case BS_UPD_MODES_01:
        case BS_UPD_MODES_02:
        case BS_UPD_MODES_03:
            switch ( a )
            {
                case BS_UPD_MODE_DU:
                    max = b;
                break;
    
                case BS_UPD_MODE_GC4:
                    max = (BS_UPD_MODE_GC16 == b) ? b : a;
                break;
    
                case BS_UPD_MODE_GC16:
                    max = a;
                break;
            }
            break;
        
        case BS_UPD_MODES_04:
            switch ( a )
            {
                case BS_UPD_MODE_DU:
                    max = b;
                break;
                
                case BS_UPD_MODE_GC16:
                    max = a;
                break;
            }
        break;
    }

    return ( max );
}

static bool bs_reg_valid(u16 ra)
{
    bool reg_valid = false;
    int i, n;

    for ( i = 0, n = sizeof(bs_regs)/sizeof(u16); (i < n) && !reg_valid; i++ )
        if ( ra == bs_regs[i] )
            reg_valid = true;

    return ( reg_valid );
}

static bool bs_poll_cmd(bs_cmd cmd)
{
    bool poll_cmd = false;
    int i, n;

    for ( i = 0, n = sizeof(bs_poll_cmds)/sizeof(bs_cmd); (i < n) && !poll_cmd; i++ )
        if ( cmd == bs_poll_cmds[i] )
            poll_cmd = true;

    return ( poll_cmd );
}

static bool bs_cmd_queue_empty(void)
{
    return ( bs_cmd_queue_entry == bs_cmd_queue_exit );
}

static int bs_cmd_queue_count(void)
{
    int count = 0;

    if ( !bs_cmd_queue_empty() )
    {
        if ( bs_cmd_queue_entry < bs_cmd_queue_exit )
            count = (BS_NUM_CMD_QUEUE_ELEMS - bs_cmd_queue_exit) + bs_cmd_queue_entry;
       else
            count = bs_cmd_queue_entry - bs_cmd_queue_exit;
    }

    return ( count );
}

static bs_cmd_queue_elem_t *bs_get_queue_cmd_elem(int i)
{
    bs_cmd_queue_elem_t *result = NULL;

    if ( bs_cmd_queue_count() > i )
    {
        int elem = bs_cmd_queue_exit + i;

        if ( bs_cmd_queue_entry < bs_cmd_queue_exit )
            elem %= BS_NUM_CMD_QUEUE_ELEMS;

        EINKFB_MEMCPYK(&bs_cmd_elem, &bs_cmd_queue[elem], sizeof(bs_cmd_queue_elem_t));
        result = &bs_cmd_elem;
    }

    return ( result );
}

static void bs_enqueue_cmd(bs_cmd_block_t *bs_cmd_block)
{
    if ( bs_cmd_block )
    {
        einkfb_debug_full("bs_cmd_queue_entry = %03d\n", bs_cmd_queue_entry);

        // Enqueue the passed-in command.
        //
        EINKFB_MEMCPYK(&bs_cmd_queue[bs_cmd_queue_entry].bs_cmd_block, bs_cmd_block, sizeof(bs_cmd_block_t));
        bs_cmd_queue[bs_cmd_queue_entry++].time_stamp = FB_GET_CLOCK_TICKS();

        // Wrap back to the start if we've reached the end.
        //
        if ( BS_NUM_CMD_QUEUE_ELEMS == bs_cmd_queue_entry )
            bs_cmd_queue_entry = 0;

        // Ensure that we don't accidently go empty.
        //
        if ( bs_cmd_queue_empty() )
        {
            bs_cmd_queue_exit++;

            if ( BS_NUM_CMD_QUEUE_ELEMS == bs_cmd_queue_exit )
                bs_cmd_queue_exit = 0;
        }
    }
}

// static bs_cmd_queue_elem_t *bs_dequeue_cmd(void)
// {
//     bs_cmd_queue_elem_t *result = NULL;
//
//     if ( !bs_cmd_queue_empty() )
//     {
//         einkfb_debug_full("bs_cmd_queue_exit = %03d\n", bs_cmd_queue_exit);
//
//         // Dequeue the event.
//         //
//         EINKFB_MEMCPYK(&bs_cmd_elem, &bs_cmd_queue[bs_cmd_queue_exit++], sizeof(bs_cmd_queue_elem_t));
//         result = &bs_cmd_elem;
//
//         // Wrap back to the start if we've reached the end.
//         //
//         if ( BS_NUM_CMD_QUEUE_ELEMS == bs_cmd_queue_exit )
//             bs_cmd_queue_exit = 0;
//     }
//
//     return ( result );
// }

#define BS_WR_DATA(s, d) bs_wr_dat(BS_WR_DAT_DATA, s, d)
#define BS_WR_ARGS(s, d) bs_wr_dat(BS_WR_DAT_ARGS, s, d)

#define BS_SLEEPING()   ((bs_power_state_standby == bs_power_state) || (bs_power_state_sleep == bs_power_state))

static int bs_send_cmd(bs_cmd_block_t *bs_cmd_block)
{
    int result = EINKFB_FAILURE;

    if ( (broadsheet_ignore_hw_ready() || bs_ready) && bs_cmd_block )
    {
        bs_cmd command = bs_cmd_block->command;
        bs_cmd_type type = bs_cmd_block->type;

        // If we're in the sleep or standby state and the next command isn't run,
        // get us back to run first!
        //
        if ( BS_SLEEPING() && (bs_cmd_RUN_SYS != command) )
        {
            einkfb_print_warn("still in sleep/standby when trying to do something other than go back to run!\n");
            broadsheet_set_power_state(bs_power_state_run);
        }
        
        // Save the command block.
        //
        bs_enqueue_cmd(bs_cmd_block);

        // Send the command.
        //
        result = bs_wr_cmd(command, bs_poll_cmd(command));

        // Send the command's arguments if there are any.
        //
        if ( (EINKFB_SUCCESS == result) && bs_cmd_block->num_args )
            result = BS_WR_ARGS(bs_cmd_block->num_args, bs_cmd_block->args);

        // Send the subcommand if there is one.
        //
        if ( (EINKFB_SUCCESS == result) && bs_cmd_block->sub )
            result = bs_send_cmd(bs_cmd_block->sub);

        // Send/receive any data.
        //
        if ( EINKFB_SUCCESS == result )
        {
            u32 data_size = 0;
            u16 *data = NULL;

            // If there's any data to send/receive, say so.
            //
            if ( bs_cmd_block->data_size )
            {
                data_size = bs_cmd_block->data_size >> 1;
                data = (u16 *)bs_cmd_block->data;
            }
            else
            {
                if ( bs_cmd_type_read == type )
                {
                    data_size = BS_RD_DAT_ONE;
                    data = &bs_cmd_block->io;
                }
            }

            if ( data_size )
            {
                if ( bs_cmd_type_read == type )
                    result = bs_rd_dat(data_size, data);
                else
                    result = BS_WR_DATA(data_size, data);
            }
        }

        // The controller appears to have died!
        //
        if ( EINKFB_FAILURE == result )
        {
            // Say that we're no longer ready to accept commands.
            //
            bs_ready = false;

            // Dump out the most recent BS_CMD_Q_DEBUG commands.
            //
            einkfb_memset(bs_recent_commands_page, 0, BS_RECENT_CMDS_SIZE);

            if ( broadsheet_get_recent_commands(bs_recent_commands_page, BS_CMD_Q_DEBUG) )
                einkfb_print_crit("The last few commands sent to Broadsheet were:\n\n%s\n",
                    bs_recent_commands_page);

            // Reprime the watchdog to get us reset.
            //
            broadsheet_prime_watchdog_timer(EINKFB_DELAY_TIMER);
        }
    }

    return ( result );
}

static inline int BS_SEND_CMD(bs_cmd cmd)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = cmd;
    bs_cmd_block.type     = bs_cmd_type_write;

    return ( bs_send_cmd(&bs_cmd_block) );
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet Host Interface Command API
    #pragma mark -
    #pragma mark -- System Commands --
#endif

// See AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs_cmd/bs_cmd.cpp.

void bs_cmd_init_cmd_set(u16 arg0, u16 arg1, u16 arg2)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_INIT_CMD_SET;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_INIT_CMD_SET;
    bs_cmd_block.args[0]  = arg0;
    bs_cmd_block.args[1]  = arg1;
    bs_cmd_block.args[2]  = arg2;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_init_cmd_set_isis(u32 bc, u8 *data)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command    = bs_cmd_INIT_CMD_SET;
    bs_cmd_block.type       = bs_cmd_type_write;

    bs_cmd_block.data_size  = bc;
    bs_cmd_block.data       = data;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_init_pll_stby(u16 cfg0, u16 cfg1, u16 cfg2)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_INIT_PLL_STBY;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_INIT_PLL_STBY;
    bs_cmd_block.args[0]  = cfg0;
    bs_cmd_block.args[1]  = cfg1;
    bs_cmd_block.args[2]  = cfg2;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_run_sys(void)
{
    BS_SEND_CMD(bs_cmd_RUN_SYS);
}

void bs_cmd_stby(void)
{
    BS_SEND_CMD(bs_cmd_STBY);
}

void bs_cmd_slp(void)
{
    BS_SEND_CMD(bs_cmd_SLP);
}

void bs_cmd_init_sys_run(void)
{
    BS_SEND_CMD(bs_cmd_INIT_SYS_RUN);
}

void bs_cmd_init_sys_stby(void)
{
    BS_SEND_CMD(bs_cmd_INIT_SYS_STBY);
}

void bs_cmd_init_sdram(u16 cfg0, u16 cfg1, u16 cfg2, u16 cfg3)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_INIT_SDRAM;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_INIT_SDRAM;
    bs_cmd_block.args[0]  = cfg0;
    bs_cmd_block.args[1]  = cfg1;
    bs_cmd_block.args[2]  = cfg2;
    bs_cmd_block.args[3]  = cfg3;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_init_dspe_cfg(u16 hsize, u16 vsize, u16 sdcfg, u16 gfcfg, u16 lutidxfmt)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_INIT_DSPE_CFG;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_DSPE_CFG;
    bs_cmd_block.args[0]  = hsize;
    bs_cmd_block.args[1]  = vsize;
    bs_cmd_block.args[2]  = sdcfg;
    bs_cmd_block.args[3]  = gfcfg;
    bs_cmd_block.args[4]  = lutidxfmt;

    bs_send_cmd(&bs_cmd_block);

    bs_hsize = hsize;
    bs_vsize = vsize;
    einkfb_debug("hsize=%d vsize=%d\n", bs_hsize, bs_vsize);
}

void bs_cmd_init_dspe_tmg(u16 fs, u16 fbe, u16 ls, u16 lbe, u16 pixclkcfg)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_INIT_DSPE_TMG;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_DSPE_TMG;
    bs_cmd_block.args[0]  = fs;
    bs_cmd_block.args[1]  = fbe;
    bs_cmd_block.args[2]  = ls;
    bs_cmd_block.args[3]  = lbe;
    bs_cmd_block.args[4]  = pixclkcfg;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_set_rotmode(u16 rotmode)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_SET_ROTMODE;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_SET_ROTMODE;
    bs_cmd_block.args[0]  = (rotmode & 0x3) << 8;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_init_waveform(u16 wfdev)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_INIT_WAVEFORMDEV;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_INIT_WFDEV;
    bs_cmd_block.args[0]  = wfdev;

    bs_send_cmd(&bs_cmd_block);
}

#if PRAGMAS
    #pragma mark -- Register and Memory Access Commands --
#endif

u16  bs_cmd_rd_reg(u16 ra)
{
    bs_cmd_block_t bs_cmd_block = { 0 };
    u16 result = 0;

    bs_cmd_block.command  = bs_cmd_RD_REG;
    bs_cmd_block.type     = bs_cmd_type_read;
    bs_cmd_block.num_args = BS_CMD_ARGS_RD_REG;
    bs_cmd_block.args[0]  = ra;

    if ( EINKFB_SUCCESS == bs_send_cmd(&bs_cmd_block) )
        result = bs_cmd_block.io;

    return ( result );
}

void bs_cmd_wr_reg(u16 ra, u16 wd)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_WR_REG;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_WR_REG;
    bs_cmd_block.args[0]  = ra;
    bs_cmd_block.args[1]  = wd;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_rd_sfm(void)
{
    BS_SEND_CMD(bs_cmd_RD_SFM);
}

void bs_cmd_wr_sfm(u8 wd)
{
    bs_cmd_block_t bs_cmd_block = { 0 };
    u16 data = wd;

    bs_cmd_block.command  = bs_cmd_WR_SFM;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_WR_SFM;
    bs_cmd_block.args[0]  = data;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_end_sfm(void)
{
    BS_SEND_CMD(bs_cmd_END_SFM);
}

u16 BS_CMD_RD_REG(u16 ra)
{
    u16 result = 0;

    if ( bs_reg_valid(ra) )
        result = bs_cmd_rd_reg(ra);

    return ( result );
}

void BS_CMD_WR_REG(u16 ra, u16 wd)
{
    if ( bs_reg_valid(ra) )
    {
        bool handled = false;
        
        // We have to special-case the use of the BS_PWR_PIN_CONF_REG for VCOM
        // when there's a PMIC in use.
        //
        if ( BS_HAS_PMIC() && (BS_PWR_PIN_CONF_REG == ra) )
        {
            switch ( wd )
            {
                case BS_PWR_PIN_VCOM_ON:
                     bs_vcom_diags = false;
                     
                     bs_pmic_set_power_state(bs_pmic_power_state_active);
                     BS_VCOM_ALWAYS_ON();
                     bs_vcom_diags = true;
                     
                     handled = true;
                break;
                
                case BS_PWR_PIN_VCOM_AUTO:
                    bs_vcom_diags = false;
                    
                    bs_cmd_wr_reg(BS_PWR_PIN_CONF_REG, BS_PWR_PIN_INIT);
                    BS_VCOM_CONTROL();

                    handled = true;
                break;
            }
        }

        if ( !handled )
            bs_cmd_wr_reg(ra, wd);
    }
}

#if PRAGMAS
    #pragma mark -- Burst Access Commands --
#endif

static void bs_cmd_bst_sdr(bs_cmd cmd, u32 ma, u32 bc, u8 *data)
{
    bs_cmd_block_t  bs_cmd_block = { 0 },
                    bs_sub_block = { 0 };

    bs_cmd_block.command    = cmd;
    bs_cmd_block.type       = (bs_cmd_BST_RD_SDR == cmd) ? bs_cmd_type_read : bs_cmd_type_write;
    bs_cmd_block.num_args   = BS_CMD_ARGS_BST_SDR;
    bs_cmd_block.args[0]    = ma & 0xFFFF;
    bs_cmd_block.args[1]    = (ma >> 16) & 0xFFFF;
    bs_cmd_block.args[2]    = bc & 0xFFFF;
    bs_cmd_block.args[3]    = (bc >> 16) & 0xFFFF;

    bs_cmd_block.data_size  = bc;
    bs_cmd_block.data       = data;

    bs_cmd_block.sub        = &bs_sub_block;

    bs_sub_block.command    = bs_cmd_WR_REG;
    bs_sub_block.type       = bs_cmd_type_write;
    bs_sub_block.num_args   = BS_CMD_ARGS_WR_REG_SUB;
    bs_sub_block.args[0]    = BS_CMD_BST_SDR_WR_REG;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_bst_rd_sdr(u32 ma, u32 bc, u8 *data)
{
    bs_cmd_bst_sdr(bs_cmd_BST_RD_SDR, ma, bc, data);
}

void bs_cmd_bst_wr_sdr(u32 ma, u32 bc, u8 *data)
{
    bs_cmd_bst_sdr(bs_cmd_BST_WR_SDR, ma, bc, data);
}

void bs_cmd_bst_end(void)
{
    BS_SEND_CMD(bs_cmd_BST_END);
}

#if PRAGMAS
    #pragma mark -- Image Loading Commands --
#endif

static void bs_cmd_ld_img_which(bs_cmd cmd, u16 dfmt, u16 x, u16 y, u16 w, u16 h)
{
    bs_cmd_block_t  bs_cmd_block = { 0 },
                    bs_sub_block = { 0 };
    u16 arg = (dfmt & 0x03) << 4;

    bs_cmd_block.command  = cmd;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.args[0]  = arg;

    if ( bs_cmd_LD_IMG_AREA == cmd )
    {
        bs_cmd_block.num_args = BS_CMD_ARGS_LD_IMG_AREA;

        bs_cmd_block.args[1]  = x;
        bs_cmd_block.args[2]  = y;
        bs_cmd_block.args[3]  = w;
        bs_cmd_block.args[4]  = h;
    }
    else
        bs_cmd_block.num_args = BS_CMD_ARGS_LD_IMG;

    bs_cmd_block.sub          = &bs_sub_block;

    bs_sub_block.command      = bs_cmd_WR_REG;
    bs_sub_block.type         = bs_cmd_type_write;
    bs_sub_block.num_args     = BS_CMD_ARGS_WR_REG_SUB;
    bs_sub_block.args[0]      = BS_CMD_LD_IMG_WR_REG;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_ld_img(u16 dfmt)
{
    bs_cmd_ld_img_which(bs_cmd_LD_IMG, dfmt, 0, 0, 0, 0);
}

void bs_cmd_ld_img_area(u16 dfmt, u16 x, u16 y, u16 w, u16 h)
{
    bs_cmd_ld_img_which(bs_cmd_LD_IMG_AREA, dfmt, x, y, w, h);
}

void bs_cmd_ld_img_end(void)
{
    BS_SEND_CMD(bs_cmd_LD_IMG_END);
}

void bs_cmd_ld_img_wait(void)
{
    BS_SEND_CMD(bs_cmd_LD_IMG_WAIT);
}

#if PRAGMAS
    #pragma mark -- Polling Commands --
#endif

void bs_cmd_wait_dspe_trg(void)
{
    BS_SEND_CMD(bs_cmd_WAIT_DSPE_TRG);
}

void bs_cmd_wait_dspe_frend(void)
{
    BS_SEND_CMD(bs_cmd_WAIT_DSPE_FREND);
}

void bs_cmd_wait_dspe_lutfree(void)
{
    BS_SEND_CMD(bs_cmd_WAIT_DSPE_LUTFREE);
}

void bs_cmd_wait_dspe_mlutfree(u16 lutmsk)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_WAIT_DSPE_MLUTFREE;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_DSPE_MLUTFREE;
    bs_cmd_block.args[0]  = lutmsk;

    bs_send_cmd(&bs_cmd_block);
}

#if PRAGMAS
    #pragma mark -- Waveform Update Commands --
#endif

void bs_cmd_rd_wfm_info(u32 ma)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_RD_WFM_INFO;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_RD_WFM_INFO;
    bs_cmd_block.args[0]  = ma & 0xFFFF;
    bs_cmd_block.args[1]  = (ma >> 16) & 0xFFFF;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_upd_init(void)
{
    BS_SEND_CMD(bs_cmd_UPD_INIT);
}

static void bs_cmd_upd(bs_cmd cmd, u16 mode, u16 lutn, u16 bdrupd, u16 x, u16 y, u16 w, u16 h)
{
    bs_cmd_block_t bs_cmd_block = { 0 };
    u16 arg = ((mode & 0xF) << 8) | ((lutn & 0xF) << 4) | ((bdrupd & 0x1) << 14);

    bs_cmd_block.command  = cmd;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.args[0]  = arg;

    if ( (bs_cmd_UPD_FULL_AREA == cmd) || (bs_cmd_UPD_PART_AREA == cmd) )
    {
        bs_cmd_block.num_args = BS_CMD_UPD_AREA_ARGS;

        bs_cmd_block.args[1]  = x;
        bs_cmd_block.args[2]  = y;
        bs_cmd_block.args[3]  = w;
        bs_cmd_block.args[4]  = h;
    }
    else
        bs_cmd_block.num_args = BS_CMD_UPD_ARGS;

    bs_send_cmd(&bs_cmd_block);
}

void bs_cmd_upd_full(u16 mode, u16 lutn, u16 bdrupd)
{
    bs_cmd_upd(bs_cmd_UPD_FULL, mode, lutn, bdrupd, 0, 0, 0, 0);
}

void bs_cmd_upd_full_area(u16 mode, u16 lutn, u16 bdrupd, u16 x, u16 y, u16 w, u16 h)
{
    bs_cmd_upd(bs_cmd_UPD_FULL_AREA, mode, lutn, bdrupd, x, y, w, h);
}

void bs_cmd_upd_part(u16 mode, u16 lutn, u16 bdrupd)
{
    bs_cmd_upd(bs_cmd_UPD_PART, mode, lutn, bdrupd, 0, 0, 0, 0);
}

void bs_cmd_upd_part_area(u16 mode, u16 lutn, u16 bdrupd, u16 x, u16 y, u16 w, u16 h)
{
    bs_cmd_upd(bs_cmd_UPD_PART_AREA, mode, lutn, bdrupd, x, y, w, h);
}

void bs_cmd_upd_gdrv_clr(void)
{
    BS_SEND_CMD(bs_cmd_UPD_GDRV_CLR);
}

void bs_cmd_upd_set_imgadr(u32 ma)
{
    bs_cmd_block_t bs_cmd_block = { 0 };

    bs_cmd_block.command  = bs_cmd_UPD_SET_IMGADR;
    bs_cmd_block.type     = bs_cmd_type_write;
    bs_cmd_block.num_args = BS_CMD_ARGS_UPD_SET_IMGADR;
    bs_cmd_block.args[0]  = ma & 0xFFFF;
    bs_cmd_block.args[1]  = (ma >> 16) & 0xFFFF;

    bs_send_cmd(&bs_cmd_block);
}

#if PRAGMAS
    #pragma mark -
    #pragma mark SPI Flash Interface API
    #pragma mark -
#endif

#define BSC_RD_REG(a)       bs_cmd_rd_reg(a)
#define BSC_WR_REG(a, d)    bs_cmd_wr_reg(a, d)

struct bit_ready_t
{
    int address,
        position,
        value;
};
typedef struct bit_ready_t bit_ready_t;

static bool bs_sfm_bit_ready(void *data)
{
    bit_ready_t *br = (bit_ready_t *)data;

    return ( ((BSC_RD_REG(br->address) >> br->position) & 0x1) == (br->value & 0x1) );
}

static void bs_sfm_wait_for_bit(int ra, int pos, int val)
{
    bit_ready_t br = { ra, pos, val };
    einkfb_schedule_timeout_interruptible(BS_SFM_TIMEOUT, bs_sfm_bit_ready, (void *)&br);
}

bool bs_sfm_preflight(bool isis_override)
{
    bool preflight = true, check = BS_ISIS() ? isis_override : true;

    if ( check )
    {
        bs_sfm_start();

        switch ( bs_sfm_esig() )
        {
            case BS_SFM_ESIG_M25P10:
                bs_sfm_sector_size  = BS_SFM_SECTOR_SIZE_128K;
                bs_sfm_size         = BS_SFM_SIZE_128K;
                bs_sfm_page_count   = BS_SFM_PAGE_COUNT_128K;
                bs_tst_addr         = BS_TST_ADDR_128K;
            break;

            case BS_SFM_ESIG_M25P20:
                bs_sfm_sector_size  = BS_SFM_SECTOR_SIZE_256K;
                bs_sfm_size         = BS_SFM_SIZE_256K;
                bs_sfm_page_count   = BS_SFM_PAGE_COUNT_256K;
                bs_tst_addr         = BS_TST_ADDR_256K;
            break;

            default:
                einkfb_print_error("Unrecognized flash signature\n");
                preflight = false;
            break;
        }

        bs_sfm_end();
    }

    return ( preflight );
}

int bs_get_sfm_size(void)
{
    return ( bs_sfm_size );
}

void bs_sfm_start(void)
{
    int access_mode = 0, enable = 1, v;

    // If we're not in bootstrapping mode (where we aren't actually using
    // the display), ensure that any display transactions finish before
    // we start flashing, including any that might result from the
    // watchdog.
    //
    if ( !bs_bootstrap )
    {
        broadsheet_prime_watchdog_timer(EINKFB_EXPIRE_TIMER);

        bs_cmd_wait_dspe_trg();
        bs_cmd_wait_dspe_frend();
    }

    sfm_cd = BSC_RD_REG(0x0204);  // spi flash control reg
    BSC_WR_REG(0x0208, 0);
    BSC_WR_REG(0x0204, 0);        // disable

    v = (access_mode           << 7) |
        (SFM_READ_COMMAND      << 6) |
        (SFM_CLOCK_DIVIDE      << 3) |
        (SFM_CLOCK_PHASE       << 2) |
        (SFM_CLOCK_POLARITY    << 1) |
        (enable                << 0);
    BSC_WR_REG(0x0204, v);

    einkfb_debug_full("... staring sfm access\n");
    einkfb_debug_full("... esig=0x%02X\n", bs_sfm_esig());
}

void bs_sfm_end(void)
{
    einkfb_debug_full( "... ending sfm access\n");
    BSC_WR_REG(0x0204, sfm_cd);
}

void bs_sfm_wr_byte(int data)
{
    int v = (data & 0xFF) | 0x100;
    BSC_WR_REG(0x202, v);
    bs_sfm_wait_for_bit(0x206, 3, 0);
}

int bs_sfm_rd_byte(void)
{
    int v;

    BSC_WR_REG(0x202, 0);
    bs_sfm_wait_for_bit(0x206, 3, 0);
    v = BSC_RD_REG(0x200);
    return ( v & 0xFF );
}

int bs_sfm_esig( void )
{
    int es;

    BSC_WR_REG(0x208, 1);
    bs_sfm_wr_byte(BS_SFM_RES);
    bs_sfm_wr_byte(0);
    bs_sfm_wr_byte(0);
    bs_sfm_wr_byte(0);
    es = bs_sfm_rd_byte();
    BSC_WR_REG(0x208, 0);
    return ( es );
}

void bs_sfm_read(int addr, int size, char * data)
{
    int i;

    einkfb_debug_full( "... reading the serial flash memory (address=0x%08X, size=%d)\n", addr, size );

    BSC_WR_REG(0x0208, 1);
    bs_sfm_wr_byte(BS_SFM_READ);
    bs_sfm_wr_byte(( addr >> 16 ) & 0xFF);
    bs_sfm_wr_byte(( addr >>  8 ) & 0xFF);
    bs_sfm_wr_byte(addr & 0xFF);

    for ( i = 0; i < size; i++ )
        data[i] = bs_sfm_rd_byte();

    BSC_WR_REG(0x0208, 0);
}

static void bs_sfm_write_enable(void)
{
    BSC_WR_REG(0x0208, 1);
    bs_sfm_wr_byte(BS_SFM_WREN);
    BSC_WR_REG(0x0208, 0);
}

void bs_sfm_write_disable(void)
{
    BSC_WR_REG(0x0208, 1);
    bs_sfm_wr_byte(BS_SFM_WRDI);
    BSC_WR_REG(0x0208, 0);
}

static int bs_sfm_read_status(void)
{
    int s;

    BSC_WR_REG(0x0208, 1);
    bs_sfm_wr_byte(BS_SFM_RDSR);
    s = bs_sfm_rd_byte();
    BSC_WR_REG(0x0208, 0);

    return ( s );
}

static bool bs_sfs_read_ready(void *unused)
{
    return ( (bs_sfm_read_status() & 0x1) == 0 );
}

static void bs_sfm_erase(int addr)
{
    einkfb_debug_full( "... erasing sector (0x%08X)\n", addr);

    bs_sfm_write_enable();
    BSC_WR_REG(0x0208, 1);
    bs_sfm_wr_byte(BS_SFM_SE);
    bs_sfm_wr_byte((addr >> 16) & 0xFF);
    bs_sfm_wr_byte((addr >>  8) & 0xFF);
    bs_sfm_wr_byte( addr & 0xFF);
    BSC_WR_REG(0x0208, 0);

    EINKFB_SCHEDULE_TIMEOUT_INTERRUPTIBLE(BS_SFM_TIMEOUT, bs_sfs_read_ready);
    bs_sfm_write_disable();
}

static void bs_sfm_program_page(int pa, int size, char *data)
{
    int d;

    bs_sfm_write_enable();
    BSC_WR_REG(0x0208, 1);
    bs_sfm_wr_byte(BS_SFM_PP);
    bs_sfm_wr_byte((pa >> 16) & 0xFF);
    bs_sfm_wr_byte((pa >>  8) & 0xFF);
    bs_sfm_wr_byte(pa & 0xFF);

    for ( d = 0; d < BS_SFM_PAGE_SIZE; d++ )
        bs_sfm_wr_byte(data[d]);

    BSC_WR_REG(0x0208, 0);

    EINKFB_SCHEDULE_TIMEOUT_INTERRUPTIBLE(BS_SFM_TIMEOUT, bs_sfs_read_ready);
    bs_sfm_write_disable();
}

static void bs_sfm_program_sector(int sa, int size, char *data)
{
    int p, y, pa = sa;

    einkfb_debug_full( "... programming sector (0x%08X)\n", sa);

    for ( p = 0; p < bs_sfm_page_count; p++ )
    {
        y = p * BS_SFM_PAGE_SIZE;
        bs_sfm_program_page(pa, BS_SFM_PAGE_SIZE, &data[y]);
        pa += BS_SFM_PAGE_SIZE;
    }
}

static void BS_SFM_WRITE(int addr, int size, char *data)
{
    int s1 = addr/bs_sfm_sector_size,
        s2 = (addr + size - 1)/bs_sfm_sector_size,
        x, i, s, sa, start, limit, count;

    for ( x = 0, s = s1; s <= s2; s++ )
    {
        sa    = s * bs_sfm_sector_size;
        start = 0;
        count = bs_sfm_sector_size;

        if ( s == s1 )
        {
            if ( addr > sa )
            {
	            start = addr - sa;
	            bs_sfm_read(sa, start, sd);
            }
        }

        if ( s == s2 )
        {
            limit = addr + size;

            if ( (sa + bs_sfm_sector_size) > limit )
            {
	            count = limit - sa;
	            bs_sfm_read(limit, (sa + bs_sfm_sector_size - limit), &sd[count]);
            }
        }

        bs_sfm_erase(sa);

        for ( i = start; i < count; i++ )
            sd[i] = data[x++];

        bs_sfm_program_sector(sa, bs_sfm_sector_size, sd);
    }
}

void bs_sfm_write(int addr, int size, char *data)
{
    bool valid;
    int i;

    einkfb_debug_full( "... writing the serial flash memory (address=0x%08X, size=%d)\n", addr, size);
    einkfb_memset(sd, 0, bs_sfm_size);
    BS_SFM_WRITE(addr, size, data);

    einkfb_debug_full( "... verifying the serial flash memory write\n");
    einkfb_memset(rd, 0, bs_sfm_size);
    bs_sfm_read(addr, size, rd);

    for ( i = 0, valid = true; (i < size) && valid; i++ )
    {
        if ( rd[i] != data[i] )
        {
            einkfb_debug_full( "+++++++++++++++ rd[%d]=0x%02x  data[%d]=0x%02x\n", i, rd[i], i, data[i]);
            valid = false;
        }
    }

    einkfb_debug_full( "... writing the serial flash memory --- done\n");

    if ( !valid )
        einkfb_print_crit("Failed to verify write of Broadsheet Flash.\n");
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet Host Interface Helper API
    #pragma mark -
#endif

// See AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs_cmd/bs_cmd.h.

#define bs_cmd_bit_ready bs_sfm_bit_ready

void bs_cmd_wait_for_bit(int reg, int bitpos, int bitval)
{
    bit_ready_t br = { reg, bitpos, bitval };
    einkfb_schedule_timeout_interruptible(BS_CMD_TIMEOUT, bs_cmd_bit_ready, (void *)&br);
}

static u16 bs_cmd_bpp_to_dfmt(u32 bpp)
{
    u16 result;

    switch ( bpp )
    {
        case EINKFB_2BPP:
            result = BS_ISIS() ? BS_DFMT_2BPP_ISIS : BS_DFMT_2BPP;
        break;

        case EINKFB_4BPP:
            result = BS_DFMT_4BPP;
        break;

        case EINKFB_8BPP:
        default:
            result = BS_DFMT_8BPP;
        break;
    }

    return ( result );
}

enum bs_ld_img_data_t
{
    bs_ld_img_data_fast,
    bs_ld_img_data_slow
};
typedef enum bs_ld_img_data_t bs_ld_img_data_t;

static u16 bs_ld_img_data(u8 *data, bool upd_area, bool upd_fast, u16 x, u16 y, u16 w, u16 h)
{
    bs_ld_img_data_t bs_ld_img_data_which = bs_ld_img_data_slow;
    u16 upd_mode = 0, dfmt = 0;
    int i, data_size = 0;

    struct einkfb_info info;
    einkfb_get_info(&info);

    // Say which image-loading type we are:  Simple fill is fast (no upd_mode overrides
    // or we're using ISIS's auto-waveform upd_mode selection capabilities).  Otherwise,
    // we just use the default (slow) image-loading methodology.
    //
    if ( bs_ld_fb || upd_fast )
        bs_ld_img_data_which = bs_ld_img_data_fast;
    else
    {
        u16 override_upd_mode = (u16)broadsheet_get_override_upd_mode();

        if ( (BS_UPD_MODE_INIT == override_upd_mode) && BS_ISIS() )
            bs_ld_img_data_which = bs_ld_img_data_fast;
    }

    if ( bs_ld_fb )
    {
        data_size = BPP_SIZE((w * h), EINKFB_8BPP);
        dfmt = bs_cmd_bpp_to_dfmt(EINKFB_8BPP);
    }
    else
    {
        data_size = BPP_SIZE((w * h), info.bpp);

        switch ( info.bpp )
        {
            // In the fast case, we're sending just the data.  But, in the slow case,
            // we're stretching it to 4bpp first to normalize its processing.
            //
            case EINKFB_2BPP:
                if ( bs_ld_img_data_fast == bs_ld_img_data_which )
                    dfmt = bs_cmd_bpp_to_dfmt(EINKFB_2BPP);
                else
                    dfmt = bs_cmd_bpp_to_dfmt(EINKFB_4BPP);
            break;

            case EINKFB_4BPP:
                dfmt = bs_cmd_bpp_to_dfmt(EINKFB_4BPP);
            break;

            case EINKFB_8BPP:
                dfmt = bs_cmd_bpp_to_dfmt(EINKFB_8BPP);
            break;
        }
    }

    // Convert fast updates to area updates to reduce the amount of data we must transmit
    // to the hardware as well deal with the slop issues (i.e., 824 vs. 825) on the
    // 9.7-inch-sized displays.
    //
    if ( upd_area || upd_fast )
        bs_cmd_ld_img_area(dfmt, x, y, w, h);
    else
        bs_cmd_ld_img(dfmt);

    bs_ld_img_start = jiffies;

    switch ( bs_ld_img_data_which )
    {
        case bs_ld_img_data_fast:
            BS_WR_DATA((data_size >> 1), (u16 *)data);
            upd_mode = BS_UPD_MODE(BS_UPD_MODE_GC);
        break;

        case bs_ld_img_data_slow:
        {
            u8 white = broadsheet_pixels(info.bpp, einkfb_white(info.bpp)), *controller_data = broadsheet_get_scratchfb();
            int j, k, l = 0, m = 0, n = 0;
            unsigned char s, pixels[2];

            upd_mode = BS_INIT_UPD_MODE();
            bs_wr_one_ready();

            // Determine the hardware slop factors for non-area updates.
            //
            if ( !upd_area )
            {
                switch ( broadsheet_get_orientation() )
                {
                    case EINKFB_ORIENT_PORTRAIT:
                        if ( (BS97_INIT_VSIZE == bs_vsize) && (BS97_SOFT_VSIZE == w) )
                            m = BS97_HARD_VSIZE - BS97_SOFT_VSIZE;
                    break;

                    case EINKFB_ORIENT_LANDSCAPE:
                        if ( (BS97_INIT_VSIZE == bs_vsize) && (BS97_SOFT_VSIZE == h) )
                            n = (BS97_HARD_VSIZE - BS97_SOFT_VSIZE) * w;
                    break;
                }
            }

            // Make BPP-specific adjustments.
            //
            w = BPP_SIZE(w, info.bpp);

            switch ( info.bpp )
            {
                case EINKFB_8BPP:
                case EINKFB_4BPP:
                    l = 1;
                break;

                case EINKFB_2BPP:
                    l = 2;
                break;
            }

            for ( i = j = 0; i < data_size; )
            {
                // First, get the source pixel(s).
                //
                s = data[i];

                // Next, if they're not already at 4/8bpp, get them there.
                //
                switch ( info.bpp )
                {
                    case EINKFB_8BPP:
                    case EINKFB_4BPP:
                        pixels[0] = s;
                    break;

                    case EINKFB_2BPP:
                        pixels[0] = STRETCH_HI_NYBBLE(s, info.bpp);
                        pixels[1] = STRETCH_LO_NYBBLE(s, info.bpp);
                    break;
                }

                // Now, get the 4/8bpp pixels to their destination in the right
                // format.
                //
                for ( k = 0; k < l; k++ )
                {
                    // Get pixels.
                    //
                    s = pixels[k];

                    // Accumulate the update mode.
                    //
                    upd_mode = BS_FIND_UPD_MODE(upd_mode, s, info.bpp);

                    // Invert (4/8bpp) and/or nybble-swap (4bpp) the pixels going out to the
                    // controller on Broadsheet, but not on ISIS.
                    //
                    controller_data[j++] = BS_ISIS() ? s : (EINKFB_8BPP == info.bpp) ? ~s :
                        bs_4bpp_nybble_swap_table_inverted[s];

                    if ( 0 == (j % 2) )
                        bs_wr_one(*((u16 *)&controller_data[j-2]));
                }

                // Finally, determine what's next.
                //
                if ( 0 == (++i % w) )
                {
                    // Handle the horizontal hardware slop factor when necessary.
                    //
                    for ( k = 0; k < m; k++ )
                    {
                        controller_data[j++] = white;

                        if ( 0 == (j % 2) )
                            bs_wr_one(*((u16 *)&controller_data[j-2]));
                    }
                }

                EINKFB_SCHEDULE_BLIT(i+1);
            }

            // Handle the vertical slop factor when necessary.
            //
            for ( k = 0; k < n; k++ )
            {
                controller_data[j++] = white;

                if ( 0 == (j % 2) )
                    bs_wr_one(*((u16 *)&controller_data[j-2]));
            }

            upd_mode = BS_DONE_UPD_MODE(upd_mode);
        }
        break;
    }

    bs_cmd_ld_img_end();

    bs_debug_upd_mode(upd_mode);

    return ( upd_mode );
}

static bool bs_display_is_upside_down(void)
{
    bool result = false;

    // Displays connected to ADS and Mario are generally upside down with
    // respect to the rest of the board.
    //
    if ( IS_ADS() || IS_MARIO() )
        result = true;

    return ( result );
}

static u16 bs_cmd_orientation_to_rotmode(bool orientation, bool upside_down)
{
    u16 result = BS_ROTMODE_180;

    if ( EINKFB_ORIENT_PORTRAIT == orientation )
        result = BS_ROTMODE_90;

    // If the display is connected to the device in upside down way, address that here.
    //
    if ( bs_display_is_upside_down() )
    {
        switch ( result )
        {
            case BS_ROTMODE_180:
                result = BS_ROTMODE_0;
            break;

            case BS_ROTMODE_90:
                result = BS_ROTMODE_270;
            break;
        }
    }

    // Flip everything upside down if we're supposed to.
    //
    if ( upside_down )
    {
        switch ( result )
        {
            case BS_ROTMODE_0:
                result = BS_ROTMODE_180;
            break;

            case BS_ROTMODE_90:
                result = BS_ROTMODE_270;
            break;

            case BS_ROTMODE_180:
                result = BS_ROTMODE_0;
            break;

            case BS_ROTMODE_270:
                result = BS_ROTMODE_90;
            break;
        }
    }

    return ( result );
}

static bool bs_disp_ready(void *unused)
{
    int vsize = bs_cmd_rd_reg(0x300),
        hsize = bs_cmd_rd_reg(0x306);

    return ( (hsize == bs_hsize) && (vsize == bs_vsize) );
}

#define BS_DISP_READY() bs_disp_ready(NULL)

void bs_cmd_print_disp_timings(void)
{
    int vsize = bs_cmd_rd_reg(0x300),
        vsync = bs_cmd_rd_reg(0x302),
        vblen = bs_cmd_rd_reg(0x304),
        velen = (vblen >> 8) & 0xFF,
        hsize = bs_cmd_rd_reg(0x306),
        hsync = bs_cmd_rd_reg(0x308),
        hblen = bs_cmd_rd_reg(0x30A),
        helen = (hblen >> 8) & 0xFF;

    vblen &= 0xFF;
    hblen &= 0xFF;

    if ( !BS_DISP_READY() )
        EINKFB_SCHEDULE_TIMEOUT_INTERRUPTIBLE(BS_CMD_TIMEOUT, bs_disp_ready);

    vsize = bs_cmd_rd_reg(0x300);
    hsize = bs_cmd_rd_reg(0x306);

    einkfb_debug("disp_timings: vsize=%d vsync=%d vblen=%d velen=%d\n", vsize, vsync, vblen, velen);
    einkfb_debug("disp_timings: hsize=%d hsync=%d hblen=%d helen=%d\n", hsize, hsync, hblen, helen);
}

void bs_cmd_set_wfm(int addr)
{
    bs_cmd_rd_wfm_info(addr);
    bs_cmd_wait_dspe_trg();
}

void bs_cmd_get_wfm_info(void)
{
  u16   addr_base = BS_ISIS() ? 0x390 : 0x350,
        a = bs_cmd_rd_reg(addr_base+0x4),
        b = bs_cmd_rd_reg(addr_base+0x6),
        c = bs_cmd_rd_reg(addr_base+0x8),
        d = bs_cmd_rd_reg(addr_base+0xC),
        e = bs_cmd_rd_reg(addr_base+0xE);

  wfm_fvsn  = a & 0xFF;
  wfm_luts  = (a >> 8) & 0xFF;
  wfm_trc   = (b >> 8) & 0xFF;
  wfm_mc    = b & 0xFF;
  wfm_sb    = (c >> 8) & 0xFF;
  wfm_eb    = c & 0xFF;
  wfm_wmta  = d | (e << 16);
}

void bs_cmd_print_wfm_info(void)
{
    if ( EINKFB_DEBUG() )
    {
        bs_cmd_get_wfm_info();

        einkfb_print_info("wfm: fvsn=%d luts=%d mc=%d trc=%d eb=0x%02x sb=0x%02x wmta=%d\n",
            wfm_fvsn, wfm_luts, wfm_mc, wfm_trc, wfm_eb, wfm_sb, wfm_wmta);
    }
}

void bs_cmd_clear_gd(void)
{
    bs_cmd_upd_gdrv_clr();
    bs_cmd_wait_dspe_trg();
    bs_cmd_wait_dspe_frend();
}

u32 bs_cmd_get_sdr_img_base(void)
{
    return ( ((bs_cmd_rd_reg(BS_SDR_IMG_MSW_REG) & 0xFFFF) << 16) | (bs_cmd_rd_reg(BS_SDR_IMG_LSW_REG) & 0xFFFF) );
}

static void bs_cmd_sdr(bs_cmd cmd, u32 ma, u32 bc, u8 *data)
{
    if ( bs_cmd_BST_RD_SDR == cmd )
        bs_cmd_bst_rd_sdr(ma, bc, data);
    else
        bs_cmd_bst_wr_sdr(ma, bc, data);

    bs_cmd_bst_end();
}

void bs_cmd_rd_sdr(u32 ma, u32 bc, u8 *data)
{
    bs_cmd_sdr(bs_cmd_BST_RD_SDR, ma, bc, data);
}

void bs_cmd_wr_sdr(u32 ma, u32 bc, u8 *data)
{
    bs_cmd_sdr(bs_cmd_BST_WR_SDR, ma, bc, data);
}

int bs_cmd_get_lut_auto_sel_mode(void)
{
    int v = bs_cmd_rd_reg(0x330);
    return ( (v >> 7) & 0x1 );
}

void bs_cmd_set_lut_auto_sel_mode(int v)
{
    int d = bs_cmd_rd_reg(0x330);

    if ( v & 0x1 )
        d |= 0x80;
    else
        d &= ~0x80;

    // Prevent the ISIS display engine from resetting itself!
    //
    d &= 0x0FFF;

    bs_cmd_wr_reg(0x330, d);
}

int bs_cmd_get_wf_auto_sel_mode(void)
{
    int v = bs_cmd_rd_reg(0x330);
    return ( (v >> 6) & 0x1 );
}

void bs_cmd_set_wf_auto_sel_mode(int v)
{
    int d = bs_cmd_rd_reg(0x330);

    if ( v & 0x1 )
        d |= 0x40;
    else
        d &= ~0x40;

    // Prevent the ISIS display engine from resetting itself!
    //
    d &= 0x0FFF;

    bs_cmd_wr_reg(0x330, d);
}

void bs_cmd_bypass_vcom_enable(int v)
{
    if ( !bs_vcom_diags )
    {
        int d = bs_cmd_rd_reg(BS_PWR_PIN_CONF_REG);
        
        // We're either enabling VCOM directly ourselves,
        // or we're letting the controller handle it.
        //
        if ( v & 0x1 )
            d |=  BS_PWR_PIN_VCOM_ON;
        else
            d &= ~BS_PWR_PIN_VCOM_ON;
    
        bs_cmd_wr_reg(BS_PWR_PIN_CONF_REG, d);
    }
}

// Lab126
//
#define UPD_MODE_INIT(u)                                    \
    ((BS_UPD_MODE_INIT   == (u))   ||                       \
     (BS_UPD_MODE_REINIT == (u)))

#define IMAGE_TIMING_STRT_TYPE  0
#define IMAGE_TIMING_PROC_TYPE  1
#define IMAGE_TIMING_LOAD_TYPE  2
#define IMAGE_TIMING_DISP_TYPE  3
#define IMAGE_TIMING_STOP_TYPE  4

#define IMAGE_TIMING            "image_timing"
#define IMAGE_TIMING_STRT_NAME  "strt"
#define IMAGE_TIMING_PROC_NAME  "proc"
#define IMAGE_TIMING_LOAD_NAME  "load"
#define IMAGE_TIMING_DISP_NAME  "disp"

static void einkfb_print_image_timing(unsigned long time, int which)
{
    char *name = NULL;

    switch ( which )
    {
        case IMAGE_TIMING_STRT_TYPE:
            name = IMAGE_TIMING_STRT_NAME;
        goto relative_common;

        case IMAGE_TIMING_PROC_TYPE:
            name =  IMAGE_TIMING_PROC_NAME;
        goto relative_common;

        case IMAGE_TIMING_LOAD_TYPE:
            name =  IMAGE_TIMING_LOAD_NAME;
        goto relative_common;

        case IMAGE_TIMING_DISP_TYPE:
            name =  IMAGE_TIMING_DISP_NAME;
        /* goto relative_common; */

        relative_common:
            EINKFB_PRINT_PERF_REL(IMAGE_TIMING, time, name);
        break;

        case IMAGE_TIMING_STOP_TYPE:
            EINKFB_PRINT_PERF_ABS(IMAGE_TIMING, time, bs_upd_mode_string);
        break;
    }
}

static void bs_cmd_ld_img_upd_data_which(bs_cmd cmd, fx_type update_mode, u8 *data, u16 x, u16 y, u16 w, u16 h)
{
    if ( data )
    {
        int     saved_override_upd_mode = broadsheet_get_override_upd_mode();
        bool    upd_area = (bs_cmd_LD_IMG_AREA == cmd),
                skip_buffer_display = false,
                skip_buffer_load = false,
                wait_dspe_frend = false,
                upd_fast = false;
        
        u16     upd_mode = bs_upd_mode;
        fx_type local_update_mode;

        struct  einkfb_info info;
        einkfb_get_info(&info);

        // Set up to process, load, and/or display the image data.
        //
        bs_set_ld_img_start = jiffies;

        switch ( update_mode )
        {
            // Just load up the hardware's buffer; don't display it.
            //
            case fx_buffer_load:
                local_update_mode = fx_update_partial;
                skip_buffer_display = true;
            break;

            // Just display what's already in the hardware's buffer.
            //
            case fx_buffer_display_partial:
            case fx_buffer_display_full:
                skip_buffer_load = true;
            goto set_update_mode;

            // Regardless of what gets put into the hardware's buffer,
            // only update the black and white pixels.
            //
            case fx_update_fast:
                broadsheet_set_override_upd_mode(BS_UPD_MODE_PU);
                
                // Always use the fast-load path on ISIS.
                //
                if ( BS_ISIS() )
                    upd_fast = true;
                else
                { 
                    // But, on Broadsheet, we'll only allow the fast-load path
                    // in 4bpp and 8bpp mode (2bpp is a compatibility mode,
                    // and we always stretch that up).
                    //
                    switch ( info.bpp )
                    {
                        case EINKFB_4BPP:
                        case EINKFB_8BPP:
                            upd_fast = true;
                        break;
                    }
                }
                
                
            goto set_update_mode;
            
            // Regardless of what gets put into the hardware's buffer,
            // refresh all pixels as cleanly as possible.
            //
            case fx_update_slow:
                broadsheet_set_override_upd_mode(BS_UPD_MODE_GC);
            /* goto set_update_mode; */

            set_update_mode:
            default:
                local_update_mode = upd_area ? UPDATE_AREA_MODE(update_mode)
                                             : UPDATE_MODE(update_mode);
            break;
        }

        // Process and load the image data if we should.
        //
        if ( !skip_buffer_load )
        {
            if ( upd_area )
            {
                // On area-updates, stall the LUT pipeline only on flashing updates.
                //
                if ( UPDATE_AREA_FULL(local_update_mode) )
                    bs_cmd_wait_dspe_frend();
            }
            else
            {
                // Always stall the LUT pipeline on full-screen updates.
                //
                bs_cmd_wait_dspe_frend();
            }

            // Load the image data into the controller, determining what the non-flashing
            // upd_mode would be.
            //
            bs_upd_mode = bs_ld_img_data(data, upd_area, upd_fast, x, y, w, h);
        }

        // Update the display in the specified way (upd_mode || bs_upd_mode) if we should.
        //
        bs_upd_data_start = jiffies;

        if ( !skip_buffer_display )
        {
            int  temp = broadsheet_get_temperature();
            bool set_temp = false;
            
            // Check to see whether we need to externally drive the temperature and
            // manually drive VCOM.
            //
            if ( BS_HAS_PMIC() )
            {
                bs_pmic_set_power_state(bs_pmic_power_state_active);
                set_temp = true;
                
                // Check to see whether we should get the temperature from Papyrus or not.
                //
                if ( !IN_RANGE(temp, BS_TEMP_MIN, BS_TEMP_MAX) )
                {
                    int  temp_batt = get_battery_temperature();
                    bool temp_batt_in_use = false;
                    
                    // Papyrus has an issue where it can potentially get stuck returning
                    // a temperature that's outside both the acceptable as well the
                    // ideal temperature range.
                    //
                    temp = BS_GET_TEMP_CACHED();
                    
                    // To get around the problem where Papyrus is stuck returning a
                    // bogus (-10C) temperature...
                    //
                    if ( BS_TEMP_STUCK == temp )
                    {
                        // ...use the battery's temperature if it's in range.
                        //
                        // Notes: Only log the bogus Papyrus temperature on
                        //        flashing updates to keep the logging traffic
                        //        to a relative minimum.
                        //
                        //        If we're clipping the temperature and the
                        //        temperature that the battery returns needs
                        //        to be clipped, that will still happen.
                        //
                        //        If the battery's temperature is also out
                        //        of range, we'll just leave things alone:
                        //        maybe it really is very cold!
                        //
                        //        We do this for both non-flashing and flashing
                        //        updates since Papyrus is presumably stuck
                        //        returning a bogus value.
                        //
                        if ( fx_update_full == local_update_mode )
                        {
                            einkfb_print_warn(PMIC_TEMP_FORMAT_WARN_C_m_M, temp, BS_TEMP_MIN_IDEAL,
                                BS_TEMP_MAX_IDEAL);
                        }
                        
                        if ( IN_RANGE(temp_batt, BS_TEMP_MIN, BS_TEMP_MAX) )
                        {
                            temp_batt_in_use = true;
                            temp = temp_batt;
                        }
                    }
                    
                    if ( bs_clip_temp && !IN_RANGE(temp, BS_TEMP_MIN_IDEAL, BS_TEMP_MAX_IDEAL) )
                    {
                        // Only warn/clip that we're outside of the ideal temperature range when we
                        // get a flashing (page-turn-like) update.
                        //
                        if ( fx_update_full == local_update_mode )
                        {
                            log_battery_temperature();
                            
                            einkfb_print_warn(PMIC_TEMP_FORMAT_WARN_C_m_M, temp, BS_TEMP_MIN_IDEAL,
                                BS_TEMP_MAX_IDEAL);

                            // Papyrus is showing that we're outside the ideal temperature range.  If
                            // it's hotter than the ideal temperature range, we're going to clip it.
                            //
                            if ( BS_TEMP_MAX_IDEAL < temp )
                            {
                                // If the battery temperature is in the ideal range...
                                //
                                if ( IN_RANGE(temp_batt, BS_TEMP_MIN_IDEAL, BS_TEMP_MAX_IDEAL) )
                                {
                                    // ...and it's less than what Papyrus is showing, use it.
                                    //
                                    if ( BS_TEMP_MAX_IDEAL > temp_batt )
                                    {
                                        einkfb_print_warn(BATTERY_TEMP_FORMAT_C, temp_batt);
                                        temp = temp_batt;
                                    }
                                }
                                
                                // Clip it to max if necessary.
                                //
                                if ( BS_TEMP_MAX_IDEAL < temp )
                                {
                                    einkfb_print_warn(CLIPPED_TEMP_FORMAT_C, BS_TEMP_MAX_IDEAL);
                                    temp = BS_TEMP_MAX_IDEAL;
                                }
                            }
                        }
                    }
                    else
                    {
                        char *which = temp_batt_in_use ? TEMP_WHICH_C_BATT : TEMP_WHICH_C_PMIC;
                        
                        // We now always want to log the temperature on flashing (page-turn like)
                        // updates.
                        //
                        // Note:  If we substituted the battery's temperature for that of 
                        //        Papyrus because Papyrus is stuck returning a bogus one,
                        //        we'll see both the bogus tempature and the substitute
                        //        in the log.
                        //
                        if ( fx_update_full == local_update_mode )
                        {
                            einkfb_print_info(TEMP_FORMAT_C_WHICH, temp, which);
                        }
                        else
                            einkfb_debug(TEMP_FORMAT_C_WHICH, temp, which);
                    }
                }
                else
                {
                    if ( fx_update_full == local_update_mode )
                    {
                        einkfb_print_info(OVERRIDE_TEMP_FORMAT_C, temp);
                    }
                    else
                        einkfb_debug(OVERRIDE_TEMP_FORMAT_C, temp);
                }

                BS_VCOM_ALWAYS_ON();
            }
            else
            {
                // Check to see whether we're manually overriding the temperature.
                //
                if ( IN_RANGE(temp, BS_TEMP_MIN, BS_TEMP_MAX) )
                {
                    bs_cmd_wr_reg(BS_TEMP_DEV_SELECT_REG, BS_TEMP_DEV_EXT);
                    einkfb_debug(OVERRIDE_TEMP_FORMAT_C, temp);
                    set_temp = true;
                }
                else
                    bs_cmd_wr_reg(BS_TEMP_DEV_SELECT_REG, BS_TEMP_DEV_INT);
            }
            
            if ( set_temp )
                bs_cmd_wr_reg(BS_TEMP_VALUE_REG, (u16)temp);
            
            // Set up to display the loaded image data.
            //
            if ( upd_area )
            {
                if ( !UPDATE_AREA_FULL(local_update_mode) )
                {
                    // On non-flashing updates, start accumulating the area that will need
                    // repair when we're not stalling the LUT pipeline (i.e., Broadsheet
                    // will be hitting the panel faster than the panel can respond).
                    //
                    if ( 0 == bs_upd_repair_count )
                    {
                        bs_upd_repair_x1 = x;
                        bs_upd_repair_y1 = y;
                        bs_upd_repair_x2 = x + w;
                        bs_upd_repair_y2 = y + h;

                        bs_upd_repair_mode = bs_upd_mode;
                        bs_upd_repair_skipped = false;
                    }
                    else
                    {
                        bs_upd_repair_x1   = min(x, bs_upd_repair_x1);
                        bs_upd_repair_y1   = min(y, bs_upd_repair_y1);
                        bs_upd_repair_x2   = max((u16)(x + w), bs_upd_repair_x2);
                        bs_upd_repair_y2   = max((u16)(y + h), bs_upd_repair_y2);

                        // This update mode doesn't take into account the accumulated
                        // area.  We'll address that in the repair.
                        //
                        bs_upd_repair_mode = bs_upd_mode_max(bs_upd_mode, bs_upd_repair_mode);
                    }

                    // Indicate that we'll be needing a repair.
                    //
                    bs_upd_repair_count++;
                }
            }
            else
            {
                // Don't do the update repair since, in effect, we'll be repairing it now.
                //
                bs_upd_repair_count = 0;
            }

            broadsheet_prime_watchdog_timer(bs_upd_repair_count ? EINKFB_DELAY_TIMER : EINKFB_EXPIRE_TIMER);

            if ( UPD_MODE_INIT(upd_mode) )
            {
                bs_cmd_wait_dspe_trg();
                bs_cmd_wait_dspe_frend();

                bs_cmd_upd_init();
                bs_cmd_wait_dspe_trg();

                switch ( upd_mode )
                {
                    case BS_UPD_MODE_REINIT:
                        bs_cmd_upd_full(BS_UPD_MODE(BS_UPD_MODE_GC), 0, 0);
                    break;

                    case BS_UPD_MODE_INIT:
                        bs_cmd_upd_full(BS_UPD_MODE(BS_UPD_MODE_INIT), 0, 0);
                    break;
                }

                wait_dspe_frend = true;
            }
            else
            {
                bool promote_flashing_updates = broadsheet_get_promote_flashing_updates(),
                     flashing_update = false;
                u16  override_upd_mode = skip_buffer_load ? BS_UPD_MODE_GC
                                                          : (u16)broadsheet_get_override_upd_mode();

                // Promote flashing updates to GC if requested.
                //
                if ( promote_flashing_updates )
                {
                    if ( upd_area )
                        flashing_update = UPDATE_AREA_FULL(local_update_mode);
                    else
                        flashing_update = UPDATE_FULL(local_update_mode);

                    if ( flashing_update )
                    {
                        bs_upd_mode = BS_UPD_MODE(BS_UPD_MODE_GC);

                        // Promote all of ISIS's auto-waveform updates to use GC16s.
                        //
                        if ( BS_ISIS() )
                        {
                            bs_cmd_wr_reg(BS_AUTO_WF_REG_DU,   BS_AUTO_WF_MODE_GC16);
                            bs_cmd_wr_reg(BS_AUTO_WF_REG_GC4,  BS_AUTO_WF_MODE_GC16);
                            bs_cmd_wr_reg(BS_AUTO_WF_REG_GC16, BS_AUTO_WF_MODE_GC16);
                        }

                        einkfb_debug("promoting upd_mode\n");
                        bs_debug_upd_mode(bs_upd_mode);
                    }
                }

                // Override the upd_mode if requested/required.
                //
                if ( BS_UPD_MODE_INIT != override_upd_mode )
                {
                    bs_upd_repair_mode = bs_upd_mode = BS_UPD_MODE(override_upd_mode);

                    if ( BS_ISIS() )
                        bs_cmd_set_wf_auto_sel_mode(0);

                    einkfb_debug("overriding upd_mode\n");
                    bs_debug_upd_mode(bs_upd_mode);
                }
                else
                {
                    if ( BS_ISIS() )
                        bs_cmd_set_wf_auto_sel_mode(1);
                }

                if ( upd_area )
                {
                    if ( UPDATE_AREA_FULL(local_update_mode) )
                    {
                        bs_cmd_upd_full_area(bs_upd_mode, 0, 0, x, y, w, h);
                        wait_dspe_frend = true;
                    }
                    else
                        bs_cmd_upd_part_area(bs_upd_repair_mode, 0, 0,
                            bs_upd_repair_x1, bs_upd_repair_y1,
                            (bs_upd_repair_x2 - bs_upd_repair_x1),
                            (bs_upd_repair_y2 - bs_upd_repair_y1));
                }
                else
                {
                    if ( UPDATE_FULL(local_update_mode) )
                    {
                        bs_cmd_upd_full(bs_upd_mode, 0, 0);
                        wait_dspe_frend = true;
                    }
                    else
                        bs_cmd_upd_part(bs_upd_mode, 0, 0);
                }

                // Restore the normal auto-waveform update functionality back to ISIS.
                //
                if ( promote_flashing_updates && flashing_update && BS_ISIS() )
                {
                    bs_cmd_wr_reg(BS_AUTO_WF_REG_DU,   BS_AUTO_WF_MODE_DU);
                    bs_cmd_wr_reg(BS_AUTO_WF_REG_GC4,  BS_AUTO_WF_MODE_GC4);
                    bs_cmd_wr_reg(BS_AUTO_WF_REG_GC16, BS_AUTO_WF_MODE_GC16);
                }
            }

            // Done.
            //
            bs_cmd_wait_dspe_trg();
            
            if ( BS_HAS_PMIC() )
                BS_VCOM_CONTROL();
            
            // Get the actual last update mode used (since it might have been overridden and/or
            // generated automatically).
            //
            bs_upd_mode = BS_LAST_WF_USED(BS_CMD_RD_REG(BS_UPD_BUF_CFG_REG));
            einkfb_debug("actual upd_mode\n");
            bs_debug_upd_mode(bs_upd_mode);

            // Stall the LUT pipeline in all full-refresh situations (i.e., prevent any potential
            // partial-refresh parallel updates from stomping on the full-refresh we just
            // initiated).
            //
            if ( wait_dspe_frend )
                bs_cmd_wait_dspe_frend();
         }

        // Say how long it took to perform various image-related operations.
        //
        bs_image_stop_time = jiffies;

        bs_image_start_time      = jiffies_to_msecs(bs_set_ld_img_start - info.jif_on);
        bs_image_processing_time = jiffies_to_msecs(bs_ld_img_start     - bs_set_ld_img_start);
        bs_image_loading_time    = jiffies_to_msecs(bs_upd_data_start   - bs_ld_img_start);
        bs_image_display_time    = jiffies_to_msecs(bs_image_stop_time  - bs_upd_data_start);
        bs_image_stop_time       = jiffies_to_msecs(bs_image_stop_time  - info.jif_on);

        if ( EINKFB_PERF() )
        {
            bs_debug_upd_mode(bs_upd_mode);

            einkfb_print_image_timing(bs_image_start_time,      IMAGE_TIMING_STRT_TYPE);
            einkfb_print_image_timing(bs_image_processing_time, IMAGE_TIMING_PROC_TYPE);
            einkfb_print_image_timing(bs_image_loading_time,    IMAGE_TIMING_LOAD_TYPE);
            einkfb_print_image_timing(bs_image_display_time,    IMAGE_TIMING_DISP_TYPE);
            einkfb_print_image_timing(bs_image_stop_time,       IMAGE_TIMING_STOP_TYPE);
        }

        broadsheet_set_override_upd_mode(saved_override_upd_mode);
        bs_upd_mode = BS_UPD_MODE(BS_UPD_MODE_GC);
    }
}

void bs_clear_phys_addr(void)
{
    bs_phys_addr = 0;
}

static void bs_set_phys_addr(bs_cmd cmd, u16 w, u16 h, bool restore)
{
    // Only set the physical address if DMA is needed.
    //
    bs_clear_phys_addr();

    if ( broadsheet_needs_dma() )
    {
        bool upd_area = bs_cmd_LD_IMG_AREA == cmd;

        struct einkfb_info info;
        einkfb_get_info(&info);

        // Area updates come from the eInk HAL's scratch buffer.
        //
        if ( upd_area )
            bs_phys_addr = info.phys->addr + EINKFB_PHYS_BUF_OFFSET(info);
        else
        {
            // Otherwise, we're using Broadsheet's scratch buffer...
            //
            if ( bs_ld_fb )
                bs_phys_addr = broadsheet_get_scratchfb_phys();
            else
            {
                // ...or we're using one of the eInk HAL's buffers (real or virtual).
                //
                if ( EINKFB_RESTORE(info) || restore )
                    bs_phys_addr = info.phys->addr + EINKFB_PHYS_VFB_OFFSET(info);
                else
                    bs_phys_addr = info.phys->addr;
            }
        }

        if ( EINKFB_DEBUG() )
        {
            int data_size = 0;

            if ( bs_ld_fb )
                data_size = BPP_SIZE((w * h), EINKFB_8BPP);
            else
                data_size = BPP_SIZE((w * h), info.bpp);

            einkfb_debug("phys_addr = 0x%08X, data_size = %d\n", bs_phys_addr, data_size);
        }
    }
}

dma_addr_t bs_get_phys_addr(void)
{
    return ( bs_phys_addr );
}

void bs_cmd_ld_img_upd_data(fx_type update_mode, bool restore)
{
    struct einkfb_info info;
    u8 *buffer;

    einkfb_get_info(&info);

    if ( restore )
        buffer = info.vfb;
    else
        buffer = info.start;

    bs_set_phys_addr(bs_cmd_LD_IMG, info.xres, info.yres, restore);
    bs_cmd_ld_img_upd_data_which(bs_cmd_LD_IMG, update_mode, buffer, 0, 0, info.xres, info.yres);
}

void BS_CMD_LD_IMG_UPD_DATA(fx_type update_mode)
{
    bs_cmd_ld_img_upd_data(update_mode, UPD_DATA_NORMAL);
}

void bs_cmd_ld_img_area_upd_data(u8 *data, fx_type update_mode, u16 x, u16 y, u16 w, u16 h)
{
    bs_cmd cmd = bs_ld_fb ? bs_cmd_LD_IMG : bs_cmd_LD_IMG_AREA;

    bs_set_phys_addr(cmd, w, h, UPD_DATA_NORMAL);
    bs_cmd_ld_img_upd_data_which(cmd, update_mode, data, x, y, w, h);
}

static u16 bs_cmd_upd_repair_get_upd_mode(int xstart, int xend, int ystart, int yend)
{
    u16 override_upd_mode = (u16)broadsheet_get_override_upd_mode(),
        upd_mode = BS_INIT_UPD_MODE();

    if ( BS_UPD_MODE_INIT != override_upd_mode )
    {
        upd_mode = BS_UPD_MODE(override_upd_mode);

        if ( BS_ISIS() )
            bs_cmd_set_wf_auto_sel_mode(0);

        einkfb_debug("overriding upd_mode\n");
        bs_debug_upd_mode(upd_mode);
    }
    else
    {
        // On ISIS, we're using its auto-waveform update selection capability.
        //
        if ( BS_ISIS() )
        {
            upd_mode = BS_UPD_MODE(BS_UPD_MODE_GC);
            bs_cmd_set_wf_auto_sel_mode(1);
        }
        else
        {
            int x, y, rowbytes, bytes;
            u8 pixels;

            struct einkfb_info info;
            einkfb_get_info(&info);

            // Make bpp-related adjustments.
            //
            xstart   = BPP_SIZE(xstart,    info.bpp);
            xend     = BPP_SIZE(xend,	   info.bpp);
            rowbytes = BPP_SIZE(info.xres, info.bpp);

            // Check EINKFB_MEMCPY_MIN bytes at a time before yielding.
            //
            for (bytes = 0, y = ystart; y < yend; y++ )
            {
                for ( x = xstart; x < xend; x++ )
                {
                    pixels = info.vfb[(rowbytes * y) + x];

                    // We only support 2bpp and 4bpp, but we always transmit the buffer out to
                    // Broadsheet in 4bpp (16-levels of gray) mode.
                    //
                    if ( EINKFB_2BPP == info.bpp )
                    {
                        upd_mode = BS_FIND_UPD_MODE(upd_mode, STRETCH_HI_NYBBLE(pixels, EINKFB_2BPP), EINKFB_2BPP);
                        upd_mode = BS_FIND_UPD_MODE(upd_mode, STRETCH_LO_NYBBLE(pixels, EINKFB_2BPP), EINKFB_2BPP);
                    }
                    else
                        upd_mode = BS_FIND_UPD_MODE(upd_mode, pixels, EINKFB_4BPP);

                    if ( BS_TEST_UPD_MODE(upd_mode, info.bpp) )
                        goto done;

                    EINKFB_SCHEDULE_BLIT(++bytes);
                }
            }

            done:
                upd_mode = BS_DONE_UPD_MODE(upd_mode);
                bs_debug_upd_mode(upd_mode);
        }
    }

    return ( upd_mode );
}

void bs_cmd_upd_repair(void)
{
    // Give any non-composite updates an opportunity to become composite.
    //
    if ( (1 < bs_upd_repair_count) || ((0 != bs_upd_repair_count) && bs_upd_repair_skipped) )
    {
        u16 w = bs_upd_repair_x2 - bs_upd_repair_x1,
            h = bs_upd_repair_y2 - bs_upd_repair_y1,
            upd_mode = bs_upd_repair_mode;

        einkfb_debug("Repairing %d parallel-update(s):\n", bs_upd_repair_count);
        einkfb_debug(" x1 = %d\n", bs_upd_repair_x1);
        einkfb_debug(" y1 = %d\n", bs_upd_repair_y1);
        einkfb_debug(" x2 = %d\n", bs_upd_repair_x2);
        einkfb_debug(" y2 = %d\n", bs_upd_repair_y2);

        // Determine the upd_mode for the area we're about to repair if the
        // repair is composite.
        //
        if ( 1 < bs_upd_repair_count )
            upd_mode = bs_cmd_upd_repair_get_upd_mode(bs_upd_repair_x1, bs_upd_repair_x2,
                bs_upd_repair_y1, bs_upd_repair_y2);

        // Perform the repair.
        //
        if ( BS_HAS_PMIC() )
        {
            bs_pmic_set_power_state(bs_pmic_power_state_active);
            BS_VCOM_ALWAYS_ON();
        }
            
        bs_cmd_wait_dspe_frend();

        bs_cmd_upd_part_area(upd_mode, 0, 0, bs_upd_repair_x1, bs_upd_repair_y1,
            w, h);

        bs_cmd_wait_dspe_trg();
        
        if ( BS_HAS_PMIC() )
            BS_VCOM_CONTROL();

        einkfb_debug("actual upd_mode\n");
        bs_debug_upd_mode(BS_LAST_WF_USED(BS_CMD_RD_REG(BS_UPD_BUF_CFG_REG)));

        // Done.
        //
        bs_upd_repair_skipped = false;
        bs_upd_repair_count = 0;
    }
    else
    {
        // Prime the timer again and note that we skipped this repair.
        //
        broadsheet_prime_watchdog_timer(EINKFB_DELAY_TIMER);
        bs_upd_repair_skipped = true;
    }
}

static void bs_ld_value(u8 v, u16 hsize, u16 vsize, fx_type update_mode)
{
    struct einkfb_info info;
    u8 u;

    // Adjust for polarity.
    //
    einkfb_get_info(&info);

    switch ( v )
    {
        case EINKFB_WHITE:
            v = einkfb_white(info.bpp);
        break;

        case EINKFB_BLACK:
            v = einkfb_black(info.bpp);
        break;
    }

    u = broadsheet_pixels(info.bpp, v);

    // Adjust for hardware slop.
    //
    if ( BS97_INIT_VSIZE == bs_vsize )
        vsize = BS97_HARD_VSIZE;

    bs_ld_fb = broadsheet_get_scratchfb();
    einkfb_memset(bs_ld_fb, u, broadsheet_get_scratchfb_size());

    bs_cmd_ld_img_area_upd_data(bs_ld_fb, update_mode, 0, 0, hsize, vsize);
    bs_ld_fb = NULL;
}

unsigned long *bs_get_img_timings(int *num_timings)
{
    unsigned long *timings = NULL;

    if ( num_timings )
    {
        bs_image_timings[BS_IMAGE_TIMING_START] = bs_image_start_time;
        bs_image_timings[BS_IMAGE_TIMING_PROC]  = bs_image_processing_time;
        bs_image_timings[BS_IMAGE_TIMING_LOAD]  = bs_image_loading_time;
        bs_image_timings[BS_IMAGE_TIMING_DISP]  = bs_image_display_time;
        bs_image_timings[BS_IMAGE_TIMING_STOP]  = bs_image_stop_time;

        *num_timings = BS_NUM_IMAGE_TIMINGS;
        timings = bs_image_timings;
    }

    return ( timings );
}

void bs_set_ib_addr(u32 iba)
{
    bs_cmd_wr_reg(0x310, ((iba) & 0xFFFF));
    bs_cmd_wr_reg(0x312, (((iba) >> 16) & 0xFFFF));
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet API
    #pragma mark -
#endif

static bs_panel_init_t bs_panel_init_table[num_bs_panels] =
{
    // bs_panel_60_26_50
    //
    {
        BS60_INIT_HSIZE,        BS60_INIT_VSIZE,
        BS60_INIT_FSLEN_26_50,  BS60_INIT_FBLEN_26_50,  BS60_INIT_FELEN_26_50,
        BS60_INIT_LSLEN_26_50,  BS60_INIT_LBLEN_26_50,  BS60_INIT_LELEN_26_50,
        BS60_INIT_PXCKD_26_50
     },

    // bs_panel_60_26_85
    //
    {
        BS60_INIT_HSIZE,        BS60_INIT_VSIZE,
        BS60_INIT_FSLEN_26_85,  BS60_INIT_FBLEN_26_85,  BS60_INIT_FELEN_26_85,
        BS60_INIT_LSLEN_26_85,  BS60_INIT_LBLEN_26_85,  BS60_INIT_LELEN_26_85,
        BS60_INIT_PXCKD_26_85
     },
     
     // bs_panel_60_24_50
     //
     {
        BS60_INIT_HSIZE,        BS60_INIT_VSIZE,
        BS60_INIT_FSLEN_24_50,  BS60_INIT_FBLEN_24_50, BS60_INIT_FELEN_24_50,
        BS60_INIT_LSLEN_24_50,  BS60_INIT_LBLEN_24_50, BS60_INIT_LELEN_24_50,
        BS60_INIT_PXCKD_24_50
     },
     
     // bs_panel_60_24_85
     //
     {
        BS60_INIT_HSIZE,        BS60_INIT_VSIZE,
        BS60_INIT_FSLEN_24_85,  BS60_INIT_FBLEN_24_85, BS60_INIT_FELEN_24_85,
        BS60_INIT_LSLEN_24_85,  BS60_INIT_LBLEN_24_85, BS60_INIT_LELEN_24_85,
        BS60_INIT_PXCKD_24_85
     },
     
     // bs_panel_97_26_50
     //
     {
        BS97_INIT_HSIZE,        BS97_INIT_VSIZE,
        BS97_INIT_FSLEN_26_50,  BS97_INIT_FBLEN_26_50,  BS97_INIT_FELEN_26_50,
        BS97_INIT_LSLEN_26_50,  BS97_INIT_LBLEN_26_50,  BS97_INIT_LELEN_26_50,
        BS97_INIT_PXCKD_26_50
     },
     
     // bs_panel_97_26_85
     //
     {
        BS97_INIT_HSIZE,        BS97_INIT_VSIZE,
        BS97_INIT_FSLEN_26_85,  BS97_INIT_FBLEN_26_85,  BS97_INIT_FELEN_26_85,
        BS97_INIT_LSLEN_26_85,  BS97_INIT_LBLEN_26_85,  BS97_INIT_LELEN_26_85,
        BS97_INIT_PXCKD_26_85
     },
     
     // bs_panel_97_24_50
     //
     {
        BS97_INIT_HSIZE,        BS97_INIT_VSIZE,
        BS97_INIT_FSLEN_24_50,  BS97_INIT_FBLEN_24_50, BS97_INIT_FELEN_24_50,
        BS97_INIT_LSLEN_24_50,  BS97_INIT_LBLEN_24_50, BS97_INIT_LELEN_24_50,
        BS97_INIT_PXCKD_24_50
     },

     // bs_panel_97_24_85
     //
     {
        BS97_INIT_HSIZE,        BS97_INIT_VSIZE,
        BS97_INIT_FSLEN_24_85,  BS97_INIT_FBLEN_24_85, BS97_INIT_FELEN_24_85,
        BS97_INIT_LSLEN_24_85,  BS97_INIT_LBLEN_24_85, BS97_INIT_LELEN_24_85,
        BS97_INIT_PXCKD_24_85
     },
     
     // bs_panel_97_26_60
     //
     {
        BS97_INIT_HSIZE,        BS97_INIT_VSIZE,
        BS97_INIT_FSLEN_26_60,  BS97_INIT_FBLEN_26_60, BS97_INIT_FELEN_26_60,
        BS97_INIT_LSLEN_26_60,  BS97_INIT_LBLEN_26_60, BS97_INIT_LELEN_26_60,
        BS97_INIT_PXCKD_26_60
     },
     
     // bs_panel_97_24_60
     //
     {
        BS97_INIT_HSIZE,        BS97_INIT_VSIZE,
        BS97_INIT_FSLEN_24_60,  BS97_INIT_FBLEN_24_60, BS97_INIT_FELEN_24_60,
        BS97_INIT_LSLEN_24_60,  BS97_INIT_LBLEN_24_60, BS97_INIT_LELEN_24_60,
        BS97_INIT_PXCKD_24_60
     },
     
     // bs_panel_99_26_50
     //
     {
        BS99_INIT_HSIZE,        BS99_INIT_VSIZE,
        BS99_INIT_FSLEN_26_50,  BS99_INIT_FBLEN_26_50,  BS99_INIT_FELEN_26_50,
        BS99_INIT_LSLEN_26_50,  BS99_INIT_LBLEN_26_50,  BS99_INIT_LELEN_26_50,
        BS99_INIT_PXCKD_26_50
     },
     
     // bs_panel_99_24_50
     //
     {
        BS99_INIT_HSIZE,        BS99_INIT_VSIZE,
        BS99_INIT_FSLEN_24_50,  BS99_INIT_FBLEN_24_50,  BS99_INIT_FELEN_24_50,
        BS99_INIT_LSLEN_24_50,  BS99_INIT_LBLEN_24_50,  BS99_INIT_LELEN_24_50,
        BS99_INIT_PXCKD_24_50
     }     
};

static char *bs_panel_init_info[num_bs_panels] =
{
    "6.0-inch panels (26MHz input, 50Hz output)", // bs_panel_60_26_50
    "6.0-inch panels (26MHz input, 85Hz output)", // bs_panel_60_26_85
    "6.0-inch panels (24MHz input, 50Hz output)", // bs_panel_60_24_50
    "6.0-inch panels (24MHz input, 85Hz output)", // bs_panel_60_24_85
    
    "9.7-inch panels (26MHz input, 50Hz output)", // bs_panel_97_26_50
    "9.7-inch panels (26MHz input, 85Hz output)", // bs_panel_97_26_85
    "9.7-inch panels (24MHz input, 50Hz output)", // bs_panel_97_24_50
    "9.7-inch panels (24MHz input, 85Hz output)", // bs_panel_97_24_85
    
    "9.7-inch panels (26MHz input, 60Hz output)", // bs_panel_97_26_60
    "9.7-inch panels (24MHz input, 60Hz output)", // bs_panel_97_24_60
    
    "9.7-inch HR panels (1600x1200, 26MHz/50Hz)", // bs_panel_99_26_50
    "9.7-inch HR panels (1600x1200, 24MHz/50Hz)"  // bs_panel_99_24_50    
};

#define BS_PANEL_INIT_60_BASE   bs_panel_60_26_50
#define BS_PANEL_INIT_97_BASE   bs_panel_97_26_50

#define BS_PANEL_INIT_26_BASE   (bs_panel_60_26_50 - bs_panel_60_26_50)
#define BS_PANEL_INIT_24_BASE   (bs_panel_60_24_50 - bs_panel_60_26_50)

#define BS_PANEL_INIT_50_BASE   (bs_panel_60_26_50 - bs_panel_60_26_50)
#define BS_PANEL_INIT_85_BASE   (bs_panel_60_26_85 - bs_panel_60_26_50)

#define IS_ICLK_24()            IS_LUIGI_PLATFORM()
#define IS_OCLK_85(r)           (EINK_FPL_RATE_85 == (r))
#define IS_OCLK_60(r)           (EINK_FPL_RATE_60 == (r))

void bs_panel_init(int wa, bool full, int size, int rate)
{
    bs_panels bs_panel_init_iclk_base, bs_panel_init_oclk_base, bs_panel;
    bool bs_pwr_pin_toggle = full || (BS_UPD_MODE_INIT == bs_upd_mode);
    bs_panel_init_t *bs_panel_init_which = NULL;

    u16 bs_init_lutidxfmt = BS_INIT_LUTIDXFMT, hsize, vsize;
    u32 iba = 0;
   
    // Determine which panel initialization parameters to use for 26MHz/24MHz input and 50Hz/85Hz
    // output.
    //
    bs_panel_init_iclk_base = IS_ICLK_24()      ?   BS_PANEL_INIT_24_BASE : BS_PANEL_INIT_26_BASE;
    bs_panel_init_oclk_base = IS_OCLK_85(rate)  ?   BS_PANEL_INIT_85_BASE : BS_PANEL_INIT_50_BASE;
    
    switch ( size )
    {
        case EINK_FPL_SIZE_60:
        case EINK_FPL_SIZE_63:
        default:
            bs_panel = BS_PANEL_INIT_60_BASE + bs_panel_init_iclk_base + bs_panel_init_oclk_base;
        break;
        
        case EINK_FPL_SIZE_97:
            if ( IS_OCLK_60(rate) )
                bs_panel = IS_ICLK_24() ? bs_panel_97_24_60 : bs_panel_97_26_60;
            else
                bs_panel = BS_PANEL_INIT_97_BASE + bs_panel_init_iclk_base + bs_panel_init_oclk_base;
        break;
        
        case EINK_FPL_SIZE_99:
            bs_panel = IS_ICLK_24() ? bs_panel_99_24_50 : bs_panel_99_26_50;
        break;
    }
    
    bs_panel_init_which = &bs_panel_init_table[bs_panel];
    hsize = bs_panel_init_which->hsize;
    vsize = bs_panel_init_which->vsize;

    if ( BS_ISIS() )
         bs_init_lutidxfmt |= (BS_INIT_AUTO_WF | BS_INIT_PIX_INVRT);
    else
    {
        u32 h, v;
        
        switch ( size )
        {
            case EINK_FPL_SIZE_97:
                h = hsize;
                v = BS97_HARD_VSIZE;
            break;
            
            case EINK_FPL_SIZE_99:
                h = hsize;
                v = vsize;
            break;
            
            default:
                h = v = 0;
            break;
        }
        
        if ( h && v )
            iba = BS_ROUNDUP32(h) * (v << 1);
    }
         
    // Initialize the panel with the selected parameters.
    //
    einkfb_debug("initializing for %s\n", bs_panel_init_info[bs_panel]);

    bs_cmd_init_dspe_cfg(hsize, vsize,
            BS_INIT_SDRV_CFG, BS_INIT_GDRV_CFG, bs_init_lutidxfmt);
    bs_cmd_init_dspe_tmg(bs_panel_init_which->fslen,
            (bs_panel_init_which->felen << 8) | bs_panel_init_which->fblen,
            bs_panel_init_which->lslen,
            (bs_panel_init_which->lelen << 8) | bs_panel_init_which->lblen,
            bs_panel_init_which->pixclkdiv);
    
    if ( iba )
        bs_set_ib_addr(iba);
        
    bs_cmd_print_disp_timings();

    bs_cmd_set_wfm(wa);
    bs_cmd_print_wfm_info();

    einkfb_debug("display engine initialized with waveform 0x%X\n", wa);

    bs_cmd_clear_gd();

    bs_cmd_wr_reg(0x01A, 4); // i2c clock divider
    
    // Check to see whether we need to disable internal temperature reads or not
    // and start VCOM control up in automatic mode if we're not doing VCOM
    // diagnostics.
    // 
    if ( BS_HAS_PMIC() )
    {
         bs_cmd_wr_reg(BS_TEMP_DEV_SELECT_REG, BS_TEMP_DEV_EXT);
         
         if ( !bs_vcom_diags )
            bs_cmd_wr_reg(BS_PWR_PIN_CONF_REG, bs_pwr_pin_toggle ? BS_PWR_PIN_VCOM_AUTO
                                                                 : BS_PWR_PIN_INIT);
    }
 
    if ( full )
    {
        bs_flash(hsize, vsize);
        bs_black(hsize, vsize);
        bs_white(hsize, vsize);
    }
    else
    {
        if ( BS_UPD_MODE_INIT == bs_upd_mode )
        {
            bs_upd_mode = BS_UPD_MODE_REINIT;
            bs_white(hsize, vsize);
        }
    }
    
    // Now, if necessary, disable the controller's control of the power
    // rails so that the PMIC can control them itself.
    //
    if ( BS_HAS_PMIC() && bs_pwr_pin_toggle )
    {
        if ( !bs_vcom_diags )
            bs_cmd_wr_reg(BS_PWR_PIN_CONF_REG, BS_PWR_PIN_INIT);
    }
}

void bs_flash(u16 hsize, u16 vsize)
{
    bs_upd_mode = BS_UPD_MODE_INIT;
    einkfb_debug("performing flash\n");
    bs_ld_value(EINKFB_WHITE, hsize, vsize, fx_update_full);
}

void bs_white(u16 hsize, u16 vsize)
{
    einkfb_debug("displaying white\n");
    bs_ld_value(EINKFB_WHITE, hsize, vsize, fx_update_full);
}

void bs_black(u16 hsize, u16 vsize)
{
    einkfb_debug("displaying black\n");
    bs_ld_value(EINKFB_BLACK, hsize, vsize, fx_update_full);
}

static bool bs_pll_ready(void *unused)
{
    return ( (BSC_RD_REG(0x00A) & 0x1) != 0 );
}

static void bs_init_pll(void)
{
    u16 init_pll_cfg_0, init_pll_cfg_1;
    int v;

    if ( BS_ASIC() )
    {
        init_pll_cfg_0 = INIT_PLL_CFG_0_ASIC;
        init_pll_cfg_1 = INIT_PLL_CFG_1_ASIC;

        BSC_WR_REG(0x006, INIT_PWR_SAVE_MODE);
    }
    else
    {
        init_pll_cfg_0 = INIT_PLL_CFG_0_FPGA;
        init_pll_cfg_1 = INIT_PLL_CFG_1_FPGA;

        bs_hw_test();
    }

    BSC_WR_REG(0x010, init_pll_cfg_0);
    BSC_WR_REG(0x012, init_pll_cfg_1);
    BSC_WR_REG(0x014, INIT_PLL_CFG_2);

    if ( BS_FPGA() )
        BSC_WR_REG(0x006, INIT_PWR_SAVE_MODE);

    BSC_WR_REG(0x016, INIT_CLK_CFG);

    if ( EINKFB_SUCCESS == EINKFB_SCHEDULE_TIMEOUT_INTERRUPTIBLE(BS_PLL_TIMEOUT, bs_pll_ready) )
        bs_pll_steady = true;
    else
        bs_pll_steady = false;

    if ( BS_ASIC() )
    {
        v = BSC_RD_REG(0x006);
        BSC_WR_REG(0x006, v & ~0x1);
    }
}

static void bs_init_spi(void)
{
    BSC_WR_REG(0x204, INIT_SPI_FLASH_CTL);
    BSC_WR_REG(0x208, INIT_SPI_FLASH_CSC);
}

static void bs_bootstrap_init(void)
{
    broadsheet_set_ignore_hw_ready(true);

    bs_init_pll();

    if ( BS_ASIC() )
        BSC_WR_REG(0x106, 0x0203);

    bs_init_spi();

    if ( BS_ASIC() )
        bs_hw_test();
}

#define BS_PANEL_BCD_READ() \
    (0 != bs_panel_bcd[0])

static char *bs_get_panel_bcd(void)
{
    // If the panel ID hasn't already been read in, then read it in now.
    //
    if ( !BS_PANEL_BCD_READ() )
    {
        u8 eeprom_panel_bcd[EEPROM_SIZE_BCD] = { 0 };

        broadsheet_eeprom_read(EEPROM_BASE_BCD, eeprom_panel_bcd, EEPROM_SIZE_BCD);
        strncpy(bs_panel_bcd, eeprom_panel_bcd, EEPROM_SIZE_BCD);
        bs_panel_bcd[EEPROM_SIZE_BCD] = '\0';
    }
    
    return ( bs_panel_bcd );
}

static bool bs_panel_data_valid(char *panel_data)
{
    bool result = false;
    
    if ( panel_data )
    {
        if ( strchr(panel_data, EEPROM_CHAR_UNKNOWN) )
        {
            einkfb_print_error("Unrecognized values in panel data\n");
            einkfb_debug("panel data = %s\n", panel_data);
        }
        else
            result = true;
    }
    
    return ( result );
}

#define BS_PANEL_ID_READ() \
    (('_' == bs_panel_id[4]) && ('_' == bs_panel_id[8]) && ('_' == bs_panel_id[11]))

static char *bs_translate_panel_sku_to_mmm(char *eeprom_sku_string)
{
    char *panel_sku_string = panel_skus[0],
         *result = ISIS_PANEL_MMM_DEFAULT;
    int   panel_sku_index = 0;
    
    // Walk the panel SKU list until we either find a match or come to
    // the end of the list.
    //
    while ( panel_sku_string && (0 != panel_sku_string[0]) )
    {
        if ( 0 != strncmp(panel_sku_string, eeprom_sku_string, EEPROM_SIZE_PART_NUMBER) )
        {
            // No match; so, get the next entry in the list.
            //
            panel_sku_string = panel_skus[++panel_sku_index];
        }
        else
        {
            // Found a match; so, return it and stop looking.
            //
            result = panel_mmms[panel_sku_index];
            panel_sku_string = NULL;
        }
    }

    return ( result );
}

static char *bs_get_panel_id(void)
{
    // If the panel ID hasn't already been read in, then read it in now.
    //
    if ( !BS_PANEL_ID_READ() )
    {
        // We'll always hard code this on Mario-based platforms because we only
        // have an ISIS FPGA for Mario and no true EEPROM read support for it.
        //
        if ( IS_MARIO_PLATFORM() )
            strcpy(bs_panel_id, BS_PANEL_ID_ISIS_MARIO);
        else
        {
            u8 eeprom_buffer[EEPROM_SIZE] = { 0 };
            char *mmm;

            // Waveform file names are of the form PPPP_XLLL_DD_TTVVSS_B, and
            // panel IDs are of the form PPPP_LLL_DD_MMM.
            //
            broadsheet_eeprom_read(EEPROM_BASE, eeprom_buffer, EEPROM_SIZE);

            // The platform is (usually) the PPPP substring.  And, in those cases, we copy
            // the platform data from the EEPROM's waveform name.  However, we must special-case
            // the V220E waveforms since EINK isn't using the same convention as they did in
            // the V110A case (i.e., they named V110A waveforms 110A but they are just
            // calling the V220E waveforms V220 with a run-type of E; run-type is the X
            // field in the PPPP_XLLL_DD_TTVVSS_B part of waveform file names).
            //
            switch ( eeprom_buffer[EEPROM_BASE_WAVEFORM+5] )
            {
                case 'E':
                    bs_panel_id[0] = '2';
                    bs_panel_id[1] = '2';
                    bs_panel_id[2] = '0';
                    bs_panel_id[3] = 'E';
                break;

                default:
                    bs_panel_id[0] = eeprom_buffer[EEPROM_BASE_WAVEFORM+0];
                    bs_panel_id[1] = eeprom_buffer[EEPROM_BASE_WAVEFORM+1];
                    bs_panel_id[2] = eeprom_buffer[EEPROM_BASE_WAVEFORM+2];
                    bs_panel_id[3] = eeprom_buffer[EEPROM_BASE_WAVEFORM+3];
                break;
            }

            bs_panel_id[ 4] = '_';

            // The lot number (aka FPL) is the the LLL substring:  Just
            // copy the number itself, skipping the batch (X) designation.
            //
            bs_panel_id[ 5] = eeprom_buffer[EEPROM_BASE_FPL+1];
            bs_panel_id[ 6] = eeprom_buffer[EEPROM_BASE_FPL+2];
            bs_panel_id[ 7] = eeprom_buffer[EEPROM_BASE_FPL+3];

            bs_panel_id[ 8] = '_';

            // The display size is the the DD substring.
            //
            bs_panel_id[ 9] = eeprom_buffer[EEPROM_BASE_WAVEFORM+10];
            bs_panel_id[10] = eeprom_buffer[EEPROM_BASE_WAVEFORM+11];
            bs_panel_id[11] = '_';

            // The manufacturer is the MMM substring, which is derived from
            // the part number field.
            //
            mmm = bs_translate_panel_sku_to_mmm((char *)&eeprom_buffer[EEPROM_BASE_PART_NUMBER]);
            
            bs_panel_id[12] = mmm[0];
            bs_panel_id[13] = mmm[1];
            bs_panel_id[14] = mmm[2];
            
            // If the panel id constructed from the panel data is invalid, then
            // use the default one instead.
            //
            bs_panel_id[15] = '\0';
             
            if ( !bs_panel_data_valid(bs_panel_id) )
                strcpy(bs_panel_id, BS_PANEL_ID_ISIS_LUIGI);
        }
    }

    return ( bs_panel_id );
}

#define BS_VCOM_INT_TO_STR(i, s)   \
    sprintf((s), "-%1d.%02d", ((i)/100)%10, (i)%100)

#define BS_VCOM_STR_READ()      \
    (('-' == bs_vcom_str[0]) && ('.' == bs_vcom_str[2]))

static char *bs_get_vcom_str(void)
{
    // If the VCOM hasn't already been read in, read it in now.
    //
    if ( !BS_VCOM_STR_READ() )
    {
        u8 eeprom_vcom[EEPROM_SIZE_VCOM] = { 0 };

        broadsheet_eeprom_read(EEPROM_BASE_VCOM, eeprom_vcom, EEPROM_SIZE_VCOM);
        strncpy(bs_vcom_str, eeprom_vcom, EEPROM_SIZE_VCOM);
        
        // If the VCOM string returned from the panel data is invalid, then
        // use the default one instead.
        //
        bs_vcom_str[EEPROM_SIZE_VCOM] = '\0';
        
        if ( !bs_panel_data_valid(bs_vcom_str) )
        {
            int vcom_default = bs_pmic_get_vcom_default();
            BS_VCOM_INT_TO_STR(vcom_default, bs_vcom_str);
        }
    }

    return ( bs_vcom_str );
}

#define BS_VCOM_READ() \
    (0 != bs_vcom)

static int bs_get_vcom(void)
{
    if ( !BS_VCOM_READ() )
    {
        char *vcom_str = bs_get_vcom_str();
        int i;

        // Parse the VCOM value.
        //
        if ('-' == (char)vcom_str[0])
        {
            // Skip the negative sign (i.e., i = 1, instead of i = 0).
            //
            for( i = 1; i < EEPROM_SIZE_VCOM; i++ )
            {
                // Skip the dot.
                //
                if ( '.' == (char)vcom_str[i] )
                    continue;

                if ( (vcom_str[i] >= '0') && (vcom_str[i] <= '9') )
                {
                    bs_vcom *= 10;
                    bs_vcom += ((char)vcom_str[i] - '0');
                }
            }
        }
    }

    return ( bs_vcom );
}

bool broadsheet_supports_panel_bcd()
{
    return ( IS_LUIGI_PLATFORM() && BS_PANEL_DATA_FLASH() );
}

char *broadsheet_get_panel_bcd()
{
    char *result = NULL;

    if ( broadsheet_supports_panel_bcd() )
        result = bs_get_panel_bcd();

    return ( result );
}

bool broadsheet_supports_panel_id()
{
    return ( IS_LUIGI_PLATFORM() || BS_ISIS() );
}

char *broadsheet_get_panel_id()
{
    char *result = NULL;

    if ( broadsheet_supports_panel_id() )
        result = bs_get_panel_id();

    return ( result );
}

bool broadsheet_supports_vcom()
{
    return ( IS_LUIGI_PLATFORM() );
}

void broadsheet_set_vcom(int vcom)
{
    if ( broadsheet_supports_vcom() )
    {
        // Allow us to unset an override VCOM by passing in 0.
        //
        if ( vcom )
        {
            bs_vcom_override = bs_pmic_set_vcom(vcom);
            BS_VCOM_INT_TO_STR(bs_vcom_override,
                bs_vcom_override_str);
        }
        else
        {
            bs_pmic_set_vcom(bs_get_vcom());
            bs_vcom_override = 0;
        }
    }
}

char *broadsheet_get_vcom()
{
    char *result = NULL;

    if ( broadsheet_supports_vcom() )
    {
        // Use the override VCOM value if it's been set.
        //
        if ( bs_vcom_override )
        {
            einkfb_debug("vcom = %d\n", bs_vcom_override);
            result = bs_vcom_override_str;
        }
        else
        {
            einkfb_debug("vcom = %d\n", bs_get_vcom());
            result = bs_get_vcom_str();
        }
    }

    return ( result );
}

#define BS_READ_WAVEFORM_FILE()                     \
    ((0 == bs_waveform_size)                     || \
     (SIZEOF_ISIS_WAVEFORM == bs_waveform_size)  || \
     (NULL == bs_waveform_buffer)                || \
     (isis_waveform == bs_waveform_buffer))

#define BS_READ_WAVEFORM_FILE_INIT(f, l)            \
    f = -1; l = -1

#define BS_WAVEFORM_FILE_READ(f, l)                 \
    ((0 < (f)) && (0 < (l)))

#define BS_LATER_WAVEFORM_VERSION(a, b)             \
    ((a).type       >= (b).type                 &&  \
     (a).version    >= (b).version              &&  \
     (a).subversion >= (b).subversion)

static int bs_read_waveform_file(char *waveform_path, int *len)
{
    int waveform_file = sys_open(waveform_path, O_RDONLY, 0);

    if ( 0 < waveform_file )
    {
        *len = sys_read(waveform_file, BS_WAVEFORM_BUFFER, BS_WAVEFORM_BUFFER_SIZE);

        if ( 0 < *len )
            bs_waveform_size = *len;

        sys_close(waveform_file);
    }
    
    einkfb_debug("path = %s, file = %d, size = %d\n",
        waveform_path, waveform_file, *len);
    
    return ( waveform_file );
}

static int bs_read_waveform_from_flash(int *len)
{
    int waveform_read = -1; *len = -1;
    
    // Only attempt to read the waveform from flash if flash on the panel is available.
    //
    if ( BS_PANEL_DATA_WHICH() == bs_panel_data_flash )
    {
        bs_flash_select saved_flash_select = broadsheet_get_flash_select();
        broadsheet_set_flash_select(bs_flash_commands);
        
        broadsheet_read_from_flash(BS_WF_BASE, BS_WAVEFORM_BUFFER, BS_WF_SIZE);
        broadsheet_set_flash_select(saved_flash_select);
        
        if ( broadsheet_waveform_valid() )
        {
            broadsheet_waveform_info_t waveform_info = { 0 };
            broadsheet_get_waveform_info(&waveform_info);
            
            *len = bs_waveform_size = waveform_info.filesize;
            waveform_read = 1;
        }
    }
    
    return ( waveform_read );
}

static void bs_get_isis_waveform(void)
{
	// Only attempt to read the waveform in if we haven't already successfully read it.
	//
	if ( BS_READ_WAVEFORM_FILE() )
	{
        char waveform_file_path[BS_WAVEFORM_FILE_SIZE] = { 0 };
        mm_segment_t saved_fs = get_fs();
        int waveform_file, len;

        // Set up to read in a waveform file.
        //
        bs_waveform_size   = BS_WAVEFORM_BUFFER_SIZE;
        bs_waveform_buffer = BS_WAVEFORM_BUFFER;

        set_fs(get_ds());

        // For waveform development and/or test purposes, we first look to see
        // whether a test waveform for ISIS is in place.
        //
        sprintf(waveform_file_path, BS_WAVEFORM_FILE_PATH, BS_WAVEFORM_FILE_ISIS);
        BS_READ_WAVEFORM_FILE_INIT(waveform_file, len);
        
        waveform_file = bs_read_waveform_file(waveform_file_path, &len);

        // If a test waveform file isn't in place, then we look to see whether a
        // waveform file based on the panel ID is available.
        //
        if ( !BS_WAVEFORM_FILE_READ(waveform_file, len) )
        {
            broadsheet_waveform_t waveform_version_file,
                                  waveform_version_flash;
            bool use_file_version = false;
           
            sprintf(waveform_file_path, BS_WAVEFORM_FILE_PATH, bs_get_panel_id());
            BS_READ_WAVEFORM_FILE_INIT(waveform_file, len);
            
            waveform_file = bs_read_waveform_file(waveform_file_path, &len);

            // If a file from the rootfs that matches the panel ID exists, get its
            // version information.
            //
            if ( BS_WAVEFORM_FILE_READ(waveform_file, len) )
            {
                broadsheet_get_waveform_version(&waveform_version_file);
                use_file_version = true;
            }

            // If there's a waveform in flash, get its version information.
            //
            BS_READ_WAVEFORM_FILE_INIT(waveform_file, len);
            waveform_file = bs_read_waveform_from_flash(&len);

            if ( BS_WAVEFORM_FILE_READ(waveform_file, len) )
            {
                broadsheet_get_waveform_version(&waveform_version_flash);
                
                // If there was an override waveform file from the rootfs, only
                // use it if the one from flash isn't later.
                //
                if ( use_file_version )
                    if ( BS_LATER_WAVEFORM_VERSION(waveform_version_flash, waveform_version_file) )
                        use_file_version = false;
            }
            
            // Reload the file from the rootfs if we should.
            //
            if ( use_file_version )
            {
                BS_READ_WAVEFORM_FILE_INIT(waveform_file, len);
                waveform_file = bs_read_waveform_file(waveform_file_path, &len);
            }

            // Finally, use the waveform that's built into the driver if we couldn't
            // read in one from either flash or the file system.
            //
            if ( !BS_WAVEFORM_FILE_READ(waveform_file, len) )
            {
                einkfb_print_warn("using built-in waveform as panel-specific waveform couldn't be found\n");
    
                bs_waveform_size   = SIZEOF_ISIS_WAVEFORM;
                bs_waveform_buffer = isis_waveform;
            }
        }

        set_fs(saved_fs);
    }
}

static void bs_sys_init(void)
{
    if ( BS_ISIS() )
    {
        bs_cmd_init_pll_stby(INIT_PLL_CFG_0_ASIC, INIT_PLL_CFG_1_ASIC, INIT_PLL_CFG_2);
        bs_cmd_init_cmd_set_isis(SIZEOF_ISIS_COMMANDS, isis_commands);

        bs_sdr_size = BS_SDR_SIZE_ISIS;
    }

    bs_cmd_init_sys_run();
    broadsheet_init_panel_data();
    
    if ( BS_ISIS() )
    {
        bs_get_isis_waveform();
        bs_cmd_bst_wr_sdr(BS_WFM_ADDR_ISIS, bs_waveform_size, bs_waveform_buffer);
        bs_cmd_init_waveform(BS_WFM_ADDR_SDRAM);

        bs_set_ib_addr(BS_IB_ADDR_ISIS);
    }

    bs_pmic_set_vcom(bs_vcom_override ? bs_vcom_override : bs_get_vcom());

    if ( BS_ASIC() )
    {
        // Don't touch the SDRAM refresh rate on ISIS.
        //
        if ( !BS_ISIS() )
            bs_cmd_wr_reg(0x106, 0x0203);

        bs_hw_test();
    }
}

#define BS_FPL_INFO_VALID(c, s, r)  \
    ((BS_FPL_INFO_COUNT == (c)) && (0 != (s)) && (0 != (r)))

static void bs_read_fpl_info(unsigned char *size, unsigned char *rate)
{
    mm_segment_t saved_fs = get_fs();
    int fpl_info_file, len = -1;

    set_fs(get_ds());
    
    fpl_info_file = sys_open(BS_FPL_INFO_FILE, O_RDONLY, 0);
    
    if ( 0 < fpl_info_file )
    {
        char fpl_info_string[BS_FPL_INFO_SIZE] = { 0 };
        
        len = sys_read(fpl_info_file, fpl_info_string, BS_FPL_INFO_SIZE);

        if ( 0 < len )
        {
            unsigned int local_size, local_rate,
                         count = sscanf(fpl_info_string,
                            BS_FPL_INFO_FORMAT,
                            &local_size,
                            &local_rate);
            
            if ( BS_FPL_INFO_VALID(count, local_size, local_rate) )
            {
                *size = (unsigned char)local_size;
                *rate = (unsigned char)local_rate;
                
                einkfb_debug("fpl_info: size = 0x%02X, rate = 0x%02X\n",
                    local_size, local_rate);
            }
        }

        sys_close(fpl_info_file);
    }

    einkfb_debug("path = %s, file = %d, size = %d\n",
        BS_FPL_INFO_FILE, fpl_info_file, len);
        
    set_fs(saved_fs);
}

static void bs_get_fpl_info(unsigned char *size, unsigned char *rate)
{
    *size = EINK_FPL_SIZE_60;
    *rate = EINK_FPL_RATE_50;

    if ( !bs_bootstrap )
    {
        if ( (0 == bs_fpl_size) || (0 == bs_fpl_rate) )
        {
            broadsheet_waveform_info_t waveform_info;
            broadsheet_get_waveform_info(&waveform_info);

            if ( waveform_info.fpl_size )
                *size = bs_fpl_size = waveform_info.fpl_size;
            else
                bs_fpl_size = *size;
                
            if ( waveform_info.fpl_rate )
                *rate = bs_fpl_rate = waveform_info.fpl_rate;
            else
                bs_fpl_rate = *rate;
                
            if ( EINK_IMPROVED_TEMP_RANGE == waveform_info.tuning_bias )
                bs_clip_temp = false;
        }
        else
        {
            *rate = bs_fpl_rate;
            *size = bs_fpl_size;
        }
        
        // Allow the fpl_info to be overridden from an external file.
        //
        bs_read_fpl_info(size, rate);
    }
}

static void bs_get_resolution(unsigned char fpl_size, bool orientation, bs_resolution_t *res)
{
    switch ( fpl_size )
    {
        
        // Portrait  -> 1200 x 1600 software/hardware
        // Landscape -> 1600 x 1200 software/hardware
        //
        case EINK_FPL_SIZE_99:
            res->x_hw = BS99_INIT_HSIZE;
            res->y_hw = BS99_INIT_VSIZE;

            res->x_sw = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS99_INIT_VSIZE : BS99_INIT_HSIZE;
            res->y_sw = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS99_INIT_HSIZE : BS99_INIT_VSIZE;

            res->x_mm = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS99_MM_1200 : BS99_MM_1600;
            res->y_mm = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS99_MM_1600 : BS99_MM_1200;
        break;            

        // Portrait  ->  824 x 1200 software,  826 x 1200 hardware
        // Landscape -> 1200 x  824 software, 1200 x  826 hardware
        //
        case EINK_FPL_SIZE_97:
            res->x_hw = BS97_INIT_HSIZE;
            res->y_hw = BS97_HARD_VSIZE;

            res->x_sw = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS97_SOFT_VSIZE : BS97_INIT_HSIZE;
            res->y_sw = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS97_INIT_HSIZE : BS97_SOFT_VSIZE;

            res->x_mm = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS97_MM_825  : BS97_MM_1200;
            res->y_mm = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS97_MM_1200 : BS97_MM_825;
        break;

        // Portrait  ->  600 x  800 software/hardware
        // Landscape ->  800 x  600 software/hardware
        //
        case EINK_FPL_SIZE_60:
        case EINK_FPL_SIZE_63:
        default:
            res->x_hw = BS60_INIT_HSIZE;
            res->y_hw = BS60_INIT_VSIZE;

            res->x_sw = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS60_INIT_VSIZE : BS60_INIT_HSIZE;
            res->y_sw = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS60_INIT_HSIZE : BS60_INIT_VSIZE;

            res->x_mm = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS60_MM_600 : BS60_MM_800;
            res->y_mm = (EINKFB_ORIENT_PORTRAIT == orientation) ? BS60_MM_800 : BS60_MM_600;
        break;
    }

    // Broadsheet actually aligns the hardware horizontal resolution to a 32-bit boundary.
    //
    res->x_hw = BS_ROUNDUP32(res->x_hw);
}

#define BS_ASIC_BXX() \
    ((BS_PRD_CODE == bs_prd_code) && U_IN_RANGE(bs_rev_code, BS_REV_ASIC_B00, BS_REV_ASIC_B02))

static bool bs_isis(void)
{
    if ( (BS_PRD_CODE_UNKNOWN == bs_prd_code) || (BS_REV_CODE_UNKNOWN == bs_rev_code) )
    {
        bs_prd_code = BS_CMD_RD_REG(BS_PRD_CODE_REG);
        bs_rev_code = BS_CMD_RD_REG(BS_REV_CODE_REG);
    }

    return ( BS_PRD_CODE_ISIS == bs_prd_code );
}

static bool bs_asic(void)
{
    if ( (BS_PRD_CODE_UNKNOWN == bs_prd_code) || (BS_REV_CODE_UNKNOWN == bs_rev_code) )
    {
        bs_prd_code = BS_CMD_RD_REG(BS_PRD_CODE_REG);
        bs_rev_code = BS_CMD_RD_REG(BS_REV_CODE_REG);
    }

    return ( BS_ASIC_BXX() || bs_isis() );
}

static bool bs_fpga(void)
{
    if ( (BS_PRD_CODE_UNKNOWN == bs_prd_code) || (BS_REV_CODE_UNKNOWN == bs_rev_code) )
    {
        bs_prd_code = BS_CMD_RD_REG(BS_PRD_CODE_REG);
        bs_rev_code = BS_CMD_RD_REG(BS_REV_CODE_REG);
    }

    return ( (BS_PRD_CODE == bs_prd_code) && !bs_asic() );
}

bool broadsheet_needs_dma(void)
{
    // Because DMA requires the eInk HAL to allocate a DMA-capable buffer, and because such
    // buffers, when allocated, are set up to be coherent (either directly via write-thru or
    // indirectly via MMU page-hit-detection), we ultimiately end up taking a performance
    // penality.  This is because the current eInk HAL implementation needs to write to
    // the buffer(s) itself.
    //
    // So, for now, we tell the eInk HAL that we don't need DMA.
    //
    //return ( !IS_ADS() );
    //
    return ( false );
}

u8 *broadsheet_posterize_table(void)
{
    u8 *result = NULL;
    
    // We don't need to swap nybbles or invert the data on ISIS.  But we do on
    // Broadsheet.  This lets us use DMA on Broadsheet in the fast update
    // mode case.
    //
    // Note:  We only use 2bpp for compatibility testing.  So, for now, instead
    //        of generating the 2bpp-to-1bpp posterization table for it, we
    //        just let it go through the 4bpp stretch case, as usual.
    //
    if ( !BS_ISIS() )
    {
        struct einkfb_info info;
        einkfb_get_info(&info);

        switch ( info.bpp )
        {
            case EINKFB_4BPP:
                result = bs_posterize_table_4bpp;
            break;
            
            case EINKFB_8BPP:
                result = bs_posterize_table_8bpp;
            break;
        }
    }
    
    return ( result );
}

#define BS_SW_INIT_CONTROLLER(f) bs_sw_init_controller(f, EINKFB_ORIENT_PORTRAIT, NULL)

void bs_sw_init_controller(bool full, bool orientation, bs_resolution_t *res)
{
    unsigned char fpl_size = 0;

    full = bs_bootstrap || full;

    // Perform general Broadsheet initialization.
    //
    if ( full )
    {
        // Take the hardware down first if it's still up.
        //
        if ( bs_hw_up )
            bs_sw_done();

        // Attempt to bring the hardware (back) up.
        //
        if ( BS_HW_INIT() )
        {
            // Say that the hardware is up and that
            // we're ready to accept commands.
            //
            bs_hw_up = bs_ready = true;

            // Perform FPGA- vs. ASIC-specific initialization if we're not
            // bootstrapping the device.
            //
            if ( !bs_bootstrap )
            {
                if ( BS_FPGA() )
                    bs_hw_test();

                if ( BS_ASIC() && !BS_ISIS() )
                    bs_cmd_wr_reg(0x006, 0x0000);
            }
        }
        else
            bs_sw_done();
    }

    // Under special circumstances (as when Broadsheet's flash is blank), we need
    // to bootstrap Broadsheet.  And, in that case, we just return the default,
    // 6-inch panel resolution if requested.
    //
    if ( bs_bootstrap )
    {
        if ( bs_hw_up )
            bs_bootstrap_init();

        if ( res )
            fpl_size = EINK_FPL_SIZE_60;
    }
    else
    {
        // Return panel-specific resolutions by reading the panel size from the
        // waveform header if requested.
        //
        bs_sys_init();

        if ( res )
        {
            unsigned char fpl_rate;
            
            bs_get_fpl_info(&fpl_size, &fpl_rate);
        }
    }

    if ( fpl_size )
        bs_get_resolution(fpl_size, orientation, res);
}

#define BROADSHEET_GET_ORIENTATION() broadsheet_get_orientation(), broadsheet_get_upside_down()

bool bs_sw_init_panel(bool full)
{
    // Don't go through the panel initialization path if
    // we're in bootstrap mode.
    //
    if ( bs_bootstrap )
        full = false;
    else
    {
        u32 bs_wfm_addr = BS_ISIS() ? BS_WFM_ADDR_ISIS : BS_WFM_ADDR;
        unsigned char fpl_size, fpl_rate;
        
        bs_get_fpl_info(&fpl_size, &fpl_rate);

        // Otherwise, go through panel-specific initialization.
        //
        bs_panel_init(bs_wfm_addr, full, fpl_size, fpl_rate);

        // Put us into the right orientation.
        //
        bs_cmd_set_rotmode(bs_cmd_orientation_to_rotmode(BROADSHEET_GET_ORIENTATION()));

        // Determine which update modes to use.
        //
        bs_set_upd_modes();

        // On ISIS, get the pixels in the right order in hardware, and
        // set up the auto-waveform update selection in hardware.
        //
        if ( BS_ISIS() )
        {
            bs_cmd_wr_reg(BS_PIX_CNFG_REG, BS_PIX_REVSWAP);

            // When promoting flashing updates, we must change these to
            // all GC16s.
            //
            bs_cmd_wr_reg(BS_AUTO_WF_REG_DU,   BS_AUTO_WF_MODE_DU);
            bs_cmd_wr_reg(BS_AUTO_WF_REG_GC4,  BS_AUTO_WF_MODE_GC4);
            bs_cmd_wr_reg(BS_AUTO_WF_REG_GC16, BS_AUTO_WF_MODE_GC16);
        }

        // Remember that the framebuffer has been initialized.
        //
        set_fb_init_flag(BOOT_GLOBALS_FB_INIT_FLAG);
    }

    // Say that we're now in the run state.
    //
    bs_power_state = bs_power_state_run;

    return ( full );
}

#define BS_SW_INIT(f) bs_sw_init(f, f)

bool bs_sw_init(bool controller_full, bool panel_full)
{
    BS_SW_INIT_CONTROLLER(controller_full);
    return ( bs_sw_init_panel(panel_full) );
}

void bs_sw_done(void)
{
    // Take the hardware all the way down.
    //
    BS_HW_DONE();

    // Say that hardware is down and that we're
    // no longer ready to accept commands.
    //
    bs_hw_up = bs_ready = bs_pll_steady = false;

    // Note that we're in the off state.
    //
    bs_power_state = bs_power_state_off;
}

bs_preflight_failure bs_get_preflight_failure(void)
{
    return ( preflight_failure );
}

static bool BS_PREFLIGHT_PASSES(void)
{
    preflight_failure = bs_preflight_failure_none;          // Assume no failures.

    if ( !(bs_hw_up && bs_ready && bs_pll_steady) )         // Is the hardware responding?
        preflight_failure |= bs_preflight_failure_hw;

    if ( !preflight_failure )
    {
        if ( !(BS_ASIC() || BS_FPGA()) )                    // Do the ID bits look good?
            preflight_failure |= bs_preflight_failure_id;
    }

    if ( !preflight_failure )
    {
        if ( !bs_hw_test() )                                // Is the bus okay?
            preflight_failure |= bs_preflight_failure_bus;
    }

    if ( !preflight_failure )                               // Do we recognize the Flash?
    {
        if ( !BS_SFM_PREFLIGHT() )
            preflight_failure |= bs_preflight_failure_flid;
    }

    if ( !preflight_failure )
    {
        if ( !broadsheet_commands_valid() )                 // Is the commands area okay?
            preflight_failure |= bs_preflight_failure_cmd;
    }

    if ( !preflight_failure )
    {
        if ( !broadsheet_waveform_valid() )                 // Is the waveform okay?
            preflight_failure |= bs_preflight_failure_wf;
    }

    if ( !preflight_failure )
    {
        if ( !bs_hrdy_preflight() )                         // Is HRDY responding?
            preflight_failure |= bs_preflight_failure_hrdy;
    }

    return ( (bs_preflight_failure_none == preflight_failure) ? true : false );
}

bool bs_preflight_passes(void)
{
    // Preserve bootstrapping state.
    //
    int saved_bs_bootstrap = bs_bootstrap;
    bool result = false;

    // For preflighting, force us into bootstrapping mode.
    //
    bs_bootstrap = true;
    BS_SW_INIT_CONTROLLER(FULL_BRINGUP_CONTROLLER);

    // If all is well, clean up.
    //
    if ( BS_PREFLIGHT_PASSES() )
    {
        // Before taking us down, save/set how the display should be
        // initialized.
        //
        broadsheet_preflight_init_display_flag(BS_INIT_DISPLAY_READ);

        // Finish up by pretending we were never here.
        //
        bs_sw_done();

        broadsheet_set_ignore_hw_ready(false);
        bs_power_state = bs_power_state_init;

        result = true;
    }
    else
    {
        // Note that we can't ascertain directly how to initialize
        // the display.
        //
        broadsheet_preflight_init_display_flag(BS_INIT_DISPLAY_DONT);
    }

    // Restore bootstrapping state.
    //
    bs_bootstrap = saved_bs_bootstrap;

    return ( result );
}

bool broadsheet_get_bootstrap_state(void)
{
    return ( bs_bootstrap ? true : false );
}

void broadsheet_set_bootstrap_state(bool bootstrap_state)
{
    bs_bootstrap = bootstrap_state ? 1 : 0;
}

bool broadsheet_get_ready_state(void)
{
    return ( bs_ready || (bs_power_state_off == bs_power_state) );
}

bool broadsheet_get_upd_repair_state(void)
{
    return ( (0 == bs_upd_repair_count) ? true : false );
}

void broadsheet_clear_screen(fx_type update_mode)
{
    bs_ld_value(EINKFB_WHITE, bs_hsize, bs_vsize, update_mode);
}

bs_power_states broadsheet_get_power_state(void)
{
    return ( bs_power_state );
}

static char *broadsheet_get_power_state_string(bs_power_states power_state)
{
    char *power_state_string = NULL;
    
    switch ( power_state )
    {
        case bs_power_state_off_screen_clear:
        case bs_power_state_off:
        case bs_power_state_run:
        case bs_power_state_standby:
        case bs_power_state_sleep:
            power_state_string = power_state_strings[power_state + 1];
        break;
        
        case bs_power_state_init:
        default:
            power_state_string = power_state_strings[0];
        break;
    }
    
    return ( power_state_string );
}

void broadsheet_set_power_state(bs_power_states power_state)
{
    if ( bs_power_state != power_state )
    {
        einkfb_debug_power("power_state = %d (%s) -> %d (%s)\n",
            bs_power_state, broadsheet_get_power_state_string(bs_power_state),
            power_state,    broadsheet_get_power_state_string(power_state));
        
        switch ( power_state )
        {
            // run
            //
            case bs_power_state_run:
                switch ( bs_power_state )
                {
                    // Initial state.  Do nothing as bs_sw_init() will be called "naturally."
                    //
                    case bs_power_state_init:
                    break;

                    // off_screen_clear -> run
                    //
                    case bs_power_state_off_screen_clear:
                        BS_SW_INIT(FULL_BRINGUP);
                    break;

                    // off -> run
                    //
                    case bs_power_state_off:
                        bs_sw_init(FULL_BRINGUP_CONTROLLER, DONT_BRINGUP_PANEL);
                        bs_cmd_ld_img_upd_data(fx_update_partial, UPD_DATA_RESTORE);
                    break;

                    // sleep/standby -> run
                    //
                    default:
                        bs_cmd_run_sys();
                        bs_power_state = bs_power_state_run;
                    break;
                }
            break;

            // off
            //
            case bs_power_state_off:
                broadsheet_set_power_state(bs_power_state_sleep);
                bs_sw_done();
            break;

            // off -> off_screen_clear
            //
            case bs_power_state_off_screen_clear:
                broadsheet_set_power_state(bs_power_state_off);
                bs_power_state = bs_power_state_off_screen_clear;
            break;

            //  run -> standby (not currently in use)
            //
            case bs_power_state_standby:
                broadsheet_set_power_state(bs_power_state_run);

                bs_cmd_stby();
                
                if ( !bs_vcom_diags )
                    bs_pmic_set_power_state(bs_pmic_power_state_standby);
                
                bs_power_state = bs_power_state_standby;
            break;

            // run -> sleep
            //
            case bs_power_state_sleep:
                broadsheet_set_power_state(bs_power_state_run);

                bs_cmd_slp();
                
                if ( !bs_vcom_diags )
                    bs_pmic_set_power_state(bs_pmic_power_state_standby);
                
                bs_power_state = bs_power_state_sleep;
            break;

            // Prevent the compiler from complaining.
            //
            default:
            break;
        }
    }
    else
    {
        // Report that we're skipping this power state transition if we're already
        // in the state we're supposed to be going to.
        //
        einkfb_debug_power("already in power state %d (%s), skipping\n",
            power_state, broadsheet_get_power_state_string(power_state));
    }
}

void bs_iterate_cmd_queue(bs_cmd_queue_iterator_t bs_cmd_queue_iterator, int max_elems)
{
    if ( bs_cmd_queue_iterator )
    {
        bs_cmd_queue_elem_t *bs_cmd_queue_elem;
        int i, count = bs_cmd_queue_count();

        if ( (count < max_elems) || (0 > max_elems) )
            i = 0;
        else
            i = count - max_elems;

        for ( ; (NULL != (bs_cmd_queue_elem = bs_get_queue_cmd_elem(i))) && (i < count); i++ )
            (*bs_cmd_queue_iterator)(bs_cmd_queue_elem);
    }
}

int bs_read_temperature(void)
{
    int temp = broadsheet_get_temperature();

    if ( !IN_RANGE(temp, BS_TEMP_MIN, BS_TEMP_MAX) )
    {
        if ( BS_HAS_PMIC() )
        {
            // The act of moving Papyrus to the active state causes the temperature to be read,
            // so, we just need to read back the value that was just cached.
            //
            bs_pmic_set_power_state(bs_pmic_power_state_active);
            temp = BS_GET_TEMP_CACHED();
        }
        else
        {
            int raw_temp;
        
            bs_cmd_wr_reg(0x214, 0x0001);           // Trigger a temperature-sensor read.
            bs_cmd_wait_for_bit(0x212, 0, 0);       // Wait for the sensor to be idle.
        
            raw_temp = (int)bs_cmd_rd_reg(0x216);   // Read the result.
        
            // The temperature sensor is actually returning a signed, 8-bit
            // value from -25C to 80C.  But we go ahead and use the entire
            // 8-bit range:  0x00..0x7F ->  0C to  127C
            //               0xFF..0x80 -> -1C to -128C
            //
            if ( IN_RANGE(raw_temp, 0x0000, 0x007F) )
                temp = raw_temp;
            else
            {
                raw_temp &= 0x00FF;
                temp = raw_temp - 0x0100;
            }
        }
    }

    return ( temp );
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet Read/Write SDRAM API
    #pragma mark -
#endif

int broadsheet_get_ram_size(void)
{
    return ( bs_sdr_size );
}

static unsigned long broadsheet_get_ram_base(void)
{
    return ( (unsigned long)(broadsheet_get_ram_select() * broadsheet_get_ram_select_size()) );
}

int broadsheet_read_from_ram(unsigned long addr, unsigned char *data, unsigned long size)
{
    unsigned long start = broadsheet_get_ram_base() + addr;
    int result = 0;

    if ( bs_sdr_size >= (start + size) )
    {
        bs_cmd_rd_sdr(start, size, data);
        result = 1;
    }
    else
        einkfb_print_warn("Attempting to read off the end of memory, start = %ld, length %ld\n",
            start, size);

    return ( result );
}

int broadsheet_read_from_ram_byte(unsigned long addr, unsigned char *data)
{
    unsigned long byte_addr = addr & 0xFFFFFFFE;
    unsigned char byte_array[2] = { 0 };
    int result = 0;

    if ( broadsheet_read_from_ram(byte_addr, byte_array, sizeof(byte_array)) )
    {
        *data  = (1 == (addr & 1)) ? byte_array[1] : byte_array[0];
        result = 1;
    }

    return ( result );
}

int broadsheet_read_from_ram_short(unsigned long addr, unsigned short *data)
{
    return ( broadsheet_read_from_ram(addr, (unsigned char *)data, sizeof(unsigned short)) );
}

int broadsheet_read_from_ram_long(unsigned long addr, unsigned long *data)
{
    return ( broadsheet_read_from_ram(addr, (unsigned char *)data, sizeof(unsigned long)) );
}

int broadsheet_program_ram(unsigned long start_addr, unsigned char *buffer, unsigned long blen)
{
   unsigned long start = broadsheet_get_ram_base() + start_addr;
   int result = 0;

    if ( bs_sdr_size >= (start + blen) )
    {
        bs_cmd_wr_sdr(start, blen, buffer);
        result = 1;
    }
    else
        einkfb_print_warn("Attempting to write off the end of memory, start = %ld, length %ld\n",
            start, blen);

    return ( result );
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet Waveform/Commands Flashing API
    #pragma mark -
#endif

#include "broadsheet_waveform.c"
#include "broadsheet_commands.c"

static unsigned char   *buffer_byte  = NULL;
static unsigned short  *buffer_short = NULL;
static unsigned long   *buffer_long  = NULL;

static u8 *isis_get_flash_base(void)
{
    u8 *result;

    switch ( broadsheet_get_flash_select() )
    {
        case bs_flash_waveform:
            result = bs_waveform_buffer ? bs_waveform_buffer : isis_waveform;
        break;

        case bs_flash_commands:
            result = isis_commands;
        break;

        case bs_flash_test:
        default:
            result = NULL;
        break;
    }

    return ( result );
}

static int isis_read_from_flash_byte(unsigned long addr, unsigned char *data)
{
    if ( data && buffer_byte )
        *data = buffer_byte[addr];

    return ( NULL != buffer_byte );
}

static int isis_read_from_flash_short(unsigned long addr, unsigned short *data)
{
    if ( data && buffer_short && buffer_byte )
    {
        // Read 16 bits if we're 16-bit aligned.
        //
        if ( addr == ((addr >> 1) << 1) )
            *data = buffer_short[addr >> 1];
        else
            *data = (buffer_byte[addr + 0] << 0) |
                    (buffer_byte[addr + 1] << 8);
    }

    return ( NULL != buffer_short );
}

static int isis_read_from_flash_long(unsigned long addr, unsigned long *data)
{
    if ( data && buffer_long && buffer_byte )
    {
        // Read 32 bits if we're 32-bit aligned.
        //
        if ( addr == ((addr >> 2) << 2) )
            *data = buffer_long[addr >> 2];
        else
            *data = (buffer_byte[addr + 0] <<  0) |
                    (buffer_byte[addr + 1] <<  8) |
                    (buffer_byte[addr + 2] << 16) |
                    (buffer_byte[addr + 3] << 24);
    }

    return ( NULL != buffer_long );
}

static int isis_read_from_flash(unsigned long addr, unsigned char *data, unsigned long size)
{
    u8 *buffer = isis_get_flash_base();
    int result = 0;

    if ( buffer )
    {
        buffer_byte  = buffer;
        buffer_short = (unsigned short *)buffer;
        buffer_long  = (unsigned long *)buffer;

        switch ( size )
        {
            case sizeof(unsigned char):
                result = isis_read_from_flash_byte(addr, data);
            break;

            case sizeof(unsigned short):
                result = isis_read_from_flash_short(addr, (unsigned short *)data);
            break;

            case sizeof(unsigned long):
                result = isis_read_from_flash_long(addr, (unsigned long *)data);
            break;
        }

        buffer_byte  = NULL;
        buffer_short = NULL;
        buffer_long  = NULL;
    }

    return ( result );
}

static unsigned long broadsheet_get_flash_base(void)
{
    unsigned long result;

    switch ( broadsheet_get_flash_select() )
    {
        case bs_flash_waveform:
            result = BS_WFM_ADDR;
        break;

        case bs_flash_commands:
            result = BS_CMD_ADDR;
        break;

        case bs_flash_test:
        default:
            result = bs_tst_addr;
        break;
    }

    return ( result );
}

int broadsheet_read_from_flash(unsigned long addr, unsigned char *data, unsigned long size)
{
    unsigned long start = broadsheet_get_flash_base() + addr;
    int result = 0;

    einkfb_debug("start = %ld, size = %ld\n", start, size);

    if ( bs_sfm_size >= (start + size) )
    {
        bs_sfm_start();
        bs_sfm_read(start, size, data);
        bs_sfm_end();

        result = 1;
    }
    else
        einkfb_print_warn("Attempting to read off the end of flash, start = %ld, length %ld\n",
            start, size);

    return ( result );
}

#define BROADSHEET_READ_FROM_FLASH(a, d, s) \
    (BS_ISIS() ? isis_read_from_flash((a), (d), (s)) : broadsheet_read_from_flash((a), (d), (s)))

bool broadsheet_flash_is_readonly(void)
{
    // On Luigi-based devices whose panel data is in Flash, we only support reading it.
    //
    return ( IS_LUIGI_PLATFORM() && BS_ISIS() && BS_PANEL_DATA_FLASH() );
}

bool broadsheet_supports_flash(void)
{
   // Basically, all devices with Broadsheet (i.e., not ISIS) or Luigi-based devices with ISIS
   // whose panel data is in Flash support Flash.
   //
   return ( !BS_ISIS() || broadsheet_flash_is_readonly() );
}

int broadsheet_read_from_flash_byte(unsigned long addr, unsigned char *data)
{
    return ( BROADSHEET_READ_FROM_FLASH(addr, data, sizeof(unsigned char)) );
}

int broadsheet_read_from_flash_short(unsigned long addr, unsigned short *data)
{
    return ( BROADSHEET_READ_FROM_FLASH(addr, (unsigned char *)data, sizeof(unsigned short)) );
}

int broadsheet_read_from_flash_long(unsigned long addr, unsigned long *data)
{
    return ( BROADSHEET_READ_FROM_FLASH(addr, (unsigned char *)data, sizeof(unsigned long)) );
}

int broadsheet_program_flash(unsigned long start_addr, unsigned char *buffer, unsigned long blen)
{
    unsigned long flash_base = broadsheet_get_flash_base(),
                  start = flash_base + start_addr;
    int result = 0;

    if ( bs_sfm_size >= (start + blen) )
    {
        bs_sfm_start();
        bs_sfm_write(start, blen, buffer);
        bs_sfm_end();

        switch ( flash_base )
        {
            // Soft reset after flashing the waveform.
            //
            case BS_WFM_ADDR:
                bs_sw_init(DONT_BRINGUP_CONTROLLER, DONT_BRINGUP_PANEL);
                if ( !bs_bootstrap )
                    bs_cmd_ld_img_upd_data(fx_update_partial, UPD_DATA_RESTORE);
            break;

            // Hard reset after flashing the commands (allow
            // the test area of flash to act like commands
            // when in bootstrap mode).
            //
            case BS_TST_ADDR_128K:
            case BS_TST_ADDR_256K:
                if ( !bs_bootstrap )
                    break;

            case BS_CMD_ADDR:
                bs_sw_init(FULL_BRINGUP_CONTROLLER, DONT_BRINGUP_PANEL);
                if ( !bs_bootstrap )
                    bs_cmd_ld_img_upd_data(fx_update_partial, UPD_DATA_RESTORE);
            break;
        }

        result = 1;
    }
    else
        einkfb_print_warn("Attempting to write off the end of flash, start = %ld, length %ld\n",
            start, blen);

    return ( result );
}

unsigned long broadsheet_get_init_display_flag(void)
{
    unsigned long init_display_flag = get_broadsheet_init_display_flag();

    // Keep the values we recognize, and force everything else to the slow path.
    //
    switch ( init_display_flag )
    {
        case BS_INIT_DISPLAY_FAST:
        case BS_INIT_DISPLAY_SLOW:
        case BS_INIT_DISPLAY_ASIS:
        break;

        default:
            init_display_flag = BS_INIT_DISPLAY_SLOW;
        break;
    }

    return ( init_display_flag );
}

void broadsheet_set_init_display_flag(unsigned long init_display_flag)
{
    // Keep the values we recognize, and force everything else to the slow path.
    //
    switch ( init_display_flag )
    {
        case BS_INIT_DISPLAY_FAST:
        case BS_INIT_DISPLAY_SLOW:
        case BS_INIT_DISPLAY_ASIS:
        break;

        default:
            init_display_flag = BS_INIT_DISPLAY_SLOW;
        break;
    }

    set_broadsheet_init_display_flag(init_display_flag);
}

void broadsheet_preflight_init_display_flag(bool read)
{
    unsigned long init_display_flag = BS_INIT_DISPLAY_SLOW;

    // Read the initialization flag from the boot globals if we should.
    //
    if ( read )
        init_display_flag = broadsheet_get_init_display_flag();

    switch ( init_display_flag )
    {
        case BS_INIT_DISPLAY_FAST:
            set_fb_init_flag(BOOT_GLOBALS_FB_INIT_FLAG);
        break;

        case BS_INIT_DISPLAY_ASIS:
        case BS_INIT_DISPLAY_SLOW:
        default:
            set_fb_init_flag(0);
        break;
    }

    broadsheet_set_init_display_flag(init_display_flag);
}

void broadsheet_init_panel_data(void)
{
    panel_data = bs_panel_data_none;
    
    // The panel data is either on an EEPROM (i2c) or it's in Flash (SPI).
    //
    if ( broadsheet_supports_eeprom_read() )
    {
        if ( BS_SFM_PREFLIGHT_PD() )
            panel_data = bs_panel_data_flash;
        else
            panel_data = bs_panel_data_eeprom;
    }
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet Broadsheet EEPROM API
    #pragma mark -
#endif

#include "broadsheet_eeprom.c"
