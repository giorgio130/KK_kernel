/*
 * Copyright (C) 2009 Amazon Technologies Inc.
 * Manish Lachwani (lachwani@lab126.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/sysfs.h>
#include <linux/sysdev.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/miscdevice.h>

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#include "wm8960.h"

#define AUDIO_NAME "wm8960"
#define WM8960_VERSION "0.1"

/*
 * Debug
 */
#define WM8960_DEBUG 0

#ifdef WM8960_DEBUG
#define dbg(format, arg...) \
	printk(KERN_ERR ": " format "\n" , ## arg)
#else
#define dbg(format, arg...) do {} while (0)
#endif
#define err(format, arg...) \
	printk(KERN_ERR AUDIO_NAME ": " format "\n" , ## arg)
#define info(format, arg...) \
	printk(KERN_INFO AUDIO_NAME ": " format "\n" , ## arg)
#define warn(format, arg...) \
	printk(KERN_WARNING AUDIO_NAME ": " format "\n" , ## arg)

#define WM8960_OFF_THRESHOLD	0	/* Threshold for powering off the codec */

struct snd_soc_codec_device soc_codec_dev_wm8960;

static int wm8960_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level);

static int wm8960_reboot(struct notifier_block *self, unsigned long action, void *cpu);

static struct notifier_block wm8960_reboot_nb =
{
	.notifier_call	= wm8960_reboot,
};

extern int mx35_luigi_audio_playing_flag;
static atomic_t power_status;
static int power_off_timeout = WM8960_OFF_THRESHOLD; /* in msecs */

/* Send event to userspace for headset */

#define HEADSET_MINOR	165	/* Headset event passing */

static struct miscdevice mxc_headset = {
	HEADSET_MINOR,
	"mxc_ALSA",
	NULL,
};

static int power_status_read(void) 
{
    return atomic_read(&power_status);
}

static void power_status_write(int value)
{
    atomic_set(&power_status, value);
    if (!value)
        mx35_luigi_audio_playing_flag = 0;
}

#define SPKR_ENABLE	0xc0	/* Bits 6 and 7 enable the speakers */
#define HPSWPOLARITY	0x20	/* Turn on the headset polarity bit to detect headset */
#define LOUT2		0x1FF	/* Enable SPKR LEFT Volume and set the levels */
#define ROUT2		0x1FF	/* Enable SPKR RIGHT Volume and set the levels */
#define SPKR_POWER	0x18	/* Enable Power for right and left speakers */
#define LOUT1		0x16d	/* Enable HEADPHONE LEFT Volume */
#define ROUT1		0x16d	/* Enable HEADPHONE RIGHT Volume */
#define LDAC		0x1f5	/* Left DAC volume */
#define RDAC		0x1f5	/* Right DAC volume */
#define POWER1		0xc0	/* Power register 1 */
#define POWER2		0x1fb	/* Power register 2 */
#define POWER3		0x0c	/* Power register 3 */
#define SPKR_CONFIG	0xF7	/* Speaker Config */
#define CLASSD3_CONFIG	0x80	/* CLASSD Control 3 */

#define HSDET_THRESHOLD	500	/* 500ms debounce */

/* R25 - Power 1 */
#define WM8960_VREF      0x40

/* R28 - Anti-pop 1 */
#define WM8960_POBCTRL   0x80
#define WM8960_BUFDCOPEN 0x10
#define WM8960_BUFIOEN   0x08
#define WM8960_SOFT_ST   0x04
#define WM8960_HPSTBY    0x01

/* R29 - Anti-pop 2 */
#define WM8960_DISOP     0x40

/*
 * wm8960 register cache
 * We can't read the WM8960 register space when we are
 * using 2 wire for device control, so we cache them instead.
 */
static const u16 wm8960_reg[WM8960_CACHEREGNUM] = {
	0x0097, 0x0097, 0x0000, 0x0000,
	0x0000, 0x0008, 0x0000, 0x000a,
	0x01c0, 0x0000, 0x00ff, 0x00ff,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x007b, 0x0100, 0x0032,
	0x0000, 0x00c3, 0x00c3, 0x01c0,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0100, 0x0100, 0x0050, 0x0050,
	0x0050, 0x0050, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0040, 0x0000,
	0x0000, 0x0050, 0x0050, 0x0000,
	0x0002, 0x0037, 0x004d, 0x0080,
	0x0008, 0x0031, 0x0026, 0x00e9,
};

extern int gpio_headset_status(void);
extern void gpio_activate_audio_ports(void);
extern int gpio_headset_irq(void);

/*
 * WM8960 Codec /sys
 */

/* Default is Reg #0. Value of > 55 means dump all the codec registers */
static int wm8960_reg_number = 0;

struct snd_soc_codec *wm8960_codec;
static inline unsigned int wm8960_read_reg_cache(struct snd_soc_codec *codec,
				unsigned int reg);
static int wm8960_write(struct snd_soc_codec *codec, unsigned int reg,
				unsigned int value);

static void wm8960_configure_capture(struct snd_soc_codec *codec);

/*
 * Debug - dump all the codec registers
 */
static void dump_regs(struct snd_soc_codec *codec)
{
	int i = 0;
	u16 *cache = codec->reg_cache;

	for (i=0; i < WM8960_CACHEREGNUM; i++) {
		printk("Register %d - Value:%x\n", i, cache[i]);
	}
}

DEFINE_MUTEX(wm8960_lock);

/*
 * sysfs entries for the codec
 */
static ssize_t wm8960_reg_store(struct sys_device *dev, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Could not store the codec register value\n");
		return -EINVAL;
	}

	wm8960_reg_number = value;

	return size;
}

static ssize_t wm8960_reg_show(struct sys_device *dev, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "WM8960 Register Number: %d\n", wm8960_reg_number);
	curr += sprintf(curr, "\n");

	return curr - buf;
}

static SYSDEV_ATTR(wm8960_reg, 0644, wm8960_reg_show, wm8960_reg_store);

static struct sysdev_class wm8960_reg_sysclass = {
	.name	= "wm8960_reg",
};

static struct sys_device device_wm8960_reg = {
	.id	= 0,
	.cls	= &wm8960_reg_sysclass,
};

static ssize_t wm8960_register_show(struct sys_device *dev, char *buf)
{
	char *curr = buf;

	if (wm8960_reg_number >= WM8960_CACHEREGNUM) {
		/* dump all the regs */
		curr += sprintf(curr, "WM8960 Registers\n");
		curr += sprintf(curr, "\n");
		dump_regs(wm8960_codec);
	}
	else {
		curr += sprintf(curr, "WM8960 Register %d\n", wm8960_reg_number);
		curr += sprintf(curr, "\n");
		curr += sprintf(curr, " Value: 0x%x\n",
				wm8960_read_reg_cache(wm8960_codec, wm8960_reg_number));
		curr += sprintf(curr, "\n");
	}

	return curr - buf;
}

static ssize_t wm8960_register_store(struct sys_device *dev, const char *buf, size_t size)
{
	int value = 0;

	if (wm8960_reg_number >= WM8960_CACHEREGNUM) {
		printk(KERN_ERR "No codec register %d\n", wm8960_reg_number);
		return size;
	}

	if (sscanf(buf, "%x", &value) <= 0) {
		printk(KERN_ERR "Error setting the value in the register\n");
		return -EINVAL;
	}
	
	wm8960_write(wm8960_codec, wm8960_reg_number, value);
	return size;
}
static SYSDEV_ATTR(wm8960_register, 0644, wm8960_register_show, wm8960_register_store);

static struct sysdev_class wm8960_register_sysclass = {
	.name	= "wm8960_register",
};

static struct sys_device device_wm8960_register = {
	.id	= 0,
	.cls	= &wm8960_register_sysclass,
};

static ssize_t headset_status_show(struct sys_device *dev, char *buf)
{
	char *curr = buf;

	curr += sprintf(curr, "%d\n", gpio_headset_status());

	return curr - buf;
}
static SYSDEV_ATTR(headset_status, 0644, headset_status_show, NULL);

static struct sysdev_class headset_status_sysclass = {
	.name	= "headset_status",
};

static struct sys_device device_headset_status = {
	.id	= 0,
	.cls	= &headset_status_sysclass,
};

/*
 * Codec power status
 * NOTE: does not work for the capture case
 */ 

static ssize_t power_status_show(struct sys_device *dev, char *buf)
{
    char *curr = buf;

    curr += sprintf(curr, "%d\n", power_status_read());

    return curr - buf;
}
static SYSDEV_ATTR(wm8960_power_status, 0644, power_status_show, NULL);

static struct sysdev_class wm8960_power_status_sysclass = {
    .name = "wm8960_power_status",
};

static struct sys_device device_wm8960_power_status = {
    .id   = 0,
    .cls  = &wm8960_power_status_sysclass,
};

/*
 * Codec power off timeout
 * NOTE: does not work for the capture case
 */

static ssize_t power_off_timeout_show(struct sys_device *dev, char *buf)
{
    char *curr = buf;
    
    curr += sprintf(curr, "%d\n", power_off_timeout);

    return curr - buf;
}

static ssize_t power_off_timeout_store(struct sys_device *dev, const char *buf, size_t size)
{
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0) {
		printk(KERN_ERR "Error setting the value for timeout\n");
		return -EINVAL;
	}

        power_off_timeout = value;

	return size;
}
static SYSDEV_ATTR(wm8960_power_off_timeout, 0644, power_off_timeout_show, power_off_timeout_store);

