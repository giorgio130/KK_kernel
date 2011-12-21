/*
 * imx-3stack-sgtl5000.c  --  i.MX 3Stack Driver for Freescale SGTL5000 Codec
 *
 * Copyright 2008-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  Revision history
 *    21th Oct 2008   Initial version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/bitops.h>
#include <linux/platform_device.h>
#include <linux/regulator/regulator.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <asm/gpio.h>
#include <asm/arch/dma.h>
#include <asm/arch/spba.h>
#include <asm/arch/clock.h>
#include <asm/arch/mxc.h>

#include "../codecs/sgtl5000.h"
#include "imx-ssi.h"
#include "imx-pcm.h"

#ifdef CONFIG_MXC_ASRC
#include <linux/mxc_asrc.h>
#endif

#ifdef CONFIG_MXC_ASRC
static unsigned int sgtl5000_rates[] = {
	0,
	32000,
	44100,
	48000,
	96000,
};

struct asrc_esai {
	struct snd_soc_pcm_stream *codec_dai_playback;
	struct snd_soc_pcm_stream *cpu_dai_playback;
	const struct snd_soc_pcm_stream *org_codec_playback;
	const struct snd_soc_pcm_stream *org_cpu_playback;
	enum asrc_pair_index asrc_index;
	unsigned int output_sample_rate;
};

static struct asrc_esai asrc_ssi_data;

#endif

void gpio_activate_audio_ports(void);

/* SSI BCLK and LRC master */
#define SGTL5000_SSI_MASTER	1

struct imx_3stack_pcm_state {
	int hw;
	int playback_active;
	int capture_active;
};

struct imx_3stack_priv {
	struct regulator *reg_vddio;
	struct regulator *reg_vdda;
	struct regulator *reg_vddd;
};

static int imx_3stack_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct imx_3stack_pcm_state *state = pcm_link->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		state->capture_active = 1;
	else {
#ifdef CONFIG_MXC_ASRC
		if (asrc_ssi_data.output_sample_rate != 0) {
			struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
			struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
			asrc_ssi_data.codec_dai_playback =
			    (struct snd_soc_pcm_stream *)
			    kmalloc(sizeof(struct snd_soc_pcm_stream),
				    GFP_KERNEL);
			asrc_ssi_data.cpu_dai_playback =
			    (struct snd_soc_pcm_stream *)
			    kmalloc(sizeof(struct snd_soc_pcm_stream),
				    GFP_KERNEL);
			memcpy(asrc_ssi_data.codec_dai_playback,
			       codec_dai->playback,
			       sizeof(struct snd_soc_pcm_stream));
			memcpy(asrc_ssi_data.cpu_dai_playback,
			       cpu_dai->playback,
			       sizeof(struct snd_soc_pcm_stream));
			asrc_ssi_data.org_codec_playback =
			    (const struct snd_soc_pcm_stream *)codec_dai->
			    playback;
			asrc_ssi_data.org_cpu_playback =
			    (const struct snd_soc_pcm_stream *)cpu_dai->
			    playback;
			asrc_ssi_data.codec_dai_playback->rates =
			    SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT;
			asrc_ssi_data.cpu_dai_playback->rates =
			    SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT;
			codec_dai->playback = asrc_ssi_data.codec_dai_playback;
			cpu_dai->playback = asrc_ssi_data.cpu_dai_playback;
		}
#endif
		state->playback_active = 1;
	}
	return 0;
}

static int imx_3stack_audio_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
	struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
	struct imx_3stack_pcm_state *state = pcm_link->private_data;
	unsigned int channels = params_channels(params);
	unsigned int rate = params_rate(params);
	u32 dai_format;

	/* only need to do this once as capture and playback are sync */
	if (state->hw)
		return 0;
	state->hw = 1;

