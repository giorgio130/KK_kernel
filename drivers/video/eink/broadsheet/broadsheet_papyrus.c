/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_papyrus.c --
 *  eInk frame buffer HAL PMIC Papyrus
 *
 *      Copyright (C) 2005-2010 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include "broadsheet_papyrus.h"
#include "broadsheet.h"

#define PAPYRUS_TEMP_READ_TIMEOUT       BS_TMP_TIMEOUT
#define PAPYRUS_I2C_RDY_TIMEOUT         BS_RDY_TIMEOUT
#define PAPYRUS_RAILS_UP_TIMEOUT        BS_TMP_TIMEOUT
#define PAPYRUS_INT_STATUS_TEMP_READY   0x1
#define PAPYRUS_INT_MASK_TEMP_READY     0x1
#define PAPYRUS_PG_STATUS_RAILS_UP      0xfa

static DECLARE_WAIT_QUEUE_HEAD(papyrus_temp_event_wait);
static bool temp_ready = false;
static u8 temp_status = 0;

static DECLARE_WAIT_QUEUE_HEAD(papyrus_pg_event_wait);
static u8 pg_status = 0;

static int broadsheet_papyrus_wait_for_i2c(void);
extern int mxc_i2c_suspended;

#if PRAGMAS
#pragma mark -
#pragma mark Papyrus I2C
#pragma mark -
#endif

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
static struct i2c_client *papyrus_i2c = NULL;
static bool papyrus_i2c_driver_added  = false;

/*
 * Papyrus 2 wire address is 0x48.
 */
 #define PAPYRUS_ADDR 0x48

