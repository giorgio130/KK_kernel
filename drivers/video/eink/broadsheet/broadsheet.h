/*
 *  linux/drivers/video/eink/broadsheet/broadsheet.h --
 *  eInk frame buffer device HAL broadsheet defs
 *
 *      Copyright (C) 2005-2010 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef _BROADSHEET_H
#define _BROADSHEET_H

#include "broadsheet_def.h"

struct bs_cmd_block_t
{
    bs_cmd          command;                // bs_cmd_XXXX
    bs_cmd_type     type;                   // read/write
    
    u32             num_args;               // [io = ]command(args[0], ..., args[BS_CMD_ARGS_MAX - 1])
    u16             args[BS_CMD_ARGS_MAX],  //
                    io;                     // 

    u32             data_size;              // data[0..data_size-1]
    u8              *data;                  //
    
    struct
    bs_cmd_block_t  *sub;                   // subcommand(args[0], ..., args[BS_CMD_ARGS_MAX - 1])
};
typedef struct bs_cmd_block_t bs_cmd_block_t;

struct bs_cmd_queue_elem_t
{
    bs_cmd_block_t  bs_cmd_block;
    unsigned long   time_stamp;
};
typedef struct bs_cmd_queue_elem_t bs_cmd_queue_elem_t;

typedef void (*bs_cmd_queue_iterator_t)(bs_cmd_queue_elem_t *bs_cmd_queue_elem);

struct bs_resolution_t
{
    u32 x_hw, x_sw, x_mm,
        y_hw, y_sw, y_mm;
};
typedef struct bs_resolution_t bs_resolution_t;

struct bs_panel_init_t
{
    u16 hsize, vsize,
        fslen, fblen, felen,
        lslen, lblen, lelen,
        pixclkdiv;
};
typedef struct bs_panel_init_t bs_panel_init_t;

enum bs_panels
{
    bs_panel_60_26_50,                      // Mario-based 6.0-inch panels (26MHz input, 50Hz output).
    bs_panel_60_26_85,                      // Mario-based 6.0-inch panels (26MHz input, 85Hz output).
    bs_panel_60_24_50,                      // Luigi-based 6.0-inch panels (24MHz input, 50Hz output).
    bs_panel_60_24_85,                      // Luigi-based 6.0-inch panels (24MHz input, 85Hz output).
    
    bs_panel_97_26_50,                      // Mario-based 9.7-inch panels (26MHz input, 50Hz output).
    bs_panel_97_26_85,                      // Luigi-based 9.7-inch panels (26MHz input, 85Hz output).
    bs_panel_97_24_50,                      // Mario-based 9.7-inch panels (24MHz input, 50Hz output).
    bs_panel_97_24_85,                      // Luigi-based 9.7-inch panels (24MHz input, 85Hz output).
    
    bs_panel_97_26_60,                      // Mario-based 9.7-inch panels (26MHz input, 60Hz output).
    bs_panel_97_24_60,                      // Luigi-based 9.7-inch panels (24MHz input, 60Hz output).
    
    bs_panel_99_26_50,                      // Mario-based 9.7-inch HR panels (1600x1200, 26MHz/50Hz).
    bs_panel_99_24_50,                      // Luigi-based 9.7-inch HR panels (1600x1200, 24MHz/50Hz).
    
    num_bs_panels
};
typedef enum bs_panels bs_panels;

// Broadsheet Host Interface Commands API (AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs_cmd/bs_cmd.h)
//
// System Commands
//
extern void bs_cmd_init_cmd_set(u16 arg0, u16 arg1, u16 arg2);
extern void bs_cmd_init_cmd_set_isis(u32 bc, u8 *data);
extern void bs_cmd_init_pll_stby(u16 cfg0, u16 cfg1, u16 cfg2);
extern void bs_cmd_run_sys(void);
extern void bs_cmd_stby(void);
extern void bs_cmd_slp(void);
extern void bs_cmd_init_sys_run(void);
extern void bs_cmd_init_sys_stby(void);
extern void bs_cmd_init_sdram(u16 cfg0, u16 cfg1, u16 cfg2, u16 cfg3);
extern void bs_cmd_init_dspe_cfg(u16 hsize, u16 vsize, u16 sdcfg, u16 gfcfg, u16 lutidxfmt);
extern void bs_cmd_init_dspe_tmg(u16 fs, u16 fbe, u16 ls, u16 lbe, u16 pixclkcfg);
extern void bs_cmd_set_rotmode(u16 rotmode);
extern void bs_cmd_init_waveform(u16 wfdev);

// Register and Memory Access Commands
//
extern u16  bs_cmd_rd_reg(u16 ra);
extern void bs_cmd_wr_reg(u16 ra, u16 wd);
extern void bs_cmd_rd_sfm(void);
extern void bs_cmd_wr_sfm(u8 wd);
extern void bs_cmd_end_sfm(void);

extern u16  BS_CMD_RD_REG(u16 ra);          // Validates ra.
extern void BS_CMD_WR_REG(u16 ra, u16 wd);  // Validates ra.

// Burst Access Commands
//
extern void bs_cmd_bst_rd_sdr(u32 ma, u32 bc, u8 *data);
extern void bs_cmd_bst_wr_sdr(u32 ma, u32 bc, u8 *data);
extern void bs_cmd_bst_end(void);

// Image Loading Commands
//
extern void bs_cmd_ld_img(u16 dfmt);
extern void bs_cmd_ld_img_area(u16 dfmt, u16 x, u16 y, u16 w, u16 h);
extern void bs_cmd_ld_img_end(void);
extern void bs_cmd_ld_img_wait(void);

// Polling Commands
//
extern void bs_cmd_wait_dspe_trg(void);
extern void bs_cmd_wait_dspe_frend(void);
extern void bs_cmd_wait_dspe_lutfree(void);
extern void bs_cmd_wait_dspe_mlutfree(u16 lutmsk);

// Waveform Update Commands
//
extern void bs_cmd_rd_wfm_info(u32 ma);
extern void bs_cmd_upd_init(void);
extern void bs_cmd_upd_full(u16 mode, u16 lutn, u16 bdrupd);
extern void bs_cmd_upd_full_area(u16 mode, u16 lutn, u16 bdrupd, u16 x, u16 y, u16 w, u16 h);
extern void bs_cmd_upd_part(u16 mode, u16 lutn, u16 bdrupd);
extern void bs_cmd_upd_part_area(u16 mode, u16 lutn, u16 bdrupd, u16 x, u16 y, u16 w, u16 h);
extern void bs_cmd_upd_gdrv_clr(void);
extern void bs_cmd_upd_set_imgadr(u32 ma);

// SPI Flash Interface API (AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs_sfm.h)
//
extern bool bs_sfm_preflight(bool isis_override);
extern int  bs_get_sfm_size(void);

extern void bs_sfm_start(void);
extern void bs_sfm_end(void);

extern void bs_sfm_wr_byte(int data);
extern int  bs_sfm_rd_byte(void);
extern int  bs_sfm_esig(void);

extern void bs_sfm_read(int addr, int size, char *data);
extern void bs_sfm_write(int addr, int size, char *data);

// Broadsheet Host Interface Helper API (AM300_MMC_IMAGE_X03a/source/broadsheet_soft/bs_cmd/bs_cmd.h)
//
extern void bs_cmd_wait_for_bit(int reg, int bitpos, int bitval);
extern void bs_cmd_print_disp_timings(void);

extern void bs_cmd_set_wfm(int addr);
extern void bs_cmd_get_wfm_info(void);
extern void bs_cmd_print_wfm_info(void);
extern void bs_cmd_clear_gd(void);

extern void bs_cmd_rd_sdr(u32 ma, u32 bc, u8 *data);
extern void bs_cmd_wr_sdr(u32 ma, u32 bc, u8 *data);

extern int  bs_cmd_get_lut_auto_sel_mode(void);
extern void bs_cmd_set_lut_auto_sel_mode(int v);

extern int  bs_cmd_get_wf_auto_sel_mode(void);
extern void bs_cmd_set_wf_auto_sel_mode(int v);

extern void bs_cmd_bypass_vcom_enable(int v);

// Lab126
//
extern u32  bs_cmd_get_sdr_img_base(void);

extern void bs_clear_phys_addr(void);
extern dma_addr_t bs_get_phys_addr(void);

extern void BS_CMD_LD_IMG_UPD_DATA(fx_type update_mode);
extern void bs_cmd_ld_img_upd_data(fx_type update_mode, bool restore);
extern void bs_cmd_ld_img_area_upd_data(u8 *data, fx_type update_mode, u16 x, u16 y, u16 w, u16 h);

extern void bs_cmd_upd_repair(void);

extern unsigned long *bs_get_img_timings(int *num_timings);
extern void bs_set_ib_addr(u32 iba);

// Broadsheet API (broadsheet.c)
//
extern void bs_panel_init(int wfmaddr, bool full, int size, int rate);

extern void bs_flash(u16 hsize, u16 vsize);
extern void bs_white(u16 hsize, u16 vsize );
extern void bs_black(u16 hsize, u16 vsize);

extern void bs_sw_init_controller(bool full, bool orientation, bs_resolution_t *res);
extern bool bs_sw_init_panel(bool full);

extern bool bs_sw_init(bool controller_full, bool panel_full);
extern void bs_sw_done(void);

extern bs_preflight_failure bs_get_preflight_failure(void);
extern bool bs_preflight_passes(void);

extern bool broadsheet_get_bootstrap_state(void);
extern void broadsheet_set_bootstrap_state(bool bootstrap_state);

extern bool broadsheet_get_ready_state(void);
extern bool broadsheet_get_upd_repair_state(void);

extern void broadsheet_clear_screen(fx_type update_mode);
extern bs_power_states broadsheet_get_power_state(void);
extern void broadsheet_set_power_state(bs_power_states power_state);

extern void bs_iterate_cmd_queue(bs_cmd_queue_iterator_t bs_cmd_queue_iterator, int max_elems);
extern int bs_read_temperature(void);

extern bool broadsheet_supports_panel_bcd(void);
extern char *broadsheet_get_panel_bcd(void);

extern bool broadsheet_supports_panel_id(void);
extern char *broadsheet_get_panel_id(void);

extern bool broadsheet_supports_vcom(void);
extern void broadsheet_set_vcom(int vcom);
extern char *broadsheet_get_vcom(void);

// Broadsheet HW Primitives (broadsheet_mxc.c)
//
extern int  bs_wr_cmd(bs_cmd cmd, bool poll);
extern int  bs_wr_dat(bool which, u32 data_size, u16 *data);
extern int  bs_rd_dat(u32 data_size, u16 *data);

extern bool bs_wr_one_ready(void);
extern void bs_wr_one(u16 data);

extern bool bs_hw_init(void); // Similar to bsc.init_gpio() from bs_chip.cpp.
extern bool bs_hw_test(void); // Similar to bsc.test_gpio() from bs_chip.cpp.

extern void bs_hw_done(void);

extern bool bs_hrdy_preflight(void);

// Broadsheet PMIC API (broadsheet_pmic.c)
//
extern bool bs_pmic_init(void);
extern void bs_pmic_done(void);

extern bs_pmic_status bs_pmic_get_status(void);

extern void bs_pmic_create_proc_enteries(void);
extern void bs_pmic_remove_proc_enteries(void);

extern int  bs_pmic_get_temperature(bool fresh);
extern int  bs_pmic_get_vcom_default(void);
extern int  bs_pmic_set_vcom(int vcom);

extern void bs_pmic_set_power_state(bs_pmic_power_states power_state);
 
// Broadsheet eInk HAL API (broadsheet_hal.c)
//
extern unsigned char *broadsheet_get_scratchfb(void);
extern unsigned long  broadsheet_get_scratchfb_size(void);
extern dma_addr_t     broadsheet_get_scratchfb_phys(void);

void broadsheet_prime_watchdog_timer(bool delay_timer);

extern bs_flash_select broadsheet_get_flash_select(void);
extern void broadsheet_set_flash_select(bs_flash_select flash_select);

extern int  broadsheet_get_ram_select_size(void);
extern int  broadsheet_get_ram_select(void);
extern void broadsheet_set_ram_select(int ram_select);

extern bool broadsheet_get_orientation(void);
extern bool broadsheet_get_upside_down(void);
extern void broadsheet_set_override_upd_mode(int upd_mode);
extern int  broadsheet_get_override_upd_mode(void);
extern bool broadsheet_get_promote_flashing_updates(void);

extern int  broadsheet_get_temperature(void);

extern bool broadsheet_ignore_hw_ready(void);
extern void broadsheet_set_ignore_hw_ready(bool value);
extern bool broadsheet_force_hw_not_ready(void);

extern int  broadsheet_get_recent_commands(char *page, int max_commands);

extern bool broadsheet_needs_dma(void);

extern u8   *broadsheet_posterize_table(void);

// Broadsheet Read/Write SDRAM API (broadsheet.c)
//
extern int broadsheet_get_ram_size(void);

extern int broadsheet_read_from_ram(unsigned long addr, unsigned char *data, unsigned long size);
extern int broadsheet_read_from_ram_byte(unsigned long addr, unsigned char *data);
extern int broadsheet_read_from_ram_short(unsigned long addr, unsigned short *data);
extern int broadsheet_read_from_ram_long(unsigned long addr, unsigned long *data);

extern int broadsheet_program_ram(unsigned long start_addr, unsigned char *buffer, unsigned long blen);

// Broadsheet Waveform/Commands Flashing API (broadsheet.c, broadsheet_waveform.c, broadsheet_commands.c)
//
#include "broadsheet_waveform.h"
#include "broadsheet_commands.h"

extern bool broadsheet_flash_is_readonly(void);
extern bool broadsheet_supports_flash(void);

extern int broadsheet_read_from_flash(unsigned long addr, unsigned char *data, unsigned long size);
extern int broadsheet_read_from_flash_byte(unsigned long addr, unsigned char *data);
extern int broadsheet_read_from_flash_short(unsigned long addr, unsigned short *data);
extern int broadsheet_read_from_flash_long(unsigned long addr, unsigned long *data);

extern int broadsheet_program_flash(unsigned long start_addr, unsigned char *buffer, unsigned long blen);

// Broadsheet Panel Initialization API (broadsheet.c)
//
extern unsigned long broadsheet_get_init_display_flag(void);
extern void broadsheet_set_init_display_flag(unsigned long init_display_flag);

extern void broadsheet_preflight_init_display_flag(bool read);
extern void broadsheet_init_panel_data(void);
extern bs_panel_data panel_data;

// Broadsheet EEPROM API (broadsheet_eeprom.c)
//
#include "broadsheet_eeprom.h"

// Build Specifics
//
#define BATTERY_TEMP_FORMAT_F       "temp=%dF:from battery\n"
#define BATTERY_TEMP_FORMAT_C       "temp=%dC:from battery\n"

#define PMIC_TEMP_FORMAT_NORMAL_C   "temp=%dC:from pmic\n"
#define PMIC_TEMP_FORMAT_WARN_C_m_M "temp=%dC:from pmic, outside of ideal range of %dC to %dC\n"

#define TEMP_FORMAT_C_WHICH         "temp=%dC:from %s\n"
#define TEMP_WHICH_C_BATT           "battery"
#define TEMP_WHICH_C_PMIC           "pmic"

#define CLIPPED_TEMP_FORMAT_C       "temp=%dC:clipped\n"

#define OVERRIDE_TEMP_FORMAT_C      "temp=%dC:overridden\n"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
extern int luigi_temperature;

static inline void log_battery_temperature(void)
{
    if ( IS_LUIGI_PLATFORM() )
        einkfb_print_warn(BATTERY_TEMP_FORMAT_F, luigi_temperature);
}

static inline int get_battery_temperature(void)
{
    int temp = BS_TEMP_INVALID;
    
    if ( IS_LUIGI_PLATFORM() )
    {
        int f_to_c[BS_MAX_F-BS_MIN_F+1] =
        {
            0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10,
           11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 21,
           21, 22, 22, 23, 23, 24, 24, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 31, 31,
           32, 32, 33, 33, 34, 34, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40, 41, 41, 42,
           42, 43, 43, 44, 44, 45, 46, 46, 47, 47, 48, 48, 49, 49, 50
        };
        
        temp = luigi_temperature;
        
        if ( IN_RANGE(temp, BS_MIN_F, BS_MAX_F) )
            temp = f_to_c[temp-BS_MIN_F];
    }
    
    return ( temp );
}
#else
#define log_battery_temperature() do { } while (0)
#define get_battery_temperature() BS_TEMP_INVALID
#endif

#endif // _BROADSHEET_H
