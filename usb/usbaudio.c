/*
 *   (Tentative) USB Audio Driver for ALSA
 *
 *   Main and PCM part
 *
 *   Copyright (c) 2002 by Takashi Iwai <tiwai@suse.de>
 *
 *   Many codes borrowed from audio.c by 
 *	    Alan Cox (alan@lxorguk.ukuu.org.uk)
 *	    Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */


#include <sound/driver.h>
#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/usb.h>
#include <sound/core.h>
#include <sound/pcm.h>
#define SNDRV_GET_ID
#include <sound/initval.h>

#include "usbaudio.h"


EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Takashi Iwai <tiwai@suse.de>");
MODULE_DESCRIPTION("USB Audio");
MODULE_LICENSE("GPL");
MODULE_CLASSES("{sound}");
MODULE_DEVICES("{{Generic,USB Audio}}");


static int snd_index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *snd_id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
static int snd_enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;	/* Enable this card */

MODULE_PARM(snd_index, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(snd_index, "Index value for the USB audio adapter.");
MODULE_PARM_SYNTAX(snd_index, SNDRV_INDEX_DESC);
MODULE_PARM(snd_id, "1-" __MODULE_STRING(SNDRV_CARDS) "s");
MODULE_PARM_DESC(snd_id, "ID string for the USB audio adapter.");
MODULE_PARM_SYNTAX(snd_id, SNDRV_ID_DESC);
MODULE_PARM(snd_enable, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(snd_enable, "Enable USB audio adapter.");
MODULE_PARM_SYNTAX(snd_enable, SNDRV_ENABLE_DESC);


static unsigned long snd_index_bitmap;


#define NRPACKS		4	/* 4ms per urb */
#define MAX_URBS	5	/* max. 20ms long packets */
#define SYNC_URBS	2	/* always two urbs for sync */

typedef struct snd_usb_substream snd_usb_substream_t;
typedef struct snd_usb_stream snd_usb_stream_t;
typedef struct snd_urb_ctx snd_urb_ctx_t;

#define snd_usb_stream_t_magic	0xa15a403	/* should be in sndmagic.h later */

struct audioformat {
	struct list_head list;
	snd_pcm_format_t format;
	int channels;
	unsigned char altsetting;
	unsigned char attributes;
	unsigned int rates;
	int rate_min, rate_max;
	int nr_rates;
	int *rate_table;
};

struct snd_urb_ctx {
	struct urb *urb;
	snd_usb_substream_t *subs;
	int index;	/* index for urb array */
	int packets;	/* number of packets per urb */
	int transfer;	/* transferred size */
	char *buf;	/* buffer for capture */
};

struct snd_urb_ops {
	int (*prepare)(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime, struct urb *u);
	int (*retire)(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime, struct urb *u);
	int (*prepare_sync)(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime, struct urb *u);
	int (*retire_sync)(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime, struct urb *u);
};

struct snd_usb_substream {
	snd_usb_stream_t *stream;
	struct usb_device *dev;
	snd_pcm_substream_t *pcm_substream;
	int interface;           /* Interface number, -1 means not used */
	unsigned int format;     /* USB data format */
	unsigned int datapipe;   /* the data i/o pipe */
	unsigned int syncpipe;   /* 1 - async out or adaptive in */
	unsigned int syncinterval;  /* P for adaptive mode, 0 otherwise */
	unsigned int freqn;      /* nominal sampling rate in USB format, i.e. fs/1000 in Q10.14 */
	unsigned int freqm;      /* momentary sampling rate in USB format, i.e. fs/1000 in Q10.14 */
	unsigned int freqmax;    /* maximum sampling rate, used for buffer management */
	unsigned int phase;      /* phase accumulator */
	unsigned int maxpacksize;	/* max packet size in bytes */
	unsigned int maxframesize;	/* max packet size in frames */
	unsigned int curpacksize;
	unsigned int fill_max: 1;

	int hwptr, hwptr_done;
	int transfer_done;
	unsigned long transmask;

	int npacks;
	int nurbs;
	snd_urb_ctx_t dataurb[MAX_URBS];
	snd_urb_ctx_t syncurb[SYNC_URBS];
	char syncbuf[SYNC_URBS * NRPACKS * 3]; /* so small - let's get static */
	char *tmpbuf;

	unsigned int formats;
	int num_formats;
	struct list_head fmt_list;
	spinlock_t lock;

	struct snd_urb_ops ops;
};


struct snd_usb_stream {
	snd_usb_audio_t *chip;
	snd_pcm_t *pcm;
	snd_usb_substream_t substream[2];
};


/*
 */

#define chip_t snd_usb_stream_t


/*
 */

/*
 * convert a sampling rate into USB format (fs/1000 in Q10.14)
 * this will overflow at approx 2MSPS
 */
inline static unsigned get_usb_rate(unsigned int rate)
{
	return ((rate << 11) + 62) / 125;
}


/*
 */
static int prepare_capture_sync_urb(snd_usb_substream_t *subs,
				    snd_pcm_runtime_t *runtime,
				    struct urb *urb)
{
	unsigned char *cp = urb->transfer_buffer;
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;
	int i, offs;

	urb->number_of_packets = ctx->packets;
	urb->dev = ctx->subs->dev; /* this seems necessary.. */
	for (i = offs = 0; i < urb->number_of_packets; i++, offs += 3, cp += 3) {
		urb->iso_frame_desc[i].length = 3;
		urb->iso_frame_desc[i].offset = offs;
		cp[0] = subs->freqn;
		cp[1] = subs->freqn >> 8;
		cp[2] = subs->freqn >> 16;
	}
	urb->interval = 1;
	return 0;
}

static int retire_capture_sync_urb(snd_usb_substream_t *subs,
				   snd_pcm_runtime_t *runtime,
				   struct urb *urb)
{
	return 0;
}

static int prepare_capture_urb(snd_usb_substream_t *subs,
			       snd_pcm_runtime_t *runtime,
			       struct urb *urb)
{
	int i, offs;
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;

	ctx->transfer = subs->curpacksize * urb->number_of_packets;
	urb->number_of_packets = ctx->packets;
	urb->transfer_buffer = ctx->buf;
	urb->transfer_buffer_length = subs->curpacksize * urb->number_of_packets;
	urb->dev = ctx->subs->dev; /* this seems necessary.. */
	for (i = offs = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].offset = offs;
		urb->iso_frame_desc[i].length = subs->curpacksize;
		offs += subs->curpacksize;
	}
	urb->interval = 1;
	return 0;
}

static int retire_capture_urb(snd_usb_substream_t *subs,
			      snd_pcm_runtime_t *runtime,
			      struct urb *urb)
{
	unsigned long flags;
	unsigned char *cp;
	int stride, i, len;

	stride = runtime->frame_bits >> 3;

	for (i = 0; i < urb->number_of_packets; i++) {
		cp = (unsigned char *)urb->transfer_buffer + urb->iso_frame_desc[i].offset;
		if (urb->iso_frame_desc[i].status)
			continue;
		len = urb->iso_frame_desc[i].actual_length / stride;
		if (! len)
			continue;
		spin_lock_irqsave(&subs->lock, flags);
		if (subs->hwptr_done + len >= runtime->buffer_size) {
			int cnt = runtime->buffer_size - subs->hwptr_done;
			int blen = cnt * stride;
			memcpy(runtime->dma_area + subs->hwptr_done * stride, cp, blen);
			memcpy(runtime->dma_area, cp + blen, len * stride - blen);
			subs->hwptr_done = len - cnt;
		} else {
			memcpy(runtime->dma_area + subs->hwptr_done * stride, cp, len * stride);
			subs->hwptr += len;
		}
		subs->transfer_done += len;
		if (subs->transfer_done >= runtime->period_size) {
			subs->transfer_done -= runtime->period_size;
			spin_unlock_irqrestore(&subs->lock, flags);
			snd_pcm_period_elapsed(subs->pcm_substream);
		} else
			spin_unlock_irqrestore(&subs->lock, flags);
	}
	return 0;
}


/*
 * 
 */

static int prepare_playback_sync_urb(snd_usb_substream_t *subs,
				     snd_pcm_runtime_t *runtime,
				     struct urb *urb)
{
	int i, offs;
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;

	urb->number_of_packets = ctx->packets;
	urb->dev = ctx->subs->dev; /* this seems necessary.. */
	for (i = offs = 0; i < urb->number_of_packets; i++, offs += 3) {
		urb->iso_frame_desc[i].length = 3;
		urb->iso_frame_desc[i].offset = offs;
	}
	return 0;
}

static int retire_playback_sync_urb(snd_usb_substream_t *subs,
				    snd_pcm_runtime_t *runtime,
				    struct urb *urb)
{
	unsigned int f, i, found;
	unsigned char *cp = urb->transfer_buffer;
	unsigned long flags;

	found = 0;
	for (i = 0; i < urb->number_of_packets; i++, cp += 3) {
		if (urb->iso_frame_desc[i].status ||
		    urb->iso_frame_desc[i].actual_length < 3)
			continue;
		f = combine_triple(cp);
		if (f < subs->freqn - (subs->freqn>>3) || f > subs->freqmax) {
			snd_printd(KERN_WARNING "requested frequency %u (nominal %u) out of range!\n", f, subs->freqn);
			continue;
		}
		found = f;
	}
	if (found) {
		spin_lock_irqsave(&subs->lock, flags);
		subs->freqm = found;
		spin_unlock_irqrestore(&subs->lock, flags);
	}

	return 0;
}

static int prepare_playback_urb(snd_usb_substream_t *subs,
				snd_pcm_runtime_t *runtime,
				struct urb *urb)
{
	int i, stride, whole_size, offs;
	int counts[NRPACKS];
	unsigned long flags;
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;

	stride = runtime->frame_bits >> 3;

	whole_size = 0;
	urb->number_of_packets = ctx->packets;
	urb->dev = ctx->subs->dev; /* this seems necessary.. */
	spin_lock_irqsave(&subs->lock, flags);
	for (i = 0; i < urb->number_of_packets; i++) {
		subs->phase = (subs->phase & 0x3fff) + subs->freqm;
		counts[i] = subs->phase >> 14;
		//if (counts[i] > subs->maxframesize)
		//	counts[i] = subs->maxframesize;
		whole_size += counts[i];
	}
#if 0 // FIXME: should be checked here?
	{
		int rest_size;
		if (subs->hwptr < runtime->control->appl_ptr)
			rest_size = runtime->control->appl_ptr - subs->hwptr;
		else
			rest_size = runtime->buffer_size + runtime->control->appl_ptr - subs->hwptr;
		if (rest_size < whole_size) {
			spin_unlock_irqrestore(&subs->lock, flags);
			snd_printd(KERN_ERR "underrun: rest size = %d / require %d\n", rest_size, whole_size);
			return -EPIPE; /* underrun */
		}
	}
#endif

	if (subs->hwptr + whole_size > runtime->buffer_size) {
		int len;
		len = runtime->buffer_size - subs->hwptr;
		urb->transfer_buffer = subs->tmpbuf;
		memcpy(subs->tmpbuf, runtime->dma_area + subs->hwptr * stride, len * stride);
		memcpy(subs->tmpbuf + len * stride, runtime->dma_area, (whole_size - len) * stride);
		subs->hwptr += whole_size;
		subs->hwptr -= runtime->buffer_size;
	} else {
		urb->transfer_buffer = runtime->dma_area + subs->hwptr * stride;
		subs->hwptr += whole_size;
	}
	spin_unlock_irqrestore(&subs->lock, flags);
	//printk("whole size = %d, freqm = %d/%d, stride = %d\n",
	//       whole_size, subs->freqm >> 14, subs->freqm & 0x3fff, stride);
	urb->transfer_buffer_length = whole_size * stride;
	ctx->transfer = whole_size;

	for (i = offs = 0; i < urb->number_of_packets; i++) {
		int cnt = counts[i] * stride;
		urb->iso_frame_desc[i].offset = offs;
		if (! counts[i]) {
			urb->iso_frame_desc[i].length = 0;
			continue;
		}
		urb->iso_frame_desc[i].length = cnt;
		offs += cnt;
	}
	return 0;
}

static int retire_playback_urb(snd_usb_substream_t *subs,
			       snd_pcm_runtime_t *runtime,
			       struct urb *urb)
{
	unsigned long flags;
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;

	spin_lock_irqsave(&subs->lock, flags);
	subs->transfer_done += ctx->transfer;
	subs->hwptr_done += ctx->transfer;
	ctx->transfer = 0;
	if (subs->hwptr_done >= runtime->buffer_size)
		subs->hwptr_done -= runtime->buffer_size;
	if (subs->transfer_done >= runtime->period_size) {
		subs->transfer_done -= runtime->period_size;
		spin_unlock_irqrestore(&subs->lock, flags);
		snd_pcm_period_elapsed(subs->pcm_substream);
	} else
		spin_unlock_irqrestore(&subs->lock, flags);
	return 0;
}


/*
 */
static void snd_complete_urb(struct urb *urb)
{
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;
	snd_usb_substream_t *subs = ctx->subs;
	snd_pcm_substream_t *substream = ctx->subs->pcm_substream;

	if (subs->ops.retire(subs, substream->runtime, urb)) {
		clear_bit(ctx->index, &subs->transmask);
		return;
	}

	clear_bit(ctx->index, &subs->transmask);

	if (snd_pcm_running(substream)) {
		if (subs->ops.prepare(subs, substream->runtime, urb) < 0||
		    do_usb_submit_urb(urb, GFP_KERNEL) < 0) {
			snd_printd(KERN_ERR "cannot submit urb\n");
			snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
			return;
		} else
			set_bit(ctx->index, &subs->transmask);
	}
}


/*
 */
static void snd_complete_sync_urb(struct urb *urb)
{
	snd_urb_ctx_t *ctx = (snd_urb_ctx_t *)urb->context;
	snd_usb_substream_t *subs = ctx->subs;
	snd_pcm_substream_t *substream = ctx->subs->pcm_substream;

	if (subs->ops.retire_sync(subs, substream->runtime, urb)) {
		clear_bit(ctx->index + 16, &subs->transmask);
		return;
	}

	clear_bit(ctx->index + 16, &subs->transmask);

	if (snd_pcm_running(substream)) {
		if (subs->ops.prepare_sync(subs, substream->runtime, urb) < 0||
		    do_usb_submit_urb(urb, GFP_KERNEL) < 0) {
			snd_printd(KERN_ERR "cannot submit urb\n");
			snd_pcm_stop(substream, SNDRV_PCM_STATE_XRUN);
			return;
		} else
			set_bit(ctx->index + 16, &subs->transmask);
	}
}


/*
 */
static int snd_start_urbs(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime)
{
	int i, err;

	for (i = 0; i < subs->nurbs; i++) {
		snd_assert(subs->dataurb[i].urb, return -EINVAL);
		if (subs->ops.prepare(subs, runtime, subs->dataurb[i].urb) < 0) {
			snd_printk(KERN_ERR "cannot prepare datapipe for urb %d\n", i);
			goto __error;
		}
	}
	if (subs->syncpipe) {
		for (i = 0; i < SYNC_URBS; i++) {
			snd_assert(subs->syncurb[i].urb, return -EINVAL);
			if (subs->ops.prepare_sync(subs, runtime, subs->syncurb[i].urb) < 0) {
				snd_printk(KERN_ERR "cannot prepare syncpipe for urb %d\n", i);
				goto __error;
			}
		}
	}
	for (i = 0; i < subs->nurbs; i++) {
		if ((err = do_usb_submit_urb(subs->dataurb[i].urb, GFP_KERNEL)) < 0) {
			snd_printk(KERN_ERR "cannot submit datapipe for urb %d, err = %d\n", i, err);
#if 0
			printk("--> urb trsize = %d, buf = %p, pipe = 0x%x, # packets = %d\n",
			       subs->dataurb[i].urb->transfer_buffer_length,
			       subs->dataurb[i].urb->transfer_buffer,
			       subs->dataurb[i].urb->pipe,
			       subs->dataurb[i].urb->number_of_packets);
#endif
			goto __error;
		}
		set_bit(subs->dataurb[i].index, &subs->transmask);
	}
	if (subs->syncpipe) {
		for (i = 0; i < SYNC_URBS; i++) {
			if ((err = do_usb_submit_urb(subs->syncurb[i].urb, GFP_KERNEL)) < 0) {
				snd_printk(KERN_ERR "cannot submit syncpipe for urb %d, err = %d\n", i, err);
				goto __error;
			}
			set_bit(subs->syncurb[i].index + 16, &subs->transmask);
		}
	}
	return 0;

 __error:
	snd_pcm_stop(subs->pcm_substream, SNDRV_PCM_STATE_XRUN);
	return -EPIPE;
}


/*
 */
static int snd_stop_urbs(snd_usb_substream_t *subs, int do_loop)
{
	int timeout = HZ;
	int i, alive;

	do {
		alive = 0;
		for (i = 0; i < subs->nurbs; i++) {
			if (test_bit(i, &subs->transmask)) {
				usb_unlink_urb(subs->dataurb[i].urb);
				alive++;
			}
		}
		if (subs->syncpipe) {
			for (i = 0; i < SYNC_URBS; i++) {
				if (test_bit(i + 16, &subs->transmask)) {
					usb_unlink_urb(subs->syncurb[i].urb);
					alive++;
				}
			}
		}
		if (! alive || ! do_loop)
			break;
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(1);
		set_current_state(TASK_RUNNING);
	} while (--timeout > 0);
	if (alive && do_loop) {
		snd_printk(KERN_ERR "timeout: still %d active urbs..\n", alive);
	}
	return 0;
}


/*
 */
static snd_pcm_uframes_t snd_usb_pcm_pointer(snd_pcm_substream_t *substream)
{
	snd_usb_substream_t *subs = (snd_usb_substream_t *)substream->runtime->private_data;
	return subs->hwptr_done;
}

/*
 * start/stop
 */
static int snd_usb_pcm_trigger(snd_pcm_substream_t *substream, int cmd)
{
	snd_usb_substream_t *subs = (snd_usb_substream_t *)substream->runtime->private_data;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		return snd_start_urbs(subs, substream->runtime);
	case SNDRV_PCM_TRIGGER_STOP:
		return snd_stop_urbs(subs, 0);
	default:
		return -EINVAL;
	}
}


