/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_pmic.c --
 *  eInk frame buffer device HAL PMIC hardware access
 *
 *      Copyright (C) 2009 Amazon Technologies
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include "../hal/einkfb_hal.h"
#include "broadsheet_papyrus.h"
#include "broadsheet.h"

#if PRAGMAS
    #pragma mark Definitions/Globals
#endif

#define BS_PMIC_TEMP_MIN          -10     // -10C
#define BS_PMIC_TEMP_MAX          85      //  85C
#define BS_PMIC_TEMP_DEFAULT      PAPYRUS_TEMP_DEFAULT

#define BS_PMIC_VCOM_MIN          0     // bs_pmic_vcom_conversion[  0] ->  0.00V
#define BS_PMIC_VCOM_MAX          255   // bs_pmic_vcom_conversion[255] -> -2.75V
#define BS_PMIC_VCOM_DEFAULT      125   // bs_pmic_vcom_conversion[125] -> -1.25V

#define BS_PMIC_PG_TIMEOUT        16      // msec

//#define BS_PMIC_VCOM_CONVERSION ((256 * 100)/275)
//#define BS_PMIC_VCOM(v)         (((v) * BS_PMIC_VCOM_CONVERSION)/100)
//
// 276 input values    -> 256 output values
//
// A couple of real-world examples:
//
// VCOM is -1.69 (169) -> 156
// VCOM is -1.56 (156) -> 144
//
static u8 bs_pmic_vcom_conversion[276] = 
{
//    0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16   17   18      <-- -(VCOM / 100) (real VCOM output from Papyrus)
      0,   1,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  14,  15,  16,  //  --> PMIC_VCOM    (register VCOM value to set for Papyrus)

//   19   20   21   22   23   24   25   26   27   28   29   30   31   32   33   34   35   36   37
     17,  18,  19,  20,  21,  22,  23,  24,  25,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,

//   38   39   40   41   42   43   44   45   46   47   48   49   50   51   52   53   54   55   56
     35,  36,  37,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  49,  50,  51,

//   57   58   59   60   61   62   63   64   65   66   67   68   69   70   71   72   73   74   75
     52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  63,  64,  65,  66,  67,  68,  69,

//   76   77   78   79   80   81   82   83   84   85   86   87   88   89   90   91   92   93   94
     70,  71,  72,  73,  74,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,

//   95   96   97   98   99  100  101  102  103  104  105  106  107  108  109  110  111  112  113
     87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99, 100, 100, 101, 102, 103, 104,

//  114  115  116  117  118  119  120  121  122  123  124  125  126  127  128  129  130  131  132
    105, 106, 107, 108, 109, 110, 111, 112, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122,

//  133  134  135  136  137  138  139  140  141  142  143  144  145  146  147  148  149  150  151
    123, 124, 125, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 138, 139,

//  152  153  154  155  156  157  158  159  160  161  162  163  164  165  166  167  168  169  170
    140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 151, 152, 153, 154, 155, 156, 157,

//  171  172  173  174  175  176  177  178  179  180  181  182  183  184  185  186  187  188  189  
    158, 159, 160, 161, 162, 163, 164, 165, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,

//  190  191  192  193  194  195  196  197  198  199  200  201  202  203  204  205  206  207  208
    176, 177, 178, 179, 180, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193,

//  209  210  211  212  213  214  215  216  217  218  219  220  221  222  223  224  225  226  227
    194, 195, 196, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211,

//  228  229  230  231  232  233  234  235  236  237  238  239  240  241  242  243  244  245  246
    211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 223, 224, 225, 226, 227, 228,

//  247  248  249  250  251  252  253  254  255  256  257  258  259  260  261  262  263  264  265
    229, 230, 231, 232, 233, 234, 235, 236, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,

//  266  267  268  269  270  271  272  273  274  275
    247, 248, 249, 249, 250, 251, 252, 253, 254, 255
};
#define BS_PMIC_VCOM(v)           bs_pmic_vcom_conversion[(unsigned int)v]

#define BS_PMIC_TEMP_TIMEOUT      30000  // 30 seconds

static char *power_state_strings[] =
{
    "init",     // bs_pmic_power_state_uninitialized

    "active",   // bs_pmic_power_state_active
    "standby",  // bs_pmic_power_state_standby
    "sleep"     // bs_pmic_power_state_sleep 
};

// global temperature variable; set to default value
// the temperature gets updated periodically
static s8 bs_pmic_temp = BS_PMIC_TEMP_DEFAULT;
EINKFB_MUTEX(bs_pmic_temp_sem);

// power state of the pmic
bs_pmic_power_states present_state = bs_pmic_power_state_uninitialized;

static bool init_done = false;

static s8 bs_read_temp(void) {
    s8 value = BS_PMIC_TEMP_DEFAULT;
    bs_pmic_power_states old_state;

    // only allow one person at a time to read temp
    einkfb_down(&bs_pmic_temp_sem);

    // save the old state first
    old_state = present_state;
    // set the state to active
    bs_pmic_set_power_state(bs_pmic_power_state_active);
    // read the temp
    value = broadsheet_papyrus_read_temp();
    // set the state back to what it was before
    bs_pmic_set_power_state(old_state);

    // release
    up(&bs_pmic_temp_sem);

    einkfb_debug("temperature read from pmic = %d\n", value);

    return value;
}