static struct sysdev_class wm8960_power_off_timeout_sysclass = {
    .name = "wm8960_power_off_timeout",
};

static struct sys_device device_wm8960_power_off_timeout = {
    .id   = 0,
    .cls  = &wm8960_power_off_timeout_sysclass,
};

/*
 * read wm8960 register cache
 */
static inline unsigned int wm8960_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache;

	mutex_lock(&wm8960_lock);

	cache = codec->reg_cache;
	if (reg == WM8960_RESET) {
		mutex_unlock(&wm8960_lock);
		return 0;
	}

	if (reg >= WM8960_CACHEREGNUM) {
		mutex_unlock(&wm8960_lock);
		return -1;
	}

	mutex_unlock(&wm8960_lock);
	return cache[reg];
}

/*
 * write wm8960 register cache
 */
static inline void wm8960_write_reg_cache(struct snd_soc_codec *codec,
	u16 reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg >= WM8960_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the WM8960 register space
 */
static int wm8960_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];
	int ret = 0;

        mutex_lock(&wm8960_lock);

	/* data is
	 *   D15..D9 WM8960 register offset
	 *   D8...D0 register data
	 */
	data[0] = (reg << 1) | ((value >> 8) & 0x0001);
	data[1] = value & 0x01ff;

	wm8960_write_reg_cache (codec, reg, value);

	ret = codec->hw_write(codec->control_data, data, 2);

	if (ret == 2) {
		mutex_unlock(&wm8960_lock);
		return 0;
	}
	else {	
		mutex_unlock(&wm8960_lock);
		return ret;
	}
}

#define wm8960_reset(c)	wm8960_write(c, WM8960_RESET, 0)

/* enumerated controls */
static const char *wm8960_deemph[] = {"None", "32Khz", "44.1Khz", "48Khz"};
static const char *wm8960_polarity[] = {"No Inversion", "Left Inverted",
	"Right Inverted", "Stereo Inversion"};
static const char *wm8960_3d_upper_cutoff[] = {"High", "Low"};
static const char *wm8960_3d_lower_cutoff[] = {"Low", "High"};
static const char *wm8960_alcfunc[] = {"Off", "Right", "Left", "Stereo"};
static const char *wm8960_alcmode[] = {"ALC", "Limiter"};

static const struct soc_enum wm8960_enum[] = {
	SOC_ENUM_SINGLE(WM8960_DACCTL1, 1, 4, wm8960_deemph),
	SOC_ENUM_SINGLE(WM8960_DACCTL1, 5, 4, wm8960_polarity),
	SOC_ENUM_SINGLE(WM8960_DACCTL2, 5, 4, wm8960_polarity),
	SOC_ENUM_SINGLE(WM8960_3D, 6, 2, wm8960_3d_upper_cutoff),
	SOC_ENUM_SINGLE(WM8960_3D, 5, 2, wm8960_3d_lower_cutoff),
	SOC_ENUM_SINGLE(WM8960_ALC1, 7, 4, wm8960_alcfunc),
	SOC_ENUM_SINGLE(WM8960_ALC3, 8, 2, wm8960_alcmode),
};

static const DECLARE_TLV_DB_SCALE(adc_tlv, -9700, 50, 0);
static const DECLARE_TLV_DB_SCALE(dac_tlv, -12700, 50, 1);
static const DECLARE_TLV_DB_SCALE(bypass_tlv, -2100, 300, 0);
static const DECLARE_TLV_DB_SCALE(out_tlv, -12100, 100, 1);