/*
 */

static void release_urb_ctx(snd_urb_ctx_t *u)
{
	if (u->urb) {
		usb_free_urb(u->urb);
		u->urb = 0;
	}
	if (u->buf) {
		kfree(u->buf);
		u->buf = 0;
	}
}

/* all urbs must have been already unlinked */
static void release_substream_urbs(snd_usb_substream_t *subs)
{
	int i;

	snd_stop_urbs(subs, 1);
	for (i = 0; i < MAX_URBS; i++)
		release_urb_ctx(&subs->dataurb[i]);
	for (i = 0; i < SYNC_URBS; i++)
		release_urb_ctx(&subs->syncurb[i]);
	if (subs->tmpbuf) {
		kfree(subs->tmpbuf);
		subs->tmpbuf = 0;
	}
	subs->npacks = 0;
	subs->nurbs = 0;
}

static int init_substream_urbs(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime)
{
	int maxsize, n, i;
	int is_playback = subs->pcm_substream->stream == SNDRV_PCM_STREAM_PLAYBACK;

	release_substream_urbs(subs);

	subs->freqn = subs->freqm = get_usb_rate(runtime->rate);
	subs->freqmax = subs->freqn + (subs->freqn >> 2);
	subs->phase = 0;

	subs->hwptr = 0;
	subs->hwptr_done = 0;
	subs->transfer_done = 0;
	subs->transmask = 0;

	maxsize = ((subs->freqmax + 0x3fff) * (runtime->frame_bits >> 3)) >> 14;
	if (subs->maxpacksize && maxsize > subs->maxpacksize) {
		snd_printk(KERN_INFO "maxsize %d is greater than defined size %d\n",
			   maxsize, subs->maxpacksize);
		maxsize = subs->maxpacksize;
	}

	subs->curpacksize = maxsize;

	subs->tmpbuf = kmalloc(maxsize * NRPACKS, GFP_KERNEL);
	if (! subs->tmpbuf) {
		snd_printk(KERN_ERR "cannot malloc tmpbuf\n");
		return -ENOMEM;
	}

	subs->npacks = (frames_to_bytes(runtime, runtime->period_size) + maxsize - 1) / maxsize;
	subs->nurbs = (subs->npacks + NRPACKS - 1) / NRPACKS;
	if (subs->nurbs > MAX_URBS) {
		subs->nurbs = MAX_URBS;
		subs->npacks = MAX_URBS * NRPACKS;
	}

#if 0
	printk("# packs = %d, # urbs = %d\n", subs->npacks, subs->nurbs);
	printk("fill_max = %d, maxsize = %d, cursize = %d\n",
	       subs->fill_max, subs->maxpacksize, subs->curpacksize);
#endif

	n = subs->npacks;
	for (i = 0; i < subs->nurbs; i++) {
		snd_urb_ctx_t *u = &subs->dataurb[i];
		u->index = i;
		u->subs = subs;
		u->transfer = 0;
		u->packets = n > NRPACKS ? NRPACKS : n;
		if (! is_playback) {
			u->buf = kmalloc(maxsize * u->packets, GFP_KERNEL);
			if (! u->buf) {
				release_substream_urbs(subs);
				return -ENOMEM;
			}
		}
		u->urb = do_usb_alloc_urb(u->packets, GFP_KERNEL);
		if (! u->urb) {
			release_substream_urbs(subs);
			return -ENOMEM;
		}
		u->urb->dev = subs->dev;
		u->urb->pipe = subs->datapipe;
		u->urb->transfer_flags = USB_ISO_ASAP;
		u->urb->number_of_packets = u->packets;
		u->urb->context = u;
		u->urb->complete = snd_complete_urb;
		n -= NRPACKS;
	}

	if (subs->syncpipe) {
		for (i = 0; i < SYNC_URBS; i++) {
			snd_urb_ctx_t *u = &subs->syncurb[i];
			u->index = i;
			u->subs = subs;
			u->transfer = 0;
			u->packets = NRPACKS;
			u->urb = do_usb_alloc_urb(u->packets, GFP_KERNEL);
			if (! u->urb) {
				release_substream_urbs(subs);
				return -ENOMEM;
			}
			u->urb->transfer_buffer = subs->syncbuf + i * NRPACKS * 3;
			u->urb->transfer_buffer_length = NRPACKS * 3;
			u->urb->dev = subs->dev;
			u->urb->pipe = subs->syncpipe;
			u->urb->transfer_flags = USB_ISO_ASAP;
			u->urb->number_of_packets = u->packets;
			u->urb->context = u;
			u->urb->complete = snd_complete_sync_urb;
		}
	}
	return 0;
}

