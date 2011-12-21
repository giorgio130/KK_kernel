/*
 * imx-3stack-wm8580.c  --  SoC 5.1 audio for imx_3stack
 *
 * Copyright 2008-2009 Freescale  Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/mcu_max8660-bus.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <asm/hardware.h>
#include <asm/arch/clock.h>
#include <asm/arch/mxc.h>

#include "imx-pcm.h"
#include "imx-esai.h"

#ifdef CONFIG_MXC_ASRC
#include <linux/mxc_asrc.h>
#endif

#ifdef CONFIG_MXC_ASRC
static unsigned int asrc_rates[] = {
	0,
	8000,
	11025,
	16000,
	22050,
	32000,
	44100,
	48000,
	64000,
	88200,
	96000,
	176400,
	192000,
};

static struct snd_pcm_hw_constraint_list hw_asrc_rates = {
	.count = ARRAY_SIZE(asrc_rates),
	.list = asrc_rates,
	.mask = 0,
};

struct asrc_esai {
	struct snd_soc_pcm_stream *codec_dai_playback;
	struct snd_soc_pcm_stream *cpu_dai_playback;
	const struct snd_soc_pcm_stream *org_codec_playback;
	const struct snd_soc_pcm_stream *org_cpu_playback;
	enum asrc_pair_index asrc_index;
	unsigned int output_sample_rate;
};

static struct asrc_esai asrc_esai_data;

#endif

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE;
module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for 5.1 codec sound card.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for 5.1 codec sound card.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable 5.1 codec sound card.");

struct imx_3stack_pcm_state {
	int lr_clk_active;
};
extern void gpio_activate_esai_ports(void);
extern void gpio_deactivate_esai_ports(void);
extern const char wm8580_codec[SND_SOC_CODEC_NAME_SIZE];
extern const char wm8580_dai[SND_SOC_CODEC_NAME_SIZE];

static struct snd_soc_machine *imx_3stack_mach;

static int imx_3stack_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct imx_3stack_pcm_state *state = pcm_link->private_data;
#ifdef CONFIG_MXC_ASRC
	if (asrc_esai_data.output_sample_rate != 0) {
		struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
		struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
		asrc_esai_data.codec_dai_playback =
		    (struct snd_soc_pcm_stream *)
		    kmalloc(sizeof(struct snd_soc_pcm_stream), GFP_KERNEL);
		asrc_esai_data.cpu_dai_playback = (struct snd_soc_pcm_stream *)
		    kmalloc(sizeof(struct snd_soc_pcm_stream), GFP_KERNEL);
		memcpy(asrc_esai_data.codec_dai_playback, codec_dai->playback,
		       sizeof(struct snd_soc_pcm_stream));
		memcpy(asrc_esai_data.cpu_dai_playback, cpu_dai->playback,
		       sizeof(struct snd_soc_pcm_stream));
		asrc_esai_data.org_codec_playback =
		    (const struct snd_soc_pcm_stream *)codec_dai->playback;
		asrc_esai_data.org_cpu_playback =
		    (const struct snd_soc_pcm_stream *)cpu_dai->playback;
		asrc_esai_data.codec_dai_playback->rates =
		    SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT;
		asrc_esai_data.cpu_dai_playback->rates =
		    SNDRV_PCM_RATE_8000_192000 | SNDRV_PCM_RATE_KNOT;
		codec_dai->playback = asrc_esai_data.codec_dai_playback;
		cpu_dai->playback = asrc_esai_data.cpu_dai_playback;
	}
#endif
	state->lr_clk_active++;
	return 0;
}

static void imx_3stack_shutdown(struct snd_pcm_substream
				*substream)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
	struct imx_3stack_pcm_state *state = pcm_link->private_data;

#ifdef CONFIG_MXC_ASRC
	if (asrc_esai_data.output_sample_rate != 0) {
		struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
		kfree(asrc_esai_data.codec_dai_playback);
		kfree(asrc_esai_data.cpu_dai_playback);
		codec_dai->playback = asrc_esai_data.org_codec_playback;
		cpu_dai->playback = asrc_esai_data.org_cpu_playback;
		asrc_release_pair(asrc_esai_data.asrc_index);
	}
#endif

	/* disable the PLL if there are no active Tx or Rx channels */
	if (!codec_dai->active)
		codec_dai->ops->set_pll(codec_dai, 0, 0, 0);
	state->lr_clk_active--;
}