static unsigned short normal_i2c[] = { PAPYRUS_ADDR, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static int papyrus_i2c_attach(struct i2c_adapter *adap);
static int papyrus_i2c_detach(struct i2c_client *client);

static void papyrus_clear_temp_status(void) {
    einkfb_debug("clearing temp_status\n");
    temp_ready = false;
    temp_status = 0;
}

static void papyrus_clear_pg_status(void) {
    einkfb_debug("clearing pg_status\n");
    pg_status = 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
static void papyrus_temp_work(struct work_struct *dummy)
{
	u8 dummy_status;
	
	// read the temp status
	broadsheet_papyrus_read_register(PAPYRUS_REGISTER_INT_STATUS1, &dummy_status);
	broadsheet_papyrus_read_register(PAPYRUS_REGISTER_INT_STATUS2, &temp_status);
	einkfb_debug("temp_status = 0x%02X\n", temp_status);
	
	if ((temp_status & PAPYRUS_INT_STATUS_TEMP_READY) != 0)
	    temp_ready = true;

	// wake up anyone waiting on temp
	wake_up_interruptible(&papyrus_temp_event_wait);
}

static void papyrus_pg_work(struct work_struct *dummy)
{
	// read the pg status
	broadsheet_papyrus_read_register(PAPYRUS_REGISTER_PG_STATUS, &pg_status);
	einkfb_debug("pg_status = 0x%02X\n", pg_status);

	// wake up anyone waiting on pg status
	wake_up_interruptible(&papyrus_pg_event_wait);
}

static DECLARE_WORK(papyrus_temp_wq, papyrus_temp_work);
static DECLARE_WORK(papyrus_pg_wq, papyrus_pg_work);

static irqreturn_t papyrus_temp(int irq, void *data, struct pt_regs *r)
{
    schedule_work(&papyrus_temp_wq);
    return IRQ_HANDLED;
}

static irqreturn_t papyrus_pg(int irq, void *data, struct pt_regs *r)
{
    schedule_work(&papyrus_pg_wq);    
    return IRQ_HANDLED;
}
#endif
  
static struct i2c_driver papyrus_i2c_driver = {
    .driver = {
        .name = EINKFB_NAME"_papyrus_i2c",
    },
    .attach_adapter = papyrus_i2c_attach,
    .detach_client =  papyrus_i2c_detach,
    .command =        NULL,
};

static struct i2c_client client_template = {
    .name =   EINKFB_NAME"_papyrus",
    .addr =   PAPYRUS_ADDR,
    .driver = &papyrus_i2c_driver,
};

static int papyrus_probe(struct i2c_adapter *adap, int addr, int kind) {
    struct i2c_client *i2c;
    int ret;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
    int err = 0;
#endif

    einkfb_debug("Probe called with addr : 0x%x\n", addr);

    if (addr != PAPYRUS_ADDR)
        return -ENODEV;
    
    client_template.adapter = adap;
    client_template.addr = addr;

    i2c = kmemdup(&client_template, sizeof(client_template), GFP_KERNEL);
    
    if (i2c == NULL) {
        return -ENOMEM;
    }
    
    ret = i2c_attach_client(i2c);
    if (ret < 0) {
        einkfb_debug("Failed to attach papyrus at addr %x\n", addr);
        goto err;
    }
    papyrus_i2c = i2c;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
    // configure temp
    GPIO_PAPYRUS_IRQ_CONFIGURE(1);
    // set up the irq
    set_irq_type(GPIO_PAPYRUS_IRQ, IRQF_TRIGGER_FALLING);
    err = request_irq(GPIO_PAPYRUS_IRQ, 
                      (irq_handler_t) papyrus_temp, 
                      0, "papyrus_temp", NULL);
    if (err != 0) {
        einkfb_print_info("PAPYRUS IRQ line = 0x%x; request status = %d\n", GPIO_PAPYRUS_IRQ, err);
    } else {
        einkfb_debug("enabling temp\n");
    }

    // configure pg
    GPIO_PAPYRUS_PG_CONFIGURE(1);   
    // set up the irq
    set_irq_type(GPIO_PAPYRUS_PG, IRQF_TRIGGER_RISING);
    err = request_irq(GPIO_PAPYRUS_PG, 
                      (irq_handler_t) papyrus_pg, 
                      0, "papyrus_pg", NULL);
    if (err != 0) {
        einkfb_print_info("PAPYRUS PG line = 0x%x; request status = %d\n", GPIO_PAPYRUS_PG, err);
    } else {
        einkfb_debug("enabling pg\n");
    }
#endif

    return ret;

 err:
    kfree(i2c);
    return ret;
}

static int papyrus_i2c_detach(struct i2c_client *client) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
    if (papyrus_i2c) {    
        free_irq(GPIO_PAPYRUS_IRQ, NULL);
        free_irq(GPIO_PAPYRUS_PG, NULL);
        GPIO_PAPYRUS_IRQ_CONFIGURE(0);
        GPIO_PAPYRUS_PG_CONFIGURE(0);   
    }
#endif
    i2c_detach_client(client);
    kfree(client);
    papyrus_i2c = NULL;
    return 0;
}

static int papyrus_i2c_attach(struct i2c_adapter *adap) {
    return i2c_probe(adap, &addr_data, papyrus_probe);
}
#endif

#if PRAGMAS
#pragma mark -
#pragma mark Papyrus PMIC sysfs enteries
#pragma mark -
#endif

static int papyrus_reg_number = 0;

// /sys/devices/platform/eink_fb.0/papyrus_register_number (read/write)
static ssize_t show_papyrus_register_number(FB_DSHOW_PARAMS) {
    char *curr = buf;
    
    curr += sprintf(curr, "Papyrus register number: %d\n\n", papyrus_reg_number);
    
    return curr - buf;
}

static ssize_t store_papyrus_register_number(FB_DSTOR_PARAMS) {
    int value = 0;
    if (sscanf(buf, "%d", &value) <= 0) {
        einkfb_debug("Could not store the Papyrus register number\n");
        return -EINVAL;
    }

    if (!IN_RANGE(value, 0, 16)) {
        einkfb_debug("Invalid register number for Papyrus, range is 0-16\n");
        return -EINVAL;
    }

    papyrus_reg_number = value;

    return count;
}

// /sys/devices/platform/eink_fb.0/papyrus_register_value (read/write)
static int read_papyrus_register_value(char *page, unsigned long off, int count) {
    char *curr = NULL;
    int value = -1 ;

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
    if (EINKFB_SUCCESS == broadsheet_papyrus_wait_for_i2c()) {
        value = i2c_smbus_read_byte_data(papyrus_i2c, (u8)papyrus_reg_number);
    } else {
        value = -1;
    }
#endif
    
    if (value < 0) {
        einkfb_debug("Could not read register value for register number: %d\n", papyrus_reg_number);
        return -EINVAL;
    }
        
    curr = page;
    curr += sprintf(curr, "Papyrus register number: %d\n Value: 0x%x\n\n", papyrus_reg_number, value);
    
    return curr - page;
}

static ssize_t show_papyrus_register_value(FB_DSHOW_PARAMS) {
    return ( EINKFB_PROC_SYSFS_RW_LOCK(buf, NULL, 0, 0, 0, 0, read_papyrus_register_value) );
}

static int write_papyrus_register_value(char *page, unsigned long off, int count) {
    int value = -1;

    if (sscanf(page, "%x", &value) > 0) {
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
        if (EINKFB_SUCCESS == broadsheet_papyrus_wait_for_i2c()) {
            if (i2c_smbus_write_byte_data(papyrus_i2c, (u8)papyrus_reg_number, (u8)value) != 0) {
                // error
                value = -1;
            }
        }
#endif
    }

    if (value < 0) {
        einkfb_debug("Error setting value for register number: %d\n", papyrus_reg_number);
        return -EINVAL;
    } else {
        return count;
    }
}

static ssize_t store_papyrus_register_value(FB_DSTOR_PARAMS) {
    return ( EINKFB_PROC_SYSFS_RW_LOCK((char *)buf, NULL, 0, count, 0, 0, write_papyrus_register_value) );
}

// /sys/devices/platform/eink_fb.0/papyrus_reset (write-only)
static int reset_papyrus(char *page, unsigned long off, int count) {
    int result = -EINVAL, value;
    
    if (sscanf(page, "%d", &value) > 0) {
        EINKFB_BLANK(FB_BLANK_POWERDOWN);
        RESET_PAPYRUS();
        EINKFB_BLANK(FB_BLANK_UNBLANK);
        
        result = count;
    }
    
    return result;
}

static ssize_t store_papyrus_reset(FB_DSTOR_PARAMS) {
    return ( EINKFB_PROC_SYSFS_RW_NO_LOCK((char *)buf, NULL, 0, count, 0, 0, reset_papyrus) );
}

/* Papyrus related entries */
static DEVICE_ATTR(papyrus_register_number,  DEVICE_MODE_RW,  show_papyrus_register_number, store_papyrus_register_number);
static DEVICE_ATTR(papyrus_register_value,   DEVICE_MODE_RW,  show_papyrus_register_value,  store_papyrus_register_value);
static DEVICE_ATTR(papyrus_reset,            DEVICE_MODE_W,   NULL,                         store_papyrus_reset);

void broadsheet_papyrus_create_proc_enteries(void) {
    struct einkfb_info info; einkfb_get_info(&info);
    /* Add Papyrus sysfs entries */
    if (papyrus_i2c) {
        FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_papyrus_register_number);
        FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_papyrus_register_value);
        FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_papyrus_reset);
    }
}