static struct audioformat *find_format(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime)
{
	struct list_head *p;

	list_for_each(p, &subs->fmt_list) {
		struct audioformat *fp;
		fp = list_entry(p, struct audioformat, list);
		if (fp->format != runtime->format ||
		    fp->channels != runtime->channels)
			continue;
		if (runtime->rate < fp->rate_min || runtime->rate > fp->rate_max)
			continue;
		if (fp->rates & SNDRV_PCM_RATE_CONTINUOUS)
			return fp;
		else {
			int i;
			for (i = 0; i < fp->nr_rates; i++)
				if (fp->rate_table[i] == runtime->rate)
					return fp;
		}
	}
	return NULL;
}

static int set_format(snd_usb_substream_t *subs, snd_pcm_runtime_t *runtime)
{
	struct usb_device *dev = subs->dev;
	struct usb_config_descriptor *config = dev->actconfig;
	struct usb_interface_descriptor *alts;
	struct usb_interface *iface;	
	struct audioformat *fmt;
	unsigned int ep;
	unsigned char data[3];
	int is_playback = subs->pcm_substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	int err;

	fmt = find_format(subs, runtime);
	if (! fmt) {
		snd_printd(KERN_DEBUG "cannot set format: format = %d, rate = %d, channels = %d\n",
			   runtime->format, runtime->rate, runtime->channels);
		return -EINVAL;
	}

	iface = &config->interface[subs->interface];
	alts = &iface->altsetting[fmt->altsetting];
	if (is_playback)
		subs->datapipe = usb_sndisocpipe(dev, alts->endpoint[0].bEndpointAddress & 0xf);
	else
		subs->datapipe = usb_rcvisocpipe(dev, alts->endpoint[0].bEndpointAddress & 0xf);
	subs->syncpipe = subs->syncinterval = 0;
	subs->maxpacksize = alts->endpoint[0].wMaxPacketSize;
	subs->maxframesize = bytes_to_frames(runtime, subs->maxpacksize);
	subs->fill_max = 0;

	/* async OUT or adaptive IN */
	if ((is_playback && (alts->endpoint[0].bmAttributes & 0x0c) == 0x04) ||
	    (! is_playback && (alts->endpoint[0].bmAttributes & 0x0c) == 0x08) ) {
		if (alts->bNumEndpoints < 2 ||
		    alts->endpoint[1].bmAttributes != 0x01 ||
		    alts->endpoint[1].bSynchAddress != 0) {
			snd_printk(KERN_ERR "%d:%d:%d : invalid synch pipe\n",
				   dev->devnum, subs->interface, fmt->altsetting);
			return -EINVAL;
		}
		if ((is_playback && alts->endpoint[1].bEndpointAddress != (alts->endpoint[0].bSynchAddress | 0x80)) ||
		    (! is_playback && alts->endpoint[1].bEndpointAddress != (alts->endpoint[0].bSynchAddress & 0x7f))) {
			snd_printk(KERN_ERR "%d:%d:%d : invalid synch pipe\n",
				   dev->devnum, subs->interface, fmt->altsetting);
			return -EINVAL;
		}
		if (is_playback)
			subs->syncpipe = usb_rcvisocpipe(dev, alts->endpoint[1].bEndpointAddress & 0xf);
		else
			subs->syncpipe = usb_sndisocpipe(dev, alts->endpoint[1].bEndpointAddress & 0xf);
		subs->syncinterval = alts->endpoint[1].bRefresh;
	}

	if (usb_set_interface(dev, iface->altsetting->bInterfaceNumber, fmt->altsetting) < 0) {
		snd_printk(KERN_ERR "%d:%d:%d: usb_set_interface failed\n",
			   dev->devnum, subs->interface, fmt->altsetting);
		return -EIO;
	}

	ep = usb_pipeendpoint(subs->datapipe) | (subs->datapipe & USB_DIR_IN);
	/* if endpoint has pitch control, enable it */
	if (fmt->attributes & 0x02) {
		data[0] = 1;
		if ((err = usb_control_msg(dev, usb_sndctrlpipe(dev, 0), SET_CUR, USB_TYPE_CLASS|USB_RECIP_ENDPOINT|USB_DIR_OUT, 
					   PITCH_CONTROL << 8, ep, data, 1, HZ)) < 0) {
			snd_printk(KERN_ERR "%d:%d:%d: cannot set enable PITCH\n",
				   dev->devnum, subs->interface, ep);
			return err;
		}
	}
	/* if endpoint has sampling rate control, set it */
	if (fmt->attributes & 0x01) {
		data[0] = runtime->rate;
		data[1] = runtime->rate >> 8;
		data[2] = runtime->rate >> 16;
		if ((err = usb_control_msg(dev, usb_sndctrlpipe(dev, 0), SET_CUR, USB_TYPE_CLASS|USB_RECIP_ENDPOINT|USB_DIR_OUT, 
					   SAMPLING_FREQ_CONTROL << 8, ep, data, 3, HZ)) < 0) {
			snd_printk(KERN_ERR "%d:%d:%d: cannot set freq %d\n",
				   dev->devnum, subs->interface, ep, runtime->rate);
			return err;
		}
		if ((err = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0), GET_CUR, USB_TYPE_CLASS|USB_RECIP_ENDPOINT|USB_DIR_IN,
					   SAMPLING_FREQ_CONTROL << 8, ep, data, 3, HZ)) < 0) {
			snd_printk(KERN_ERR "%d:%d:%d: cannot get freq\n",
				   dev->devnum, subs->interface, ep);
			return err;
		}
		runtime->rate = data[0] | (data[1] << 8) | (data[2] << 16);
		// printk("ok, getting back rate to %d\n", runtime->rate);
	}
	if (fmt->attributes & 0x80)
		subs->fill_max = 1;