static int imx_3stack_surround_hw_params(struct
					 snd_pcm_substream
					 *substream, struct
					 snd_pcm_hw_params
					 *params)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
	struct snd_soc_dai *codec_dai = pcm_link->codec_dai;
	struct imx_3stack_pcm_state *state = pcm_link->private_data;
	unsigned int rate = params_rate(params);
	u32 dai_format;
	unsigned int pll_out = 0, lrclk_ratio = 0;
#ifdef CONFIG_MXC_ASRC
	if (asrc_esai_data.output_sample_rate != 0) {
		unsigned int asrc_input_rate = rate;
		unsigned int channel = params_channels(params);
		struct mxc_runtime_data *pcm_data =
		    substream->runtime->private_data;
		struct asrc_config config;
		int retVal = 0;;
		retVal = snd_pcm_hw_constraint_list(substream->runtime, 0,
							 SNDRV_PCM_HW_PARAM_RATE,
							 &hw_asrc_rates);
		if (retVal < 0)
			return retVal;

		retVal = asrc_req_pair(channel, &asrc_esai_data.asrc_index);
		if (retVal < 0) {
			pr_err("asrc_req_pair fail\n");
			return -1;
		}

		config.pair = asrc_esai_data.asrc_index;
		config.channel_num = channel;
		config.input_sample_rate = asrc_input_rate;
		config.output_sample_rate = asrc_esai_data.output_sample_rate;
		config.inclk = INCLK_NONE;
		config.word_width = 32;
		config.outclk = OUTCLK_ESAI_TX;
		retVal = asrc_config_pair(&config);
		if (retVal < 0) {
			pr_err("Fail to config asrc\n");
			asrc_release_pair(asrc_esai_data.asrc_index);
			return retVal;
		}
		rate = asrc_esai_data.output_sample_rate;
		pcm_data->asrc_index = asrc_esai_data.asrc_index;
		pcm_data->asrc_enable = 1;
	}
#endif
	if (state->lr_clk_active > 1)
		return 0;
	switch (rate) {
	case 8000:
		lrclk_ratio = 5;
		pll_out = 6144000;
		break;
	case 11025:
		lrclk_ratio = 4;
		pll_out = 5644800;
		break;
	case 16000:
		lrclk_ratio = 3;
		pll_out = 6144000;
		break;
	case 32000:
		lrclk_ratio = 3;
		pll_out = 12288000;
		break;
	case 48000:
		lrclk_ratio = 2;
		pll_out = 12288000;
		break;
	case 64000:
		lrclk_ratio = 1;
		pll_out = 12288000;
		break;
	case 96000:
		lrclk_ratio = 2;
		pll_out = 24576000;
		break;
	case 128000:
		lrclk_ratio = 1;
		pll_out = 24576000;
		break;
	case 22050:
		lrclk_ratio = 4;
		pll_out = 11289600;
		break;
	case 44100:
		lrclk_ratio = 2;
		pll_out = 11289600;
		break;
	case 88200:
		lrclk_ratio = 0;
		pll_out = 11289600;
		break;
	case 176400:
		lrclk_ratio = 0;
		pll_out = 22579200;
		break;
	case 192000:
		lrclk_ratio = 0;
		pll_out = 24576000;
		break;
	default:
		pr_info("Rate not support.\n");
		return -EINVAL;;
	}

	dai_format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
	    SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_ASYNC;
	dai_format |= SND_SOC_DAIFMT_TDM;
	/* set codec DAI configuration */
	codec_dai->ops->set_fmt(codec_dai, dai_format);
	/* set cpu DAI configuration */
	cpu_dai->ops->set_fmt(cpu_dai, dai_format);
	/* set i.MX active slot mask */
	cpu_dai->ops->set_tdm_slot(cpu_dai, 0xffffffff, 32);
	/* set the ESAI system clock as input (unused) */
	cpu_dai->ops->set_sysclk(cpu_dai, 0, 0, SND_SOC_CLOCK_IN);
	codec_dai->ops->set_sysclk(codec_dai, 1, pll_out, 2);
	codec_dai->ops->set_sysclk(codec_dai, 2, pll_out, 2);
	/* set codec LRCLK and BCLK */
	codec_dai->ops->set_clkdiv(codec_dai, 0, 0);
	codec_dai->ops->set_clkdiv(codec_dai, 1, lrclk_ratio);
	codec_dai->ops->set_pll(codec_dai, 1, 12000000, pll_out);
	return 0;
}