#ifdef CONFIG_MXC_ASRC
	if ((asrc_ssi_data.output_sample_rate != 0)
	    && (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)) {
		unsigned int asrc_input_rate = rate;
		unsigned int channel = params_channels(params);
		struct mxc_runtime_data *pcm_data =
		    substream->runtime->private_data;
		struct asrc_config config;
		struct mxc_sgtl5000_platform_data *plat;
		int retVal = 0;
		retVal = asrc_req_pair(channel, &asrc_ssi_data.asrc_index);
		if (retVal < 0) {
			pr_err("asrc_req_pair fail\n");
			return -1;
		}
		config.pair = asrc_ssi_data.asrc_index;
		config.channel_num = channel;
		config.input_sample_rate = asrc_input_rate;
		config.output_sample_rate = asrc_ssi_data.output_sample_rate;
		config.inclk = INCLK_NONE;
		config.word_width = 32;
		plat =
		    pcm_link->machine->pdev->dev.platform_data;
		if (plat->src_port == 1)
			config.outclk = OUTCLK_SSI1_TX;
		else
			config.outclk = OUTCLK_SSI2_TX;
		retVal = asrc_config_pair(&config);
		if (retVal < 0) {
			pr_err("Fail to config asrc\n");
			asrc_release_pair(asrc_ssi_data.asrc_index);
			return retVal;
		}
		rate = asrc_ssi_data.output_sample_rate;
		pcm_data->asrc_index = asrc_ssi_data.asrc_index;
		pcm_data->asrc_enable = 1;
	}
#endif

#if SGTL5000_SSI_MASTER
	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
	    SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_SYNC;
	if (channels == 2)
		dai_format |= SND_SOC_DAIFMT_TDM;

	/* set codec DAI configuration */
	codec_dai->ops->set_fmt(codec_dai, dai_format);

	/* set cpu DAI configuration */
	cpu_dai->ops->set_fmt(cpu_dai, dai_format);
#else
	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
	    SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_SYNC;
	if (channels == 2)
		dai_format |= SND_SOC_DAIFMT_TDM;

	/* set codec DAI configuration */
	codec_dai->ops->set_fmt(codec_dai, dai_format);

	/* set cpu DAI configuration */
	cpu_dai->ops->set_fmt(cpu_dai, dai_format);
#endif

	/* set i.MX active slot mask */
	cpu_dai->ops->set_tdm_slot(cpu_dai,
				   channels == 1 ? 0xfffffffe : 0xfffffffc,
				   2);

	/* set the SSI system clock as input (unused) */
	cpu_dai->ops->set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_IN);

	/* Set codec lrclk clock */
	codec_dai->ops->set_sysclk(codec_dai, SGTL5000_LRCLK, rate, 0);

	return 0;
}

static void imx_3stack_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct imx_3stack_pcm_state *state = pcm_link->private_data;

	state->hw = 0;
	/*
	 * We need to keep track of active streams in master mode and
	 * switch LRC source if necessary.
	 */

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		state->capture_active = 0;
	else {
#ifdef CONFIG_MXC_ASRC
		if (asrc_ssi_data.output_sample_rate != 0) {
			struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
			struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
			kfree(asrc_ssi_data.codec_dai_playback);
			kfree(asrc_ssi_data.cpu_dai_playback);
			codec_dai->playback = asrc_ssi_data.org_codec_playback;
			cpu_dai->playback = asrc_ssi_data.org_cpu_playback;
			asrc_release_pair(asrc_ssi_data.asrc_index);
		}
	}
#endif
	state->playback_active = 0;
}

/*
 * imx_3stack SGTL5000 audio DAI opserations.
 */
static struct snd_soc_ops imx_3stack_audio_ops = {
	.startup = imx_3stack_startup,
	.shutdown = imx_3stack_shutdown,
	.hw_params = imx_3stack_audio_hw_params,
};