static const struct snd_kcontrol_new wm8960_snd_controls[] = {
SOC_DOUBLE_R_TLV("Capture Volume", WM8960_LINVOL, WM8960_RINVOL,
		 0, 63, 0, adc_tlv),
SOC_DOUBLE_R("Capture Volume ZC Switch", WM8960_LINVOL, WM8960_RINVOL,
	6, 1, 0),
SOC_DOUBLE_R("Capture Switch", WM8960_LINVOL, WM8960_RINVOL,
	7, 1, 0),

SOC_DOUBLE_R_TLV("Playback Volume", WM8960_LDAC, WM8960_RDAC,
		 0, 255, 0, dac_tlv),

SOC_DOUBLE_R_TLV("Headphone Playback Volume", WM8960_LOUT1, WM8960_ROUT1,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Headphone Playback ZC Switch", WM8960_LOUT1, WM8960_ROUT1,
	7, 1, 0),

SOC_DOUBLE_R_TLV("Speaker Playback Volume", WM8960_LOUT2, WM8960_ROUT2,
		 0, 127, 0, out_tlv),
SOC_DOUBLE_R("Speaker Playback ZC Switch", WM8960_LOUT2, WM8960_ROUT2,
	7, 1, 0),
SOC_SINGLE("Speaker DC Volume", WM8960_CLASSD3, 3, 5, 0),
SOC_SINGLE("Speaker AC Volume", WM8960_CLASSD3, 0, 5, 0),

SOC_SINGLE("PCM Playback -6dB Switch", WM8960_DACCTL1, 7, 1, 0),
SOC_ENUM("ADC Polarity", wm8960_enum[1]),
SOC_ENUM("Playback De-emphasis", wm8960_enum[0]),
SOC_SINGLE("ADC High Pass Filter Switch", WM8960_DACCTL1, 0, 1, 0),

SOC_ENUM("DAC Polarity", wm8960_enum[2]),

SOC_ENUM("3D Filter Upper Cut-Off", wm8960_enum[3]),
SOC_ENUM("3D Filter Lower Cut-Off", wm8960_enum[4]),
SOC_SINGLE("3D Volume", WM8960_3D, 1, 15, 0),
SOC_SINGLE("3D Switch", WM8960_3D, 0, 1, 0),

SOC_ENUM("ALC Function", wm8960_enum[5]),
SOC_SINGLE("ALC Max Gain", WM8960_ALC1, 4, 7, 0),
SOC_SINGLE("ALC Target", WM8960_ALC1, 0, 15, 1),
SOC_SINGLE("ALC Min Gain", WM8960_ALC2, 4, 7, 0),
SOC_SINGLE("ALC Hold Time", WM8960_ALC2, 0, 15, 0),
SOC_ENUM("ALC Mode", wm8960_enum[6]),
SOC_SINGLE("ALC Decay", WM8960_ALC3, 4, 15, 0),
SOC_SINGLE("ALC Attack", WM8960_ALC3, 0, 15, 0),

SOC_SINGLE("Noise Gate Threshold", WM8960_NOISEG, 3, 31, 0),
SOC_SINGLE("Noise Gate Switch", WM8960_NOISEG, 0, 1, 0),

SOC_DOUBLE_R("ADC PCM Capture Volume", WM8960_LINPATH, WM8960_RINPATH,
	0, 127, 0),

SOC_SINGLE_TLV("Left Output Mixer Boost Bypass Volume",
	       WM8960_BYPASS1, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Left Output Mixer LINPUT3 Volume",
	       WM8960_LOUTMIX, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer Boost Bypass Volume",
	       WM8960_BYPASS2, 4, 7, 1, bypass_tlv),
SOC_SINGLE_TLV("Right Output Mixer RINPUT3 Volume",
	       WM8960_ROUTMIX, 4, 7, 1, bypass_tlv),
};

