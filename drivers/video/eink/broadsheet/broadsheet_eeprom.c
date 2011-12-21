/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_eeprom.c
 *  -- eInk frame buffer device HAL broadsheet panel EEPROM
 *
 *      Copyright (C) 2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#if PRAGMAS
    #pragma mark Definitions/Globals
    #pragma mark -
#endif

#define EEPROM_ADDR 0x50

enum eeprom_characters
{
    zero = 0x0, one, two, three, four, five, six, seven, eight, nine,
    underline = 0x0a, dot = 0x0b, negative = 0x0c,
    _a = 0xcb, _b, _c, _d, _e, _f, _g, _h, _i, _j, _k, _l, _m, _n,
               _o, _p, _q, _r, _s, _t, _u, _v, _w, _x, _y, _z,
    
    _A = 0xe5, _B, _C, _D, _E, _F, _G, _H, _I, _J, _K, _L, _M, _N,
               _O, _P, _Q, _R, _S, _T, _U, _V, _W, _X, _Y, _Z
};
typedef enum eeprom_characters eeprom_characters;

static int eeprom_address = EEPROM_BASE;

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet Broadsheet EEPROM API
    #pragma mark -
#endif

// /sys/devices/platform/eink_fb.0/eeprom_addr (read/write)
static ssize_t show_eeprom_addr(FB_DSHOW_PARAMS) {
    char *curr = buf;
    
    curr += sprintf(curr, "EEPROM addr to read from: 0x%02X\n", eeprom_address);
    curr += sprintf(curr, "\n");
    
    return curr - buf;
}

static ssize_t store_eeprom_addr(FB_DSTOR_PARAMS) {
    int value = 0;
    if (sscanf(buf, "%x", &value) <= 0) {
        einkfb_print_info("Could not store the EEPROM address\n");
        return -EINVAL;
    }

    if (!IN_RANGE(value, EEPROM_BASE, EEPROM_LAST)) {
        einkfb_print_info("Invalid EEPROM address, range is 0x%02X - 0x%02X\n", EEPROM_BASE, EEPROM_LAST);
        return -EINVAL;
    }

    eeprom_address = value;

    return count;
}

// /sys/devices/platform/eink_fb.0/eeprom_value (read-only)
static ssize_t show_eeprom_value(FB_DSHOW_PARAMS) {
    char *curr = NULL;
    u8 value  = 0;

    if (EINKFB_SUCCESS == EINKFB_LOCK_ENTRY()) {
        broadsheet_eeprom_read(eeprom_address, &value, 1);
        EINKFB_LOCK_EXIT();
    }

    curr = buf;
    curr += sprintf(curr, "EEPROM address : 0x%02X\n", eeprom_address);
    curr += sprintf(curr, "\n");
    curr += sprintf(curr, " Value: %c\n", (char)value);
    curr += sprintf(curr, "\n");

    return curr - buf;
}

// /sys/devices/platform/eink_fb.0/eeprom_whole (read-only)
static ssize_t show_eeprom_whole(FB_DSHOW_PARAMS) {
    u8 buffer[EEPROM_SIZE];
    char *curr = NULL;    
    int i = 0, j = 0;
    
    if (EINKFB_SUCCESS == EINKFB_LOCK_ENTRY()) {
        broadsheet_eeprom_read(EEPROM_BASE, buffer, EEPROM_SIZE);
        EINKFB_LOCK_EXIT();
    }    

    curr = buf;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            curr += sprintf(curr, "%c", (char)buffer[(i*16) + j]);
        }
        curr += sprintf(curr, "\n");
    }

    return curr - buf;
}

static DEVICE_ATTR(eeprom_addr,     DEVICE_MODE_RW, show_eeprom_addr,   store_eeprom_addr);
static DEVICE_ATTR(eeprom_value,    DEVICE_MODE_R,  show_eeprom_value,  NULL);
static DEVICE_ATTR(eeprom_whole,    DEVICE_MODE_R,  show_eeprom_whole,  NULL);

void broadsheet_eeprom_create_proc_enteries(void) {
    struct einkfb_info info; einkfb_get_info(&info);
    FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_eeprom_addr);
    FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_eeprom_value);
    FB_DEVICE_CREATE_FILE(&info.dev->dev, &dev_attr_eeprom_whole);
}