#if 0
	printk("setting done: format = %d, rate = %d, channels = %d\n",
	       runtime->format, runtime->rate, runtime->channels);
	printk("  datapipe = 0x%0x, syncpipe = 0x%0x\n",
	       subs->datapipe, subs->syncpipe);
#endif

	return 0;
}

static int snd_usb_hw_params(snd_pcm_substream_t *substream,
			     snd_pcm_hw_params_t *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_usb_hw_free(snd_pcm_substream_t *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static int snd_usb_pcm_prepare(snd_pcm_substream_t *substream)
{
	snd_pcm_runtime_t *runtime = substream->runtime;
	snd_usb_substream_t *subs = (snd_usb_substream_t *)runtime->private_data;
	int err;

	err = set_format(subs, runtime);
	if (err < 0)
		return err;

	init_substream_urbs(subs, runtime);

	return 0;
}

static snd_pcm_hardware_t snd_usb_playback =
{
	info:			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID),
	buffer_bytes_max:	(128*1024),
	period_bytes_min:	64,
	period_bytes_max:	(128*1024),
	periods_min:		1,
	periods_max:		1024,
};

static snd_pcm_hardware_t snd_usb_capture =
{
	info:			(SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
				 SNDRV_PCM_INFO_BLOCK_TRANSFER |
				 SNDRV_PCM_INFO_MMAP_VALID),
	buffer_bytes_max:	(128*1024),
	period_bytes_min:	64,
	period_bytes_max:	(128*1024),
	periods_min:		1,
	periods_max:		1024,
};

static void setup_hw_info(snd_pcm_runtime_t *runtime, snd_usb_substream_t *subs)
{
	struct list_head *p;

	runtime->hw.formats = subs->formats;

	runtime->hw.rate_min = 0x7fffffff;
	runtime->hw.rate_max = 0;
	runtime->hw.channels_min = 256;
	runtime->hw.channels_max = 0;
	runtime->hw.rates = 0;
	list_for_each(p, &subs->fmt_list) {
		struct audioformat *fp;
		fp = list_entry(p, struct audioformat, list);
		runtime->hw.rates |= fp->rates;
		if (runtime->hw.rate_min > fp->rate_min)
			runtime->hw.rate_min = fp->rate_min;
		if (runtime->hw.rate_max < fp->rate_max)
			runtime->hw.rate_max = fp->rate_max;
		if (runtime->hw.channels_min > fp->channels)
			runtime->hw.channels_min = fp->channels;
		if (runtime->hw.channels_max < fp->channels)
			runtime->hw.channels_max = fp->channels;
	}

	/* FIXME */
	snd_pcm_hw_constraint_minmax(runtime, SNDRV_PCM_HW_PARAM_PERIOD_TIME,
				     1000,
				     /*(NRPACKS * MAX_URBS) * 1000*/ UINT_MAX);
}

static int snd_usb_playback_open(snd_pcm_substream_t *substream)
{
	snd_usb_stream_t *as = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	snd_usb_substream_t *subs = &as->substream[SNDRV_PCM_STREAM_PLAYBACK];

	runtime->hw = snd_usb_playback;
	runtime->private_data = subs;
	subs->pcm_substream = substream;
	setup_hw_info(runtime, subs);

	return 0;
}

static int snd_usb_playback_close(snd_pcm_substream_t *substream)
{
	snd_usb_stream_t *as = snd_pcm_substream_chip(substream);
	snd_usb_substream_t *subs = &as->substream[SNDRV_PCM_STREAM_PLAYBACK];
	struct usb_interface *iface;	
	iface = &subs->dev->actconfig->interface[subs->interface];
	usb_set_interface(subs->dev, iface->altsetting->bInterfaceNumber, 0);
	release_substream_urbs(subs);
	subs->pcm_substream = NULL;
	return 0;
}

static int snd_usb_capture_open(snd_pcm_substream_t *substream)
{
	snd_usb_stream_t *as = snd_pcm_substream_chip(substream);
	snd_pcm_runtime_t *runtime = substream->runtime;
	snd_usb_substream_t *subs = &as->substream[SNDRV_PCM_STREAM_CAPTURE];

	runtime->hw = snd_usb_capture;
	runtime->private_data = subs;
	subs->pcm_substream = substream;
	setup_hw_info(runtime, subs);

	return 0;
}

static int snd_usb_capture_close(snd_pcm_substream_t *substream)
{
	snd_usb_stream_t *as = snd_pcm_substream_chip(substream);
	snd_usb_substream_t *subs = &as->substream[SNDRV_PCM_STREAM_CAPTURE];
	struct usb_interface *iface;	
	iface = &subs->dev->actconfig->interface[subs->interface];
	usb_set_interface(subs->dev, iface->altsetting->bInterfaceNumber, 0);
	release_substream_urbs(subs);
	subs->pcm_substream = NULL;
	return 0;
}

static snd_pcm_ops_t snd_usb_playback_ops = {
	open:		snd_usb_playback_open,
	close:		snd_usb_playback_close,
	ioctl:		snd_pcm_lib_ioctl,
	hw_params:	snd_usb_hw_params,
	hw_free:	snd_usb_hw_free,
	prepare:	snd_usb_pcm_prepare,
	trigger:	snd_usb_pcm_trigger,
	pointer:	snd_usb_pcm_pointer,
};

static snd_pcm_ops_t snd_usb_capture_ops = {
	open:		snd_usb_capture_open,
	close:		snd_usb_capture_close,
	ioctl:		snd_pcm_lib_ioctl,
	hw_params:	snd_usb_hw_params,
	hw_free:	snd_usb_hw_free,
	prepare:	snd_usb_pcm_prepare,
	trigger:	snd_usb_pcm_trigger,
	pointer:	snd_usb_pcm_pointer,
};



/*
 * helper functions
 */

unsigned int snd_usb_combine_bytes(unsigned char *bytes, int size)
{
	switch (size) {
	case 1:  return *bytes;
	case 2:  return combine_word(bytes);
	case 3:  return combine_triple(bytes);
	case 4:  return combine_quad(bytes);
	default: return 0;
	}
}

void *snd_usb_find_desc(void *descstart, unsigned int desclen, void *after, 
			u8 dtype, int iface, int altsetting)
{
	u8 *p, *end, *next;
	int ifc = -1, as = -1;

	p = descstart;
	end = p + desclen;
	for (; p < end;) {
		if (p[0] < 2)
			return NULL;
		next = p + p[0];
		if (next > end)
			return NULL;
		if (p[1] == USB_DT_INTERFACE) {
			/* minimum length of interface descriptor */
			if (p[0] < 9)
				return NULL;
			ifc = p[2];
			as = p[3];
		}
		if (p[1] == dtype && (!after || (void *)p > after) &&
		    (iface == -1 || iface == ifc) && (altsetting == -1 || altsetting == as)) {
			return p;
		}
		p = next;
	}
	return NULL;
}

void *snd_usb_find_csint_desc(void *descstart, unsigned int desclen, void *after, u8 dsubtype, int iface, int altsetting)
{
	unsigned char *p = after;

	while ((p = snd_usb_find_desc(descstart, desclen, p, USB_DT_CS_INTERFACE,
				      iface, altsetting)) != NULL) {
		if (p[0] >= 3 && p[2] == dsubtype)
			return p;
	}
	return NULL;
}


/*
 */

static void * usb_audio_probe(struct usb_device *dev, unsigned int ifnum,
			      const struct usb_device_id *id);
static void usb_audio_disconnect(struct usb_device *dev, void *ptr);

static struct usb_device_id usb_audio_ids [] = {
    { match_flags: (USB_DEVICE_ID_MATCH_INT_CLASS | USB_DEVICE_ID_MATCH_INT_SUBCLASS),
      bInterfaceClass: USB_CLASS_AUDIO, bInterfaceSubClass: 1},
    { }						/* Terminating entry */
};

MODULE_DEVICE_TABLE (usb, usb_audio_ids);

static struct usb_driver usb_audio_driver = {
	name:		"audio",
	probe:		usb_audio_probe,
	disconnect:	usb_audio_disconnect,
	driver_list:	LIST_HEAD_INIT(usb_audio_driver.driver_list), 
	id_table:	usb_audio_ids,
};

/*
 *
 */

static void init_substream(snd_usb_stream_t *stream, snd_usb_substream_t *subs,
			   int iface_no, int is_input,
			   unsigned char *buffer, unsigned int buflen)
{
	struct usb_device *dev;
	struct usb_config_descriptor *config;
	struct usb_interface *iface;
	struct usb_interface_descriptor *alts;
	int i, pcm_format;
	int format, channels, format_type, nr_rates;
	struct audioformat *fp;
	unsigned char *fmt, *csep;

	dev = stream->chip->dev;
	config = dev->actconfig;

	subs->stream = stream;
	subs->dev = dev;
	subs->interface = iface_no;
	INIT_LIST_HEAD(&subs->fmt_list);
	spin_lock_init(&subs->lock);
	if (is_input) {
		subs->ops.prepare = prepare_capture_urb;
		subs->ops.retire = retire_capture_urb;
		subs->ops.prepare_sync = prepare_capture_sync_urb;
		subs->ops.retire_sync = retire_capture_sync_urb;
	} else {
		subs->ops.prepare = prepare_playback_urb;
		subs->ops.retire = retire_playback_urb;
		subs->ops.prepare_sync = prepare_playback_sync_urb;
		subs->ops.retire_sync = retire_playback_sync_urb;
	}

	if (iface_no < 0)
		return;

	/* parse the interface's altsettings */
	iface = &config->interface[iface_no];
	for (i = 0; i < iface->num_altsetting; i++) {
		alts = &iface->altsetting[i];
		/* skip invalid one */
		if (alts->bInterfaceClass != USB_CLASS_AUDIO ||
		    alts->bInterfaceSubClass != 2 ||
		    alts->bNumEndpoints < 1)
			continue;
		/* must be isochronous */
		if ((alts->endpoint[0].bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) !=
		    USB_ENDPOINT_XFER_ISOC)
			continue;
		/* check direction */
		if (alts->endpoint[0].bEndpointAddress & USB_DIR_IN) {
			if (! is_input)
				continue;
		} else {
			if (is_input)
				continue;
		}

		/* get audio formats */
		fmt = snd_usb_find_csint_desc(buffer, buflen, NULL, AS_GENERAL,
					      iface_no, i);
		if (!fmt) {
			snd_printk(KERN_ERR "%d:%u:%d : AS_GENERAL descriptor not found\n",
				   dev->devnum, iface_no, i);
			continue;
		}

		if (fmt[0] < 7) {
			snd_printk(KERN_ERR "%d:%u:%d : invalid AS_GENERAL desc\n", 
				   dev->devnum, iface_no, i);
			continue;
		}

		format = (fmt[6] << 8) | fmt[5]; /* remember the format value */
			
		/* get format type */
		fmt = snd_usb_find_csint_desc(buffer, buflen, NULL, FORMAT_TYPE, iface_no, i);
		if (!fmt) {
			snd_printk(KERN_ERR "%d:%u:%d : no FORMAT_TYPE desc\n", 
				   dev->devnum, iface_no, i);
			continue;
		}
		if (fmt[0] < 8) {
			snd_printk(KERN_ERR "%d:%u:%d : invalid FORMAT_TYPE desc\n", 
				   dev->devnum, iface_no, i);
			continue;
		}

		format_type = fmt[3];
		/* FIXME: needed support for TYPE II and III */
		if (format_type != USB_FORMAT_TYPE_I) {
			snd_printk(KERN_WARNING "%d:%u:%d : format type %d is not supported yet\n",
				   dev->devnum, iface_no, i, format_type);
			continue;
		}

		nr_rates = fmt[7];
		if (fmt[0] < 8 + 3 * (nr_rates ? nr_rates : 2)) {
			snd_printk(KERN_ERR "%d:%u:%d : invalid FORMAT_TYPE desc\n", 
				   dev->devnum, iface_no, i);
			continue;
		}

		/* FIXME: correct endianess and sign? */
		pcm_format = -1;
		switch (format) {
		case USB_AUDIO_FORMAT_PCM:
			/* check the sample bit width */
			switch (fmt[6]) {
			case 8:
				subs->formats |= SNDRV_PCM_FMTBIT_U8;
				pcm_format = SNDRV_PCM_FORMAT_U8;
				break;
			case 16:
				subs->formats |= SNDRV_PCM_FMTBIT_S16_LE;
				pcm_format = SNDRV_PCM_FORMAT_S16_LE;
				break;
			case 24:
				if (fmt[5] != 4) {
					snd_printk(KERN_ERR "%d:%u:%d : frame size %d doesn't match ALSA 24bit sample format\n",
						   dev->devnum, iface_no, i, fmt[5]);
					break;
				}
				subs->formats |= SNDRV_PCM_FMTBIT_S24_LE;
				pcm_format = SNDRV_PCM_FORMAT_S24_LE;
				break;
			case 32:
				subs->formats |= SNDRV_PCM_FMTBIT_S32_LE;
				pcm_format = SNDRV_PCM_FORMAT_S32_LE;
				break;
			default:
				snd_printk(KERN_INFO "%d:%u:%d : unsupported sample bitwidth %d\n",
					   dev->devnum, iface_no, i, fmt[6]);
				break;
			}
			break;
		case USB_AUDIO_FORMAT_PCM8:
			/* Dallas DS4201 workaround */
			if (dev->descriptor.idVendor == 0x04fa && dev->descriptor.idProduct == 0x4201) {
				subs->formats |= ~SNDRV_PCM_FMTBIT_S8;
				pcm_format = SNDRV_PCM_FORMAT_S8;
			} else {
				subs->formats |= SNDRV_PCM_FMTBIT_U8;
				pcm_format = SNDRV_PCM_FORMAT_U8;
			}
			break;
		case USB_AUDIO_FORMAT_IEEE_FLOAT:
			subs->formats |= SNDRV_PCM_FMTBIT_FLOAT_LE;
			pcm_format = SNDRV_PCM_FORMAT_FLOAT_LE;
			break;
		case USB_AUDIO_FORMAT_ALAW:
			subs->formats |= SNDRV_PCM_FMTBIT_A_LAW;
			pcm_format = SNDRV_PCM_FORMAT_A_LAW;
			break;
		case USB_AUDIO_FORMAT_MU_LAW:
			subs->formats |= SNDRV_PCM_FMTBIT_MU_LAW;
			pcm_format = SNDRV_PCM_FORMAT_MU_LAW;
			break;
		}

		if (pcm_format < 0)
			continue;

		channels = fmt[4];
		if (channels < 1) {
			snd_printk(KERN_ERR "%d:%u:%d : invalid channels %d\n",
				   dev->devnum, iface_no, i, channels);
			continue;
		}

		csep = snd_usb_find_desc(buffer, buflen, NULL, USB_DT_CS_ENDPOINT,
					 iface_no, i);
		if (!csep || csep[0] < 7 || csep[2] != EP_GENERAL) {
			snd_printk(KERN_ERR "%d:%u:%d : no or invalid class specific endpoint descriptor\n", 
				   dev->devnum, iface_no, i);
			continue;
		}

		fp = kmalloc(sizeof(*fp), GFP_KERNEL);
		if (! fp) {
			snd_printk(KERN_ERR "cannot malloc\n");
			break;
		}

		memset(fp, 0, sizeof(*fp));
		fp->format = pcm_format;
		fp->altsetting = i;
		fp->channels = channels;
		fp->attributes = csep[3];

		if (nr_rates) {
			/*
			 * build the rate table and bitmap flags
			 */
			int r, idx, c;
			/* this table corresponds to the SNDRV_PCM_RATE_XXX bit */
			static int conv_rates[] = {
				5512, 8000, 11025, 16000, 22050, 32000, 44100, 48000,
				64000, 88200, 96000, 176400, 192000
			};
			fp->rate_table = kmalloc(sizeof(int) * nr_rates, GFP_KERNEL);
			if (fp->rate_table == NULL) {
				snd_printk(KERN_ERR "cannot malloc\n");
				kfree(fp);
				break;
			}

			fp->nr_rates = nr_rates;
			fp->rate_min = fp->rate_max = combine_triple(&fmt[8]);
			for (r = 0, idx = 8; r < nr_rates; r++, idx += 3) {
				int rate = fp->rate_table[r] = combine_triple(&fmt[idx]);
				if (rate < fp->rate_min)
					fp->rate_min = rate;
				else if (rate > fp->rate_max)
					fp->rate_max = rate;
				for (c = 0; c < 13; c++) {
					if (rate == conv_rates[c]) {
						fp->rates |= (1 << c);
						break;
					}
				}
#if 0 // FIXME - we need to define constraint
				if (c >= 13)
					fp->rates |= SNDRV_PCM_KNOT; /* unconventional rate */
#endif
			}

		} else {
			/* continuous rates */
			fp->rates = SNDRV_PCM_RATE_CONTINUOUS;
			fp->rate_min = combine_triple(&fmt[8]);
			fp->rate_max = combine_triple(&fmt[11]);
		}

		list_add(&fp->list, &subs->fmt_list);
		subs->num_formats++;
	}
}


static void free_substream(snd_usb_substream_t *subs)
{
	struct list_head *p, *n;

	if (subs->interface < 0)
		return;

	list_for_each_safe(p, n, &subs->fmt_list) {
		struct audioformat *fp = list_entry(p, struct audioformat, list);
		if (fp->rate_table)
			kfree(fp->rate_table);
		kfree(fp);
	}
}

static void snd_usb_audio_stream_free(snd_usb_stream_t *stream)
{
	free_substream(&stream->substream[0]);
	free_substream(&stream->substream[1]);
	snd_magic_kfree(stream);
}

static void snd_usb_audio_pcm_free(snd_pcm_t *pcm)
{
	snd_usb_stream_t *stream = pcm->private_data;
	if (stream) {
		stream->pcm = NULL;
		/* snd_pcm_lib_preallocate_free_for_all(pcm); */
		snd_usb_audio_stream_free(stream);
	}
}

static int snd_usb_audio_stream_new(snd_usb_audio_t *chip, unsigned char *buffer,
				    unsigned int buflen, int asifin, int asifout)
{
	snd_usb_stream_t *as;
	snd_pcm_t *pcm;
	char name[32];
	int err;

	as = snd_magic_kmalloc(snd_usb_stream_t, 0, GFP_KERNEL);
	if (as == NULL) {
		snd_printk(KERN_ERR "cannot malloc\n");
		return -ENOMEM;
	}
	memset(as, 0, sizeof(*as));
	as->chip = chip;

	init_substream(as, &as->substream[0], asifout, 0, buffer, buflen);
	init_substream(as, &as->substream[1], asifin, 1, buffer, buflen);

	if (as->substream[0].formats == 0 && as->substream[1].formats == 0) {
		snd_usb_audio_stream_free(as);
		return 0;
	}

	if (chip->pcm_devs > 0)
		sprintf(name, "USB Audio #%d", chip->pcm_devs);
	else
		strcpy(name, "USB Audio");
	err = snd_pcm_new(chip->card, "USB Audio", chip->pcm_devs,
			  as->substream[0].formats ? 1 : 0,
			  as->substream[1].formats ? 1 : 0,
			  &pcm);
	if (err < 0) {
		snd_usb_audio_stream_free(as);
		return err;
	}

	as->pcm = pcm;
	if (as->substream[0].formats)
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_usb_playback_ops);
	if (as->substream[1].formats)
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_usb_capture_ops);

	pcm->private_data = as;
	pcm->private_free = snd_usb_audio_pcm_free;
	pcm->info_flags = 0;

	strcpy(pcm->name, name);

	/* snd_pcm_lib_preallocate_pci_pages_for_all(chip->pci, pcm, chip->multichannel ? 128*1024 : 64*1024, 128*1024); */

	chip->pcm_devs++;

	return 0;
}


