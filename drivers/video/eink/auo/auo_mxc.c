/*
 *  linux/drivers/video/eink/auo/broadsheet_mxc.c --
 *  eInk frame buffer device HAL broadsheet hw
 *
 *      Copyright (C) 2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include "auo.h"
#include <asm/arch/controller_common.h>
#include <asm/arch/controller_common_mxc.c>

int auo_mxc_wr_dat(bool which, u32 data_size, u16 *data) {
    return ( controller_wr_dat(which, data_size, data) );
}

int auo_mxc_wr_cmd(auo_mxc_cmd cmd, bool poll) {
    return ( controller_wr_cmd(cmd, poll) );
}

int auo_mxc_rd_one(u16 *data) {
    return ( controller_rd_one(data) );
}

/*
 * Initialize the AUO hardware interface
 */
bool auo_mxc_init(uint16_t width, uint16_t height, uint16_t bpp) {
    controller_properties_t auo_props;
    AUO_CONFIG_CONTROLLER_PROPS(auo_props, width, height, bpp, auo_get_phys_addr, auo_clear_phys_addr);

    return ( controller_hw_init(auo_needs_dma(), &auo_props) );
}

void auo_mxc_done(void) { 
    controller_hw_done();
}

