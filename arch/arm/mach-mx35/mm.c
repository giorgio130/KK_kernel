/*
 * Copyright 2008 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/hardware.h>
#include <asm/pgtable.h>
#include <asm/map.h>
#include <linux/sched.h>

/*!
 * @file mach-mx35/mm.c
 *
 * @brief This file creates static mapping between physical to virtual memory.
 *
 * @ingroup Memory_MX35
 */

/*!
 * This structure defines the MX35 memory map.
 */
static struct map_desc mxc_io_desc[] __initdata = {
	{
	 .virtual = IRAM_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(IRAM_BASE_ADDR),
	 .length = IRAM_SIZE,
	 .type = MT_NONSHARED_DEVICE},
	{
	 .virtual = X_MEMC_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(X_MEMC_BASE_ADDR),
	 .length = X_MEMC_SIZE,
	 .type = MT_DEVICE},
	{
	 .virtual = NFC_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(NFC_BASE_ADDR),
	 .length = NFC_SIZE,
	 .type = MT_NONSHARED_DEVICE},
	{
	 .virtual = ROMP_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(ROMP_BASE_ADDR),
	 .length = ROMP_SIZE,
	 .type = MT_NONSHARED_DEVICE},
	{
	 .virtual = AVIC_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(AVIC_BASE_ADDR),
	 .length = AVIC_SIZE,
	 .type = MT_NONSHARED_DEVICE},
	{
	 .virtual = AIPS1_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(AIPS1_BASE_ADDR),
	 .length = AIPS1_SIZE,
	 .type = MT_NONSHARED_DEVICE},
	{
	 .virtual = SPBA0_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(SPBA0_BASE_ADDR),
	 .length = SPBA0_SIZE,
	 .type = MT_NONSHARED_DEVICE},
	{
	 .virtual = AIPS2_BASE_ADDR_VIRT,
	 .pfn = __phys_to_pfn(AIPS2_BASE_ADDR),
	 .length = AIPS2_SIZE,
	 .type = MT_NONSHARED_DEVICE},
};

/*!
 * This function initializes the memory map. It is called during the
 * system startup to create static physical to virtual memory map for
 * the IO modules.
 */
void __init mxc_map_io(void)
{
	iotable_init(mxc_io_desc, ARRAY_SIZE(mxc_io_desc));
}

/** Wrapper for the get_task_comm function in fs/exec.c */
char *mxc_get_task_comm(char *buf, struct task_struct *tsk)
{
	return get_task_comm(buf, tsk);
}

EXPORT_SYMBOL(mxc_get_task_comm);