/*
 * imx_3stack wm8580 HiFi DAI opserations.
 */
static struct snd_soc_ops imx_3stack_surround_ops = {
	.startup = imx_3stack_startup,
	.shutdown = imx_3stack_shutdown,
	.hw_params = imx_3stack_surround_hw_params,
};

static int imx_3stack_pcm_new(struct snd_soc_pcm_link
			      *pcm_link)
{
	int ret;
	struct imx_3stack_pcm_state *state;
	state = kzalloc(sizeof(struct imx_3stack_pcm_state), GFP_KERNEL);
	pcm_link->audio_ops = &imx_3stack_surround_ops;
	pcm_link->private_data = state;
	ret = snd_soc_pcm_new(pcm_link, 1, 0);
	if (ret < 0) {
		pr_err("%s: Failed to create surround pcm\n", __func__);
		kfree(state);
		return ret;
	}

	return 0;
}

static int imx_3stack_pcm_free(struct snd_soc_pcm_link
			       *pcm_link)
{
	kfree(pcm_link->private_data);
	return 0;
}

static const struct snd_soc_pcm_link_ops imx_3stack_pcm_ops = {
	.new = imx_3stack_pcm_new,
	.free = imx_3stack_pcm_free,
};

/* imx_3stack machine dapm widgets */
static const struct snd_soc_dapm_widget imx_3stack_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
};

/* example machine audio_mapnections */
static const char *audio_map[][3] = {

	/* Line out jack */
	{"Line Out Jack", NULL, "VOUT1L"},
	{"Line Out Jack", NULL, "VOUT1R"},
	{"Line Out Jack", NULL, "VOUT2L"},
	{"Line Out Jack", NULL, "VOUT2R"},
	{"Line Out Jack", NULL, "VOUT3L"},
	{"Line Out Jack", NULL, "VOUT3R"},
	{NULL, NULL, NULL},
};

#ifdef CONFIG_MXC_ASRC

static int asrc_func = 6;

static const char *asrc_function[] =
    { "disable", "8KHz", "11.025KHz", "16KHz", "22.05KHz", "32KHz", "44.1KHz",
	"48KHz", "64KHz", "88.2KHz", "96KHz", "176.4KHz", "192KHz"
};

static const struct soc_enum asrc_enum[] = {
	SOC_ENUM_SINGLE_EXT(13, asrc_function),
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
	asrc_esai_data.output_sample_rate = asrc_rates[asrc_func];

	return 1;
}

static const struct snd_kcontrol_new asrc_controls[] = {
	SOC_ENUM_EXT("ASRC", asrc_enum[0], asrc_get_rate,
		     asrc_set_rate),
};

#endif

static int imx_3stack_mach_probe(struct snd_soc_machine

				 *machine)
{
	struct snd_soc_codec *codec;
	struct snd_soc_pcm_link *pcm_link;
	int i, ret;
	pcm_link = list_first_entry(&machine->active_list,
				    struct snd_soc_pcm_link, active_list);
	codec = pcm_link->codec;
	codec->ops->io_probe(codec, machine);
	/* Add imx_3stack specific widgets */
	for (i = 0; i < ARRAY_SIZE(imx_3stack_dapm_widgets); i++) {
		snd_soc_dapm_new_control(machine, codec,
					 &imx_3stack_dapm_widgets[i]);
	}
	/* set up imx_3stack specific audio path audio_mapnects */
	for (i = 0; audio_map[i][0] != NULL; i++) {
		snd_soc_dapm_connect_input(machine,
					   audio_map[i][0],
					   audio_map[i][1], audio_map[i][2]);
	}

#ifdef CONFIG_MXC_ASRC
	for (i = 0; i < ARRAY_SIZE(asrc_controls); i++) {
		ret = snd_ctl_add(machine->card,
				  snd_soc_cnew(&asrc_controls[i], codec, NULL));
		if (ret < 0)
			return ret;
	}
	asrc_esai_data.output_sample_rate = asrc_rates[asrc_func];

#endif

	snd_soc_dapm_set_endpoint(machine, "Line Out Jack", 1);
	snd_soc_dapm_set_policy(machine, SND_SOC_DAPM_POLICY_STREAM);
	/* register card with ALSA upper layers */
	ret = snd_soc_register_card(machine);
	if (ret < 0) {
		pr_err("%s: failed to register sound card\n", __func__);
		return ret;
	}

	return 0;
}