static const struct snd_kcontrol_new wm8960_lin_boost[] = {
SOC_DAPM_SINGLE("LINPUT2 Switch", WM8960_LINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8960_LINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("LINPUT1 Switch", WM8960_LINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8960_lin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM8960_LINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm8960_rin_boost[] = {
SOC_DAPM_SINGLE("RINPUT2 Switch", WM8960_RINPATH, 6, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8960_RINPATH, 7, 1, 0),
SOC_DAPM_SINGLE("RINPUT1 Switch", WM8960_RINPATH, 8, 1, 0),
};

static const struct snd_kcontrol_new wm8960_rin[] = {
SOC_DAPM_SINGLE("Boost Switch", WM8960_RINPATH, 3, 1, 0),
};

static const struct snd_kcontrol_new wm8960_loutput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM8960_LOUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("LINPUT3 Switch", WM8960_LOUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM8960_BYPASS1, 7, 1, 0),
};

static const struct snd_kcontrol_new wm8960_routput_mixer[] = {
SOC_DAPM_SINGLE("PCM Playback Switch", WM8960_ROUTMIX, 8, 1, 0),
SOC_DAPM_SINGLE("RINPUT3 Switch", WM8960_ROUTMIX, 7, 1, 0),
SOC_DAPM_SINGLE("Boost Bypass Switch", WM8960_BYPASS2, 7, 1, 0),
};

static const struct snd_kcontrol_new wm8960_mono_out[] = {
SOC_DAPM_SINGLE("Left Switch", WM8960_MONOMIX1, 7, 1, 0),
SOC_DAPM_SINGLE("Right Switch", WM8960_MONOMIX2, 7, 1, 0),
};

/* No Dynamic PM at the moment */
static const struct snd_soc_dapm_widget wm8960_dapm_widgets[] = {
SND_SOC_DAPM_INPUT("LINPUT1"),
SND_SOC_DAPM_INPUT("RINPUT1"),
SND_SOC_DAPM_INPUT("LINPUT2"),
SND_SOC_DAPM_INPUT("RINPUT2"),
SND_SOC_DAPM_INPUT("LINPUT3"),
SND_SOC_DAPM_INPUT("RINPUT3"),

SND_SOC_DAPM_MICBIAS("MICB", SND_SOC_NOPM, 0, 0),

SND_SOC_DAPM_MIXER("Left Boost Mixer", SND_SOC_NOPM, 0, 0,
		   wm8960_lin_boost, ARRAY_SIZE(wm8960_lin_boost)),
SND_SOC_DAPM_MIXER("Right Boost Mixer", SND_SOC_NOPM, 4, 0,
		   wm8960_rin_boost, ARRAY_SIZE(wm8960_rin_boost)),

SND_SOC_DAPM_MIXER("Left Input Mixer", SND_SOC_NOPM, 5, 0,
		   wm8960_lin, ARRAY_SIZE(wm8960_lin)),
SND_SOC_DAPM_MIXER("Right Input Mixer", SND_SOC_NOPM, 4, 0,
		   wm8960_rin, ARRAY_SIZE(wm8960_rin)),

SND_SOC_DAPM_ADC("Left ADC", "Capture", SND_SOC_NOPM, 3, 0),
SND_SOC_DAPM_ADC("Right ADC", "Capture", SND_SOC_NOPM, 2, 0),

SND_SOC_DAPM_DAC("Left DAC", "Playback", SND_SOC_NOPM, 8, 0),
SND_SOC_DAPM_DAC("Right DAC", "Playback", SND_SOC_NOPM, 7, 0),

SND_SOC_DAPM_MIXER("Left Output Mixer", SND_SOC_NOPM, 3, 0,
	&wm8960_loutput_mixer[0],
	ARRAY_SIZE(wm8960_loutput_mixer)),
SND_SOC_DAPM_MIXER("Right Output Mixer", SND_SOC_NOPM, 2, 0,
	&wm8960_routput_mixer[0],
	ARRAY_SIZE(wm8960_routput_mixer)),

SND_SOC_DAPM_MIXER("Mono Output Mixer", SND_SOC_NOPM, 7, 0,
	&wm8960_mono_out[0],
	ARRAY_SIZE(wm8960_mono_out)),

SND_SOC_DAPM_PGA("LOUT1 PGA", SND_SOC_NOPM, 6, 0, NULL, 0),
SND_SOC_DAPM_PGA("ROUT1 PGA", SND_SOC_NOPM, 5, 0, NULL, 0),

SND_SOC_DAPM_PGA("Left Speaker PGA", SND_SOC_NOPM, 4, 0, NULL, 0),
SND_SOC_DAPM_PGA("Right Speaker PGA", SND_SOC_NOPM, 3, 0, NULL, 0),

SND_SOC_DAPM_PGA("Right Speaker Output", SND_SOC_NOPM, 7, 0, NULL, 0),
SND_SOC_DAPM_PGA("Left Speaker Output", SND_SOC_NOPM, 6, 0, NULL, 0),

SND_SOC_DAPM_OUTPUT("SPK_LP"),
SND_SOC_DAPM_OUTPUT("SPK_LN"),
SND_SOC_DAPM_OUTPUT("HP_L"),
SND_SOC_DAPM_OUTPUT("HP_R"),
SND_SOC_DAPM_OUTPUT("SPK_RP"),
SND_SOC_DAPM_OUTPUT("SPK_RN"),
SND_SOC_DAPM_OUTPUT("OUT3"),
};

static const struct snd_soc_dapm_route audio_paths[] = {
	{ "Left Boost Mixer", "LINPUT1 Switch", "LINPUT1" },
	{ "Left Boost Mixer", "LINPUT2 Switch", "LINPUT2" },
	{ "Left Boost Mixer", "LINPUT3 Switch", "LINPUT3" },

	{ "Left Input Mixer", "Boost Switch", "Left Boost Mixer", },
	{ "Left Input Mixer", NULL, "LINPUT1", },  /* Really Boost Switch */
	{ "Left Input Mixer", NULL, "LINPUT2" },
	{ "Left Input Mixer", NULL, "LINPUT3" },

	{ "Right Boost Mixer", "RINPUT1 Switch", "RINPUT1" },
	{ "Right Boost Mixer", "RINPUT2 Switch", "RINPUT2" },
	{ "Right Boost Mixer", "RINPUT3 Switch", "RINPUT3" },

	{ "Right Input Mixer", "Boost Switch", "Right Boost Mixer", },
	{ "Right Input Mixer", NULL, "RINPUT1", },  /* Really Boost Switch */
	{ "Right Input Mixer", NULL, "RINPUT2" },
	{ "Right Input Mixer", NULL, "LINPUT3" },

	{ "Left ADC", NULL, "Left Input Mixer" },
	{ "Right ADC", NULL, "Right Input Mixer" },

	{ "Left Output Mixer", "LINPUT3 Switch", "LINPUT3" },
	{ "Left Output Mixer", "Boost Bypass Switch", "Left Boost Mixer"} ,
	{ "Left Output Mixer", "PCM Playback Switch", "Left DAC" },

	{ "Right Output Mixer", "RINPUT3 Switch", "RINPUT3" },
	{ "Right Output Mixer", "Boost Bypass Switch", "Right Boost Mixer" } ,
	{ "Right Output Mixer", "PCM Playback Switch", "Right DAC" },

	{ "Mono Output Mixer", "Left Switch", "Left Output Mixer" },
	{ "Mono Output Mixer", "Right Switch", "Right Output Mixer" },

	{ "LOUT1 PGA", NULL, "Left Output Mixer" },
	{ "ROUT1 PGA", NULL, "Right Output Mixer" },

	{ "HP_L", NULL, "LOUT1 PGA" },
	{ "HP_R", NULL, "ROUT1 PGA" },

	{ "Left Speaker PGA", NULL, "Left Output Mixer" },
	{ "Right Speaker PGA", NULL, "Right Output Mixer" },

	{ "Left Speaker Output", NULL, "Left Speaker PGA" },
	{ "Right Speaker Output", NULL, "Right Speaker PGA" },

	{ "SPK_LN", NULL, "Left Speaker Output" },
	{ "SPK_LP", NULL, "Left Speaker Output" },
	{ "SPK_RN", NULL, "Right Speaker Output" },
	{ "SPK_RP", NULL, "Right Speaker Output" },

	{ "OUT3", NULL, "Mono Output Mixer", }
};

static void wm8960_reinit(struct snd_soc_codec *codec);

static int wm8960_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, wm8960_dapm_widgets,
				ARRAY_SIZE(wm8960_dapm_widgets));
	snd_soc_dapm_add_routes(codec, audio_paths, ARRAY_SIZE(audio_paths));

	return 0;
}

static int wm8960_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = 0;

	wm8960_reinit(codec);
	
	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBS_CFM:
		iface |= 0x040; /* Codec is Master */
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x002; /* Codec supports I2S */
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	/* Set the word length to 16 bits, i.e. bits 2 and 3 should be 0,0 */
	iface = iface & 0x1f3;

	/* set iface */
	wm8960_write(codec, WM8960_IFACE1, iface);
	return 0;
}

static int wm8960_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	u16 iface = wm8960_read_reg_cache(codec, WM8960_IFACE1) & 0xfff3;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	}

	/* set iface */
	wm8960_write(codec, WM8960_IFACE1, iface);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		wm8960_configure_capture(codec);

	return 0;
}

static int wm8960_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = wm8960_read_reg_cache(codec, WM8960_DACCTL1) & 0xfff7;

	if (mute)
		wm8960_write(codec, WM8960_DACCTL1, mute_reg | 0x8);
	else
		wm8960_write(codec, WM8960_DACCTL1, mute_reg);
	return 0;
}

static int wm8960_dapm_event(struct snd_soc_codec *codec, int event)
{
	codec->dapm_state = event;
	return 0;
}

/* PLL divisors */
struct _pll_div {
	u32 pre_div:1;
	u32 n:4;
	u32 k:24;
};

static struct _pll_div pll_div;

/* The size in bits of the pll divide multiplied by 10
 * to allow rounding later */
#define FIXED_PLL_SIZE ((1 << 24) * 10)

/*
 * Currently unused as the sampling rate values will be hard coded
 */
static void pll_factors(unsigned int target, unsigned int source)
{
	unsigned long long Kpart;
	unsigned int K, Ndiv, Nmod;

	Ndiv = target / source;
	if (Ndiv < 6) {
		source >>= 1;
		pll_div.pre_div = 1;
		Ndiv = target / source;
	} else
		pll_div.pre_div = 0;

	if ((Ndiv < 6) || (Ndiv > 12))
		printk(KERN_WARNING
			"WM8960 N value outwith recommended range! N = %d\n",Ndiv);

	pll_div.n = Ndiv;
	Nmod = target % source;
	Kpart = FIXED_PLL_SIZE * (long long)Nmod;

	do_div(Kpart, source);

	K = Kpart & 0xFFFFFFFF;

	/* Check if we need to round */
	if ((K % 10) >= 5)
		K += 5;

	/* Move down to proper range now rounding is done */
	K /= 10;

	pll_div.k = K;
}

/* Capture or playback */
int wm8960_capture = 0;
EXPORT_SYMBOL(wm8960_capture);

static int headset_attached = 0;
static int headset_detached = 0;