void broadsheet_papyrus_remove_proc_enteries(void) {
    struct einkfb_info info; einkfb_get_info(&info);
    /* Remove Papyrus sysfs entries */
    if (papyrus_i2c) {
        device_remove_file(&info.dev->dev, &dev_attr_papyrus_register_number);
        device_remove_file(&info.dev->dev, &dev_attr_papyrus_register_value);
        device_remove_file(&info.dev->dev, &dev_attr_papyrus_reset);
    }
}

#if PRAGMAS
#pragma mark -
#pragma mark Papyrus register access functions
#pragma mark -
#endif

int broadsheet_papyrus_read_register(char reg, char *value) {
    int val = -1;

    if (!value) {
        return 0;
    }

    if (reg > 16) {
        return 0;
    }

    if (!papyrus_i2c) {
        return 0;
    }

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
    if (EINKFB_SUCCESS == broadsheet_papyrus_wait_for_i2c()) {
        val = i2c_smbus_read_byte_data(papyrus_i2c, reg);
    } else {
        return 0;
    }
#endif

    if (val < 0) {
        return 0;
    }
    *value = (char)val;

    return 1;
}

int broadsheet_papyrus_write_register(char reg, char value) {
    if (reg > 16) {
        return 0;
    }

    if (!papyrus_i2c) {
        return 0;
    }

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
    if (EINKFB_SUCCESS == broadsheet_papyrus_wait_for_i2c()) {
        if ( i2c_smbus_write_byte_data(papyrus_i2c, reg, value) != 0) {
            return 0;
        }
    } else {
        return 0;
    }

    return 1;
#else
    return 0;
#endif
}