/*
 * parse audio control descriptor and create pcm streams
 */

static int snd_usb_create_pcm(snd_usb_audio_t *chip, unsigned char *buffer, unsigned int buflen, unsigned int ctrlif)
{
	struct usb_device *dev = chip->dev;
	struct usb_config_descriptor *config;
	struct usb_interface *iface;
	unsigned char *p1;
	unsigned char ifin[USB_MAXINTERFACES], ifout[USB_MAXINTERFACES];
	int numifin = 0, numifout = 0;
	int i, j, k;

	/* find audiocontrol interface */
	if (!(p1 = snd_usb_find_csint_desc(buffer, buflen, NULL, HEADER, ctrlif, -1))) {
		snd_printk(KERN_ERR "cannot find HEADER\n");
		return -EINVAL;
	}
	if (! p1[7] || p1[0] < 8 + p1[7]) {
		snd_printk(KERN_ERR "invalid HEADER\n");
		return -EINVAL;
	}

	/*
	 * parse all interfaces
	 */
	config = dev->actconfig;
	for (i = 0; i < p1[7]; i++) {
		j = p1[8 + i];
		if (j >= config->bNumInterfaces) {
			snd_printk(KERN_DEBUG "%d:%u:%d : does not exist\n",
				   dev->devnum, ctrlif, j);
			continue;
		}
		iface = &config->interface[j];
		if (iface->altsetting[0].bInterfaceClass != USB_CLASS_AUDIO ||
		    iface->altsetting[0].bInterfaceSubClass != 2) {
			snd_printk(KERN_DEBUG "non-supported interface %d\n", iface->altsetting[0].bInterfaceClass);
			/* skip non-supported classes */
			continue;
		}
		if (iface->num_altsetting < 2) {
			snd_printk(KERN_DEBUG "%d:%u:%d : no valid interface.\n",
				   dev->devnum, ctrlif, j);
			continue;
		}
		if (iface->altsetting[0].bNumEndpoints > 0) {
			/* Check all endpoints; should they all have a bandwidth of 0 ? */
			for (k = 0; k < iface->altsetting[0].bNumEndpoints; k++) {
				if (iface->altsetting[0].endpoint[k].wMaxPacketSize > 0) {
					snd_printk(KERN_ERR "%d:%u:%d ep%d : have no bandwith at alt[0]\n", dev->devnum, ctrlif, j, k);
					break;
				}
			}
			if (k < iface->altsetting[0].bNumEndpoints)
				continue;
		}
		if (iface->altsetting[1].bNumEndpoints < 1) {
			snd_printk(KERN_ERR "%d:%u:%d : has no endpoint\n",
				   dev->devnum, ctrlif, j);
			continue;
		}
		/* note: this requires the data endpoint to be ep0 and the optional sync
		   ep to be ep1, which seems to be the case */
		if (iface->altsetting[1].endpoint[0].bEndpointAddress & USB_DIR_IN) {
			if (numifin < USB_MAXINTERFACES) {
				ifin[numifin++] = j;
				usb_driver_claim_interface(&usb_audio_driver, iface, (void *)-1);
			}
		} else {
			if (numifout < USB_MAXINTERFACES) {
				ifout[numifout++] = j;
				usb_driver_claim_interface(&usb_audio_driver, iface, (void *)-1);
			}
		}
	}

	for (i = 0; i < numifin && i < numifout; i++)
		snd_usb_audio_stream_new(chip, buffer, buflen, ifin[i], ifout[i]);
	for (j = i; j < numifin; j++)
		snd_usb_audio_stream_new(chip, buffer, buflen, ifin[i], -1);
	for (j = i; j < numifout; j++)
		snd_usb_audio_stream_new(chip, buffer, buflen, -1, ifout[i]);

	return 0;
}


