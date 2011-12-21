/*
 *  linux/drivers/video/eink/broadsheet/broadsheet_waveform.c --
 *  eInk frame buffer device HAL broadsheet waveform code
 *
 *      Copyright (C) 2005-2010 Amazon Technologies, Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#define waveform_version_string_max 64
#define waveform_unknown_char       '?'
#define waveform_seperator          "_"

enum run_types
{
    rt_b, rt_t, rt_p, rt_q, rt_a, rt_c, rt_d, rt_e, rt_f, rt_g, rt_h, rt_i, rt_j, rt_k, rt_l, 
    rt_m, rt_n, unknown_run_type, num_run_types,
    
    rt_dummy = -1
};
typedef enum run_types run_types;

enum wf_types
{
    wft_wx, wft_wy, wft_wp, wft_wz, wft_wq, wft_ta, wft_wu, wft_tb, wft_td, wft_wv, wft_wt, wft_te,
    wft_xa, wft_xb, wft_we, wft_wd, wft_xc, wft_ve, wft_xd, wft_xe, wft_xf, wft_wj, wft_wk, wft_wl,
    wft_vj, unknown_wf_type, num_wf_types
};
typedef enum wf_types wf_types;

enum platforms
{
    matrix_2_0_unsupported, matrix_2_1_unsupported, matrix_Vizplex_100, matrix_Vizplex_110,
    matrix_Vizplex_110A, matrix_Vizplex_unknown, matrix_Vizplex_220, matrix_Vizplex_250,
    matrix_Vizplex_220E, unknown_platform, num_platforms
};
typedef enum platforms platforms;

enum panel_sizes
{
    five_oh, six_oh, six_three, eight_oh, nine_seven, nine_nine, unknown_panel_size, num_panel_sizes,
    five_inch = 50, six_inch = 60, six_three_inch = 63, eight_inch = 80,
    nine_seven_inch = 97, nine_nine_inch = 99
};
typedef enum panel_sizes panel_sizes;

enum tuning_biases
{
    standard_bias, increased_ds_blooming_1, increased_ds_blooming_2, improved_temperature_range,
    unknown_tuning_bias, num_tuning_biases
};
typedef enum tuning_biases tuning_biases;

static char waveform_version_string[waveform_version_string_max];

static char *run_type_names[num_run_types] =
{
    "B", "T", "P", "Q", "A", "C", "D", "E", "F", "G", "H", "I",
    "J", "K", "L", "M", "N", "?"
};

static char *wf_type_names[num_wf_types] =
{
    "WX", "WY", "WP", "WZ", "WQ", "TA", "WU", "TB", "TD", "WV",
    "WT", "TE", "??", "??", "WE", "WD", "??", "VE", "??", "??",
    "??", "WJ", "WK", "WL", "VJ", "??"
};

static char *platform_names[num_platforms] =
{
    "????", "????", "V100", "V110", "110A", "????", "V220", "V250",
    "220E", "????"
};

static char *panel_size_names[num_panel_sizes] =
{
    "50", "60", "63", "80", "97", "99", "??"
};

static char *tuning_bias_names[num_tuning_biases] =
{
    "S", "D", "D", "T", "?"
};

#define IS_VIZP(v) ((matrix_Vizplex_110  == (v)) || (matrix_Vizplex_110A == (v)) || \
                    (matrix_Vizplex_220  == (v)) || (matrix_Vizplex_250  == (v)) || \
                    (matrix_Vizplex_220E == (v)))

#define IN_RANGE(n, m, M) (((n) >= (m)) && ((n) <= (M)))

#define has_valid_serial_number(r, s) \
    ((0 != (s)) && (UINT_MAX != (s)) && (rt_t != (r)) && (rt_q != (r)))

#define has_valid_fpl_rate(r) \
    ((EINK_FPL_RATE_50 == (r)) || (EINK_FPL_RATE_60 == (r)) || (EINK_FPL_RATE_85 == (r)))

#define BS_CHECKSUM(c1, c2) (((c2) << 16) | (c1))

void broadsheet_get_waveform_info(broadsheet_waveform_info_t *info)
{
    if ( info )
    {
        bs_flash_select saved_flash_select = broadsheet_get_flash_select();
        unsigned char checksum1, checksum2;

        broadsheet_set_flash_select(bs_flash_waveform);

        broadsheet_read_from_flash_byte(EINK_ADDR_WAVEFORM_VERSION,     &info->waveform_version);
        broadsheet_read_from_flash_byte(EINK_ADDR_WAVEFORM_SUBVERSION,  &info->waveform_subversion);
        broadsheet_read_from_flash_byte(EINK_ADDR_WAVEFORM_TYPE,        &info->waveform_type);
        broadsheet_read_from_flash_byte(EINK_ADDR_RUN_TYPE,             &info->run_type);
        broadsheet_read_from_flash_byte(EINK_ADDR_FPL_PLATFORM,         &info->fpl_platform);
        broadsheet_read_from_flash_byte(EINK_ADDR_FPL_SIZE,             &info->fpl_size);
        broadsheet_read_from_flash_byte(EINK_ADDR_ADHESIVE_RUN_NUM,     &info->adhesive_run_number);
        broadsheet_read_from_flash_byte(EINK_ADDR_MODE_VERSION,         &info->mode_version);
        broadsheet_read_from_flash_byte(EINK_ADDR_MFG_CODE,             &info->mfg_code);
        broadsheet_read_from_flash_byte(EINK_ADDR_CHECKSUM1,            &checksum1);
        broadsheet_read_from_flash_byte(EINK_ADDR_CHECKSUM2,            &checksum2);
        broadsheet_read_from_flash_byte(EINK_ADDR_WAVEFORM_TUNING_BIAS, &info->tuning_bias);
        broadsheet_read_from_flash_byte(EINK_ADDR_FPL_RATE,             &info->fpl_rate);

        broadsheet_read_from_flash_short(EINK_ADDR_FPL_LOT,             &info->fpl_lot);

        broadsheet_read_from_flash_long(EINK_ADDR_CHECKSUM,             &info->checksum);
        broadsheet_read_from_flash_long(EINK_ADDR_FILESIZE,             &info->filesize);
        broadsheet_read_from_flash_long(EINK_ADDR_SERIAL_NUMBER,        &info->serial_number);

        broadsheet_set_flash_select(saved_flash_select);

        if ( 0 == info->filesize )
            info->checksum = BS_CHECKSUM(checksum1, checksum2);

	    einkfb_debug(   "\n"
                        " Waveform version:  0x%02X\n"
                        "       subversion:  0x%02X\n"
                        "             type:  0x%02X\n"
                        "         run type:  0x%02X\n"
                        "     mode version:  0x%02X\n"
                        "      tuning bias:  0x%02X\n"
                        "       frame rate:  0x%02X\n"
                        "\n"
                        "     FPL platform:  0x%02X\n"
                        "              lot:  0x%04X\n"
                        "             size:  0x%02X\n"
                        " adhesive run no.:  0x%02X\n"
                        "\n"
                        "        File size:  0x%08lX\n"
                        "         Mfg code:  0x%02X\n"
                        "       Serial no.:  0x%08lX\n"
                        "         Checksum:  0x%08lX\n",

                        info->waveform_version,
                        info->waveform_subversion,
                        info->waveform_type,
                        info->run_type,
                        info->mode_version,
                        info->tuning_bias,
                        info->fpl_rate,

                        info->fpl_platform,
                        info->fpl_lot,
                        info->fpl_size,
                        info->adhesive_run_number,

                        info->filesize,
                        info->mfg_code,
                        info->serial_number,
                        info->checksum);
    }
}

void broadsheet_get_waveform_version(broadsheet_waveform_t *waveform)
{
    if ( waveform )
    {
        broadsheet_waveform_info_t info; broadsheet_get_waveform_info(&info);

        waveform->version       = info.waveform_version;
        waveform->subversion    = info.waveform_subversion;
        waveform->type          = info.waveform_type;
        waveform->run_type      = info.run_type;
        waveform->mode_version  = !IS_VIZP(info.fpl_platform) ? 0 : info.mode_version;
        waveform->mfg_code      = info.mfg_code;
        waveform->tuning_bias   = info.tuning_bias;
        waveform->fpl_rate      = info.fpl_rate;
        waveform->serial_number = info.serial_number;

        waveform->parse_wf_hex  = info.filesize ? true : false;
    }
}

void broadsheet_get_fpl_version(broadsheet_fpl_t *fpl)
{
    if ( fpl )
    {
        broadsheet_waveform_info_t info; broadsheet_get_waveform_info(&info);

        fpl->platform            = info.fpl_platform;
        fpl->size                = info.fpl_size;
        fpl->adhesive_run_number = !IS_VIZP(info.fpl_platform) ? 0 : info.adhesive_run_number;
        fpl->lot                 = info.fpl_lot;
    }
}

char *broadsheet_get_waveform_version_string(void)
{
    char temp_string[waveform_version_string_max];
    broadsheet_waveform_t waveform;
    broadsheet_fpl_t fpl;

    panel_sizes panel_size;
    run_types run_type;

    int valid_serial_number,
        valid_fpl_rate;

    // Get the waveform version info and clear the waveform
    // version string.
    //
    broadsheet_get_waveform_version(&waveform);
    broadsheet_get_fpl_version(&fpl);

    waveform_version_string[0] = '\0';
    run_type = waveform.run_type;
    panel_size = fpl.size;

    // Build up a waveform version string in the following way:
    //
    //      <FPL PLATFORM>_<RUN TYPE><FPL LOT NUMBER>_<FPL SIZE>_
    //      <WF TYPE><WF VERSION><WF SUBVERSION>_<TUNING BIAS>
    //      (MFG CODE, S/N XXX, FRAME RATE)
    //
    // FPL PLATFORM
    //
    switch ( fpl.platform )
    {
        case matrix_Vizplex_100:
        case matrix_Vizplex_110:
        case matrix_Vizplex_110A:
        case matrix_Vizplex_220:
        case matrix_Vizplex_250:
        case matrix_Vizplex_220E:
            strcat(waveform_version_string, platform_names[fpl.platform]);
        break;

        case unknown_platform:
        default:
            strcat(waveform_version_string, platform_names[unknown_platform]);
        break;
    }

    if ( IS_VIZP(fpl.platform) )
        strcat(waveform_version_string, waveform_seperator);

    // RUN TYPE
    //
    if ( IN_RANGE(run_type, rt_b, rt_n) )
        strcat(waveform_version_string, run_type_names[run_type]);
    else
        strcat(waveform_version_string, run_type_names[unknown_run_type]);

    // FPL LOT NUMBER
    //
    sprintf(temp_string, "%03d", (fpl.lot % 1000));
    strcat(waveform_version_string, temp_string);

    // ADHESIVE RUN NUMBER
    //
    if ( !IS_VIZP(fpl.platform) )
    {
        sprintf(temp_string, "%02d", (fpl.adhesive_run_number % 100));
        strcat(waveform_version_string, temp_string);
    }

    strcat(waveform_version_string, waveform_seperator);

    // FPL SIZE
    //
    switch ( fpl.size )
    {
        case five_inch:
            panel_size = five_oh;
        break;

        case six_inch:
            panel_size = six_oh;
        break;

        case six_three_inch:
            panel_size = six_three;
        break;

        case eight_inch:
            panel_size = eight_oh;
        break;

        case nine_seven_inch:
            panel_size = nine_seven;
        break;
        
        case nine_nine_inch:
            panel_size = nine_nine;
        break;

        default:
            panel_size = unknown_panel_size;
        break;
    }

    switch ( panel_size )
    {
        case five_oh:
        case six_oh:
        case six_three:
        case eight_oh:
        case nine_seven:
        case nine_nine:
            strcat(waveform_version_string, panel_size_names[panel_size]);
        break;

        case unknown_panel_size:
        default:
            strcat(waveform_version_string, panel_size_names[unknown_panel_size]);
        break;
    }
    strcat(waveform_version_string, waveform_seperator);

    // WF TYPE
    //
    switch ( waveform.type )
    {
        case wft_wx:
        case wft_wy:
        case wft_wp:
        case wft_wz:
        case wft_wq:
        case wft_ta:
        case wft_wu:
        case wft_tb:
        case wft_td:
        case wft_wv:
        case wft_wt:
        case wft_te:
        case wft_we:
        case wft_ve:
        case wft_wj:
        case wft_wk:
        case wft_wl:
        case wft_vj:
            strcat(waveform_version_string, wf_type_names[waveform.type]);
        break;

        case unknown_wf_type:
        default:
            strcat(waveform_version_string, wf_type_names[unknown_wf_type]);
        break;
    }

    // WF VERSION
    //
    if ( waveform.parse_wf_hex )
        sprintf(temp_string, "%02X", waveform.version);
    else
        sprintf(temp_string, "%02d", (waveform.version % 100));

    strcat(waveform_version_string, temp_string);

    // WF SUBVERSION
    //
    if ( waveform.parse_wf_hex )
        sprintf(temp_string, "%02X", waveform.subversion);
    else
        sprintf(temp_string, "%02d", (waveform.subversion % 100));

    strcat(waveform_version_string, temp_string);
    strcat(waveform_version_string, waveform_seperator);

    // TUNING BIAS
    //
    switch ( waveform.tuning_bias )
    {
        case standard_bias:
        case increased_ds_blooming_1:
        case increased_ds_blooming_2:
        case improved_temperature_range:
            strcat(waveform_version_string, tuning_bias_names[waveform.tuning_bias]);
        break;

        case unknown_tuning_bias:
        default:
            strcat(waveform_version_string, tuning_bias_names[unknown_tuning_bias]);
        break;
    }

    // MFG CODE, SERIAL NUMBER, FRAME RATE
    //
    valid_serial_number = has_valid_serial_number(run_type, waveform.serial_number);
    valid_fpl_rate = has_valid_fpl_rate(waveform.fpl_rate);
    temp_string[0] = '\0';

    if ( valid_serial_number && valid_fpl_rate )
        sprintf(temp_string, " (M%02X, S/N %lu, %02XHz)", waveform.mfg_code, waveform.serial_number,
            waveform.fpl_rate);
    
    else if ( valid_fpl_rate )
            sprintf(temp_string, " (M%02X, %02XHz)", waveform.mfg_code,
                waveform.fpl_rate);
        
        else if ( valid_serial_number )
                sprintf(temp_string, " (M%02X, S/N %lu)", waveform.mfg_code,
                    waveform.serial_number);

    if ( strlen(temp_string) )
        strcat(waveform_version_string, temp_string);

    // All done.
    //
    return ( waveform_version_string );
}

bool broadsheet_waveform_valid(void)
{
    char *waveform_version = broadsheet_get_waveform_version_string();
    bool result = true;

    if ( strchr(waveform_version, waveform_unknown_char) )
    {
        einkfb_print_error("Unrecognized values in waveform header\n");
        einkfb_debug("waveform_version = %s\n", waveform_version);
        result = false;
    }

    return ( result );
}
