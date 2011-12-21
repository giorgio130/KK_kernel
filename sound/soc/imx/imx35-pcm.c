/*
 * imx35-pcm.c -- ALSA SoC interface for the Freescale i.MX35 CPU 
 *
 * Copyright 2009 Lab126 Inc.
 * Author: Manish Lachwani (lachwani@lab126.com)
 *
 * Copyright 2006 Wolfson Microelectronics PLC.
 * Author: Liam Girdwood
 *         liam.girdwood@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * Based on pxa2xx-pcm.c by	Nicolas Pitre, (C) 2004 MontaVista Software, Inc.
 * and on mxc-alsa-mc13783 (C) 2006 Freescale.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Revision history
 *    29th Aug 2006   Initial version.
 * 
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <asm/arch/dma.h>
#include <asm/arch/spba.h>
#include <asm/arch/clock.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <linux/mxc_asrc.h>
#include <asm/arch/hardware.h>

#include "imx35-pcm.h"
#include "imx-ssi.h"

/* debug */
#define IMX_PCM_DEBUG 0
#if IMX_PCM_DEBUG
#define dbg(format, arg...) printk(format, ## arg)
#else
#define dbg(format, arg...)
#endif

/*
 * Coherent DMA memory is used by default, although Freescale have used 
 * bounce buffers in all their drivers for i.MX35 to date. If you have any 
 * issues, please select bounce buffers. 
 */
static int imx35_dma_bounce = 0; /* Use IRAM or not */

#ifdef CONFIG_SND_SOC_MX35PCM_DMA
#define MAX_BUFFER_SIZE		(512*1024)
#define DMA_BUF_SIZE		(128*1024)
#else
#define MAX_BUFFER_SIZE		(128*1024)
#define DMA_BUF_SIZE		(32*1024)
#endif

#define MIN_PERIOD_SIZE		64
#define MIN_PERIOD		2
#define MAX_PERIOD		255

static const struct snd_pcm_hardware imx35_pcm_hardware = {
	.info			= (SNDRV_PCM_INFO_INTERLEAVED |
				   SNDRV_PCM_INFO_BLOCK_TRANSFER |
				   SNDRV_PCM_INFO_MMAP |
				   SNDRV_PCM_INFO_MMAP_VALID |
				   SNDRV_PCM_INFO_PAUSE |
				   SNDRV_PCM_INFO_RESUME),
	.formats		= SNDRV_PCM_FMTBIT_S24_LE |
					SNDRV_PCM_FMTBIT_S16_LE,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_min = MIN_PERIOD_SIZE,
	.period_bytes_max = DMA_BUF_SIZE,
	.periods_min = MIN_PERIOD,
	.periods_max = MAX_PERIOD,
	.fifo_size = 0,
};

#ifdef CONFIG_MXC_ASRC
#define MXC_ASRC_SSI1_BYTES 0x40
#define MXC_ASRC_A_SRC_ADDR 0x5002c064
#define MXC_ASRC_A_DEST_ADDR 0x43FA0000
#endif

struct mxc_runtime_data {
	int dma_ch;
	struct imx35_pcm_dma_param *dma_params;
	raw_spinlock_t dma_lock;
	int active, period, periods;
	int dma_wchannel;
	int dma_active;
	int old_offset;
	int dma_alloc;
#ifdef CONFIG_MXC_ASRC
	int dma_asrc;
#endif
};

static void audio_stop_dma(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned long flags;
	unsigned int dma_size;
	unsigned int offset;
	
	dma_size = frames_to_bytes(runtime, runtime->period_size);
	offset = dma_size * prtd->periods;

	/* stops the dma channel and clears the buffer ptrs */
	spin_lock_irqsave(&prtd->dma_lock, flags);
	prtd->active = 0;
	prtd->period = 0;
	prtd->periods = 0;
	mxc_dma_stop(prtd->dma_wchannel);

	if (imx35_dma_bounce) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
						DMA_TO_DEVICE);
		else
			dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
						DMA_FROM_DEVICE);
	}

	spin_unlock_irqrestore(&prtd->dma_lock, flags);
}

