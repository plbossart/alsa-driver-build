#ifndef __AC97_CODEC_H
#define __AC97_CODEC_H

/*
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *  Universal interface for Audio Codec '97
 *
 *  For more details look to AC '97 component specification revision 2.1
 *  by Intel Corporation (http://developer.intel.com).
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "mixer.h"
#include "info.h"

/*
 *  AC'97 codec registers
 */

#define AC97_RESET		0x00	/* Reset */
#define AC97_MASTER		0x02	/* Master Volume */
#define AC97_HEADPHONE		0x04	/* Headphone Volume (optional) */
#define AC97_MASTER_MONO	0x06	/* Master Volume Mono (optional) */
#define AC97_MASTER_TONE	0x08	/* Master Tone (Bass & Treble) (optional) */
#define AC97_PC_BEEP		0x0a	/* PC Beep Volume (optinal) */
#define AC97_PHONE		0x0c	/* Phone Volume (optional) */
#define AC97_MIC		0x0e	/* MIC Volume */
#define AC97_LINE		0x10	/* Line In Volume */
#define AC97_CD			0x12	/* CD Volume */
#define AC97_VIDEO		0x14	/* Video Volume (optional) */
#define AC97_AUX		0x16	/* AUX Volume (optional) */
#define AC97_PCM		0x18	/* PCM Volume */
#define AC97_REC_SEL		0x1a	/* Record Select */
#define AC97_REC_GAIN		0x1c	/* Record Gain */
#define AC97_REC_GAIN_MIC	0x1e	/* Record Gain MIC (optional) */
#define AC97_GENERAL_PURPOSE	0x20	/* General Purpose (optional) */
#define AC97_3D_CONTROL		0x22	/* 3D Control (optional) */
#define AC97_RESERVED		0x24	/* Reserved */
#define AC97_POWERDOWN		0x26	/* Powerdown control / status */
/* range 0x28-0x3a - AUDIO AC'97 2.0 extensions */
#define AC97_EXTENDED_ID	0x28	/* Extended Audio ID */
#define AC97_EXTENDED_STATUS	0x2a	/* Extended Audio Status */
#define AC97_PCM_FRONT_DAC_RATE 0x2c	/* PCM Front DAC Rate */
#define AC97_PCM_SURR_DAC_RATE	0x2e	/* PCM Surround DAC Rate */
#define AC97_PCM_LFE_DAC_RATE	0x30	/* PCM LFE DAC Rate */
#define AC97_PCM_LR_ADC_RATE	0x32	/* PCM LR DAC Rate */
#define AC97_PCM_MIC_ADC_RATE	0x34	/* PCM MIC ADC Rate */
#define AC97_CENTER_LFE_MASTER	0x36	/* Center + LFE Master Volume */
#define AC97_SURROUND_MASTER	0x38	/* Surround (Rear) Master Volume */
#define AC97_RESERVED_3A	0x3a	/* Reserved */
/* range 0x3c-0x58 - MODEM */
/* range 0x5a-0x7b - Vendor Specific */
#define AC97_VENDOR_ID1		0x7c	/* Vendor ID1 */
#define AC97_VENDOR_ID2		0x7e	/* Vendor ID2 / revision */

/* specific */
#define AC97_SIGMATEL_ANALOG	0x6c	/* Analog Special */
#define AC97_SIGMATEL_DAC2INVERT 0x6e
#define AC97_SIGMATEL_BIAS1	0x70
#define AC97_SIGMATEL_BIAS2	0x72
#define AC97_SIGMATEL_CIC1	0x76
#define AC97_SIGMATEL_CIC2	0x78

/*

 */

typedef struct snd_stru_ac97 ac97_t;

struct snd_stru_ac97 {
	void (*write) (void *private_data, unsigned short reg, unsigned short val);
	unsigned short (*read) (void *private_data, unsigned short reg);
	void (*init) (void *private_data, ac97_t *ac97);
	snd_info_entry_t *proc_entry;
	snd_info_entry_t *proc_regs_entry;
	void *private_data;
	void (*private_free) (void *private_data);
	/* --- */
	snd_kmixer_t *mixer;
	spinlock_t reg_lock;
	unsigned int id;	/* identification of codec */
	unsigned short caps;	/* capabilities (register 0) */
	unsigned short micgain;	/* mic gain is active */
	unsigned short regs[0x80]; /* register cache */
	unsigned char bass;	/* tone control - bass value */
	unsigned char treble;	/* tone control - treble value */
	unsigned char max_master; /* master maximum volume value */
	unsigned char max_master_mono; /* master mono maximum volume value */
	unsigned char max_headphone; /* headphone maximum volume value */
	unsigned char max_mono;	/* mono maximum volume value */
	unsigned char max_3d;	/* 3d maximum volume value */
	unsigned char shift_3d;	/* 3d shift value */
	unsigned char max_center; /* center master maximum volume value */
	unsigned char max_lfe;	  /* lfe master maximum volume value */
	unsigned char max_surround; /* surround master maximum volume value */