static void wm8960_power_on(int power)
{
	if (power) {
		wm8960_write(wm8960_codec, WM8960_APOP1, 0x94);
		wm8960_write(wm8960_codec, WM8960_APOP2, 0x40);

		mdelay(400);

		wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);
		wm8960_write(wm8960_codec, WM8960_APOP2, 0x0);
		wm8960_write(wm8960_codec, WM8960_POWER1, 0x80);
	
		mdelay(100);

		wm8960_write(wm8960_codec, WM8960_POWER1, 0xc0);
		wm8960_write(wm8960_codec, WM8960_APOP1, 0x0);
		wm8960_write(wm8960_codec, WM8960_POWER2, 0x1fb);
		wm8960_write(wm8960_codec, WM8960_POWER3, 0x0c);
		wm8960_write(wm8960_codec, WM8960_LOUTMIX, 0x150);
		wm8960_write(wm8960_codec, WM8960_ROUTMIX, 0x150);
		wm8960_write(wm8960_codec, WM8960_LOUT1, LOUT1);
		wm8960_write(wm8960_codec, WM8960_ROUT1, ROUT1);	
		wm8960_write(wm8960_codec, WM8960_LOUT2, LOUT2);
		wm8960_write(wm8960_codec, WM8960_ROUT2, ROUT2);
		wm8960_write(wm8960_codec, WM8960_DACCTL2, 0xc);
		wm8960_write(wm8960_codec, WM8960_DACCTL1, 0x0);

                power_status_write(1);
	}
	else {
                wm8960_write(wm8960_codec, WM8960_LOUT1, 0x0);
                wm8960_write(wm8960_codec, WM8960_ROUT1, 0x10);	
                wm8960_write(wm8960_codec, WM8960_LOUT2, 0x0);
                wm8960_write(wm8960_codec, WM8960_ROUT2, 0x10);

		wm8960_write(wm8960_codec, WM8960_DACCTL1, 0x8);
		mdelay(20);

		wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);
		mdelay(20);

		wm8960_write(wm8960_codec, WM8960_CLASSD1, 0x37);
		wm8960_write(wm8960_codec, WM8960_APOP1, 0x94);
		mdelay(20);

		wm8960_write(wm8960_codec, WM8960_POWER1, 0x0);
		mdelay(1000);

		wm8960_write(wm8960_codec, WM8960_POWER2, 0x0);

                if (wm8960_capture) {
                    wm8960_reset(wm8960_codec);
                }
                
                power_status_write(0);
		headset_attached = 0;
		headset_detached = 0;
	}
}


static void wm8960_power_on_startup(int power)
{
	wm8960_write(wm8960_codec, WM8960_LOUT1, 0x0);
	wm8960_write(wm8960_codec, WM8960_ROUT1, 0x10);	
	wm8960_write(wm8960_codec, WM8960_LOUT2, 0x0);
	wm8960_write(wm8960_codec, WM8960_ROUT2, 0x10);

	wm8960_write(wm8960_codec, WM8960_DACCTL1, 0x8);
	wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);

	wm8960_write(wm8960_codec, WM8960_CLASSD1, 0x37);
	wm8960_write(wm8960_codec, WM8960_APOP1, 0x94);

	wm8960_write(wm8960_codec, WM8960_POWER1, 0x0);
	wm8960_write(wm8960_codec, WM8960_POWER2, 0x0);

	power_status_write(0);
	headset_attached = 0;
	headset_detached = 0;
}

static void wm8960_poweroff_work(struct work_struct *dummy)
{
	wm8960_power_on(0);
	wm8960_capture = 0;       
}

static DECLARE_DELAYED_WORK(wm8960_poweroff_wq, &wm8960_poweroff_work);

static int wm8960_trigger(struct snd_pcm_substream *substream, int cmd)
{
    switch(cmd) {
    case SNDRV_PCM_TRIGGER_STOP:
        /* If value of power_off_timeout is less than zero
         * then keep the codec on forever
         */
        if (power_off_timeout >= 0)
            schedule_delayed_work(&wm8960_poweroff_wq, msecs_to_jiffies(power_off_timeout));
        break;

    default:
        break;
    }

    return 0;
}

static void wm8960_configure_capture(struct snd_soc_codec *codec)
{
	wm8960_write(wm8960_codec, WM8960_APOP1, 0x4);
	wm8960_write(wm8960_codec, WM8960_APOP2, 0x0);

	mdelay(400);

	wm8960_write(wm8960_codec, WM8960_POWER2, 0x62);
	wm8960_write(wm8960_codec, WM8960_POWER1, 0x80);

	mdelay(100);	

	/* ADC and DAC control registers */
	wm8960_write(codec, WM8960_DACCTL1, 0x0);
	wm8960_write(codec, WM8960_DACCTL2, 0xc);

	/* Left and right channel output volume */
	wm8960_write(codec, WM8960_ROUT1, 0x179);
	wm8960_write(codec, WM8960_LOUT1, 0x179);

	/* Left and right channel mixer volume */
	wm8960_write(codec, WM8960_ROUTMIX, 0x150);
	wm8960_write(codec, WM8960_LOUTMIX, 0x150);

	/* Set WL8 and ALRCGPIO enabled */
	wm8960_write(codec, WM8960_IFACE2, 0x40);

	/* Configure WL - 16bits, Master mode and I2S format */
	wm8960_write(codec, WM8960_IFACE1, 0x42);

	/* Left Mic Boost */
	wm8960_write(codec, WM8960_LINPATH, 0x138);

	/* Right Mic Boost */
	wm8960_write(codec, WM8960_RINPATH, 0x108);

	/* Left/right input volumes */
	wm8960_write(codec, WM8960_LINVOL, 0x13f);
	wm8960_write(codec, WM8960_RINVOL, 0x13f);

	/* Max input gain setting */
	wm8960_write(codec, WM8960_ALC1, 0x0);

	/* Left ADC Channel. No right channel since input is MONO */
	wm8960_write(codec, WM8960_LADC, 0x1c3);

	/* Power Management: Turn on ADC left and right, MIC and VMID */
	wm8960_write(codec, WM8960_POWER1, 0xfe);

	/* Power Management: Turn on DAC, speaker and PLL */
	wm8960_write(codec, WM8960_POWER2, 0x1fb);

	/* Power Management: Turn on Left/Right MIC and left/right Mixer */
	wm8960_write(codec, WM8960_POWER3, 0x3c);
}

static void wm8960_headset_event(void)
{
	int irq = gpio_headset_irq();

	if (gpio_headset_status()) {
		if (!headset_detached) {
			kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_REMOVE);
			set_irq_type(irq, IRQF_TRIGGER_FALLING);
			headset_attached = 0;
			headset_detached = 1;
		}
	}
	else {
		if (!headset_attached) {
			kobject_uevent(&mxc_headset.this_device->kobj, KOBJ_ADD);
			set_irq_type(irq, IRQF_TRIGGER_RISING);
			headset_attached = 1;
			headset_detached = 0;
		}
	}
};