static int dma_new_period(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime =  substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int dma_size = frames_to_bytes(runtime, runtime->period_size);
	unsigned int offset = dma_size * prtd->period;
	int ret = 0;
	dma_request_t sdma_request;
	
	if (!prtd->active) 
		return 0;
		
	memset(&sdma_request, 0, sizeof(dma_request_t));
	
	dbg("period pos  ALSA %x DMA %x\n",runtime->periods, prtd->period);
	dbg("period size ALSA %x DMA %x Offset %x dmasize %x\n",
		(unsigned int) runtime->period_size, runtime->dma_bytes, 
			offset, dma_size);
	dbg("DMA addr %x\n", runtime->dma_addr + offset);

	if (imx35_dma_bounce) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			sdma_request.sourceAddr = (char*)(dma_map_single(NULL,
				runtime->dma_area + offset, dma_size, DMA_TO_DEVICE));
		else
			sdma_request.destAddr = (char*)(dma_map_single(NULL,
				runtime->dma_area + offset, dma_size, DMA_FROM_DEVICE));	
	} 
	else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			sdma_request.sourceAddr = (char*)(runtime->dma_addr + offset);
		else
			sdma_request.destAddr = (char*)(runtime->dma_addr + offset);
	}

	sdma_request.count = dma_size;

	ret = mxc_dma_set_config(prtd->dma_wchannel, &sdma_request, 0);
	if (ret < 0) {
		printk(KERN_ERR "imx35-pcm: cannot configure audio DMA channel\n");
		goto out;
	}
	
	ret = mxc_dma_start(prtd->dma_wchannel);
	if (ret < 0) {
		printk(KERN_ERR "imx35-pcm: cannot queue audio DMA buffer\n");
		goto out;
	}

	prtd->period++;
	prtd->period %= runtime->periods;

#ifdef CONFIG_SND_SOC_MX35PCM_DMA
	for (i = 1; i < runtime->periods; i++) {
		offset = dma_size * prtd->period;
		memset(&sdma_request, 0, sizeof(dma_request_t));
		if (imx35_dma_bounce) {
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				sdma_request.sourceAddr = (char*)(dma_map_single(NULL,
					runtime->dma_area + offset, dma_size, DMA_TO_DEVICE));
			else
				sdma_request.destAddr = (char*)(dma_map_single(NULL,
					runtime->dma_area + offset, dma_size, DMA_FROM_DEVICE));
		}
		else {
			if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
				sdma_request.sourceAddr = (char*)(runtime->dma_addr + offset);
			else
				sdma_request.destAddr = (char*)(runtime->dma_addr + offset);
		}

		sdma_request.count = dma_size;

		ret = mxc_dma_set_config(prtd->dma_wchannel, &sdma_request, 0);
		if (ret < 0) {
			printk(KERN_ERR "imx35-pcm: cannot configure audio DMA channel\n");
			goto out;
		}
		ret = mxc_dma_start(prtd->dma_wchannel);
		if (ret < 0) {
			printk(KERN_ERR "imx35-pcm: cannot queue audio DMA buffer\n");
			goto out;
		}
		prtd->period++;
		prtd->period %= runtime->periods;
	}
#endif
out:
	return ret;
}

static void audio_dma_irq(void *data)
{
	struct snd_pcm_substream *substream = 
		(struct snd_pcm_substream *)data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int dma_size = frames_to_bytes(runtime, runtime->period_size);
	unsigned int offset = dma_size * prtd->periods;

	prtd->dma_active = 0;
	prtd->periods++;
	prtd->periods %= runtime->periods;

	dbg("irq per  %x %x\n", prtd->periods,prtd->period);

	if (imx35_dma_bounce) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_TO_DEVICE);
		else
			dma_unmap_single(NULL, runtime->dma_addr + offset, dma_size,
							DMA_FROM_DEVICE);
	}

 	if (prtd->active)
		snd_pcm_period_elapsed(substream);
	dma_new_period(substream);
}

static int imx35_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime =  substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;

	prtd->period = 0;
	prtd->periods = 0;
	return 0;
}

/*
 * Controls whether kernel can enter DOZE/IDLE when audio plays
 */
int mx35_luigi_audio_playing_flag = 0;
EXPORT_SYMBOL(mx35_luigi_audio_playing_flag);

static int imx35_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mxc_pcm_dma_params *dma = rtd->dai->cpu_dai->dma_data;
	int ret = 0, channel = 0;
#ifdef CONFIG_MXC_ASRC
	dma_channel_info_t info;
	mxc_dma_requestbuf_t sdma_request;