void broadsheet_eeprom_remove_proc_enteries(void) {
    struct einkfb_info info; einkfb_get_info(&info);
    device_remove_file(&info.dev->dev, &dev_attr_eeprom_addr);
    device_remove_file(&info.dev->dev, &dev_attr_eeprom_value);
    device_remove_file(&info.dev->dev, &dev_attr_eeprom_whole);
}

static void broadsheet_eeprom_translate(u8 *buffer, int to_read) {
    int i = 0;
    
    for (i = 0; i < to_read; i++) {
        if (buffer[i] >= _a && buffer[i] <= _z) {
            buffer[i] = 'a' + (buffer[i] - _a);
        } else if (buffer[i] >= _A && buffer[i] <= _Z) {
            buffer[i] = 'A' + (buffer[i] - _A);
        } else if (/* buffer[i] >= zero && */ buffer[i] <= nine) {
            buffer[i] = '0' + (buffer[i] - zero);
        } else if (buffer[i] == underline) {
            buffer[i] = '_';
        } else if (buffer[i] == dot) {
            buffer[i] = '.';
        } else if (buffer[i] == negative) {
            buffer[i] = '-';
        } else {
            buffer[i] = EEPROM_CHAR_UNKNOWN;
        }
    }
}

static void eeprom_read(u16 start_addr, u8 *buffer, int to_read) {
    int i      = 0;
    u16 tmp    = 0;
    
    einkfb_debug("start = %d, size = %d\n", start_addr, to_read);
    
    // Send {Start}{2C ID}{W}
    bs_cmd_wr_reg(0x21e, ((EEPROM_ADDR<<1)|0));
    bs_cmd_wr_reg(0x21a, 0x31);
    do {
        tmp = bs_cmd_rd_reg(0x218);
    } while (1 == (tmp & 0x1));
    
    // Send {8 bit address}
    bs_cmd_wr_reg(0x21e, start_addr);
    bs_cmd_wr_reg(0x21a, 0x01);
    do {
        tmp = bs_cmd_rd_reg(0x218);
    } while (1 == (tmp & 0x1));
    
    // Send {Start}{2C ID}{R}
    bs_cmd_wr_reg(0x21e, ((EEPROM_ADDR<<1)|1));
    bs_cmd_wr_reg(0x21a, 0x31);
    do {
        tmp = bs_cmd_rd_reg(0x218);
    } while (1 == (tmp & 0x1));
    
    for (i = 0; i < to_read - 1; i++) {
        // Send {Read Data + ACK}
        bs_cmd_wr_reg(0x21a, 0x03);
        do {
            tmp = bs_cmd_rd_reg(0x218);
        } while (1 == (tmp & 0x1));
        buffer[i] = bs_cmd_rd_reg(0x21c) & 0xff;
    }
    
    // Send {Read Data}{Stop}
    bs_cmd_wr_reg(0x21a, 0x17);
    do {
        tmp = bs_cmd_rd_reg(0x218);
    } while (1 == (tmp & 0x1));
    buffer[i] = bs_cmd_rd_reg(0x21c) & 0xff;
}

int broadsheet_eeprom_read(u32 start_addr, u8 *buffer, int to_read) {
    int result = -1;
    
    if (buffer && IN_RANGE(to_read, 1, EEPROM_SIZE)) {
        bs_flash_select saved_flash_select;
        switch(BS_PANEL_DATA_WHICH()) {
            case bs_panel_data_eeprom:
                eeprom_read((u16)start_addr, buffer, to_read);
                break;
                
            case bs_panel_data_flash:
                saved_flash_select = broadsheet_get_flash_select();
                broadsheet_set_flash_select(bs_flash_commands);
                
                broadsheet_read_from_flash((FLASH_BASE+start_addr), buffer, to_read);
                broadsheet_set_flash_select(saved_flash_select);
                break;
                
            default:
                einkfb_debug("location of panel data not specified\n");
                break;
        }
        
        broadsheet_eeprom_translate((u8 *)buffer, to_read);
        result = to_read;
    }

    return result;
}

bool broadsheet_supports_eeprom_read(void) {
    return IS_LUIGI_PLATFORM();
}