static int wm8960_set_dai_pll(struct snd_soc_dai *codec_dai,
		int pll_id, unsigned int freq_in, unsigned int freq_out)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;
	int irq = gpio_headset_irq();

	if (freq_in == 0 || freq_out == 0) {
		return 0; /* Disable the PLL */
	}

        /* 
         *  Cancel any delayed work
         */
        cancel_delayed_work_sync(&wm8960_poweroff_wq);
        
        /* 
         * Read the power status and if it is still on
         * skip doing anything.
         */
        if (power_status_read()) {
            return 0;
        }

	pll_factors(freq_out * 8, freq_in);

	/* Configure PLLK1, PLLK2 and PLLN for sampling rates */
	reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1f0;
	reg = reg | 0x20;
	wm8960_write(codec, WM8960_PLLN, reg);

	if (pll_id == 0) {
		/* 11.2896 MHz */
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1f0;
		reg = reg | 0x7;
		wm8960_write(codec, WM8960_PLLN, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK3);
		reg = reg & 0x100;
		reg = reg | 0x26;
		wm8960_write(codec, WM8960_PLLK3, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK2);
		reg = reg & 0x100;
		reg = reg | 0xC2;
		wm8960_write(codec, WM8960_PLLK2, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK1);
		reg = reg & 0x100;
		reg = reg | 0x86;
		wm8960_write(codec, WM8960_PLLK1, reg);		
	}
	else {
		/* 12.288 MHz */
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1f0;
		reg = reg | 0x8;
		wm8960_write(codec, WM8960_PLLN, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK3);
		reg = reg & 0x100;
		reg = reg | 0xE8;
		wm8960_write(codec, WM8960_PLLK3, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK2);
		reg = reg & 0x100;
		reg = reg | 0x26;
		wm8960_write(codec, WM8960_PLLK2, reg);

		reg = wm8960_read_reg_cache(codec, WM8960_PLLK1);
		reg = reg & 0x100;
		reg = reg | 0x31;
		wm8960_write(codec, WM8960_PLLK1, reg);
	}

	wm8960_write(codec, WM8960_ADDCTL3,wm8960_read_reg_cache(codec, WM8960_ADDCTL3) | 0x8);

	reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL2);
	reg |= HPSWPOLARITY;
	reg |= 0x40; /* Headphone Switch Enable */
        wm8960_write(codec, WM8960_ADDCTL2, reg);

	/* ADLRC set */
	wm8960_write(codec, WM8960_IFACE2,wm8960_read_reg_cache(codec, WM8960_IFACE2) | 0x40);
	wm8960_write(codec, WM8960_ADDCTL1,wm8960_read_reg_cache(codec, WM8960_ADDCTL1) | 0x3);

	/* Clear out MONOMIX 1 and 2 */
	wm8960_write(codec, WM8960_MONOMIX1, 0x0);
	wm8960_write(codec, WM8960_MONOMIX2, 0x0);

	if (!wm8960_capture) {	
		wm8960_write(codec, WM8960_CLASSD1, SPKR_CONFIG);
		wm8960_write(codec, WM8960_CLASSD3, CLASSD3_CONFIG);
	}

	reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL4);
        /*
         * HPSEL should be set to 10, GPIOSEL should be 001. This is needed
         * for headset/speaker switching. JD2 is used for the switching
         * in the codec
         */
        reg &= 0x1BB;
        reg |= 0x8 | 0x10 | 0x21 | 0x2; /* GPIOSEL to be Jack detect output */
        wm8960_write(codec, WM8960_ADDCTL4, reg);

        /* Get the headset IRQ */

	disable_irq(irq);

	if (!wm8960_capture)
                wm8960_power_on(1);

	wm8960_headset_event();

	enable_irq(irq);

	return 0;
}

static int wm8960_set_dai_clkdiv(struct snd_soc_dai *codec_dai,
		int div_id, int div)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 reg;

	switch (div_id) {
	case WM8960_SYSCLKSEL:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x1fe;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_SYSCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x1f9;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_DACDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x1c7;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_ADCDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK1) & 0x03f;
		wm8960_write(codec, WM8960_CLOCK1, reg | div);
		break;
	case WM8960_OPCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x03f;
		wm8960_write(codec, WM8960_PLLN, reg | div);
		break;
	case WM8960_PRESCALEDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_PLLN) & 0x1EF;
		wm8960_write(codec, WM8960_PLLN, reg | div);
		break;
	case WM8960_DCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK2) & 0x03f;
		wm8960_write(codec, WM8960_CLOCK2, reg | div);
		break;
	case WM8960_BCLKDIV:
		reg = wm8960_read_reg_cache(codec, WM8960_CLOCK2) & 0x1f0;
		wm8960_write(codec, WM8960_CLOCK2, reg | div);
		break;
	case WM8960_TOCLKSEL:
		reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL1) & 0x1fd;
		wm8960_write(codec, WM8960_ADDCTL1, reg | div);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#define WM8960_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000)

#define WM8960_FORMATS \
	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
	SNDRV_PCM_FMTBIT_S24_LE)

#define WM8960_CAPTURE_RATES		\
	(SNDRV_PCM_RATE_8000	| 	\
	 SNDRV_PCM_RATE_11025	| 	\
	 SNDRV_PCM_RATE_16000	|	\
	 SNDRV_PCM_RATE_22050	|	\
	 SNDRV_PCM_RATE_32000	|	\
	 SNDRV_PCM_RATE_44100	|	\
	 SNDRV_PCM_RATE_48000)	

static struct snd_soc_dai_ops wm8960_dai_ops = {
	.hw_params = wm8960_hw_params,
	.digital_mute = wm8960_mute,
	.set_fmt = wm8960_set_dai_fmt,
	.set_clkdiv = wm8960_set_dai_clkdiv,
	.set_pll = wm8960_set_dai_pll,
};

struct snd_soc_dai wm8960_dai = {
	.name = "WM8960",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8960_RATES,
		.formats = WM8960_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = WM8960_CAPTURE_RATES,
		.formats = WM8960_FORMATS,},
        .ops = &wm8960_dai_ops,
        .symmetric_rates = 1,
};
EXPORT_SYMBOL_GPL(wm8960_dai);

/*!
 * Set the bias level
 */
static int wm8960_set_bias_level(struct snd_soc_codec *codec,
				enum snd_soc_bias_level level)
{
	u16 reg = 0;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		/* Set VMID to 2x50k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg &= ~0x181;
		reg |= 0x80 | 0x40;
		snd_soc_write(codec, WM8960_POWER1, reg);
		break;

	case SND_SOC_BIAS_OFF:
	case SND_SOC_BIAS_STANDBY:
		/* Enable anti-pop features */
		snd_soc_write(codec, WM8960_APOP1,
			     WM8960_POBCTRL | WM8960_SOFT_ST |
			     WM8960_BUFDCOPEN | WM8960_BUFIOEN);

		/* Discharge HP output */
		reg = WM8960_DISOP;
		snd_soc_write(codec, WM8960_APOP2, reg);

		msleep(400);

		snd_soc_write(codec, WM8960_APOP2, 0);