/*
 * free the chip instance
 *
 * here we have to do not much, since pcm and controls are already freed
 *
 */

static int snd_usb_audio_free(snd_usb_audio_t *chip)
{
	snd_magic_kfree(chip);
	return 0;
}

static int snd_usb_audio_dev_free(snd_device_t *device)
{
	snd_usb_audio_t *chip = snd_magic_cast(snd_usb_audio_t, device->device_data, return -ENXIO);
	return snd_usb_audio_free(chip);
}


/*
 * parse description and create pcms and controls
 */
static int snd_usb_audio_create(snd_card_t *card, struct usb_device *dev,
				unsigned char *buffer, unsigned int buflen,
				unsigned int ctrlif, snd_usb_audio_t **rchip)
{
	snd_usb_audio_t *chip;
	int err, len;
	static snd_device_ops_t ops = {
		dev_free:	snd_usb_audio_dev_free,
	};
	
	*rchip = NULL;
	chip = snd_magic_kcalloc(snd_usb_audio_t, 0, GFP_KERNEL);
	if (! chip)
		return -ENOMEM;

	chip->dev = dev;
	chip->card = card;

	/* parse audio stream descriptors */
	if ((err = snd_usb_create_pcm(chip, buffer, buflen, ctrlif)) < 0) {
		snd_magic_kfree(chip);
		return err;
	}

	/* parse mixer descriptors */
	if ((err = snd_usb_create_mixer(chip, buffer, buflen, ctrlif)) < 0) {
		snd_magic_kfree(chip);
		return err;
	}

	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0) {
		snd_magic_kfree(chip);
		return err;
	}

	strcpy(card->driver, "USB-Audio");
	strcpy(card->shortname, "USB Audio Driver");

	/* retrieve the vendor and device strings as longname */
	len = usb_string(dev, 1, card->longname, sizeof(card->longname) - 1);
	if (len < 0) len = 0;
	card->longname[len] = ' ';
	usb_string(dev, 2, card->longname + len + 1, sizeof(card->longname) - len - 1);

	*rchip = chip;
	return 0;
}


