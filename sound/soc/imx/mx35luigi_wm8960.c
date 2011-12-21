/*
 * mx35luigi_wm8960.c -- SoC audio for MX35 Luigi
 * Copyright 2009 Lab126 Inc.
 * Author: Manish Lachwani (lachwani@lab126.com)
 *
 * Taken from mx31ads_wm8753.c  --  SoC audio for mx31ads
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>

#include <asm/hardware.h>
#include <asm/arch/dam.h>

#include "../codecs/wm8960.h"
#include "imx-pcm.h"
#include "imx-ssi.h"

/* BCLK clock dividers */
#define WM8960_BCLK_DIV_1		(0)
#define WM8960_BCLK_DIV_2		(1 << 1)
#define WM8960_BCLK_DIV_4		(1 << 2)
#define WM8960_BCLK_DIV_8		(0x7)
#define WM8960_BCLK_DIV_16		(0xa)

/*
 * DAI Left/Right Clocks.
 *
 * Specifies whether the DAI can support different samples for similtanious
 * playback and capture. This usually requires a seperate physical frame
 * clock for playback and capture.
 */
#define SND_SOC_DAIFMT_SYNC             (0 << 5) /* Tx FRM = Rx FRM */
#define SND_SOC_DAIFMT_ASYNC            (1 << 5) /* Tx FRM ~ Rx FRM */

/*
 * WM8960 Clock dividers
 */
#define WM8960_SYSCLKDIV 		0
#define WM8960_DACDIV			1
#define WM8960_OPCLKDIV 		2
#define WM8960_DCLKDIV			3
#define WM8960_TOCLKSEL  		4
#define WM8960_SYSCLKSEL		5
#define WM8960_BCLKDIV			6
#define WM8960_PRESCALEDIV		7
#define WM8960_ADCDIV			8

struct wm8960_setup_data {
	unsigned short i2c_address;
};
int wm8960_capture;

static struct snd_soc_card mx35luigi;

static int mx35luigi_hifi_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	unsigned int pll_out = 0, bclk = 0, fmt = 0;
	int ret = 0;
	unsigned int sysclk_div = 0, pll_prescale = 0, dacdiv = 0, adcdiv = 0;
	unsigned int channels = params_channels(params);
	int pll_id = 0;

	/*
	 * The WM8960 is better at generating accurate audio clocks than the
	 * MX35 SSI controller, so we will use it as master when we can.
	 */
	switch (params_rate(params)) {
	case 8000:
	case 16000:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		bclk = WM8960_BCLK_DIV_8;
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		dacdiv = 0x28;
		adcdiv = 0x140;
		pll_id = 0;
		break;
	case 48000:
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		pll_out = 12288000;
		if (channels == 1) {
			/* Mono mode */
			dacdiv = 0x10;	/* 24.0 KHz */
			bclk = WM8960_BCLK_DIV_8;
		}
		else {	
			/* Stereo Mode */
			dacdiv = 0x0;	/* 48.0 KHz */
			bclk = WM8960_BCLK_DIV_4;
		}
		pll_id = 1;
		break;
	case 32000:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		bclk = WM8960_BCLK_DIV_8;
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		dacdiv = 0x0;
		break;
	case 11025:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		bclk = WM8960_BCLK_DIV_8;
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		dacdiv = 0x20;
		break;
	case 22050:
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		if (channels == 1) {
			/* Mono Mode */
			bclk = WM8960_BCLK_DIV_16;
			dacdiv = 0x20;	/* 11.025 KHz */
		}
		else {
			/* Stereo Mode */
			bclk = WM8960_BCLK_DIV_8;
			dacdiv = 0x10;	/* 22.05 KHz */
		}
		pll_out = 11289600;
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		break;
	case 44100:
		sysclk_div = 0x4;
		pll_prescale = 0x10;
		if (channels == 1) {
			/* Mono mode */
			dacdiv = 0x10;	/* 22.05 KHz */
			bclk = WM8960_BCLK_DIV_16;
		}
		else {	
			/* Stereo Mode */
			dacdiv = 0x0;	/* 44.1 KHz */
			bclk = WM8960_BCLK_DIV_8;
		}
		fmt = SND_SOC_DAIFMT_CBM_CFM;
		pll_out = 11289600;
		break;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		wm8960_capture = 0;
	else
		wm8960_capture = 1;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai,
		SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_SYNC | fmt);
	if (ret < 0)
		return ret;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		/* set cpu DAI configuration */
		ret = snd_soc_dai_set_fmt(cpu_dai,
			SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_SYNC | SND_SOC_DAIFMT_NB_IF | fmt);
	}
	else {
		ret = snd_soc_dai_set_fmt(cpu_dai,
			SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_SYNC | SND_SOC_DAIFMT_NB_NF | fmt);
	}

	if (ret < 0)
		return ret;

	/* No clockout from IMX SSI since it is slave */
	snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_IN);

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKSEL, 1);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_SYSCLKDIV, sysclk_div);
	if (ret < 0)
		return ret;	

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_PRESCALEDIV, pll_prescale);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_DACDIV, dacdiv);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_ADCDIV, adcdiv);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8960_BCLKDIV, bclk);
	if (ret < 0)
		return ret;

	/* 24MHz CLKO coming from MX35 */
	ret = snd_soc_dai_set_pll(codec_dai, pll_id, 24000000, pll_out);

        if (ret < 0)
                return ret;

	return 0;
}

static int mx35luigi_hifi_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;

	/* disable the PLL */
	return snd_soc_dai_set_pll(codec_dai, 0, 0, 0);
}

