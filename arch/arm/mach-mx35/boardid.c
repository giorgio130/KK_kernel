/*
 * boardid.c
 *
 * Copyright (C) 2008,2009 Amazon Technologies, Inc. All rights reserved.
 * Jon Mayo <jonmayo@amazon.com>
 *
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/proc_fs.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#define DRIVER_VER "1.0"
#define DRIVER_INFO "Board ID and Serial Number driver for Lab126 boards version " DRIVER_VER

#define BOARDID_USID_PROCNAME		"usid"
#define BOARDID_PROCNAME_BOARDID	"board_id"
#define BOARDID_PROCNAME_PANELID	"panel_id"
#define BOARDID_PROCNAME_PCBSN		"pcbsn"

#define SERIAL_NUM_BASE          0
#define SERIAL_NUM_SIZE         16

#define BOARD_ID_BASE           (SERIAL_NUM_BASE + SERIAL_NUM_SIZE)
#define BOARD_ID_SIZE           16

#define PANEL_ID_BASE           (BOARD_ID_BASE + BOARD_ID_SIZE)
#define PANEL_ID_SIZE           32

#define PCB_ID_BASE		516
#define PCB_ID_SIZE		8

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#include <linux/kernel.h>
#include <linux/module.h>
#include <net/mwan.h>


static wan_status_t wan_power_status = WAN_INVALID;


void
wan_set_power_status(
	wan_status_t status)
{
	wan_power_status = status;
}

EXPORT_SYMBOL(wan_set_power_status);


wan_status_t
wan_get_power_status(
	void)
{
	return wan_power_status;
}

EXPORT_SYMBOL(wan_get_power_status);

char mx35_serial_number[SERIAL_NUM_SIZE + 1];
EXPORT_SYMBOL(mx35_serial_number);

char mx35_board_id[BOARD_ID_SIZE + 1];
EXPORT_SYMBOL(mx35_board_id);

char mx35_panel_id[PANEL_ID_SIZE + 1];
EXPORT_SYMBOL(mx35_panel_id);

char mx35_pcb_id[PCB_ID_SIZE + 1];
EXPORT_SYMBOL(mx35_pcb_id);

static int proc_id_read(char *page, char **start, off_t off, int count,
				int *eof, void *data, char *id)
{
	strcpy(page, id);
	*eof = 1;

	return strlen(page);
}

#define PROC_ID_READ(id) proc_id_read(page, start, off, count, eof, data, id)

static int proc_usid_read(char *page, char **start, off_t off, int count,
				int *eof, void *data)
{
        return PROC_ID_READ(mx35_serial_number);
}

static int proc_usid_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	char lbuf[SERIAL_NUM_SIZE];

	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, sizeof(lbuf))) {
		return -EFAULT;
	}
	strcpy(mx35_serial_number, lbuf);
	mx35_serial_number[SERIAL_NUM_SIZE]='\0';
	/* printk("MX35 Serial Number - %s\n", mx35_serial_number); */
	printk("MX35 Serial Number changed.\n");
	strncpy(system_serial16, mx35_serial_number, SERIAL16_SIZE);
	return count;
}

static int proc_board_id_read(char *page, char **start, off_t off, int count,
				int *eof, void *data)
{
        return PROC_ID_READ(mx35_board_id);
}

static int proc_panel_id_read(char *page, char **start, off_t off, int count,
				int *eof, void *data)
{
        return PROC_ID_READ(mx35_panel_id);
}

static int proc_panel_id_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	char lbuf[PANEL_ID_SIZE];
	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, sizeof(lbuf))) {
		return -EFAULT;
	}
	strcpy(mx35_panel_id, lbuf);
	mx35_panel_id[PANEL_ID_SIZE]='\0';
	printk ("MX35 Panel id - %s\n", mx35_panel_id);
	return count;
}

static int proc_pcb_id_read(char *page, char **start, off_t off, int count,
				int *eof, void *data)
{
	return PROC_ID_READ(mx35_pcb_id);
}

static int proc_pcb_id_write(struct file *file, const char __user *buf, unsigned long count, void *data)
{
	char lbuf[PCB_ID_SIZE];
	memset(lbuf, 0, sizeof(lbuf));

	if (copy_from_user(lbuf, buf, sizeof(lbuf))) {
		return -EFAULT;
	}
	strcpy(mx35_pcb_id, lbuf);
	mx35_pcb_id[PCB_ID_SIZE]='\0';
	printk ("MX35 PCB id - %s\n", mx35_pcb_id);
	return count;
}

/* initialize the proc accessors */
static void mx35_serialnumber_init(void)
{
	struct proc_dir_entry *proc_usid = create_proc_entry(BOARDID_USID_PROCNAME, S_IRUGO, NULL);
	struct proc_dir_entry *proc_board_id = create_proc_entry(BOARDID_PROCNAME_BOARDID, S_IRUGO, NULL);
	struct proc_dir_entry *proc_panel_id = create_proc_entry(BOARDID_PROCNAME_PANELID, S_IRUGO, NULL);
	struct proc_dir_entry *proc_pcb_id = create_proc_entry(BOARDID_PROCNAME_PCBSN, S_IRUGO, NULL);

	if (proc_usid != NULL) {
		proc_usid->data = NULL;
		proc_usid->read_proc = proc_usid_read;
		proc_usid->write_proc = proc_usid_write;
	}

	if (proc_board_id != NULL) {
		proc_board_id->data = NULL;
		proc_board_id->read_proc = proc_board_id_read;
		proc_board_id->write_proc = NULL;
	}

	if (proc_panel_id != NULL) {
		proc_panel_id->data = NULL;
		proc_panel_id->read_proc = proc_panel_id_read;
		proc_panel_id->write_proc = proc_panel_id_write;
	}

	if (proc_pcb_id != NULL) {
		proc_pcb_id->data = NULL;
		proc_pcb_id->read_proc = proc_pcb_id_read;
		proc_pcb_id->write_proc = proc_pcb_id_write;
	}
}

/* copy the serial numbers from the special area of memory into the kernel */
static void mx35_serial_board_numbers(void)
{
	memcpy(mx35_serial_number, system_serial16, MIN(sizeof(mx35_serial_number)-1, sizeof(system_serial16)));
	mx35_serial_number[sizeof(mx35_serial_number)-1] = '\0';

	memcpy(mx35_board_id, system_rev16, MIN(sizeof(mx35_board_id)-1, sizeof(system_rev16)));
	mx35_board_id[sizeof(mx35_board_id)-1] = '\0';

	strcpy(mx35_panel_id, ""); /* start these as empty and populate later. */
	strcpy(mx35_pcb_id, "");

	// Removed per official request to rid the log of FSN
	//printk ("MX35 Serial Number - %s\n", mx35_serial_number);
	printk ("MX35 Board id - %s\n", mx35_board_id);

	/* these wouldn't be initialized through ATAGS this early, so don't print them. */
	/*
	printk ("MX35 Panel id - %s\n", mx35_panel_id);
	printk ("MX35 PCB id - %s\n", mx35_pcb_id);
	*/
}

void mx35_init_boardid(void)
{
	pr_info(DRIVER_INFO "\n");

	mx35_serial_board_numbers();

	mx35_serialnumber_init();
}