#if PRAGMAS
    #pragma mark -
    #pragma mark Broadsheet PMIC API
    #pragma mark -
#endif

bool bs_pmic_init(void) {
    broadsheet_papyrus_init();
    init_done = true;

    // check to see if we have detected papyrus
    if (broadsheet_papyrus_present()) {
        // turn papyrus on
        bs_pmic_set_power_state(bs_pmic_power_state_active);
    }
    
    return true;
}

void bs_pmic_done(void) {
    // finish up the temperature tasks and workqueue
    if (broadsheet_papyrus_present()) {
        bs_pmic_set_power_state(bs_pmic_power_state_sleep);
    }

    broadsheet_papyrus_exit();
    init_done = false;
}

bs_pmic_status bs_pmic_get_status(void) {
    bs_pmic_status result = bs_pmic_absent;
    
    if (IS_LUIGI_PLATFORM()) {
        if (init_done) {
            if (broadsheet_papyrus_present())
                result = bs_pmic_present;
        } else {
            result = bs_pmic_not_initialized;
        }
    }
    
    return result;
}

void bs_pmic_create_proc_enteries(void) {
    if (broadsheet_papyrus_present()) {
        broadsheet_papyrus_create_proc_enteries();
    }
}

void bs_pmic_remove_proc_enteries(void) {
    if (broadsheet_papyrus_present()) {
        broadsheet_papyrus_remove_proc_enteries();
    }
}

int bs_pmic_get_temperature(bool fresh) {
    int temp = BS_PMIC_TEMP_DEFAULT;
    
    if (broadsheet_papyrus_present()) {
        if (fresh) {
            bs_pmic_temp = bs_read_temp();
        }

        if (IN_RANGE(bs_pmic_temp, BS_PMIC_TEMP_MIN, BS_PMIC_TEMP_MAX)) {
            temp = (int) bs_pmic_temp;
        }
    }

    return temp;
}

int bs_pmic_get_vcom_default(void) {
    // return regardless of whether papyrus is present or not
    return BS_PMIC_VCOM_DEFAULT;
}

int bs_pmic_set_vcom(int vcom) {
    int result = BS_PMIC_VCOM_DEFAULT;
    
    if (broadsheet_papyrus_present()) {
        u8 pmic_vcom = BS_PMIC_VCOM(BS_PMIC_VCOM_DEFAULT);
        
        if ( IN_RANGE(vcom, BS_PMIC_VCOM_MIN, BS_PMIC_VCOM_MAX) )
        {
             pmic_vcom = BS_PMIC_VCOM(vcom);
             result = vcom;
        }

        einkfb_debug("vcom = %d, pmic_vcom = %u\n", vcom, pmic_vcom);
        broadsheet_papyrus_set_vcom(pmic_vcom);
    }
    
    return result;
}

static char *bs_pmic_get_power_state_string(bs_power_states power_state) {
    char *power_state_string = NULL;
    
    switch (power_state) {
        case bs_pmic_power_state_active:
        case bs_pmic_power_state_standby:
        case bs_pmic_power_state_sleep:
            power_state_string = power_state_strings[power_state];
            break;
        
        case bs_pmic_power_state_uninitialized:
        default:
            power_state_string = power_state_strings[bs_pmic_power_state_uninitialized];
            break;
    }
    
    return power_state_string;
}

void bs_pmic_set_power_state(bs_pmic_power_states power_state) {
    if (broadsheet_papyrus_present() &&
        present_state != power_state) {
        char enable_reg_value = 0;
        bool read_temperature = false;
    
        switch (power_state) {
            case bs_pmic_power_state_active:
                enable_reg_value = 0x9f;
                // set it to read the temperature when we wake up so that
                // the display updates will use the correct temp
                read_temperature = true;
                break;
                
            case bs_pmic_power_state_standby:
            case bs_pmic_power_state_sleep:
                enable_reg_value = 0x5f;
                break;
                
            default:
                einkfb_debug("unknown papyrus pmic power state\n");
                break;
        }
        
        if (enable_reg_value) {
            einkfb_debug_power("power_state = %d (%s) -> %d (%s)\n",
                present_state, bs_pmic_get_power_state_string(present_state),
                power_state,   bs_pmic_get_power_state_string(power_state));

            if(!broadsheet_papyrus_write_register(PAPYRUS_REGISTER_ENABLE, enable_reg_value)) {
                einkfb_debug("not able to write to papyrus pmic register 0x%x, value 0x%x\n", 
                             PAPYRUS_REGISTER_ENABLE, enable_reg_value);
                read_temperature = false;
            } else {
                if (bs_pmic_power_state_active == power_state) {
                    // wait until the rails come up
                    broadsheet_papyrus_rails_are_up();
                }
                present_state = power_state;
            }

            // check if we have to read the temperature
            if (read_temperature) {
                bs_pmic_temp = bs_read_temp();
                read_temperature = false;
            }
        }
    } else {
        if (broadsheet_papyrus_present()) {
            einkfb_debug_power("already in power state %d (%s), skipping\n",
                power_state, bs_pmic_get_power_state_string(power_state));
        }
    }
}