static void imx_3stack_init_dam(int ssi_port, int dai_port)
{
	/* SGTL5000 uses SSI1 or SSI2 via AUDMUX port dai_port for audio */

	/* reset port ssi_port & dai_port */
	DAM_PTCR(ssi_port) = 0;
	DAM_PDCR(ssi_port) = 0;
	DAM_PTCR(dai_port) = 0;
	DAM_PDCR(dai_port) = 0;

	/* set to synchronous */
	DAM_PTCR(ssi_port) |= AUDMUX_PTCR_SYN;
	DAM_PTCR(dai_port) |= AUDMUX_PTCR_SYN;

#if SGTL5000_SSI_MASTER
	/* set Rx sources ssi_port <--> dai_port */
	DAM_PDCR(ssi_port) |= AUDMUX_PDCR_RXDSEL(dai_port);
	DAM_PDCR(dai_port) |= AUDMUX_PDCR_RXDSEL(ssi_port);

	/* set Tx frame direction and source  dai_port--> ssi_port output */
	DAM_PTCR(ssi_port) |= AUDMUX_PTCR_TFSDIR;
	DAM_PTCR(ssi_port) |= AUDMUX_PTCR_TFSSEL(AUDMUX_FROM_TXFS, dai_port);

	/* set Tx Clock direction and source dai_port--> ssi_port output */
	DAM_PTCR(ssi_port) |= AUDMUX_PTCR_TCLKDIR;
	DAM_PTCR(ssi_port) |= AUDMUX_PTCR_TCSEL(AUDMUX_FROM_TXFS, dai_port);
#else
	/* set Rx sources ssi_port <--> dai_port */
	DAM_PDCR(ssi_port) |= AUDMUX_PDCR_RXDSEL(dai_port);
	DAM_PDCR(dai_port) |= AUDMUX_PDCR_RXDSEL(ssi_port);

	/* set Tx frame direction and source  ssi_port --> dai_port output */
	DAM_PTCR(dai_port) |= AUDMUX_PTCR_TFSDIR;
	DAM_PTCR(dai_port) |= AUDMUX_PTCR_TFSSEL(AUDMUX_FROM_TXFS, ssi_port);

	/* set Tx Clock direction and source ssi_port--> dai_port output */
	DAM_PTCR(dai_port) |= AUDMUX_PTCR_TCLKDIR;
	DAM_PTCR(dai_port) |= AUDMUX_PTCR_TCSEL(AUDMUX_FROM_TXFS, ssi_port);
#endif

}
static int imx_3stack_pcm_new(struct snd_soc_pcm_link *pcm_link)
{
	struct imx_3stack_pcm_state *state;
	int ret;

	state = kzalloc(sizeof(struct imx_3stack_pcm_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	pcm_link->audio_ops = &imx_3stack_audio_ops;
	pcm_link->private_data = state;

	ret = snd_soc_pcm_new(pcm_link, 1, 1);
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to create audio pcm\n", __func__);
		kfree(state);
		return ret;
	}

	printk(KERN_INFO "i.MX 3STACK SGTL5000 Audio Driver");

	return 0;
}

static int imx_3stack_pcm_free(struct snd_soc_pcm_link *pcm_link)
{
	kfree(pcm_link->private_data);
	return 0;
}

static const struct snd_soc_pcm_link_ops imx_3stack_pcm_ops = {
	.new = imx_3stack_pcm_new,
	.free = imx_3stack_pcm_free,
};

/* imx_3stack machine audio map */
static const char *audio_map[][3] = {

	/* Mic Jack --> MIC_IN (with automatic bias) */
	{"MIC_IN", NULL, "Mic Jack"},

	/* Line in Jack --> LINE_IN */
	{"LINE_IN", NULL, "Line In Jack"},

	/* HP_OUT --> Headphone Jack */
	{"Headphone Jack", NULL, "HP_OUT"},

	/* LINE_OUT --> Line Out Jack */
	{"Line Out Jack", NULL, "LINE_OUT"},

	/* LINE_OUT --> Ext Speaker */
	{"Ext Spk", NULL, "LINE_OUT"},

	{NULL, NULL, NULL},
};



#ifdef CONFIG_PM
static int imx_3stack_sgtl5000_audio_suspend(struct platform_device *dev,
					   pm_message_t state)
{

	int ret = 0;

	return ret;
}

static int imx_3stack_sgtl5000_audio_resume(struct platform_device *dev)
{

	int ret = 0;

	return ret;
}

#else
#define imx_3stack_sgtl5000_audio_suspend	NULL
#define imx_3stack_sgtl5000_audio_resume	NULL
#endif

static struct snd_soc_pcm_link *sgtl5000_3stack_pcm_link;
static int sgtl5000_jack_func;
static int sgtl5000_spk_func;

static void headphone_detect_handler(struct work_struct *work)
{
	struct mxc_sgtl5000_platform_data *plat;
	int hp_status;

	sysfs_notify(&sgtl5000_3stack_pcm_link->machine->pdev->dev.kobj, NULL,
		     "headphone");

	plat = sgtl5000_3stack_pcm_link->machine->pdev->dev.platform_data;
	hp_status = plat->hp_status();
	if (hp_status)
		set_irq_type(plat->hp_irq, IRQT_FALLING);
	else
		set_irq_type(plat->hp_irq, IRQT_RISING);
	enable_irq(plat->hp_irq);
}

static DECLARE_DELAYED_WORK(hp_event, headphone_detect_handler);

static irqreturn_t imx_headphone_detect_handler(int irq, void *data)
{
	disable_irq(irq);
	schedule_delayed_work(&hp_event, msecs_to_jiffies(200));
	return IRQ_HANDLED;
}

static ssize_t show_headphone(struct device_driver *dev, char *buf)
{
	u16 hp_status;
	struct snd_soc_pcm_link *pcm_link;
	struct snd_soc_codec *codec;
	struct mxc_sgtl5000_platform_data *plat;

	pcm_link = sgtl5000_3stack_pcm_link;
	codec = pcm_link->codec;
	plat = pcm_link->machine->pdev->dev.platform_data;

	/* determine whether hp is plugged in */
	hp_status = plat->hp_status();

	if (hp_status == 0)
		strcpy(buf, "speaker\n");
	else
		strcpy(buf, "headphone\n");

	return strlen(buf);
}

static DRIVER_ATTR(headphone, S_IRUGO | S_IWUSR, show_headphone, NULL);

static const char *jack_function[] = { "off", "on"
};

static const char *spk_function[] = { "off", "on" };

static const struct soc_enum sgtl5000_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, jack_function),
	SOC_ENUM_SINGLE_EXT(2, spk_function),
};

