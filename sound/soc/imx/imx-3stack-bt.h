/*
 * imx-3stack-bt.h  --  Bluetooth PCM driver header file for Freescale IMX
 *
 * Copyright 2008 Freescale  Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef _MXC_BTPCM_H
#define _MXC_BTPCM_H

extern void gpio_activate_bt_audio_port(void);
extern void gpio_inactivate_bt_audio_port(void);
extern const char bt_codec[SND_SOC_CODEC_NAME_SIZE];
extern const char bt_dai[SND_SOC_CODEC_NAME_SIZE];
#endif