static struct snd_soc_machine_ops imx_3stack_mach_ops = {
	.mach_probe = imx_3stack_mach_probe,
};

/*
 * This function will register the snd_soc_pcm_link drivers.
 */
static int __init imx_3stack_wm8580_probe(struct
					  platform_device
					  *pdev)
{
	struct snd_soc_machine *machine;
	struct snd_soc_pcm_link *surround;
	int ret;
	static int dev;
	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}

	machine = kzalloc(sizeof(struct snd_soc_machine), GFP_KERNEL);
	if (machine == NULL)
		return -ENOMEM;
	machine->owner = THIS_MODULE;
	machine->pdev = pdev;
	machine->name = "imx_3stack";
	machine->longname = "wm8580";
	machine->ops = &imx_3stack_mach_ops;
	pdev->dev.driver_data = machine;
	/* register card */
	imx_3stack_mach = machine;
	ret = snd_soc_new_card(machine, 1, index[dev], id[dev]);
	if (ret < 0) {
		pr_err("%s: failed to create stereo sound card\n", __func__);
		goto err;
	}

	/* imx_3stack wm8580 surround interface */
	surround =
	    snd_soc_pcm_link_new(machine,
				 "imx_3stack-surround",
				 &imx_3stack_pcm_ops, imx_pcm,
				 wm8580_codec, wm8580_dai, imx_esai_tx);
	if (surround == NULL) {
		pr_err("Failed to create HiFi PCM link\n");
		goto err;
	}
	ret = snd_soc_pcm_link_attach(surround);
	if (ret < 0)
		goto link_err;
	/* Configure audio port 3 */
	gpio_activate_esai_ports();
	return ret;
link_err:
	snd_soc_machine_free(machine);
err:
	kfree(machine);
	return ret;
}

static int __devexit imx_3stack_wm8580_remove(struct
					      platform_device
					      *pdev)
{
	struct snd_soc_machine *machine = pdev->dev.driver_data;
	imx_3stack_mach = NULL;
	kfree(machine);
	gpio_deactivate_esai_ports();
	return 0;
}

#ifdef CONFIG_PM
static int imx_3stack_wm8580_suspend(struct platform_device
				     *pdev, pm_message_t state)
{
	struct snd_soc_machine *machine = pdev->dev.driver_data;
	return snd_soc_suspend(machine, state);
}

static int imx_3stack_wm8580_resume(struct platform_device
				    *pdev)
{
	struct snd_soc_machine *machine = pdev->dev.driver_data;
	return snd_soc_resume(machine);
}

#else
#define imx_3stack_wm8580_suspend NULL
#define imx_3stack_wm8580_resume  NULL
#endif

static struct platform_driver imx_3stack_wm8580_driver = {
	.probe = imx_3stack_wm8580_probe,
	.remove = __devexit_p(imx_3stack_wm8580_remove),
	.suspend = imx_3stack_wm8580_suspend,
	.resume = imx_3stack_wm8580_resume,
	.driver = {
		   .name = "imx-3stack-wm8580",
		   .owner = THIS_MODULE,
		   },
};
static int __devinit imx_3stack_asoc_init(void)
{
	return platform_driver_register(&imx_3stack_wm8580_driver);
}

static void __exit imx_3stack_asoc_exit(void)
{
	platform_driver_unregister(&imx_3stack_wm8580_driver);
}

module_init(imx_3stack_asoc_init);
module_exit(imx_3stack_asoc_exit);
/* Module information */
MODULE_DESCRIPTION("ALSA SoC wm8580 imx_3stack");
MODULE_LICENSE("GPL");