/*
 * mx35luigi WM8960 HiFi DAI opserations.
 */
static struct snd_soc_ops mx35luigi_hifi_ops = {
	.hw_params = mx35luigi_hifi_hw_params,
	.hw_free = mx35luigi_hifi_hw_free,
};

static int mx35luigi_voice_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void mx35luigi_voice_shutdown(struct snd_pcm_substream *substream)
{
}

static int mx35luigi_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int mx35luigi_resume(struct platform_device *pdev)
{
	return 0;
}

static int mx35luigi_probe(struct platform_device *pdev)
{
	return 0;
}

static int mx35luigi_remove(struct platform_device *pdev)
{
	return 0;
}

/* example machine audio_mapnections */
static const char* audio_map[][9] = {
	/* mic is connected to mic1 - with bias */
	{"MIC1", NULL, "Mic Bias"},
	{"MIC1N", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Mic1 Jack"},
	{"Mic Bias", NULL, "Mic1 Jack"},
	{"ACIN", NULL, "ACOP"},
	{"Headphone Jack", NULL, "OUT1R"},
	{"Headphone Jack", NULL, "OUT1L"},
	{"Line Out Jack", NULL, "OUT2R"},
	{"Line Out Jack", NULL, "OUT2L"},
	{NULL, NULL, NULL},
};

/* headphone detect support on my board */
static const char * hp_pol[] = {"Headphone", "Speaker"};
static const struct soc_enum wm8960_enum =
	SOC_ENUM_SINGLE(WM8960_ADDCTL2, 1, 2, hp_pol);

static const struct snd_kcontrol_new wm8960_mx35luigi_controls[] = {
	SOC_SINGLE("Headphone Detect Switch", WM8960_ADDCTL2, 6, 1, 0),
	SOC_ENUM("Headphone Detect Polarity", wm8960_enum),
};

/*
 * DAM/AUDMUX setting
 *
 * SSI1 (FIFO 0) is connected to PORT #1
 * WM8960 Codec is connected to PORT #4
 */
static void mx35luigi_wm8960_init_dam(void)
{	
	int source_port = port_1; /* ssi */
	int target_port = port_4; /* dai */

	dam_reset_register(source_port);
	dam_reset_register(target_port);	

       dam_select_mode(source_port, normal_mode);
       dam_select_mode(target_port, normal_mode);


	dam_set_synchronous(source_port, true);
	dam_set_synchronous(target_port, true);

	dam_select_RxD_source(source_port, target_port);
	dam_select_RxD_source(target_port, source_port);

	dam_select_TxFS_direction(source_port, signal_out);
	dam_select_TxFS_source(source_port, false, target_port);

	dam_select_TxClk_direction(source_port, signal_out);
	dam_select_TxClk_source(source_port, false, target_port);


       dam_select_RxFS_direction(source_port, signal_out);
       dam_select_RxFS_source(source_port, false, target_port);

       dam_select_RxClk_direction(source_port, signal_out);
       dam_select_RxClk_source(source_port, false, target_port);

}

/*
 * This is an example machine initialisation for a wm8960 connected to a
 * mx35luigi. It is missing logic to detect hp/mic insertions and logic
 * to re-route the audio in such an event.
 */
static int mx35luigi_wm8960_init(struct snd_soc_codec *codec)
{
	/* set up mx35luigi specific audio path audio_mapnects */
	snd_soc_dapm_add_routes(codec, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_enable_pin(codec, "Line Out Jack");

	snd_soc_dapm_sync(codec);

	/* Configure the AUDMUX */
	mx35luigi_wm8960_init_dam();

	return 0;
}

static struct snd_soc_dai_link mx35luigi_dai[] = {
{ /* Hifi Playback - for similatious use with voice below */
	.name = "WM8960",
	.stream_name = "WM8960 HiFi",
	.cpu_dai = &imx_ssi_dai,
	.codec_dai = &wm8960_dai,
	.init = mx35luigi_wm8960_init,
	.ops = &mx35luigi_hifi_ops,
}
};

static struct snd_soc_card mx35luigi = {
	.name = "mx35luigi",
	.probe = mx35luigi_probe,
	.remove = mx35luigi_remove,
	.suspend_pre = mx35luigi_suspend,
	.resume_post = mx35luigi_resume,
	.platform = &imx_soc_platform,
	.dai_link = mx35luigi_dai,
	.num_links = ARRAY_SIZE(mx35luigi_dai),
};


static struct snd_soc_device mx35luigi_snd_devdata = {
	.card = &mx35luigi,
	.codec_dev = &soc_codec_dev_wm8960,
};

static struct platform_device *mx35luigi_snd_device;

static int __init mx35luigi_init(void)
{
	int ret;

	mx35luigi_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mx35luigi_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mx35luigi_snd_device, &mx35luigi_snd_devdata);
	mx35luigi_snd_devdata.dev = &mx35luigi_snd_device->dev;
	ret = platform_device_add(mx35luigi_snd_device);

	if (ret)
		platform_device_put(mx35luigi_snd_device);

	return ret;
}

static void __exit mx35luigi_exit(void)
{
	platform_device_unregister(mx35luigi_snd_device);
}

module_init(mx35luigi_init);
module_exit(mx35luigi_exit);

/* Module information */
MODULE_AUTHOR("Manish Lachwani, lachwani@lab126.com, www.lab126.com");
MODULE_DESCRIPTION("ALSA SoC WM8960 mx35luigi");
MODULE_LICENSE("GPL");