#endif
	mx35_luigi_audio_playing_flag = 1;

#ifdef CONFIG_MXC_ASRC
	if (!prtd->dma_asrc) {
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			info.asrc.channs = runtime->channels;
			memset(&sdma_request, 0, sizeof(mxc_dma_requestbuf_t));
			sdma_request.num_of_bytes = MXC_ASRC_SSI1_BYTES;
			sdma_request.src_addr = MXC_ASRC_A_SRC_ADDR;
			sdma_request.dst_addr = MXC_ASRC_A_DEST_ADDR;
			prtd->dma_ch = MXC_DMA_ASRCA_SSI1_TX0;
			channel =
				mxc_dma_request_ext(prtd->dma_ch, "ALSA TX SDMA", &info);
			mxc_dma_config(channel, &sdma_request, 1, MXC_DMA_MODE_WRITE);
			mxc_dma_enable(channel);
			prtd->dma_asrc = channel;
			channel = mxc_dma_request(MXC_DMA_ASRC_A_RX, "ALSA ASRC RX");
			prtd->dma_wchannel = channel;
		}
	}
#else
	/* only allocate the DMA chn once */
	if (!prtd->dma_alloc) {
		if(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			ret  = mxc_request_dma(&channel, "ALSA TX SDMA");
			if (ret < 0) {
				printk(KERN_ERR "imx35-pcm: error requesting a write dma channel\n");
				return ret;
			}
	
		} else {
			ret = mxc_request_dma(&channel, "ALSA RX SDMA");
			if (ret < 0) {
				printk(KERN_ERR "imx35-pcm: error requesting a read dma channel\n");
				return ret;
			}
		}
		prtd->dma_wchannel = channel;
		prtd->dma_alloc = 1;
	}
#endif
	/* set up chn with params */
	dma->params.callback = audio_dma_irq;
	dma->params.arg = substream;

#ifndef CONFIG_MXC_ASRC	
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dma->params.word_size = TRANSFER_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_S24_LE:
		dma->params.word_size = TRANSFER_24BIT;
		break;
	}
#endif
	ret = mxc_dma_setup_channel(channel, &dma->params);
	if (ret < 0) {
		printk(KERN_ERR "imx35-pcm: failed to setup audio DMA chn %d\n", channel);
		mxc_free_dma(channel);
		return ret;
	}
	
	if (imx35_dma_bounce) {
		ret = snd_pcm_lib_malloc_pages(substream, 
			params_buffer_bytes(params));
		if (ret < 0) {
			printk(KERN_ERR "imx35-pcm: failed to malloc pcm pages\n");
			if (channel)
				mxc_free_dma(channel);
			return ret;
		}
		runtime->dma_addr = virt_to_phys(runtime->dma_area);
	}
	else 
		snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return ret;
}

static int imx35_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	
	if (prtd->dma_wchannel) {
		mxc_free_dma(prtd->dma_wchannel);
		prtd->dma_wchannel = 0;
		prtd->dma_alloc = 0;
#ifdef CONFIG_MXC_ASRC
		mxc_dma_free(prtd->dma_asrc);
		prtd->dma_asrc = 0;
		mxc_dma_disable(prtd->dma_asrc);
		mxc_dma_disable(prtd->dma_wchannel);
#endif
	}

	if (imx35_dma_bounce)
		snd_pcm_lib_free_pages(substream);

	mx35_luigi_audio_playing_flag = 0;

	return 0;
}

static int imx35_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct mxc_runtime_data *prtd = substream->runtime->private_data;
	int ret = 0;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->dma_active = 0;
		/* requested stream startup */
		prtd->active = 1;
		ret = dma_new_period(substream);
#ifdef CONFIG_MXC_ASRC
		asrc_start_conv(0);
#endif
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		/* requested stream shutdown */
		audio_stop_dma(substream);
#ifdef CONFIG_MXC_ASRC
		asrc_stop_conv(0);