	snd_kmixer_element_t *me_mux_mic;
	snd_kmixer_element_t *me_mux_cd;
	snd_kmixer_element_t *me_mux_video;
	snd_kmixer_element_t *me_mux_aux;
	snd_kmixer_element_t *me_mux_line;
	snd_kmixer_element_t *me_mux_mix;
	snd_kmixer_element_t *me_mux_mono_mix;
	snd_kmixer_element_t *me_mux_phone;

	snd_kmixer_element_t *me_mux2_out_mono_accu;
	snd_kmixer_element_t *me_mux2_mic;

	snd_kmixer_element_t *me_accu;
	snd_kmixer_element_t *me_pcm_accu;
	snd_kmixer_element_t *me_bypass_accu;
	snd_kmixer_element_t *me_mono_accu;
	snd_kmixer_element_t *me_mono_accu_in;
	snd_kmixer_element_t *me_mux;
	snd_kmixer_element_t *me_mono_mux;
	snd_kmixer_element_t *me_playback;
	snd_kmixer_element_t *me_vol_pcm;
	snd_kmixer_element_t *me_sw_pcm;
	snd_kmixer_element_t *me_vol_pc_beep;
	snd_kmixer_element_t *me_sw_pc_beep;
	snd_kmixer_element_t *me_vol_phone;
	snd_kmixer_element_t *me_sw_phone;
	snd_kmixer_element_t *me_vol_mic;
	snd_kmixer_element_t *me_sw_mic;	
	snd_kmixer_element_t *me_vol_line;
	snd_kmixer_element_t *me_sw_line;
	snd_kmixer_element_t *me_vol_cd;
	snd_kmixer_element_t *me_sw_cd;
	snd_kmixer_element_t *me_vol_video;
	snd_kmixer_element_t *me_sw_video;
	snd_kmixer_element_t *me_vol_aux;
	snd_kmixer_element_t *me_sw_aux;
	snd_kmixer_element_t *me_tone;
	snd_kmixer_element_t *me_vol_master;
	snd_kmixer_element_t *me_sw_master;
	snd_kmixer_element_t *me_out_master;
	snd_kmixer_element_t *me_vol_headphone;
	snd_kmixer_element_t *me_sw_headphone;
	snd_kmixer_element_t *me_out_headphone;
	snd_kmixer_element_t *me_vol_master_mono;
	snd_kmixer_element_t *me_sw_master_mono;
	snd_kmixer_element_t *me_out_master_mono;
	snd_kmixer_element_t *me_vol_igain;
	snd_kmixer_element_t *me_sw_igain;
	snd_kmixer_element_t *me_vol_igain_mic;
	snd_kmixer_element_t *me_sw_igain_mic;
	snd_kmixer_element_t *me_capture;
	snd_kmixer_element_t *me_in_center;
	snd_kmixer_element_t *me_vol_center;
	snd_kmixer_element_t *me_sw_center;
	snd_kmixer_element_t *me_out_center;
	snd_kmixer_element_t *me_in_lfe;
	snd_kmixer_element_t *me_vol_lfe;
	snd_kmixer_element_t *me_sw_lfe;
	snd_kmixer_element_t *me_out_lfe;
	snd_kmixer_element_t *me_in_surround;
	snd_kmixer_element_t *me_vol_surround;
	snd_kmixer_element_t *me_sw_surround;
	snd_kmixer_element_t *me_out_surround;
};

int snd_ac97_mixer(snd_card_t * card, int device, 
		   ac97_t * ac97, int pcm_count, int *pcm_devs,
		   snd_kmixer_t ** rmixer);

void snd_ac97_write(ac97_t *ac97, unsigned short reg, unsigned short value);
unsigned short snd_ac97_read(ac97_t *ac97, unsigned short reg);
void snd_ac97_write_lock(ac97_t *ac97, unsigned short reg, unsigned short value);
void snd_ac97_write_bitmask_lock(ac97_t *ac97, unsigned short reg, unsigned short bitmask, unsigned short value);
unsigned short snd_ac97_read_lock(ac97_t *ac97, unsigned short reg);

#endif				/* __AC97_CODEC_H */