		/* Enable & ramp VMID at 2x50k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg |= 0x80;
		snd_soc_write(codec, WM8960_POWER1, reg);
		msleep(100);

		/* Enable VREF */
		snd_soc_write(codec, WM8960_POWER1, reg | WM8960_VREF);

		/* Disable anti-pop features */
		snd_soc_write(codec, WM8960_APOP1, WM8960_BUFIOEN);

		/* Set VMID to 2x250k */
		reg = snd_soc_read(codec, WM8960_POWER1);
		reg &= ~0x180;
		reg |= 0x100;
		snd_soc_write(codec, WM8960_POWER1, reg);

		break;
	}

	return 0;
}

/*!
 * Suspend the codec
 */
static int wm8960_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	wm8960_dapm_event(codec, SNDRV_CTL_POWER_D3cold);

	/*
	 * Turn off the power to the codec components
	 */
	wm8960_write(codec, WM8960_POWER1, 0x1);
	wm8960_write(codec, WM8960_POWER2, 0x0);
	wm8960_write(codec, WM8960_POWER3, 0x0);

	wm8960_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

/*!
 * Resume the codec blocks
 */
static int wm8960_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;
	int i;
	u8 data[2];
	u16 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(wm8960_reg); i++) {
		data[0] = (i << 1) | ((cache[i] >> 8) & 0x0001);
		data[1] = cache[i] & 0x00ff;
		codec->hw_write(codec->control_data, data, 2);
	}

        codec->set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}

static void wm8960_reinit(struct snd_soc_codec *codec)
{
	unsigned int reg;

	/* 
	 * Do we need to reset codec?
	 *	wm8960_reset(codec);
	 */
	wm8960_dapm_event(codec, SNDRV_CTL_POWER_D3hot);

	reg = wm8960_read_reg_cache(codec, WM8960_LINVOL);
	wm8960_write(codec, WM8960_LINVOL, reg & 0x017F);
	reg = wm8960_read_reg_cache(codec, WM8960_RINVOL);
	wm8960_write(codec, WM8960_RINVOL, reg & 0x017F);
	reg = wm8960_read_reg_cache(codec, WM8960_ADDCTL2);
}

static void wm8960_headset_mute(int mute)
{
	u16 mute_reg = wm8960_read_reg_cache(wm8960_codec, WM8960_DACCTL1) & 0xfff7;

	if (mute)
		wm8960_write(wm8960_codec, WM8960_DACCTL1, mute_reg | 0x8);
	else
		wm8960_write(wm8960_codec, WM8960_DACCTL1, mute_reg);
}

static void do_hsdet_work(struct work_struct *dummy)
{
	int irq = gpio_headset_irq();

	/* Get the headset IRQ */
	wm8960_headset_event();

	/* Unmute */
	wm8960_headset_mute(0);

	enable_irq(irq);
}

static DECLARE_DELAYED_WORK(hsdet_work, do_hsdet_work);

static void do_hsdet_mute_work(struct work_struct *dummy)
{
	wm8960_headset_mute(1);

	/* Debounce for headset detection */
	schedule_delayed_work(&hsdet_work, msecs_to_jiffies(HSDET_THRESHOLD));
}
	
static DECLARE_WORK(hsdet_mute_work, do_hsdet_mute_work);

static irqreturn_t headset_detect_irq(int irq, void *devid)
{
	disable_irq(irq);

	/* Mute */
	schedule_work(&hsdet_mute_work);

	return IRQ_HANDLED;
}

/*
 * initialise the WM8960 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int wm8960_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret = 0, error = 0;
	int irq = gpio_headset_irq();
	int err = 0;

        /* Codec is off initially */
        power_status_write(0);

	codec->name = "WM8960";
	codec->owner = THIS_MODULE;
	codec->read = wm8960_read_reg_cache;
	codec->write = wm8960_write;
	codec->dapm_event = wm8960_dapm_event;
	codec->dai = &wm8960_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(wm8960_reg);
	codec->reg_cache = kmemdup(wm8960_reg, sizeof(wm8960_reg), GFP_KERNEL);

	if (codec->reg_cache == NULL)
		return -ENOMEM;

	wm8960_reset(codec);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "wm8960: failed to create pcms\n");
		goto pcm_err;
	}

	/* power on device */
	wm8960_dapm_event(codec, SNDRV_CTL_POWER_D3hot);

	snd_soc_add_controls(codec, wm8960_snd_controls,
				ARRAY_SIZE(wm8960_snd_controls));
	wm8960_add_widgets(codec);
	ret = snd_soc_init_card(socdev);
	if (ret < 0) {
      		printk(KERN_ERR "wm8960: failed to register card\n");
		goto card_err;
    	}

	wm8960_codec = codec;

	error = sysdev_class_register(&wm8960_reg_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8960_reg);
	if (!error)
		error = sysdev_create_file(&device_wm8960_reg, &attr_wm8960_reg);

	error = sysdev_class_register(&wm8960_register_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8960_register);
	if (!error)
		error = sysdev_create_file(&device_wm8960_register, &attr_wm8960_register);

	error = sysdev_class_register(&headset_status_sysclass);
	if (!error)
		error = sysdev_register(&device_headset_status);
	if (!error)
		error = sysdev_create_file(&device_headset_status, &attr_headset_status);

	error = sysdev_class_register(&wm8960_power_status_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8960_power_status);
	if (!error)
		error = sysdev_create_file(&device_wm8960_power_status, &attr_wm8960_power_status);

	error = sysdev_class_register(&wm8960_power_off_timeout_sysclass);
	if (!error)
		error = sysdev_register(&device_wm8960_power_off_timeout);
	if (!error)
		error = sysdev_create_file(&device_wm8960_power_off_timeout, &attr_wm8960_power_off_timeout);

	/* Set the GPIO */
	gpio_activate_audio_ports();

	err = request_irq(irq, headset_detect_irq, 0, "HS_det", NULL);
	if (err != 0) {
		printk(KERN_ERR "IRQF_DISABLED: Could not get IRQ %d\n", irq);
		return err;
	}

	wm8960_power_on_startup(0);

	if (misc_register(&mxc_headset)) {
		printk (KERN_WARNING "mxc_headset: Couldn't register device %d\n", HEADSET_MINOR);
		return -EBUSY;
	}	

	return ret;
card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *wm8960_socdev;

#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)

/*
 * WM8960 2 wire address is 0x1a
 */
