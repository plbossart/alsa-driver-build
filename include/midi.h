#ifndef __MIDI_H
#define __MIDI_H

/*
 *  Abstract layer for MIDI v1.0 stream
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

/*
 *  Raw MIDI interface
 */

#define SND_RAWMIDI_DEVICES	4

#define SND_RAWMIDI_HW_POLL	0x00000001	/* polled mode */

#define SND_RAWMIDI_MODE_STREAM	0x00000000	/* stream mode */
#define SND_RAWMIDI_MODE_SEQ	0x00000001	/* sequencer mode */

#define SND_RAWMIDI_FLG_SLEEP	0x00000001	/* process is sleeping */
#define SND_RAWMIDI_FLG_FLUSH	0x00000002	/* flush in progress */
#define SND_RAWMIDI_FLG_TRIGGER	0x00000004	/* trigger in progress */
#define SND_RAWMIDI_FLG_TIMER	0x00000008	/* polling timer armed */
#define SND_RAWMIDI_FLG_OSS	0x80000000	/* OSS compatible mode */

#define SND_RAWMIDI_LFLG_OUTPUT	0x00000001	/* open for output */
#define SND_RAWMIDI_LFLG_INPUT	0x00000002	/* open for input */
#define SND_RAWMIDI_LFLG_OPEN	0x00000003	/* open */

typedef struct snd_stru_rawmidi_direction snd_rawmidi_direction_t;
typedef struct snd_stru_rawmidi_switch snd_rawmidi_kswitch_t;

struct snd_stru_rawmidi_direction_hw {
	unsigned int flags;	/* SND_RAWMIDI_HW_XXXX */
	void *private_data;
	void (*private_free) (void *private_data);
	int (*open) (snd_rawmidi_t * rmidi);
	int (*close) (snd_rawmidi_t * rmidi);
	void (*trigger) (snd_rawmidi_t * rmidi, int up);
	union {
		void (*read) (snd_rawmidi_t * rmidi);
		void (*write) (snd_rawmidi_t * rmidi);
	} io;
	void (*abort) (snd_rawmidi_t * rmidi);
};

struct snd_stru_rawmidi_direction {
	unsigned int mode;	/* SND_RAWMIDI_MODE_XXXX */
	unsigned int flags;	/* SND_RAWMIDI_FLG_XXXX */
	struct {
		struct {
			/* midi stream buffer */
			unsigned char *buffer;	/* buffer for MIDI data */
			unsigned int size;	/* size of buffer */
			unsigned int head;	/* buffer head index */
			unsigned int tail;	/* buffer tail index */
			unsigned int used;	/* buffer used index */
			unsigned int used_max;	/* max used buffer for wakeup */
			unsigned int used_room;	/* min room in buffer for wakeup */
			unsigned int used_min;	/* min used buffer for wakeup */
			unsigned int xruns;	/* over/underruns counter */
		} s;
		struct {
			/* upper layer - parses MIDI v1.0 data */
			unsigned char *cbuffer;	/* command buffer */
			unsigned int csize;	/* command buffer size */
			unsigned int cused;	/* command buffer used */
			unsigned int cleft;	/* command buffer left */
			unsigned char cprev;	/* previous command */
			void *cmd_private_data;	/* private data for command */
			void (*command) (snd_rawmidi_t * rmidi,
					 void *cmd_private_data,
					 unsigned char *command,
					 int count);
		} p;
	} u;
	/* misc */
	unsigned int bytes;
	struct timer_list timer;	/* poll timer */
	/* callback */
	int (*reset) (snd_rawmidi_t * rmidi);	/* reset MIDI command!!! */
	int (*data) (snd_rawmidi_t * rmidi, char *buffer, int count);
	/* switches */
	unsigned int switches_count;
	snd_rawmidi_kswitch_t **switches;
	 snd_mutex_define(switches);
	/* hardware layer */
	struct snd_stru_rawmidi_direction_hw hw;
};

struct snd_stru_rawmidi_switch {
	char name[32];
	int (*get_switch) (snd_rawmidi_t * rmidi,
			   snd_rawmidi_kswitch_t * kswitch,
			   snd_rawmidi_switch_t * uswitch);
	int (*set_switch) (snd_rawmidi_t * rmidi,
			   snd_rawmidi_kswitch_t * kswitch,
			   snd_rawmidi_switch_t * uswitch);
	unsigned int private_value;
	void *private_data;	/* not freed by rawmidi.c */
};

struct snd_stru_rawmidi {
	snd_card_t *card;

	unsigned int device;		/* device number */
	unsigned int flags;		/* SND_RAWMIDI_LFLG_XXXX */
	unsigned int info_flags;	/* SND_RAWMIDI_INFO_XXXX */
	char id[32];
	char name[80];

	snd_rawmidi_direction_t input;
	snd_rawmidi_direction_t output;

	void *private_data;
	void (*private_free) (void *private_data);

	snd_spin_define(input);
	snd_spin_define(input_sleep);
	snd_spin_define(output);
	snd_spin_define(output_sleep);
	snd_sleep_define(input);
	snd_sleep_define(output);
	snd_mutex_define(open);

	snd_info_entry_t *dev;
	snd_info_entry_t *proc_entry;
};

/* main rawmidi functions */

extern snd_rawmidi_t *snd_rawmidi_new_device(snd_card_t * card, char *id);
extern int snd_rawmidi_free(snd_rawmidi_t * rmidi);
extern int snd_rawmidi_register(snd_rawmidi_t * rmidi, int rawmidi_device);
extern int snd_rawmidi_unregister(snd_rawmidi_t * rmidi);
extern snd_rawmidi_kswitch_t *snd_rawmidi_new_switch(snd_rawmidi_t * rmidi,
				snd_rawmidi_direction_t * dir,
				snd_rawmidi_kswitch_t * ksw);

/* control functions */

extern int snd_rawmidi_control_ioctl(snd_card_t * card,
				     snd_control_t * control,
				     unsigned int cmd,
				     unsigned long arg);

/* main midi functions */

extern int snd_midi_open(int cardnum, int device, int mode, snd_rawmidi_t ** out);
extern int snd_midi_close(int cardnum, int device, int mode);
extern int snd_midi_drain_output(snd_rawmidi_t * rmidi);
extern int snd_midi_flush_output(snd_rawmidi_t * rmidi);
extern int snd_midi_stop_input(snd_rawmidi_t * rmidi);
extern int snd_midi_start_input(snd_rawmidi_t * rmidi);
extern int snd_midi_transmit(snd_rawmidi_t * rmidi, char *buf, int count);

#endif				/* __MIDI_H */
