#ifndef __MIXER_H
#define __MIXER_H

/*
 *  Abstraction layer for MIXER
 *  Copyright (c) by Jaroslav Kysela <perex@jcu.cz>
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
 
#define SND_MIXER_CHANNELS	16		/* max channels per card */

#define SND_MIXER_PRI_MASTER		0x00000100
#define SND_MIXER_PRI_MASTER1		0x00000101
#define SND_MIXER_PRI_3D		0x00000108
#define SND_MIXER_PRI_BASS		0x00000110
#define SND_MIXER_PRI_TREBLE		0x00000120
#define SND_MIXER_PRI_FADER		0x00000140
#define SND_MIXER_PRI_SYNTHESIZER	0x00000200
#define SND_MIXER_PRI_SYNTHESIZER1	0x00000300
#define SND_MIXER_PRI_FM		0x00000400
#define SND_MIXER_PRI_EFFECT		0x00000500
#define SND_MIXER_PRI_PCM		0x00000600
#define SND_MIXER_PRI_PCM1		0x00000700
#define SND_MIXER_PRI_LINE		0x00000800
#define SND_MIXER_PRI_MIC		0x00000900
#define SND_MIXER_PRI_CD		0x00000a00
#define SND_MIXER_PRI_GAIN		0x00000b00
#define SND_MIXER_PRI_IGAIN		0x00000c00
#define SND_MIXER_PRI_OGAIN		0x00000d00
#define SND_MIXER_PRI_LOOPBACK		0x00000e00
#define SND_MIXER_PRI_SPEAKER		0x00000f00
#define SND_MIXER_PRI_MONO		0x00000f80
#define SND_MIXER_PRI_MONO1		0x00000f81
#define SND_MIXER_PRI_MONO2		0x00000f82
#define SND_MIXER_PRI_AUXA		0xf0000000
#define SND_MIXER_PRI_AUXB		0xf0000100
#define SND_MIXER_PRI_AUXC		0xf0000200
#define SND_MIXER_PRI_PARENT		0xffffffff

#define SND_MIX_MUTE_LEFT	1
#define SND_MIX_MUTE_RIGHT	2
#define SND_MIX_MUTE		(SND_MIX_MUTE_LEFT|SND_MIX_MUTE_RIGHT)

typedef struct snd_stru_mixer_channel snd_kmixer_channel_t;
typedef struct snd_stru_mixer_file snd_kmixer_file_t;

struct snd_stru_mixer_channel_hw {
  unsigned int priority;	/* this is sorting key - highter value = end */
  unsigned int parent_priority;	/* parent.... */
  char name[ 16 ];		/* device name */
  unsigned short ossdev;	/* assigned OSS device number */
  unsigned short mute: 1,	/* mute is supported */
                 stereo: 1,	/* stereo is supported */
                 record: 1,	/* record is supported */
                 digital: 1,	/* digital mixer channel */
                 input: 1;	/* this channel is input channel */
  int min, max;			/* min and max left & right value */
  int min_dB, max_dB, step_dB;	/* min_dB, max_dB, step_dB */
  unsigned int private_value;	/* can be used by low-level driver */

  /* input = dB value from application, output = min..max (linear volume) */
  int (*compute_linear)( snd_kmixer_t *mixer, snd_kmixer_channel_t *channel, int dB );
  /* input = min to max (user volume), output = dB value */
  int (*compute_dB)( snd_kmixer_t *mixer, snd_kmixer_channel_t *channel, int volume );
  /* --- */
  void (*set_record_source)( snd_kmixer_t *mixer, snd_kmixer_channel_t *channel, int enable );
  void (*set_mute)( snd_kmixer_t *mixer, snd_kmixer_channel_t *channel, unsigned int mute );
  void (*set_volume_level)( snd_kmixer_t *mixer, snd_kmixer_channel_t *channel, int left, int right );
};

struct snd_stru_mixer_channel {
  unsigned short channel;	/* channel index */
  unsigned short record: 1;	/* recording is enabled */
 
  unsigned char umute;		/* user mute */
  unsigned char kmute;		/* kernel mute */

  unsigned char mute;		/* real mute flags */
  unsigned char pad;		/* reserved */

  unsigned char aleft;		/* application - 0 to 100 */
  unsigned char aright;		/* application - 0 to 100 */
  int uleft;			/* user - 0 to max */
  int uright;			/* user - 0 to max */
  int left;			/* real - 0 to max */
  int right;			/* real - 0 to max */

  unsigned int private_value;
  void *private_data;		/* not freed by mixer.c */
  
  struct snd_stru_mixer_channel_hw hw; /* readonly variables */ 
};

struct snd_stru_mixer_file {
  snd_kmixer_t *mixer;
  int exact;			/* exact mode for this file */
  volatile unsigned int changes;
  snd_sleep_define( change );
  struct snd_stru_mixer_file *next;
};

struct snd_stru_mixer_hw {
  unsigned int caps;
  int (*set_special)( snd_kmixer_t *mixer, struct snd_mixer_special *special );
  int (*get_special)( snd_kmixer_t *mixer, struct snd_mixer_special *special );
};

struct snd_stru_mixer {
  snd_card_t *card;
  unsigned int device;			/* device # */

  char id[32];
  unsigned char name[80];

  unsigned int channels_count;		/* channels count */
  snd_kmixer_channel_t *channels[ SND_MIXER_CHANNELS ];

  void *private_data;
  void (*private_free)( void *private_data );
  unsigned int private_value;

  struct snd_stru_mixer_hw hw; 		/* readonly variables */

  snd_kmixer_file_t *ffile;		/* first file */
  snd_mutex_define( ffile );
  snd_spin_define( lock );

  snd_info_entry_t *dev;
  snd_info_entry_t *proc_entry;
};

extern void snd_mixer_set_kernel_mute( snd_kmixer_t *mixer, unsigned int priority, unsigned short mute );

extern snd_kmixer_t *snd_mixer_new( snd_card_t *card, char *id );
extern int snd_mixer_free( snd_kmixer_t *mixer );

extern snd_kmixer_channel_t *snd_mixer_new_channel( snd_kmixer_t *mixer, struct snd_stru_mixer_channel_hw *hw );
extern void snd_mixer_reorder_channel( snd_kmixer_t *mixer, snd_kmixer_channel_t *channel );

extern int snd_mixer_register( snd_kmixer_t *mixer, int device );
extern int snd_mixer_unregister( snd_kmixer_t *mixer );

extern snd_kmixer_channel_t *snd_mixer_find_channel( snd_kmixer_t *mixer, unsigned int priority );
#if 0
extern int snd_mixer_channel_init( snd_kmixer_t *mixer, unsigned int priority, unsigned char left, unsigned char right, unsigned int flags );
#endif

#endif /* __MIXER_H */