#endif
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		prtd->active = 0;
		prtd->periods = 0;
		break;
	case SNDRV_PCM_TRIGGER_RESUME:
		prtd->active = 1;
		prtd->dma_active = 0;
		ret = dma_new_period(substream);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		prtd->active = 0;
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		prtd->active = 1;
		if (prtd->old_offset) {
			prtd->dma_active = 0;
			ret = dma_new_period(substream);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t imx35_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	unsigned int offset = 0;

       /* is a transfer active ? */
       if (prtd->dma_active) {
               offset = (runtime->period_size * (prtd->periods)) +
                   (runtime->period_size >> 1);
               if (offset >= runtime->buffer_size)
                       offset = runtime->period_size >> 1;
       } else {
               offset = (runtime->period_size * (prtd->periods));
               if (offset >= runtime->buffer_size)
                       offset = 0;
       }
	dbg("pointer offset %x\n", offset);
	
	return offset;
}


static int imx35_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd;
	int ret;

	snd_soc_set_runtime_hwparams(substream, &imx35_pcm_hardware);

	ret = snd_pcm_hw_constraint_integer(runtime, 
		SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	prtd = kzalloc(sizeof(struct mxc_runtime_data), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	runtime->private_data = prtd;
	return 0;
}

static int imx35_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mxc_runtime_data *prtd = runtime->private_data;
	
	kfree(prtd);
	return 0;
}

static int
imx35_pcm_mmap(struct snd_pcm_substream *substream, struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	return dma_mmap_writecombine(substream->pcm->card->dev, vma,
				     runtime->dma_area,
				     runtime->dma_addr,
				     runtime->dma_bytes);
}

struct snd_pcm_ops imx35_pcm_ops = {
	.open		= imx35_pcm_open,
	.close		= imx35_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= imx35_pcm_hw_params,
	.hw_free	= imx35_pcm_hw_free,
	.prepare	= imx35_pcm_prepare,
	.trigger	= imx35_pcm_trigger,
	.pointer	= imx35_pcm_pointer,
	.mmap		= imx35_pcm_mmap,
};

static int imx35_pcm_preallocate_dma_buffer(struct snd_pcm *pcm, int stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = imx35_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_DEV;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	buf->area = dma_alloc_writecombine(pcm->card->dev, size,
					   &buf->addr, GFP_KERNEL);
	if (!buf->area)
		return -ENOMEM;
		
	buf->bytes = size;
	return 0;
}

static void imx35_pcm_free_dma_buffers(struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int stream;

	if (imx35_dma_bounce)
		return;

	for (stream = 0; stream < 2; stream++) {
		substream = pcm->streams[stream].substream;
		if (!substream)
			continue;

		buf = &substream->dma_buffer;
		if (!buf->area)
			continue;

		dma_free_writecombine(pcm->card->dev, buf->bytes,
				      buf->area, buf->addr);
		buf->area = NULL;
	}
}

static u64 imx35_pcm_dmamask = 0xffffffff;

int imx35_pcm_new(struct snd_card *card, struct snd_soc_codec_dai *dai,
	struct snd_pcm *pcm)
{
	int ret = 0;

	if (!card->dev->dma_mask)
		card->dev->dma_mask = &imx35_pcm_dmamask;
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = 0xffffffff;

	if (imx35_dma_bounce) {
		ret = snd_pcm_lib_preallocate_pages_for_all(pcm, 
				SNDRV_DMA_TYPE_CONTINUOUS,
				snd_dma_continuous_data(GFP_KERNEL),
				imx35_pcm_hardware.buffer_bytes_max * 2, 
				imx35_pcm_hardware.buffer_bytes_max * 2);
		if (ret < 0) {
			printk(KERN_ERR "imx35-pcm: failed to preallocate pages\n");
			goto out;
		}
	}
	else {	
		if (dai->playback.channels_min) {
			ret = imx35_pcm_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_PLAYBACK);
			if (ret)
				goto out;
		}

		if (dai->capture.channels_min) {
			ret = imx35_pcm_preallocate_dma_buffer(pcm,
				SNDRV_PCM_STREAM_CAPTURE);
			if (ret)
				goto out;
		}
	}

 out:
	return ret;
}

struct snd_soc_platform imx35_soc_platform = {
	.name		= "imx35-audio",
	.pcm_ops 	= &imx35_pcm_ops,
	.pcm_new	= imx35_pcm_new,
	.pcm_free	= imx35_pcm_free_dma_buffers,
};
EXPORT_SYMBOL_GPL(imx35_soc_platform);

MODULE_AUTHOR("Manish Lachwani");
MODULE_DESCRIPTION("Freescale i.MX35 PCM DMA module");
MODULE_LICENSE("GPL");
