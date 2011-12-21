/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_mxc.c --
 *  eInk frame buffer device HAL broadsheet hw
 *
 *      Copyright (C) 2005-2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include "broadsheet.h"
#include <asm/arch/controller_common_mxc.c>

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet HW Primitives
    #pragma mark -
#endif

bool bs_hrdy_preflight(void)
{
    return ( EINKFB_SUCCESS == controller_wait_for_ready() );
}

int bs_wr_cmd(bs_cmd cmd, bool poll)
{
    return ( controller_wr_cmd(cmd, poll) );
}

bool bs_wr_one_ready(void)
{
    return ( controller_wr_one_ready() );
}

void bs_wr_one(u16 data)
{
    return ( controller_wr_one(data) );
}

int bs_wr_dat(bool which, u32 data_size, u16 *data)
{
    return ( controller_wr_dat(which, data_size, data) );
}

int bs_rd_dat(u32 data_size, u16 *data)
{
    return ( controller_rd_dat(data_size, data) );
}

/*
** Initialize the Broadsheet hardware interface and reset the
** Broadsheet controller.
*/
bool bs_hw_init(void)
{
    controller_properties_t broad_props;

    BROADSHEET_CONFIG_CONTROLLER_PROPS(broad_props, bs_get_phys_addr, bs_clear_phys_addr);

    return ( controller_hw_init(broadsheet_needs_dma(), &broad_props) );
}

static bool bs_cmd_ck_reg(u16 ra, u16 rd)
{
    u16 rd_reg = bs_cmd_rd_reg(ra);
    bool result = true;

    if ( rd_reg != rd )
    {
        einkfb_print_crit("REG[0x%04X] is 0x%04X but should be 0x%04X\n",
            ra, rd_reg, rd);
            
        result = false;
    }
    
    return ( result );
}

bool bs_hw_test(void)
{
    bool result = true;

    bs_cmd_wr_reg(0x0304, 0x0123); // frame begin/end length reg
    bs_cmd_wr_reg(0x030A, 0x4567); // line  begin/end length reg
    result &= bs_cmd_ck_reg(0x0304, 0x0123);
    result &= bs_cmd_ck_reg(0x030A, 0x4567 );
    
    bs_cmd_wr_reg(0x0304, 0xFEDC);
    bs_cmd_wr_reg(0x030A, 0xBA98);
    result &= bs_cmd_ck_reg(0x0304, 0xFEDC);
    result &= bs_cmd_ck_reg(0x030A, 0xBA98);
    
    return ( result );
}

void bs_hw_done(void)
{
    controller_hw_done();
}
