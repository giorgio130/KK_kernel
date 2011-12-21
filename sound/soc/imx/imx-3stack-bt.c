/*
 * imx-3stack-bt.c  --  SoC bluetooth audio for imx_3stack
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
#include <linux/clk.h>
#include <linux/pmic_external.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include <asm/hardware.h>

#include "imx-pcm.h"
#include "imx-ssi.h"
#include "imx-3stack-bt.h"

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE;
module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for bluetooth sound card.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for bluetooth sound card.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable bluetooth sound card.");

#define BT_SSI_MASTER	1

struct bt_pcm_state {
	int active;
};

static struct snd_soc_machine *imx_3stack_mach;

static void imx_3stack_init_dam(int ssi_port, int dai_port)
{
	/* bt uses SSI1 or SSI2 via AUDMUX port dai_port for audio */

	/* reset port ssi_port & dai_port */
	DAM_PTCR(ssi_port) = 0;
	DAM_PDCR(ssi_port) = 0;
	DAM_PTCR(dai_port) = 0;
	DAM_PDCR(dai_port) = 0;

	/* set to synchronous */
	DAM_PTCR(ssi_port) |= AUDMUX_PTCR_SYN;
	DAM_PTCR(dai_port) |= AUDMUX_PTCR_SYN;

#if BT_SSI_MASTER
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

static int imx_3stack_bt_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct bt_pcm_state *state = pcm_link->private_data;

	if (!state->active)
		gpio_activate_bt_audio_port();
	state->active++;
	return 0;
}

static int imx_3stack_bt_hw_params(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct mxc_audio_platform_data *dev_data =
	    pcm_link->machine->private_data;
	struct snd_soc_dai *cpu_dai = pcm_link->cpu_dai;
	unsigned int channels = params_channels(params);
	u32 dai_format;

	imx_3stack_init_dam(dev_data->src_port, dev_data->ext_port);
#if BT_SSI_MASTER
	dai_format = SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_IB_IF |
	    SND_SOC_DAIFMT_CBM_CFM | SND_SOC_DAIFMT_SYNC;
#else
	dai_format = SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_IB_IF |
	    SND_SOC_DAIFMT_CBS_CFS | SND_SOC_DAIFMT_SYNC;
#endif
	if (channels == 2)
		dai_format |= SND_SOC_DAIFMT_TDM;

	/* set cpu DAI configuration */
	cpu_dai->ops->set_fmt(cpu_dai, dai_format);

	/* set i.MX active slot mask */
	cpu_dai->ops->set_tdm_slot(cpu_dai,
				   channels == 1 ? 0xfffffffe : 0xfffffffc,
				   channels);

	/* set the SSI system clock as input (unused) */
	cpu_dai->ops->set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0, SND_SOC_CLOCK_IN);

	return 0;
}

static void imx_3stack_bt_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_link *pcm_link = substream->private_data;
	struct bt_pcm_state *state = pcm_link->private_data;

	state->active--;
	if (!state->active)
		gpio_inactivate_bt_audio_port();
}

/*
 * imx_3stack bt DAI opserations.
 */
static struct snd_soc_ops imx_3stack_bt_ops = {
	.startup = imx_3stack_bt_startup,
	.hw_params = imx_3stack_bt_hw_params,
	.shutdown = imx_3stack_bt_shutdown,
};

static int bt_pcm_new(struct snd_soc_pcm_link *pcm_link)
{
	struct bt_pcm_state *state;
	int ret;

	state = kzalloc(sizeof(struct bt_pcm_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	pcm_link->audio_ops = &imx_3stack_bt_ops;
	state->active = 0;
	pcm_link->private_data = state;

	ret = snd_soc_pcm_new(pcm_link, 1, 1);
	if (ret < 0) {
		pr_err("%s: Failed to create bt pcm\n", __func__);
		return ret;
	}

	return 0;
}

static int bt_pcm_free(struct snd_soc_pcm_link *pcm_link)
{
	kfree(pcm_link->private_data);
	return 0;
}
struct snd_soc_pcm_link_ops bt_pcm = {
	.new = bt_pcm_new,
	.free = bt_pcm_free,
};

static int imx_3stack_mach_probe(struct snd_soc_machine
				 *machine)
{
	int ret;

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
 * It also registers devices for platform DMA, I2S, SSP and registers an
 * I2C driver to probe the codec.
 */
static int __init imx_3stack_bt_probe(struct platform_device *pdev)
{
	struct snd_soc_machine *machine;
	struct mxc_audio_platform_data *dev_data = pdev->dev.platform_data;
	struct snd_soc_pcm_link *bt_audio;
	const char *ssi_port;
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
	machine->longname = "bluetooth";
	machine->ops = &imx_3stack_mach_ops;
	machine->private_data = dev_data;
	pdev->dev.driver_data = machine;

	/* register card */
	imx_3stack_mach = machine;
	ret = snd_soc_new_card(machine, 1, index[dev], id[dev]);
	if (ret < 0) {
		pr_err("%s: failed to create bt sound card\n", __func__);
		goto err;
	}

	/* imx_3stack bluetooth audio interface */
	if (dev_data->src_port == 1)
		ssi_port = imx_ssi_1;
	else
		ssi_port = imx_ssi_3;
	bt_audio =
	    snd_soc_pcm_link_new(machine,
				 "imx_3stack-bt", &bt_pcm,
				 imx_pcm, bt_codec, bt_dai, ssi_port);
	if (bt_audio == NULL) {
		pr_err("Failed to create bt PCM link\n");
		goto err;
	}
	ret = snd_soc_pcm_link_attach(bt_audio);

	if (ret < 0)
		goto link_err;

	return ret;

link_err:
	snd_soc_machine_free(machine);
err:
	kfree(machine);
	return ret;
}

static int __devexit imx_3stack_bt_remove(struct platform_device *pdev)
{
	struct snd_soc_machine *machine = pdev->dev.driver_data;

	imx_3stack_mach = NULL;
	kfree(machine);
	return 0;
}

#ifdef CONFIG_PM
static int imx_3stack_bt_suspend(struct platform_device
				 *pdev, pm_message_t state)
{
	struct snd_soc_machine *machine = pdev->dev.driver_data;
	return snd_soc_suspend(machine, state);
}

static int imx_3stack_bt_resume(struct platform_device
				*pdev)
{
	struct snd_soc_machine *machine = pdev->dev.driver_data;
	return snd_soc_resume(machine);
}

#else
#define imx_3stack_bt_suspend NULL
#define imx_3stack_bt_resume  NULL
#endif

static struct platform_driver imx_3stack_bt_driver = {
	.probe = imx_3stack_bt_probe,
	.remove = __devexit_p(imx_3stack_bt_remove),
	.suspend = imx_3stack_bt_suspend,
	.resume = imx_3stack_bt_resume,
	.driver = {
		   .name = "imx-3stack-bt",
		   .owner = THIS_MODULE,
		   },
};

static int __init imx_3stack_asoc_init(void)
{
	return platform_driver_register(&imx_3stack_bt_driver);
}

static void __exit imx_3stack_asoc_exit(void)
{
	platform_driver_unregister(&imx_3stack_bt_driver);
}

module_init(imx_3stack_asoc_init);
module_exit(imx_3stack_asoc_exit);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC bluetooth imx_3stack");
MODULE_LICENSE("GPL");
