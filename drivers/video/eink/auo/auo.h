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

#ifndef _AUO_H_
#define _AUO_H_

#include "auo_def.h"

struct auo_resolution_t
{
    u32 x_hw, x_sw,
        y_hw, y_sw,
        bpp;
};
typedef struct auo_resolution_t auo_resolution_t;

extern int auo_mxc_wr_dat(bool which, u32 data_size, u16 *data);
extern int auo_mxc_wr_cmd(auo_mxc_cmd cmd, bool poll);
extern int auo_mxc_rd_one(u16 *data);

extern void auo_clear_phys_addr(void);
extern dma_addr_t auo_get_phys_addr(void);
extern bool auo_needs_dma(void);
extern void auo_full_load_display(fx_type update_mode);
extern void auo_area_load_display(u8 *data, fx_type update_mode, u16 x, u16 y, u16 w, u16 h);
extern bool auo_change_flashing_display_mode(auo_display_mode_t new_display_mode);
extern auo_display_mode_t auo_read_flashing_display_mode(void);
extern bool auo_change_nonflashing_display_mode(auo_display_mode_t new_display_mode);
extern auo_display_mode_t auo_read_nonflashing_display_mode(void);
extern bool auo_change_auto_mode(auo_auto_mode_t new_auot_mode);
extern auo_auto_mode_t auo_read_auto_mode(void);

extern void auo_set_power_state(auo_power_states power_state);
extern auo_power_states auo_get_power_state(void);
extern void auo_get_controller_orientation(int *auo_orientation, bool *flip);
extern void auo_set_controller_orientation(int auo_orientation, bool flip, bool send_init);
extern bool auo_sw_init_controller(int auo_orientation, 
                                   int auo_size, 
                                   int auo_bpp, 
                                   auo_resolution_t *res);
extern void auo_sw_done_controller(void);

extern bool auo_mxc_init(uint16_t width, uint16_t height, uint16_t bpp);
extern void auo_mxc_done(void);

#endif //_AUO_H_