static int sgtl5000_get_jack(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = sgtl5000_jack_func;
	return 0;
}

static int sgtl5000_set_jack(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_machine *machine = sgtl5000_3stack_pcm_link->machine;

	if (sgtl5000_jack_func == ucontrol->value.enumerated.item[0])
		return 0;

	sgtl5000_jack_func = ucontrol->value.enumerated.item[0];
	snd_soc_dapm_set_endpoint(machine, "Headphone Jack",
				  sgtl5000_jack_func);
	return 1;
}

static int sgtl5000_get_spk(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = sgtl5000_spk_func;
	return 0;
}

static int sgtl5000_set_spk(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_machine *machine = sgtl5000_3stack_pcm_link->machine;

	if (sgtl5000_spk_func == ucontrol->value.enumerated.item[0])
		return 0;

	sgtl5000_spk_func = ucontrol->value.enumerated.item[0];
	snd_soc_dapm_set_endpoint(machine, "Line Out Jack", sgtl5000_spk_func);
	return 1;
}

static int spk_amp_event(struct snd_soc_dapm_widget *w,
			 struct snd_kcontrol *kcontrol, int event) {
	struct mxc_sgtl5000_platform_data *plat;
	plat = sgtl5000_3stack_pcm_link->machine->pdev->dev.platform_data;

	if (plat->amp_enable == NULL)
		return 0;

	if (SND_SOC_DAPM_EVENT_ON(event))
		plat->amp_enable(1);
	else
		plat->amp_enable(0);

	return 0;
}

/* imx_3stack machine dapm widgets */
static const struct snd_soc_dapm_widget imx_3stack_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_LINE("Line In Jack", NULL),
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", spk_amp_event),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

static const struct snd_kcontrol_new sgtl5000_machine_controls[] = {
	SOC_ENUM_EXT("Jack Function", sgtl5000_enum[0], sgtl5000_get_jack,
		     sgtl5000_set_jack),
	SOC_ENUM_EXT("Speaker Function", sgtl5000_enum[1], sgtl5000_get_spk,
		     sgtl5000_set_spk),
};

#ifdef CONFIG_MXC_ASRC

static int asrc_func;