static bool broadsheet_papyrus_i2c_bus_ready(void *unused) {
    return ( !mxc_i2c_suspended );
}

static int broadsheet_papyrus_wait_for_i2c(void) {
    return ( EINKFB_SCHEDULE_TIMEOUT(PAPYRUS_I2C_RDY_TIMEOUT, broadsheet_papyrus_i2c_bus_ready) );
}

static bool broadsheet_papyrus_temp_ready(void) {
    u8 conv_end = 0;

    einkfb_debug("temp_ready = 0x%02X\n", temp_ready);
    
    if (!temp_ready) {
        broadsheet_papyrus_read_register(PAPYRUS_REGISTER_TMST_CONFIG, &conv_end);
        einkfb_debug("conv_end   = 0x%02X\n", conv_end);
    }
    
    return ( temp_ready || ((PAPYRUS_CONV_END & conv_end) != 0) );
}

s8 broadsheet_papyrus_read_temp(void) {
    s8 temp = PAPYRUS_TEMP_DEFAULT;
    int result = 0;

    papyrus_clear_temp_status();

    if (broadsheet_papyrus_write_register(PAPYRUS_REGISTER_TMST_CONFIG, PAPYRUS_READ_THERM)) {    
        // block until we get the interrupt
        result = wait_event_interruptible_timeout(papyrus_temp_event_wait, 
                                                  broadsheet_papyrus_temp_ready(), 
                                                  PAPYRUS_TEMP_READ_TIMEOUT);
        
        if ( result > 0) { 
            // got the interrupt; read the temp register now
            broadsheet_papyrus_read_register(PAPYRUS_REGISTER_TMST_VALUE, &temp);
            einkfb_debug("temperature read = %d\n", temp);
        } else {
             einkfb_debug("timed out waiting for temperature read, returning default temp (%d)\n",
                temp);
        }
    } else {
        einkfb_debug("not able to write to papyrus pmic register 0x%x, value 0x%x\n", 
                     PAPYRUS_REGISTER_TMST_CONFIG, PAPYRUS_READ_THERM);
    }
    
    return temp;
}

void broadsheet_papyrus_set_vcom(u8 pmic_vcom) {
    if (!broadsheet_papyrus_write_register(PAPYRUS_REGISTER_VCOM_ADJUST, pmic_vcom)) {    
        einkfb_debug("not able to write to papyrus pmic register 0x%x, value 0x%x\n", 
                     PAPYRUS_REGISTER_VCOM_ADJUST, pmic_vcom);
    }
}

static bool broadsheet_papyrus_pg_ready(void) {
    einkfb_debug("pg_status = 0x%02X\n", pg_status);
    return ( (pg_status & PAPYRUS_PG_STATUS_RAILS_UP) != 0 );
}

bool broadsheet_papyrus_rails_are_up(void) {
    bool result = wait_event_interruptible_timeout(papyrus_pg_event_wait, 
                                            broadsheet_papyrus_pg_ready(), 
                                            PAPYRUS_RAILS_UP_TIMEOUT) > 0;
    papyrus_clear_pg_status();

    if (result <= 0) {
        einkfb_debug("timed out waiting for pg\n");
    }

    return result;
}

#if PRAGMAS
#pragma mark -
#pragma mark Papyrus init and exit functions
#pragma mark -
#endif

void broadsheet_papyrus_init(void) {
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
    if (IS_LUIGI_PLATFORM() && !papyrus_i2c_driver_added) {
        papyrus_i2c_driver_added = true;
        i2c_add_driver(&papyrus_i2c_driver);
        
        // say that we only want the temperature interrupt
        broadsheet_papyrus_write_register(PAPYRUS_REGISTER_INT_ENABLE1, 0);
        broadsheet_papyrus_write_register(PAPYRUS_REGISTER_INT_ENABLE2, PAPYRUS_INT_MASK_TEMP_READY);
    }
#endif
}

void broadsheet_papyrus_exit(void) {
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
    if (papyrus_i2c_driver_added) {
        i2c_del_driver(&papyrus_i2c_driver);
        papyrus_i2c_driver_added = false;
    }
#endif
}

bool broadsheet_papyrus_present(void) {
    return NULL != papyrus_i2c;
}