static unsigned short normal_i2c[] = { 0, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static struct i2c_driver wm8960_i2c_driver;
static struct i2c_client client_template;

/* If the i2c layer weren't so broken, we could pass this kind of data
   around */

static int wm8960_codec_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct snd_soc_device *socdev = wm8960_socdev;
	struct wm8960_setup_data *setup = socdev->codec_data;
	struct snd_soc_codec *codec = socdev->card->codec;
	struct i2c_client *i2c;
	int ret;

	if (addr != setup->i2c_address)
		return -ENODEV;

	client_template.adapter = adap;
	client_template.addr = addr;

	i2c = kmemdup(&client_template, sizeof(client_template), GFP_KERNEL);
	if (i2c == NULL) {
		kfree(codec);
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = i2c_attach_client(i2c);
	if (ret < 0) {
		err("failed to attach codec at addr %x\n", addr);
		goto err;
	}

	ret = wm8960_init(socdev);
	if (ret < 0) {
		err("failed to initialise WM8960\n");
		goto err;
	}

	register_reboot_notifier(&wm8960_reboot_nb);

	return ret;
err:
	kfree(codec);
	kfree(i2c);
	return ret;
}

static void wm8960_i2c_shutdown(struct i2c_client *client)
{
	struct snd_soc_codec* codec = i2c_get_clientdata(client);

	/*
	 * Turn off the power to the codec components
	 */
	wm8960_write(codec, WM8960_POWER1, 0x1);
	wm8960_write(codec, WM8960_POWER2, 0x0);
	wm8960_write(codec, WM8960_POWER3, 0x0);

	wm8960_set_bias_level(codec, SND_SOC_BIAS_OFF);
}

static int wm8960_reboot(struct notifier_block *self, unsigned long action, void *cpu)
{
	printk(KERN_INFO "wm8960_reboot\n");

	wm8960_write(wm8960_codec, WM8960_POWER1, 0x1);
	wm8960_write(wm8960_codec, WM8960_POWER2, 0x0);
	wm8960_write(wm8960_codec, WM8960_POWER3, 0x0);

	wm8960_set_bias_level(wm8960_codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int wm8960_i2c_detach(struct i2c_client *client)
{
	struct snd_soc_codec* codec = i2c_get_clientdata(client);
	i2c_detach_client(client);
	kfree(codec->reg_cache);
	kfree(client);
	return 0;
}

static int wm8960_i2c_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, wm8960_codec_probe);
}

// tmp
#define I2C_DRIVERID_WM8960 0xfefe

/* corgi i2c codec control layer */
static struct i2c_driver wm8960_i2c_driver = {
	.driver = {
		.name = "WM8960 I2C Codec",
		.owner = THIS_MODULE,
	},
	.id =             I2C_DRIVERID_WM8960,
	.attach_adapter = wm8960_i2c_attach,
	.detach_client =  wm8960_i2c_detach,
	.shutdown = wm8960_i2c_shutdown,
	.command =        NULL,
};

static struct i2c_client client_template = {
	.name =   "WM8960",
	.driver = &wm8960_i2c_driver,
};
#endif

#ifdef CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

struct dentry *wm8960_debugfs_root;
struct dentry *wm8960_debugfs_state;

static int wm8960_debugfs_show(struct seq_file *s, void *p)
{
	int t;
	int i = 0;
	u16 *cache = wm8960_codec->reg_cache;

	for (i=0; i < WM8960_CACHEREGNUM; i++) {
		t += seq_printf(s, "Register %d - Value:0x%x\n", i, cache[i]);
	}

        if (gpio_headset_status())
		t += seq_printf(s, "wm8960: Headset not plugged in\n");
	else
		t += seq_printf(s, "wm8960: Headset plugged in\n");

	t += seq_printf(s, "WM8960_RATES: 0x%x\n", WM8960_RATES);
	t += seq_printf(s, "WM8960_CAPTURE_RATES: 0x%x\n", WM8960_CAPTURE_RATES);

	return 0;
}

static int wm8960_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, wm8960_debugfs_show, inode->i_private);
}

static const struct file_operations wm8960_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= wm8960_debugfs_open,
	.llseek		= seq_lseek,
	.read		= seq_read,
	.release	= single_release,
};

static int wm8960_debugfs_init(void)
{
	struct dentry *root, *state;
	int ret = -1;

	root = debugfs_create_dir("WM8960_Sound", NULL);
	if (IS_ERR(root) || !root)
		goto err_root;

	state = debugfs_create_file("WM8960_Codec_Settings", 0400, root, (void *)0,
				&wm8960_debugfs_fops);

	if (!state)
		goto err_state;

	wm8960_debugfs_root = root;
	wm8960_debugfs_state = state;
	return 0;
err_state:
	debugfs_remove(root);
err_root:
	printk(KERN_ERR "WM8960_Sound: debugfs is not available\n");
	return ret;
}

static void wm8960_debugfs_cleanup(void)
{
	debugfs_remove(wm8960_debugfs_state);
	debugfs_remove(wm8960_debugfs_root);
	wm8960_debugfs_state = NULL;
	wm8960_debugfs_root = NULL;
}

#endif /* CONFIG_DEBUG_FS */
	
static int wm8960_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct wm8960_setup_data *setup;
	struct snd_soc_codec *codec;
	int ret = 0;

	info("WM8960 Audio Codec %s", WM8960_VERSION);

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	wm8960_socdev = socdev;
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		normal_i2c[0] = setup->i2c_address;
		codec->hw_write = (hw_write_t)i2c_master_send;
		ret = i2c_add_driver(&wm8960_i2c_driver);
		if (ret != 0)
			printk(KERN_ERR "can't add i2c driver");
	}
#else
	/* Add other interfaces here */
#endif

#ifdef CONFIG_DEBUG_FS
	if (wm8960_debugfs_init() < 0)
		printk(KERN_ERR "WM8960 debugfs init error\n");
#endif

	return ret;
}

/* power down chip */
static int wm8960_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (codec->control_data)
		wm8960_dapm_event(codec, SNDRV_CTL_POWER_D3cold);

	/*
	 * Turn off the power to the codec components
	 */
	wm8960_write(codec, WM8960_POWER1, 0x1);
	wm8960_write(codec, WM8960_POWER2, 0x0);
	wm8960_write(codec, WM8960_POWER3, 0x0);

	wm8960_set_bias_level(codec, SND_SOC_BIAS_OFF);

	unregister_reboot_notifier(&wm8960_reboot_nb);

	sysdev_remove_file(&device_wm8960_reg, &attr_wm8960_reg);
	sysdev_remove_file(&device_wm8960_register, &attr_wm8960_register);
	sysdev_remove_file(&device_headset_status, &attr_headset_status);
	sysdev_remove_file(&device_wm8960_power_status, &attr_wm8960_power_status);
	sysdev_remove_file(&device_wm8960_power_off_timeout, &attr_wm8960_power_off_timeout);

	free_irq(gpio_headset_irq(), NULL);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined (CONFIG_I2C) || defined (CONFIG_I2C_MODULE)
	i2c_del_driver(&wm8960_i2c_driver);
#endif
	kfree(codec);

#ifdef CONFIG_DEBUG_FS
	wm8960_debugfs_cleanup();
#endif

	misc_deregister(&mxc_headset);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_wm8960 = {
	.probe = 	wm8960_probe,
	.remove = 	wm8960_remove,
	.suspend = 	wm8960_suspend,
	.resume =	wm8960_resume,
};

EXPORT_SYMBOL_GPL(soc_codec_dev_wm8960);

MODULE_DESCRIPTION("ASoC WM8960 driver");
MODULE_AUTHOR("Manish Lachwani");
MODULE_LICENSE("GPL");