/*
 * probe the active usb device
 */
static void *usb_audio_probe(struct usb_device *dev, unsigned int ifnum,
			     const struct usb_device_id *id)
{
	struct usb_config_descriptor *config = dev->actconfig;	
	unsigned char *buffer;
	unsigned char buf[8];
	unsigned int i, buflen;
	int err;
	int idx;
	snd_card_t *card;
	snd_usb_audio_t *chip;

	i = dev->actconfig - config;

	if (usb_set_configuration(dev, config->bConfigurationValue) < 0) {
		snd_printk(KERN_ERR "cannot set configuration (value 0x%x)\n", config->bConfigurationValue);
		return NULL;
	}
	err = usb_get_descriptor(dev, USB_DT_CONFIG, i, buf, 8);
	if (err < 0) {
		snd_printk(KERN_ERR "%d:%d: cannot get first 8 bytes\n", i, dev->devnum);
		return NULL;
	}
	if (buf[1] != USB_DT_CONFIG || buf[0] < 9) {
		snd_printk(KERN_ERR "%d:%d: invalid config desc\n", i, dev->devnum);
		return NULL;
	}
	buflen = combine_word(&buf[2]);
	if (!(buffer = kmalloc(buflen, GFP_KERNEL))) {
		snd_printk(KERN_ERR "cannot malloc descriptor (size = %d)\n", buflen);
		return NULL;
	}
	err = usb_get_descriptor(dev, USB_DT_CONFIG, i, buffer, buflen);
	if (err < 0) {
		snd_printk(KERN_ERR "%d:%d: cannot get DT_CONFIG: error %d\n", i, dev->devnum, err);
		kfree(buffer);
		return NULL;
	}

	/*
	 * found a config.  now register to ALSA
	 */
	for (idx = 0; idx < SNDRV_CARDS; idx++)
		if (! test_and_set_bit(idx, &snd_index_bitmap))
			break;
	if (idx >= SNDRV_CARDS || ! snd_enable[idx]) {
		snd_printk(KERN_INFO "card %d is out of range or disabled\n", idx);
		kfree(buffer);
		return NULL;
	}

	card = snd_card_new(snd_index[idx], snd_id[idx], THIS_MODULE, 0);
	if (card == NULL) {
		snd_printk(KERN_ERR "cannot create a card instance %d\n", idx);
		goto __error;
	}

	if (snd_usb_audio_create(card, dev, buffer, buflen, ifnum, &chip) < 0) {
		snd_card_free(card);
		goto __error;
	}

	if ((err = snd_card_register(card)) < 0) {
		snd_card_free(card);
		goto __error;
	}

	kfree(buffer);
	return chip;

 __error:
	clear_bit(idx, &snd_index_bitmap);
	kfree(buffer);
	return NULL;
}


/* a revoke facility would make things simpler */

static void usb_audio_disconnect(struct usb_device *dev, void *ptr)
{
	snd_usb_audio_t *chip;

	if (ptr == (void *)-1)
		return;

	chip = snd_magic_cast(snd_usb_audio_t, ptr, return);
	snd_card_free(chip->card);
}

static int __init snd_usb_audio_init(void)
{
	usb_register(&usb_audio_driver);
	return 0;
}


static void __exit snd_usb_audio_cleanup(void)
{
	usb_deregister(&usb_audio_driver);
}

module_init(snd_usb_audio_init);
module_exit(snd_usb_audio_cleanup);