static const char *asrc_function[] =
    { "disable", "32KHz", "44.1KHz", "48KHz", "96KHz" };

static const struct soc_enum asrc_enum[] = {
	SOC_ENUM_SINGLE_EXT(5, asrc_function),
};

static int asrc_get_rate(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.enumerated.item[0] = asrc_func;
	return 0;
}

static int asrc_set_rate(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_value *ucontrol)
{
	if (asrc_func == ucontrol->value.enumerated.item[0])
		return 0;

	asrc_func = ucontrol->value.enumerated.item[0];
	asrc_ssi_data.output_sample_rate = sgtl5000_rates[asrc_func];

	return 1;
}

static const struct snd_kcontrol_new asrc_controls[] = {
	SOC_ENUM_EXT("ASRC", asrc_enum[0], asrc_get_rate,
		     asrc_set_rate),
};

#endif

static int mach_probe(struct snd_soc_machine *machine)
{
	struct snd_soc_codec *codec;
	struct snd_soc_dai *codec_dai;
	struct snd_soc_pcm_link *pcm_link;
	struct platform_device *pdev = machine->pdev;
	struct mxc_sgtl5000_platform_data *plat = pdev->dev.platform_data;
	struct sgtl5000_platform_data *codec_data;
	struct imx_3stack_priv *priv;
	struct regulator *reg;

	int i, ret;

	pcm_link = list_first_entry(&machine->active_list,
				    struct snd_soc_pcm_link, active_list);
	sgtl5000_3stack_pcm_link = pcm_link;

	codec = pcm_link->codec;

	codec_dai = pcm_link->codec_dai;
	codec_dai->ops->set_sysclk(codec_dai, SGTL5000_SYSCLK, plat->sysclk, 0);

	priv = kzalloc(sizeof(struct imx_3stack_priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	codec_data = kzalloc(sizeof(struct sgtl5000_platform_data), GFP_KERNEL);
	if (!codec_data) {
		ret = -ENOMEM;
		goto err_codec_data;
	}

	ret = -EINVAL;
	if (plat->init && plat->init())
		goto err_plat_init;
	if (plat->vddio_reg) {
		reg = regulator_get(&pdev->dev, plat->vddio_reg);
		if (IS_ERR(reg))
			goto err_reg_vddio;
		priv->reg_vddio = reg;
	}
	if (plat->vdda_reg) {
		reg = regulator_get(&pdev->dev, plat->vdda_reg);
		if (IS_ERR(reg))
			goto err_reg_vdda;
		priv->reg_vdda = reg;
	}
	if (plat->vddd_reg) {
		reg = regulator_get(&pdev->dev, plat->vddd_reg);
		if (IS_ERR(reg))
			goto err_reg_vddd;
		priv->reg_vddd = reg;
	}
	machine->platform_data = priv;

	if (priv->reg_vdda) {
		ret = regulator_set_voltage(priv->reg_vdda, plat->vdda);
		regulator_enable(priv->reg_vdda);
	}
	if (priv->reg_vddio) {
		regulator_set_voltage(priv->reg_vddio, plat->vddio);
		regulator_enable(priv->reg_vddio);
	}
	if (priv->reg_vddd) {
		regulator_set_voltage(priv->reg_vddd, plat->vddd);
		regulator_enable(priv->reg_vddd);
	}

	/* The SGTL5000 has an internal reset that is deasserted 8 SYS_MCLK
	   cycles after all power rails have been brought up. After this time
	   communication can start */
	msleep(1);

	codec_data->vddio = plat->vddio / 1000; /* uV to mV */
	codec_data->vdda = plat->vdda / 1000;
	codec_data->vddd = plat->vddd / 1000;
	codec->platform_data = codec_data;

	ret = codec->ops->io_probe(codec, machine);
	if (ret < 0)
		goto err_card_reg;

	gpio_activate_audio_ports();
	imx_3stack_init_dam(plat->src_port, plat->ext_port);

	/* Add imx_3stack specific widgets */
	for (i = 0; i < ARRAY_SIZE(imx_3stack_dapm_widgets); i++) {
		snd_soc_dapm_new_control(machine, codec,
					 &imx_3stack_dapm_widgets[i]);
	}

	/* set up imx_3stack specific audio path audio map */
	for (i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(machine, audio_map[i][0],
					   audio_map[i][1], audio_map[i][2]);
	}

	/* connect and enable all imx_3stack SGTL5000 jacks (for now) */
	snd_soc_dapm_set_endpoint(machine, "Line In Jack", 1);
	snd_soc_dapm_set_endpoint(machine, "Mic Jack", 1);
	snd_soc_dapm_set_endpoint(machine, "Line Out Jack", 1);
	snd_soc_dapm_set_endpoint(machine, "Headphone Jack", 1);
	sgtl5000_jack_func = 1;
	sgtl5000_spk_func = 1;

	snd_soc_dapm_set_policy(machine, SND_SOC_DAPM_POLICY_STREAM);
	snd_soc_dapm_sync_endpoints(machine);

#ifdef CONFIG_MXC_ASRC
	for (i = 0; i < ARRAY_SIZE(asrc_controls); i++) {
		ret = snd_ctl_add(machine->card,
				  snd_soc_cnew(&asrc_controls[i], codec, NULL));
		if (ret < 0)
			goto err_card_reg;
	}
	asrc_ssi_data.output_sample_rate = sgtl5000_rates[asrc_func];

#endif

	for (i = 0; i < ARRAY_SIZE(sgtl5000_machine_controls); i++) {
		ret = snd_ctl_add(machine->card,
				  snd_soc_cnew(&sgtl5000_machine_controls[i],
					       codec, NULL));
		if (ret < 0)
			goto err_card_reg;
	}

	/* register card with ALSA upper layers */
	ret = snd_soc_register_card(machine);
	if (ret < 0) {
		pr_err("%s: failed to register sound card\n",
		       __func__);
		goto err_card_reg;
	}

	if (plat->hp_status())
		ret = request_irq(plat->hp_irq,
				  imx_headphone_detect_handler,
				  IRQT_FALLING, pdev->name, machine);
	else
		ret = request_irq(plat->hp_irq,
				  imx_headphone_detect_handler,
				  IRQT_RISING, pdev->name, machine);
	if (ret < 0) {
		pr_err("%s: request irq failed\n", __func__);
		goto err_card_reg;
	}

	return 0;

err_card_reg:
	if (priv->reg_vddd)
		regulator_put(priv->reg_vddd, &pdev->dev);
err_reg_vddd:
	if (priv->reg_vdda)
		regulator_put(priv->reg_vdda, &pdev->dev);
err_reg_vdda:
	if (priv->reg_vddio)
		regulator_put(priv->reg_vddio, &pdev->dev);
err_reg_vddio:
	if (plat->finit)
		plat->finit();
err_plat_init:
	kfree(codec_data);
	codec->platform_data = NULL;
err_codec_data:
	kfree(priv);
	machine->platform_data = NULL;
	return ret;
}

static int mach_remove(struct snd_soc_machine *machine)
{
	struct snd_soc_codec *codec;
	struct snd_soc_pcm_link *pcm_link;
	struct imx_3stack_priv *priv;
	struct platform_device *pdev = machine->pdev;
	struct mxc_sgtl5000_platform_data *plat = pdev->dev.platform_data;

	if (machine->platform_data)
		free_irq(plat->hp_irq, machine);

	pcm_link = list_first_entry(&machine->active_list,
				    struct snd_soc_pcm_link, active_list);

	codec = pcm_link->codec;

	if (codec && codec->platform_data) {
		kfree(codec->platform_data);
		codec->platform_data = NULL;
	}

	if (machine->platform_data) {
		priv = machine->platform_data;
		if (priv->reg_vddio)
			regulator_disable(priv->reg_vddio);
		if (priv->reg_vddd)
			regulator_disable(priv->reg_vddd);
		if (priv->reg_vdda)
			regulator_disable(priv->reg_vdda);
		if (plat->amp_enable)
			plat->amp_enable(0);
		if (plat->finit)
			plat->finit();
		if (priv->reg_vdda)
			regulator_put(priv->reg_vdda, &pdev->dev);
		if (priv->reg_vddio)
			regulator_put(priv->reg_vddio, &pdev->dev);
		if (priv->reg_vddd)
			regulator_put(priv->reg_vddd, &pdev->dev);
		kfree(machine->platform_data);
		machine->platform_data = NULL;
	}

	return 0;
}

static struct snd_soc_machine_ops machine_ops = {
	.mach_probe = mach_probe,
	.mach_remove = mach_remove,
};

static int __devinit imx_3stack_sgtl5000_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_machine *machine;
	struct mxc_sgtl5000_platform_data *plat = pdev->dev.platform_data;
	struct snd_soc_pcm_link *audio;
	int ret;

	machine = kzalloc(sizeof(struct snd_soc_machine), GFP_KERNEL);
	if (machine == NULL)
		return -ENOMEM;

	platform_set_drvdata(pdev, machine);
	machine->owner = THIS_MODULE;
	machine->pdev = pdev;
	machine->name = "i.MX_3STACK";
	machine->longname = "SGTL5000";
	machine->ops = &machine_ops;

	/* register card */
	ret =
	    snd_soc_new_card(machine, 1, SNDRV_DEFAULT_IDX1,
			     SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		pr_err("%s: failed to create pcms\n", __func__);
		goto err;
	}

	/* SGTL5000 interface */
	ret = -ENODEV;

	if (plat->src_port == 2)
		audio = snd_soc_pcm_link_new(machine, "imx_3stack-audio",
					    &imx_3stack_pcm_ops, imx_pcm,
					    sgtl5000_codec, sgtl5000_dai,
					    imx_ssi_3);
	else
		audio = snd_soc_pcm_link_new(machine, "imx_3stack-audio",
					    &imx_3stack_pcm_ops, imx_pcm,
					    sgtl5000_codec, sgtl5000_dai,
					    imx_ssi_1);
	if (audio == NULL) {
		pr_err("failed to create PCM link\n");
		goto link_err;
	}
	ret = snd_soc_pcm_link_attach(audio);
	if (ret < 0) {
		pr_err("%s: failed to attach audio pcm\n", __func__);
		goto link_err;
	}

	ret = driver_create_file(pdev->dev.driver, &driver_attr_headphone);
	if (ret < 0) {
		pr_err("%s:failed to create driver_attr_headphone\n", __func__);
		goto sysfs_err;
	}

	return ret;

sysfs_err:
	driver_remove_file(pdev->dev.driver, &driver_attr_headphone);
link_err:
	snd_soc_machine_free(machine);
err:
	kfree(machine);
	return ret;

}

static int __devexit
imx_3stack_sgtl5000_audio_remove(struct platform_device *pdev)
{
	struct snd_soc_machine *machine = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec;
	struct snd_soc_pcm_link *pcm_link;

	pcm_link = list_first_entry(&machine->active_list,
				    struct snd_soc_pcm_link, active_list);

	codec = pcm_link->codec;
	codec->ops->io_remove(codec, machine);

	snd_soc_machine_free(machine);
	kfree(machine);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const char imx_3stack_audio[32] = {
	"sgtl5000-imx"
};

static struct platform_driver imx_3stack_sgtl5000_audio_driver = {
	.probe = imx_3stack_sgtl5000_audio_probe,
	.remove = __devexit_p(imx_3stack_sgtl5000_audio_remove),
	.suspend = imx_3stack_sgtl5000_audio_suspend,
	.resume = imx_3stack_sgtl5000_audio_resume,
	.driver = {
		   .name = imx_3stack_audio,
		   },
};

static int __init imx_3stack_sgtl5000_audio_init(void)
{
	return platform_driver_register(&imx_3stack_sgtl5000_audio_driver);
}

static void __exit imx_3stack_sgtl5000_audio_exit(void)
{
	platform_driver_unregister(&imx_3stack_sgtl5000_audio_driver);
}

module_init(imx_3stack_sgtl5000_audio_init);
module_exit(imx_3stack_sgtl5000_audio_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc.");
MODULE_DESCRIPTION("SGTL5000 Driver for i.MX 3STACK");
MODULE_LICENSE("GPL");
