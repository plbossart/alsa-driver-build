/*
 *  Advanced Linux Sound Architecture - ALSA - Driver
 *
 *  Interface file between ALSA driver & user space
 *  Copyright (c) 1994-98 by Jaroslav Kysela <perex@jcu.cz>
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

#ifndef __ASOUND_H
#define __ASOUND_H

#if defined( LINUX ) || defined( __LINUX__ ) || defined( __linux__ )
#include <linux/ioctl.h>
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SND_LITTLE_ENDIAN
#endif
#endif
#ifndef __KERNEL__
#include <sys/time.h>
#endif

/*
 *  protocol version
 */

#define SND_PROTOCOL_VERSION( major, minor, subminor ) (((major)<<16)|((minor)<<8)|(subminor))
#define SND_PROTOCOL_MAJOR( version ) (((version)>>16)&0xffff)
#define SND_PROTOCOL_MINOR( version ) (((version)>>8)&0xff)
#define SND_PROTOCOL_SUBMINOR( version ) ((version)&0xff)
#define SND_PROTOCOL_UNCOMPATIBLE( kversion, uversion ) \
	( SND_PROTOCOL_MAJOR(kversion) != SND_PROTOCOL_MAJOR(uversion) || \
	 ( SND_PROTOCOL_MAJOR(kversion) == SND_PROTOCOL_MAJOR(uversion) && \
	   SND_PROTOCOL_MINOR(kversion) != SND_PROTOCOL_MINOR(uversion) ) )

/*
 *  number of supported soundcards in one machine
 */

#define SND_CARDS			8

/*
 *  Various structures
 */

typedef struct snd_ctl_hw_info snd_ctl_hw_info_t;
typedef struct snd_ctl_switch snd_ctl_switch_t;
typedef struct snd_mixer_info snd_mixer_info_t;
typedef struct snd_mixer_channel_info snd_mixer_channel_info_t;
typedef struct snd_mixer_channel snd_mixer_channel_t;
typedef struct snd_mixer_channel_direction_info snd_mixer_channel_direction_info_t;
typedef struct snd_mixer_channel_direction snd_mixer_channel_direction_t;
typedef struct snd_mixer_switch snd_mixer_switch_t;
typedef struct snd_pcm_info snd_pcm_info_t;
typedef struct snd_pcm_playback_info snd_pcm_playback_info_t;
typedef struct snd_pcm_record_info snd_pcm_record_info_t;
typedef struct snd_pcm_switch snd_pcm_switch_t;
typedef struct snd_pcm_format snd_pcm_format_t;
typedef struct snd_pcm_playback_params snd_pcm_playback_params_t;
typedef struct snd_pcm_record_params snd_pcm_record_params_t;
typedef struct snd_pcm_playback_status snd_pcm_playback_status_t;
typedef struct snd_pcm_record_status snd_pcm_record_status_t;
typedef struct snd_pcm_loopback_header snd_pcm_loopback_header_t;
typedef struct snd_rawmidi_info snd_rawmidi_info_t;
typedef struct snd_rawmidi_output_info snd_rawmidi_output_info_t;
typedef struct snd_rawmidi_input_info snd_rawmidi_input_info_t;
typedef struct snd_rawmidi_switch snd_rawmidi_switch_t;
typedef struct snd_rawmidi_output_params snd_rawmidi_output_params_t;
typedef struct snd_rawmidi_input_params snd_rawmidi_input_params_t;
typedef struct snd_rawmidi_output_status snd_rawmidi_output_status_t;
typedef struct snd_rawmidi_input_status snd_rawmidi_input_status_t;
typedef struct snd_timer_general_info snd_timer_general_info_t;
typedef struct snd_timer_select snd_timer_select_t;
typedef struct snd_timer_info snd_timer_info_t;
typedef struct snd_timer_params snd_timer_params_t;
typedef struct snd_timer_status snd_timer_status_t;
typedef struct snd_timer_read snd_timer_read_t;

/****************************************************************************
 *                                                                          *
 *        Section for driver control interface - /dev/snd/control?          *
 *                                                                          *
 ****************************************************************************/

#define SND_CTL_VERSION			SND_PROTOCOL_VERSION( 1, 0, 0 )

#define SND_CTL_SW_TYPE_BOOLEAN		0	/* 0 or 1 (enable) */
#define SND_CTL_SW_TYPE_BYTE		1	/* 0 to 255 (low to high) */
#define SND_CTL_SW_TYPE_WORD		2	/* 0 to 65535 (low to high) */
#define SND_CTL_SW_TYPE_DWORD		3	/* 0 to 4294967296 (low to high) */
#define SND_CTL_SW_TYPE_USER		(~0)	/* user type */

#define SND_CTL_SW_JOYSTICK		"Joystick"
#define SND_CTL_SW_JOYSTICK_ADDRESS	"Joystick Address"
#define SND_CTL_SW_JOYSTICK_SPEED	"Joystick Speed Compensation"

struct snd_ctl_hw_info {
	unsigned int type;	/* type of card - look to SND_CARD_TYPE_XXXX */
	unsigned int pcmdevs;	/* count of PCM devices (0 to N) */
	unsigned int mixerdevs;	/* count of MIXER devices (0 to N) */
	unsigned int mididevs;	/* count of raw MIDI devices (0 to N) */
	unsigned int timerdevs;	/* count of timer devices (0 to N) */
	char id[16];		/* ID of card (user selectable) */
	char abbreviation[16];	/* Abbreviation for soundcard */
	char name[32];		/* Short name of soundcard */
	char longname[80];	/* name + info text about soundcard */
	unsigned int switches;	/* count of switches */
	unsigned char reserved[128];	/* reserved for future */
};

struct snd_ctl_switch {
	unsigned int switchn;	/* switch # (filled by application) */
	unsigned char name[32];	/* identification of switch (from driver) */
	unsigned int type;	/* look to SND_CTL_SW_TYPE_XXXX */
	unsigned int low;	/* low range value */
	unsigned int high;	/* high range value */
	union {
		unsigned int enable;		/* 0 = off, 1 = on */
		unsigned char data8[32];	/* 8-bit data */
		unsigned short data16[16];	/* 16-bit data */
		unsigned int data32[8];		/* 32-bit data */
	} value;
	unsigned char reserved[32];
};

#define SND_CTL_IOCTL_PVERSION		_IOR ( 'U', 0x00, int )
#define SND_CTL_IOCTL_HW_INFO		_IOR ( 'U', 0x01, struct snd_ctl_hw_info )
#define SND_CTL_IOCTL_SWITCHES		_IOR ( 'U', 0x02, int )
#define SND_CTL_IOCTL_SWITCH_READ	_IOR ( 'U', 0x03, struct snd_ctl_switch )
#define SND_CTL_IOCTL_SWITCH_WRITE	_IOWR( 'U', 0x03, struct snd_ctl_switch )
#define SND_CTL_IOCTL_MIXER_DEVICE	_IOWR( 'U', 0x10, int )
#define SND_CTL_IOCTL_MIXER_INFO	_IOR ( 'U', 0x10, snd_mixer_info_t )
#define SND_CTL_IOCTL_MIXER_SWITCHES	_IOR ( 'U', 0x11, int )
#define SND_CTL_IOCTL_MIXER_SWITCH_READ	_IOR ( 'U', 0x12, struct snd_ctl_switch )
#define SND_CTL_IOCTL_MIXER_SWITCH_WRITE _IOWR( 'U', 0x12, struct snd_ctl_switch )
#define SND_CTL_IOCTL_PCM_DEVICE	_IOWR( 'U', 0x20, int )
#define SND_CTL_IOCTL_PCM_INFO		_IOR ( 'U', 0x21, snd_pcm_info_t )
#define SND_CTL_IOCTL_PCM_PLAYBACK_INFO	_IOR ( 'U', 0x22, snd_pcm_playback_info_t )
#define SND_CTL_IOCTL_PCM_RECORD_INFO	_IOR ( 'U', 0x23, snd_pcm_record_info_t )
#define SND_CTL_IOCTL_PCM_PSWITCHES	_IOR ( 'U', 0x24, int )
#define SND_CTL_IOCTL_PCM_PSWITCH_READ  _IOR ( 'U', 0x25, snd_pcm_switch_t )
#define SND_CTL_IOCTL_PCM_PSWITCH_WRITE _IOWR( 'U', 0x25, snd_pcm_switch_t )
#define SND_CTL_IOCTL_PCM_RSWITCHES	_IOR ( 'U', 0x26, int )
#define SND_CTL_IOCTL_PCM_RSWITCH_READ  _IOR ( 'U', 0x27, snd_pcm_switch_t )
#define SND_CTL_IOCTL_PCM_RSWITCH_WRITE _IOWR( 'U', 0x27, snd_pcm_switch_t )
#define SND_CTL_IOCTL_RAWMIDI_DEVICE	_IOWR( 'U', 0x30, int )
#define SND_CTL_IOCTL_RAWMIDI_INFO	_IOR ( 'U', 0x31, snd_rawmidi_info_t )
#define SND_CTL_IOCTL_RAWMIDI_OUTPUT_INFO _IOR( 'U', 0x32, snd_rawmidi_output_info_t )
#define SND_CTL_IOCTL_RAWMIDI_INPUT_INFO _IOR( 'U', 0x33, snd_rawmidi_input_info_t )
#define SND_CTL_IOCTL_RAWMIDI_OSWITCHES	_IOR ( 'U', 0x34, int )
#define SND_CTL_IOCTL_RAWMIDI_OSWITCH_READ _IOR ( 'U', 0x35, snd_rawmidi_switch_t )
#define SND_CTL_IOCTL_RAWMIDI_OSWITCH_WRITE _IOWR ( 'U', 0x35, snd_rawmidi_switch_t )
#define SND_CTL_IOCTL_RAWMIDI_ISWITCHES	_IOR ( 'U', 0x36, int )
#define SND_CTL_IOCTL_RAWMIDI_ISWITCH_READ _IOR ( 'U', 0x37, snd_rawmidi_switch_t )
#define SND_CTL_IOCTL_RAWMIDI_ISWITCH_WRITE _IOWR ( 'U', 0x37, snd_rawmidi_switch_t )


/****************************************************************************
 *                                                                          *
 *                  MIXER interface - /dev/snd/mixer??                      *
 *                                                                          *
 ****************************************************************************/

#define SND_MIXER_VERSION		SND_PROTOCOL_VERSION( 1, 1, 1 )

					/* max 12 chars (with '\0') */
#define SND_MIXER_ID_MASTER		"Master"
#define SND_MIXER_ID_MASTER1		"Master 1"
#define SND_MIXER_ID_MASTERD		"Master D"
#define SND_MIXER_ID_MASTERD1		"Master D1"
#define SND_MIXER_ID_HEADPHONE		"Headphone"
#define SND_MIXER_ID_MASTER_MONO	"Master M"
#define SND_MIXER_ID_3D			"3D Wide"
#define SND_MIXER_ID_3D_VOLUME		"3D Volume"
#define SND_MIXER_ID_3D_CENTER		"3D Center"
#define SND_MIXER_ID_3D_SPACE		"3D Space"
#define SND_MIXER_ID_3D_DEPTH		"3D Depth"
#define SND_MIXER_ID_BASS		"Bass"
#define SND_MIXER_ID_TREBLE		"Treble"
#define SND_MIXER_ID_FADER		"Fader"
#define SND_MIXER_ID_SYNTHESIZER	"Synth"
#define SND_MIXER_ID_SYNTHESIZER1	"Synth 1"
#define SND_MIXER_ID_FM			"FM"
#define SND_MIXER_ID_EFFECT		"Effect"
#define SND_MIXER_ID_DSP		"DSP"
#define SND_MIXER_ID_PCM		"PCM"
#define SND_MIXER_ID_PCM1		"PCM 1"
#define SND_MIXER_ID_LINE		"Line-In"
#define SND_MIXER_ID_MIC		"MIC"
#define SND_MIXER_ID_CD			"CD"
#define SND_MIXER_ID_VIDEO		"Video"
#define SND_MIXER_ID_PHONE		"Phone"
#define SND_MIXER_ID_GAIN		"Record-Gain"
#define SND_MIXER_ID_MIC_GAIN		"Mic-Gain"
#define SND_MIXER_ID_IGAIN		"In-Gain"
#define SND_MIXER_ID_OGAIN		"Out-Gain"
#define SND_MIXER_ID_LOOPBACK		"Loopback"
#define SND_MIXER_ID_SPEAKER		"PC Speaker"
#define SND_MIXER_ID_MONO		"Mono"
#define SND_MIXER_ID_MONO1		"Mono 1"
#define SND_MIXER_ID_MONO2		"Mono 2"
#define SND_MIXER_ID_AUXA		"Aux A"
#define SND_MIXER_ID_AUXB		"Aux B"
#define SND_MIXER_ID_AUXC		"Aux C"

#define SND_MIXER_SW_TYPE_BOOLEAN	0	/* 0 or 1 (enable) */
#define SND_MIXER_SW_TYPE_BYTE		1	/* 0 to 255 (low to high) */
#define SND_MIXER_SW_TYPE_WORD		2	/* 0 to 65535 (low to high) */
#define SND_MIXER_SW_TYPE_DWORD		3	/* 0 to 4294967296 (low to high) */
#define SND_MIXER_SW_TYPE_USER		(~0)	/* user type */

					/* max 32 chars (with '\0') */
#define SND_MIXER_SW_LOUDNESS		"Loudness"	/* bass boost */
#define SND_MIXER_SW_SIM_STEREO		"Simulated Stereo Enhancement"
#define SND_MIXER_SW_3D			"3D Stereo Enhancement"
#define SND_MIXER_SW_MIC_GAIN		"MIC Gain"	/* Microphone Gain */
#define SND_MIXER_SW_MIC_AGC		"MIC Auto-Gain-Control"		/* Microphone Auto-Gain-Control */
#define SND_MIXER_SW_MIC_IMPEDANCE	"Change MIC Impedance"	/* change Microphone impedance */
#define SND_MIXER_SW_LINE_TO_OUTPUT	"Line In to Output"	/* reroute Line In to Output */
#define SND_MIXER_SW_IEC958OUT		"IEC-958 (S/PDIF) Output"	/* No comment */
#define SND_MIXER_SW_IEC958IN		"IEC-958 (S/PDIF) Input"	/* No comment */

/* Mixer general capabilities */

/* Channel general capabilities */
#define SND_MIXER_CINFO_CAP_OUTPUT	0x00000001	/* the channel have got output direction */
#define SND_MIXER_CINFO_CAP_INPUT	0x00000002	/* the channel have got input direction */
#define SND_MIXER_CINFO_CAP_EXTINPUT	0x00000010	/* external input channel */
#define SND_MIXER_CINFO_CAP_EFFECT	0x00000020	/* effect channel */

/* Channel direction dependant capabilities (bits 0-15) */
#define SND_MIXER_CINFO_DCAP_VOLUME	0x00000001	/* volume setup is available */
#define SND_MIXER_CINFO_DCAP_STEREO	0x00000002
#define SND_MIXER_CINFO_DCAP_MUTE	0x00000010	/* always set at this moment, driver emulates mute */
#define SND_MIXER_CINFO_DCAP_HWMUTE	0x00000020	/* direction supports hardware mute */
#define SND_MIXER_CINFO_DCAP_JOINMUTE	0x00000040	/* join mute is supported only (left and right channel doesn't have separate mute control) */
#define SND_MIXER_CINFO_DCAP_NOMUTEALL	0x00000080	/* at least one of this channel must be unmuted */
#define SND_MIXER_CINFO_DCAP_EXCLUSIVE	0x00000100	/* exclude all others */
#define SND_MIXER_CINFO_DCAP_ROUTE	0x00000200	/* route left to right and/or vice versa is supported */
#define SND_MIXER_CINFO_DCAP_SWAPROUTE	0x00000400	/* route is only swap */
#define SND_MIXER_CINFO_DCAP_DIGITAL	0x00001000	/* direction does digital (not analog) mixing */

/* Output direction only capabilities (bits 16-31) */
/* NONE currently */

/* Input direction only capabilities (bits 16-31) */
#define SND_MIXER_CINFO_DCAP_RECORDBYMUTE 0x00010000	/* we can record data even if output path is muted (to avoid loopback) */

/* Channel general flags */
/* NONE currently */

/* Channel direction dependant flags (bits 0-15) */
#define SND_MIXER_DFLG_FORCE		0x00000001	/* force set - don't use in user space - reserved for kernel */
#define SND_MIXER_DFLG_DECIBEL		0x00000002	/* if this bit is set, driver sets volume from dB variables (left_dB, right_dB) */
#define SND_MIXER_DFLG_MUTE_LEFT	0x00000010
#define SND_MIXER_DFLG_MUTE_RIGHT	0x00000020
#define SND_MIXER_DFLG_MUTE		0x00000030
#define SND_MIXER_DFLG_LTOR		0x00010000	/* route left to right */
#define SND_MIXER_DFLG_RTOL		0x00020000	/* route right to left */
#define SND_MIXER_DFLG_SWAP		0x00030000	/* swap left & right */

/* Output direction only flags (bits 16-31) */
/* NONE currently */

/* Output direction only flags (bits 16-31) */
/* NONE currently */

#define SND_MIXER_PARENT		0xffffffff	/* this is parent channel */

struct snd_mixer_info {
	unsigned int type;	/* type of soundcard - SND_CARD_TYPE_XXXX */
	unsigned int channels;	/* count of mixer devices */
	unsigned int caps;	/* some flags about this device (SND_MIXER_INFO_CAP_XXXX) */
	unsigned char id[32];	/* ID of this mixer */
	unsigned char name[80];	/* name of this device */
	unsigned int switches;	/* count of switches */
	char reserved[28];	/* reserved for future use */
};

struct snd_mixer_channel_info {
	unsigned int channel;	/* channel # (filled by application) */
	unsigned int parent;	/* parent channel # or SND_MIXER_PARENT */
	unsigned char name[12];	/* name of this device */
	unsigned int caps;	/* some flags about this device (SND_MIXER_CINFO_XXXX) */
	unsigned char reserved[20];
};

struct snd_mixer_channel {
	unsigned int channel;	/* channel # (filled by application) */
	unsigned int flags;	/* some flags to read/write (SND_MIXER_FLG_XXXX) */
	unsigned char reserved[16];
};

struct snd_mixer_channel_direction_info {
	unsigned int channel;	/* channel # (filled by application) */
	unsigned int parent;	/* parent channel # or SND_MIXER_PARENT */
	unsigned int caps;	/* some flags about this device (SND_MIXER_CINFO_XXXX) */
	int min;		/* min. value when exact mode (or always 0) */
	int max;		/* max. value when exact mode (or always 100) */
	int min_dB;		/* minimum decibel value (*100) */
	int max_dB;		/* maximum decibel value (*100) */
	unsigned char reserved[16];
};

struct snd_mixer_channel_direction {
	unsigned int channel;	/* channel # (filled by application) */
	unsigned int flags;	/* some flags to read/write (SND_MIXER_DFLG_XXXX) */
	int left;		/* min - max when exact mode (or 0 - 100) */
	int right;		/* min - max when exact mode (or 0 - 100) */
	int left_dB;		/* dB * 100 */
	int right_dB;		/* dB * 100 */
	unsigned char reserved[16];
};

struct snd_mixer_switch {
	unsigned int switchn;	/* switch # (filled by application) */
	unsigned char name[32];	/* identification of switch (from driver) */
	unsigned int type;	/* look to SND_MIXER_SW_TYPE_XXXX */
	unsigned int low;	/* low range value */
	unsigned int high;	/* high range value */
	union {
		unsigned int enable;		/* 0 = off, 1 = on */
		unsigned char data8[32];	/* 8-bit data */
		unsigned short data16[16];	/* 16-bit data */
		unsigned int data32[8];		/* 32-bit data */
	} value;
	unsigned char reserved[32];
};

#define SND_MIXER_IOCTL_PVERSION	_IOR ( 'R', 0x00, int )
#define SND_MIXER_IOCTL_CHANNELS	_IOR ( 'R', 0x01, int )
#define SND_MIXER_IOCTL_INFO		_IOR ( 'R', 0x02, struct snd_mixer_info )
#define SND_MIXER_IOCTL_EXACT		_IOWR( 'R', 0x03, int )
#define SND_MIXER_IOCTL_CHANNEL_INFO	_IOR ( 'R', 0x04, struct snd_mixer_channel_info )
#define SND_MIXER_IOCTL_CHANNEL_READ	_IOR ( 'R', 0x05, struct snd_mixer_channel )
#define SND_MIXER_IOCTL_CHANNEL_WRITE	_IOWR( 'R', 0x05, struct snd_mixer_channel )
#define SND_MIXER_IOCTL_CHANNEL_OINFO	_IOR ( 'R', 0x06, struct snd_mixer_channel_direction_info )
#define SND_MIXER_IOCTL_CHANNEL_OREAD	_IOR ( 'R', 0x07, struct snd_mixer_channel_direction )
#define SND_MIXER_IOCTL_CHANNEL_OWRITE	_IOWR( 'R', 0x07, struct snd_mixer_channel_direction )
#define SND_MIXER_IOCTL_CHANNEL_IINFO	_IOR ( 'R', 0x08, struct snd_mixer_channel_direction_info )
#define SND_MIXER_IOCTL_CHANNEL_IREAD	_IOR ( 'R', 0x09, struct snd_mixer_channel_direction )
#define SND_MIXER_IOCTL_CHANNEL_IWRITE	_IOWR( 'R', 0x09, struct snd_mixer_channel_direction )
#define SND_MIXER_IOCTL_SWITCHES	_IOR ( 'R', 0x0a, int )
#define SND_MIXER_IOCTL_SWITCH_READ	_IOR ( 'R', 0x0b, struct snd_mixer_switch )
#define SND_MIXER_IOCTL_SWITCH_WRITE	_IOWR( 'R', 0x0b, struct snd_mixer_switch )

/* the commands of the read interface */
#define SND_MIXER_CHANGED		0
#define SND_MIXER_OUTPUT_CHANGED	1
#define SND_MIXER_INPUT_CHANGED		2
#define SND_MIXER_SWITCH_CHANGED	3

/*
 *  Obsolete interface compatible with Open Sound System API
 */

#ifdef __SND_OSS_COMPAT__

#define SND_MIXER_OSS_CAP_EXCL_INPUT	0x00000001	/* only one recording source at moment */

#define SND_MIXER_OSS_DEVS	25
#define SND_MIXER_OSS_VOLUME	0
#define SND_MIXER_OSS_BASS	1
#define SND_MIXER_OSS_TREBLE	2
#define SND_MIXER_OSS_SYNTH	3
#define SND_MIXER_OSS_PCM	4
#define SND_MIXER_OSS_SPEAKER	5
#define SND_MIXER_OSS_LINE	6
#define SND_MIXER_OSS_MIC	7
#define SND_MIXER_OSS_CD	8
#define SND_MIXER_OSS_IMIX	9	/* recording monitor */
#define SND_MIXER_OSS_ALTPCM	10
#define SND_MIXER_OSS_RECLEV	11	/* recording level */
#define SND_MIXER_OSS_IGAIN	12	/* input gain */
#define SND_MIXER_OSS_OGAIN	13	/* output gain */
#define SND_MIXER_OSS_LINE1	14
#define SND_MIXER_OSS_LINE2	15
#define SND_MIXER_OSS_LINE3	16
#define SND_MIXER_OSS_DIGITAL1	17
#define SND_MIXER_OSS_DIGITAL2	18
#define SND_MIXER_OSS_DIGITAL3	19
#define SND_MIXER_OSS_PHONEIN	20
#define SND_MIXER_OSS_PHONEOUT	21
#define SND_MIXER_OSS_VIDEO	22
#define SND_MIXER_OSS_RADIO	23
#define SND_MIXER_OSS_MONITOR	24
#define SND_MIXER_OSS_UNKNOWN	(32+1)

struct snd_oss_mixer_info {
	char id[16];
	char name[32];
	int modify_counter;
	int fillers[10];
};

struct snd_oss_mixer_info_obsolete {
	char id[16];
	char name[32];
};

#define SND_MIXER_OSS_SET_RECSRC _IOWR( 'M', 255, int )
#define SND_MIXER_OSS_RECSRC	_IOR ( 'M', 255, int )
#define SND_MIXER_OSS_DEVMASK	_IOR ( 'M', 254, int )
#define SND_MIXER_OSS_RECMASK	_IOR ( 'M', 253, int )
#define SND_MIXER_OSS_CAPS	_IOR ( 'M', 252, int )
#define SND_MIXER_OSS_STEREODEVS _IOR ( 'M', 251, int )
#define SND_MIXER_OSS_INFO      _IOR ( 'M', 101, struct snd_oss_mixer_info )
#define SND_MIXER_OSS_OLD_INFO	_IOR ( 'M', 101, struct snd_oss_mixer_info_obsolete )
#define SND_OSS_GETVERSION	_IOR ( 'M', 118, int )

#endif				/* __SND_OSS_COMPAT__ */

/*****************************************************************************
 *                                                                           *
 *             Digital Audio (PCM) interface - /dev/snd/pcm??                *
 *                                                                           *
 *****************************************************************************/

#define SND_PCM_VERSION			SND_PROTOCOL_VERSION( 1, 0, 1 )

#define SND_PCM_SFMT_MU_LAW		0
#define SND_PCM_SFMT_A_LAW		1
#define SND_PCM_SFMT_IMA_ADPCM		2
#define SND_PCM_SFMT_U8			3
#define SND_PCM_SFMT_S16_LE		4
#define SND_PCM_SFMT_S16_BE		5
#define SND_PCM_SFMT_S8			6
#define SND_PCM_SFMT_U16_LE		7
#define SND_PCM_SFMT_U16_BE		8
#define SND_PCM_SFMT_MPEG		9
#define SND_PCM_SFMT_GSM		10
#define SND_PCM_SFMT_S24_LE		11
#define SND_PCM_SFMT_S24_BE		12
#define SND_PCM_SFMT_U24_LE		13
#define SND_PCM_SFMT_U24_BE		14
#define SND_PCM_SFMT_S32_LE		15
#define SND_PCM_SFMT_S32_BE		16
#define SND_PCM_SFMT_U32_LE		17
#define SND_PCM_SFMT_U32_BE		18
#define SND_PCM_SFMT_FLOAT		19	/* 4-byte float, need specification!! */
#define SND_PCM_SFMT_SPECIAL		31

#define SND_PCM_FMT_QUERY		0
#define SND_PCM_FMT_MU_LAW		(1 << SND_PCM_SFMT_MU_LAW)
#define SND_PCM_FMT_A_LAW		(1 << SND_PCM_SFMT_A_LAW)
#define SND_PCM_FMT_IMA_ADPCM		(1 << SND_PCM_SFMT_IMA_ADPCM)
#define SND_PCM_FMT_U8			(1 << SND_PCM_SFMT_U8)
#define SND_PCM_FMT_S16_LE		(1 << SND_PCM_SFMT_S16_LE)
#define SND_PCM_FMT_S16_BE		(1 << SND_PCM_SFMT_S16_BE)
#define SND_PCM_FMT_S8			(1 << SND_PCM_SFMT_S8)
#define SND_PCM_FMT_U16_LE		(1 << SND_PCM_SFMT_U16_LE)
#define SND_PCM_FMT_U16_BE		(1 << SND_PCM_SFMT_U16_BE)
#define SND_PCM_FMT_MPEG		(1 << SND_PCM_SFMT_MPEG)
#define SND_PCM_FMT_GSM			(1 << SND_PCM_SFMT_GSM)
#define SND_PCM_FMT_S24_LE		(1 << SND_PCM_SFMT_S32_LE)
#define SND_PCM_FMT_S24_BE		(1 << SND_PCM_SFMT_S32_BE)
#define SND_PCM_FMT_U24_LE		(1 << SND_PCM_SFMT_U32_LE)
#define SND_PCM_FMT_U24_BE		(1 << SND_PCM_SFMT_U32_BE)
#define SND_PCM_FMT_S32_LE		(1 << SND_PCM_SFMT_S32_LE)
#define SND_PCM_FMT_S32_BE		(1 << SND_PCM_SFMT_S32_BE)
#define SND_PCM_FMT_U32_LE		(1 << SND_PCM_SFMT_U32_LE)
#define SND_PCM_FMT_U32_BE		(1 << SND_PCM_SFMT_U32_BE)
#define SND_PCM_FMT_FLOAT		(1 << SND_PCM_SFMT_FLOAT)
#define SND_PCM_FMT_SPECIAL		(1 << SND_PCM_SFMT_SPECIAL)

#define SND_PCM_INFO_CODEC		0x00000001
#define SND_PCM_INFO_DSP		SND_PCM_INFO_CODEC
#define SND_PCM_INFO_MMAP		0x00000002	/* for compatibility with OSS, this flag shouldn't be used with native applications */
#define SND_PCM_INFO_PLAYBACK		0x00000100
#define SND_PCM_INFO_RECORD		0x00000200
#define SND_PCM_INFO_DUPLEX		0x00000400
#define SND_PCM_INFO_DUPLEX_LIMIT	0x00000800	/* rate for playback & record channels must be same!!! */
#define SND_PCM_INFO_DUPLEX_MONO	0x00001000	/* in duplex mode - only mono (one channel) is supported */

#define SND_PCM_PINFO_BATCH		0x00000001	/* double buffering */
#define SND_PCM_PINFO_8BITONLY		0x00000002	/* hardware supports only 8-bit samples, but driver does conversions from 16-bit to 8-bit */
#define SND_PCM_PINFO_16BITONLY		0x00000004	/* hardware supports only 16-bit samples, but driver does conversions from 8-bit to 16-bit */

#define SND_PCM_RINFO_BATCH		0x00000001	/* double buffering */
#define SND_PCM_RINFO_8BITONLY		0x00000002	/* hardware supports only 8-bit samples, but driver does conversions from 16-bit to 8-bit */
#define SND_PCM_RINFO_16BITONLY		0x00000004	/* hardware supports only 16-bit samples, but driver does conversions from 8-bit to 16-bit */
#define SND_PCM_RINFO_OVERRANGE		0x00010000	/* hardware supports ADC overrange detection */

#define SND_PCM_MASK_PLAYBACK		0x0001
#define SND_PCM_MASK_RECORD		0x0002
#define SND_PCM_MASK_DUPLEX		(SND_PCM_MASK_PLAYBACK|SND_PCM_MASK_RECORD)
#define SND_PCM_MASK_BOTH		SND_PCM_MASK_DUPLEX

#define SND_PCM_SW_TYPE_BOOLEAN		0	/* 0 or 1 (enable) */
#define SND_PCM_SW_TYPE_BYTE		1	/* 0 to 255 (low to high) */
#define SND_PCM_SW_TYPE_WORD		2	/* 0 to 65535 (low to high) */
#define SND_PCM_SW_TYPE_DWORD		3	/* 0 to 4294967296 (low to high) */
#define SND_PCM_SW_TYPE_USER		(~0)	/* user type */

/*
 * Things to know:
 *   1) Real fragment size can be aligned by driver if hardware needs.
 *      Current fragment value can be taken from status structure.
 *   2) If fragments_max in playback_params structure is -N, value
 *      means total fragments - N.
 */

struct snd_pcm_info {
	unsigned int type;		/* soundcard type */
	unsigned int flags;		/* see to SND_PCM_INFO_XXXX */
	unsigned char id[32];		/* ID of this PCM device */
	unsigned char name[80];		/* name of this device */
	unsigned char reserved[64];	/* reserved for future... */
};

struct snd_pcm_playback_info {
	unsigned int flags;		/* see to SND_PCM_PINFO_XXXX */
	unsigned int formats;		/* supported formats */
	unsigned int min_rate;		/* min rate (in Hz) */
	unsigned int max_rate;		/* max rate (in Hz) */
	unsigned int min_channels;	/* min channels (probably always 1) */
	unsigned int max_channels;	/* max channels */
	unsigned int buffer_size;	/* playback buffer size */
	unsigned int min_fragment_size;	/* min fragment size in bytes */
	unsigned int max_fragment_size;	/* max fragment size in bytes */
	unsigned int fragment_align;	/* align fragment value */
	unsigned int hw_formats;	/* formats supported by hardware */
	unsigned int switches;		/* count of switches */
	unsigned char reserved[56];	/* reserved for future... */
};

struct snd_pcm_record_info {
	unsigned int flags;		/* see to SND_PCM_RINFO_XXXX */
	unsigned int formats;		/* supported formats */
	unsigned int min_rate;		/* min rate (in Hz) */
	unsigned int max_rate;		/* max rate (in Hz) */
	unsigned int min_channels;	/* min channels (probably always 1) */
	unsigned int max_channels;	/* max channels */
	unsigned int buffer_size;	/* record buffer size */
	unsigned int min_fragment_size;	/* min fragment size in bytes */
	unsigned int max_fragment_size;	/* max fragment size in bytes */
	unsigned int fragment_align;	/* align fragment value */
	unsigned int hw_formats;	/* formats supported by hardware */
	unsigned int switches;		/* count of switches */
	unsigned char reserved[56];	/* reserved for future... */
};

struct snd_pcm_switch {
	unsigned int switchn;	/* switch # (filled by application) */
	unsigned char name[32];	/* identification of switch (from driver) */
	unsigned int type;	/* look to SND_MIXER_SW_TYPE_XXXX */
	unsigned int low;	/* low range value */
	unsigned int high;	/* high range value */
	union {
		unsigned int enable;		/* 0 = off, 1 = on */
		unsigned char data8[32];	/* 8-bit data */
		unsigned short data16[16];	/* 16-bit data */
		unsigned int data32[8];		/* 32-bit data */
	} value;
	unsigned char reserved[32];
};

struct snd_pcm_format {
	unsigned int format;		/* SND_PCM_SFMT_XXXX */
	unsigned int rate;		/* rate in Hz */
	unsigned int channels;		/* channels (voices) */
	unsigned int special;		/* special description of format */
	unsigned char reserved[12];
};

struct snd_pcm_playback_params {
	int fragment_size;		/* requested size of fragment in bytes */
	int fragments_max;		/* maximum number of fragments in queue for wakeup */
	int fragments_room;		/* minimum number of fragments writeable for wakeup */
	unsigned char reserved[16];	/* must be filled with zero */
};

struct snd_pcm_record_params {
	int fragment_size;		/* requested size of fragment in bytes */
	int fragments_min;		/* minimum number of filled fragments for wakeup */
	unsigned char reserved[16];	/* must be filled with zero */
};

struct snd_pcm_playback_status {
	unsigned int rate;	/* real used rate */
	int fragments;		/* allocated fragments */
	int fragment_size;	/* current fragment size in bytes */
	int count;		/* number of bytes writeable without blocking */
	int queue;		/* number of bytes in queue */
	int underrun;		/* count of underruns from last status */
	struct timeval time;	/* time the next write is going to play */
	struct timeval stime;	/* time when playback was started */
	int scount;		/* number of bytes processed from playback start (last underrun) */
	unsigned char reserved[16];
};

struct snd_pcm_record_status {
	unsigned int rate;	/* real used rate */
	int fragments;		/* allocated fragments */
	int fragment_size;	/* current fragment size in bytes */
	int count;		/* number of bytes readable without blocking */
	int free;		/* bytes in buffer still free */
	int overrun;		/* count of overruns from last status */
	struct timeval time;	/* time the next read was taken */
	struct timeval stime;	/* time when record was started */
	int scount;		/* number of bytes processed from record start */
	int overrange;		/* ADC overrange detection */
	unsigned char reserved[12];
};

#define SND_PCM_IOCTL_PVERSION		_IOR ( 'A', 0x00, int )
#define SND_PCM_IOCTL_INFO		_IOR ( 'A', 0x01, struct snd_pcm_info )
#define SND_PCM_IOCTL_PLAYBACK_INFO	_IOR ( 'A', 0x02, struct snd_pcm_playback_info )
#define SND_PCM_IOCTL_RECORD_INFO	_IOR ( 'A', 0x03, struct snd_pcm_record_info )
#define SND_PCM_IOCTL_PSWITCHES		_IOR ( 'A', 0x04, int )
#define SND_PCM_IOCTL_PSWITCH_READ	_IOR ( 'A', 0x05, struct snd_pcm_switch )
#define SND_PCM_IOCTL_PSWITCH_WRITE	_IOWR( 'A', 0x05, struct snd_pcm_switch )
#define SND_PCM_IOCTL_RSWITCHES		_IOR ( 'A', 0x06, int )
#define SND_PCM_IOCTL_RSWITCH_READ	_IOR ( 'A', 0x07, struct snd_pcm_switch )
#define SND_PCM_IOCTL_RSWITCH_WRITE	_IOWR( 'A', 0x07, struct snd_pcm_switch )
#define SND_PCM_IOCTL_PLAYBACK_FORMAT	_IOWR( 'A', 0x10, struct snd_pcm_format )
#define SND_PCM_IOCTL_RECORD_FORMAT	_IOWR( 'A', 0x11, struct snd_pcm_format )
#define SND_PCM_IOCTL_PLAYBACK_PARAMS	_IOWR( 'A', 0x12, struct snd_pcm_playback_params )
#define SND_PCM_IOCTL_RECORD_PARAMS	_IOWR( 'A', 0x13, struct snd_pcm_record_params )
#define SND_PCM_IOCTL_PLAYBACK_STATUS	_IOR ( 'A', 0x20, struct snd_pcm_playback_status )
#define SND_PCM_IOCTL_RECORD_STATUS	_IOR ( 'A', 0x21, struct snd_pcm_record_status )
#define SND_PCM_IOCTL_DRAIN_PLAYBACK	_IO  ( 'A', 0x30 )
#define SND_PCM_IOCTL_FLUSH_PLAYBACK	_IO  ( 'A', 0x31 )
#define SND_PCM_IOCTL_FLUSH_RECORD	_IO  ( 'A', 0x32 )
#define SND_PCM_IOCTL_PLAYBACK_PAUSE	_IOWR( 'A', 0x33, int )
#define SND_PCM_IOCTL_PLAYBACK_TIME	_IOWR( 'A', 0x40, int )
#define SND_PCM_IOCTL_RECORD_TIME	_IOWR( 'A', 0x41, int )

/*
 *  Loopback interface
 */

#define SND_PCM_LB_VERSION		SND_PROTOCOL_VERSION( 1, 0, 0 )

#define SND_PCM_LB_STREAM_MODE_RAW	0
#define SND_PCM_LB_STREAM_MODE_PACKET	1

#define SND_PCM_LB_TYPE_DATA		0	/* sample data */
#define SND_PCM_LB_TYPE_FORMAT		1	/* format change */

struct snd_pcm_loopback_header {
	unsigned int size;		/* block size */
	unsigned int type;		/* block type (SND_PCM_LB_TYPE_*) */
};

#define SND_PCM_LB_IOCTL_PVERSION	_IOR ( 'L', 0x00, int )
#define SND_PCM_LB_IOCTL_STREAM_MODE	_IOWR( 'L', 0x01, int )
#define SND_PCM_LB_IOCTL_FORMAT		_IOR ( 'L', 0x02, struct snd_pcm_format )

/*
 *  Obsolete interface compatible with Open Sound System API
 */

#ifdef __SND_OSS_COMPAT__

#define SND_PCM_ENABLE_RECORD		0x00000001
#define SND_PCM_ENABLE_PLAYBACK		0x00000002

#define SND_PCM_CAP_REVISION		0x000000ff
#define SND_PCM_CAP_DUPLEX		0x00000100
#define SND_PCM_CAP_REALTIME		0x00000200
#define SND_PCM_CAP_BATCH		0x00000400
#define SND_PCM_CAP_COPROC		0x00000800
#define SND_PCM_CAP_TRIGGER		0x00001000
#define SND_PCM_CAP_MMAP		0x00002000

#define SND_PCM_AFP_NORMAL		0
#define SND_PCM_AFP_NETWORK		1
#define SND_PCM_AFP_CPUINTENS		2

struct snd_pcm_buffer_info {
	int fragments;		/* # of available fragments (partially used ones not counted) */
	int fragstotal;		/* Total # of fragments allocated */
	int fragsize;		/* Size of a fragment in bytes */
	int bytes;		/* Available space in bytes (includes partially used fragments) */
};

struct snd_pcm_count_info {
	int bytes;		/* Total # of bytes processed */
	int blocks;		/* # of fragment transitions since last time */
	int ptr;		/* Current DMA pointer value */
};

struct snd_pcm_buffer_description {
	unsigned char *buffer;
	int size;
};

#define SND_PCM_IOCTL_OSS_RESET		_IO  ( 'P', 0 )
#define SND_PCM_IOCTL_OSS_SYNC		_IO  ( 'P', 1 )
#define SND_PCM_IOCTL_OSS_RATE		_IOWR( 'P', 2, int )
#define SND_PCM_IOCTL_OSS_GETRATE	_IOR ( 'P', 2, int )
#define SND_PCM_IOCTL_OSS_STEREO	_IOWR( 'P', 3, int )
#define SND_PCM_IOCTL_OSS_GETBLKSIZE	_IOWR( 'P', 4, int )
#define SND_PCM_IOCTL_OSS_FORMAT	_IOWR( 'P', 5, int )
#define SND_PCM_IOCTL_OSS_GETFORMAT	_IOR ( 'P', 5, int )
#define SND_PCM_IOCTL_OSS_CHANNELS	_IOWR( 'P', 6, int )
#define SND_PCM_IOCTL_OSS_GETCHANNELS	_IOR ( 'P', 6, int )
#define SND_PCM_IOCTL_OSS_FILTER	_IOWR( 'P', 7, int )
#define SND_PCM_IOCTL_OSS_GETFILTER	_IOR ( 'P', 7, int )
#define SND_PCM_IOCTL_OSS_POST		_IO  ( 'P', 8 )
#define SND_PCM_IOCTL_OSS_SUBDIVIDE	_IOWR( 'P', 9, int )
#define SND_PCM_IOCTL_OSS_SETFRAGMENT	_IOWR( 'P', 10, int )
#define SND_PCM_IOCTL_OSS_GETFORMATS	_IOR ( 'P', 11, int )
#define SND_PCM_IOCTL_OSS_GETPBKSPACE	_IOR ( 'P', 12, struct snd_pcm_buffer_info )
#define SND_PCM_IOCTL_OSS_GETRECSPACE	_IOR ( 'P', 13, struct snd_pcm_buffer_info )
#define SND_PCM_IOCTL_OSS_NONBLOCK	_IO  ( 'P', 14 )
#define SND_PCM_IOCTL_OSS_GETCAPS	_IOR ( 'P', 15, int )
#define SND_PCM_IOCTL_OSS_GETTRIGGER	_IOR ( 'P', 16, int )
#define SND_PCM_IOCTL_OSS_SETTRIGGER	_IOW ( 'P', 16, int )
#define SND_PCM_IOCTL_OSS_GETRECPTR	_IOR ( 'P', 17, struct snd_pcm_count_info )
#define SND_PCM_IOCTL_OSS_GETPBKPTR	_IOR ( 'P', 18, struct snd_pcm_count_info )
#define SND_PCM_IOCTL_OSS_MAPRECBUFFER	_IOR ( 'P', 19, struct snd_pcm_buffer_description )
#define SND_PCM_IOCTL_OSS_MAPPBKBUFFER	_IOR ( 'P', 20, struct snd_pcm_buffer_description )
#define SND_PCM_IOCTL_OSS_SYNCRO	_IO  ( 'P', 21 )
#define SND_PCM_IOCTL_OSS_DUPLEX	_IO  ( 'P', 22 )
#define SND_PCM_IOCTL_OSS_GETODELAY	_IOR ( 'P', 23, int )
#define SND_PCM_IOCTL_OSS_PROFILE	_IOW ( 'P', 23, int )
#define SND_PCM_IOCTL_OSS_MASK		_IOW ( 'X', 0, int )

#endif				/* __SND_OSS_COMPAT__ */

/*****************************************************************************
 *                                                                           *
 *                            MIDI v1.0 interface                            *
 *                                                                           *
 *****************************************************************************/

#define SND_MIDI_CHANNELS		16
#define SND_MIDI_GM_DRUM_CHANNEL	(10-1)

/*
 *  MIDI commands
 */

#define SND_MCMD_NOTE_OFF		0x80
#define SND_MCMD_NOTE_ON		0x90
#define SND_MCMD_NOTE_PRESSURE		0xa0
#define SND_MCMD_CONTROL		0xb0
#define SND_MCMD_PGM_CHANGE		0xc0
#define SND_MCMD_CHANNEL_PRESSURE	0xd0
#define SND_MCMD_BENDER			0xe0

#define SND_MCMD_COMMON_SYSEX		0xf0
#define SND_MCMD_COMMON_MTC_QUARTER	0xf1
#define SND_MCMD_COMMON_SONG_POS	0xf2
#define SND_MCMD_COMMON_SONG_SELECT	0xf3
#define SND_MCMD_COMMON_TUNE_REQUEST	0xf6
#define SND_MCMD_COMMON_SYSEX_END	0xf7
#define SND_MCMD_COMMON_CLOCK		0xf8
#define SND_MCMD_COMMON_START		0xfa
#define SND_MCMD_COMMON_CONTINUE	0xfb
#define SND_MCMD_COMMON_STOP		0xfc
#define SND_MCMD_COMMON_SENSING		0xfe
#define SND_MCMD_COMMON_RESET		0xff

/*
 *  MIDI controllers
 */

#define SND_MCTL_MSB_BANK		0x00
#define SND_MCTL_MSB_MODWHEEL         	0x01
#define SND_MCTL_MSB_BREATH           	0x02
#define SND_MCTL_MSB_FOOT             	0x04
#define SND_MCTL_MSB_PORTNAMENTO_TIME 	0x05
#define SND_MCTL_MSB_DATA_ENTRY		0x06
#define SND_MCTL_MSB_MAIN_VOLUME      	0x07
#define SND_MCTL_MSB_BALANCE          	0x08
#define SND_MCTL_MSB_PAN              	0x0a
#define SND_MCTL_MSB_EXPRESSION       	0x0b
#define SND_MCTL_MSB_EFFECT1		0x0c
#define SND_MCTL_MSB_EFFECT2		0x0d
#define SND_MCTL_MSB_GENERAL_PURPOSE1 	0x10
#define SND_MCTL_MSB_GENERAL_PURPOSE2 	0x11
#define SND_MCTL_MSB_GENERAL_PURPOSE3 	0x12
#define SND_MCTL_MSB_GENERAL_PURPOSE4 	0x13
#define SND_MCTL_LSB_BANK		0x20
#define SND_MCTL_LSB_MODWHEEL        	0x21
#define SND_MCTL_LSB_BREATH           	0x22
#define SND_MCTL_LSB_FOOT             	0x24
#define SND_MCTL_LSB_PORTNAMENTO_TIME 	0x25
#define SND_MCTL_LSB_DATA_ENTRY		0x26
#define SND_MCTL_LSB_MAIN_VOLUME      	0x27
#define SND_MCTL_LSB_BALANCE          	0x28
#define SND_MCTL_LSB_PAN              	0x2a
#define SND_MCTL_LSB_EXPRESSION       	0x2b
#define SND_MCTL_LSB_EFFECT1		0x2c
#define SND_MCTL_LSB_EFFECT2		0x2d
#define SND_MCTL_LSB_GENERAL_PURPOSE1 	0x30
#define SND_MCTL_LSB_GENERAL_PURPOSE2 	0x31
#define SND_MCTL_LSB_GENERAL_PURPOSE3 	0x32
#define SND_MCTL_LSB_GENERAL_PURPOSE4 	0x33
#define SND_MCTL_SUSTAIN              	0x40
#define SND_MCTL_PORNAMENTO           	0x41
#define SND_MCTL_SOSTENUTO            	0x42
#define SND_MCTL_SOFT_PEDAL           	0x43
#define SND_MCTL_LEGATO_FOOTSWITCH	0x44
#define SND_MCTL_HOLD2                	0x45
#define SND_MCTL_SC1_SOUND_VARIATION	0x46
#define SND_MCTL_SC2_TIMBRE		0x47
#define SND_MCTL_SC3_RELEASE_TIME	0x48
#define SND_MCTL_SC4_ATTACK_TIME	0x49
#define SND_MCTL_SC5_BRIGHTNESS		0x4a
#define SND_MCTL_SC6			0x4b
#define SND_MCTL_SC7			0x4c
#define SND_MCTL_SC8			0x4d
#define SND_MCTL_SC9			0x4e
#define SND_MCTL_SC10			0x4f
#define SND_MCTL_GENERAL_PURPOSE5     	0x50
#define SND_MCTL_GENERAL_PURPOSE6     	0x51
#define SND_MCTL_GENERAL_PURPOSE7     	0x52
#define SND_MCTL_GENERAL_PURPOSE8     	0x53
#define SND_MCTL_PORNAMENTO_CONTROL	0x54
#define SND_MCTL_E1_REVERB_DEPTH	0x5b
#define SND_MCTL_E2_TREMOLO_DEPTH	0x5c
#define SND_MCTL_E3_CHORUS_DEPTH	0x5d
#define SND_MCTL_E4_DETUNE_DEPTH	0x5e
#define SND_MCTL_E5_PHASER_DEPTH	0x5f
#define SND_MCTL_DATA_INCREMENT       	0x60
#define SND_MCTL_DATA_DECREMENT       	0x61
#define SND_MCTL_NONREG_PARM_NUM_LSB  	0x62
#define SND_MCTL_NONREG_PARM_NUM_MSB  	0x63
#define SND_MCTL_REGIST_PARM_NUM_LSB  	0x64
#define SND_MCTL_REGIST_PARM_NUM_MSB	0x65
#define SND_MCTL_ALL_SOUNDS_OFF		0x78
#define SND_MCTL_RESET_CONTROLLERS	0x79
#define SND_MCTL_LOCAL_CONTROL_SWITCH	0x7a
#define SND_MCTL_ALL_NOTES_OFF		0x7b
#define SND_MCTL_OMNI_OFF		0x7c
#define SND_MCTL_OMNI_ON		0x7d
#define SND_MCTL_MONO1			0x7e
#define SND_MCTL_MONO2			0x7f

/*
 *  Raw MIDI section - /dev/snd/midi??
 */

#define SND_RAWMIDI_VERSION		SND_PROTOCOL_VERSION( 1, 0, 0 )

#define SND_RAWMIDI_INFO_OUTPUT		0x00000001
#define SND_RAWMIDI_INFO_INPUT		0x00000002
#define SND_RAWMIDI_INFO_DUPLEX		0x00000004

#define SND_RAWMIDI_SW_TYPE_BOOLEAN	0	/* 0 or 1 (enable) */
#define SND_RAWMIDI_SW_TYPE_BYTE	1	/* 0 to 255 (low to high) */
#define SND_RAWMIDI_SW_TYPE_WORD	2	/* 0 to 65535 (low to high) */
#define SND_RAWMIDI_SW_TYPE_DWORD	3	/* 0 to 4294967296 (low to high) */
#define SND_RAWMIDI_SW_TYPE_USER	(~0)	/* user type */

struct snd_rawmidi_info {
	unsigned int type;		/* soundcard type */
	unsigned int flags;		/* SND_RAWMIDI_INFO_XXXX */
	unsigned char id[32];		/* ID of this raw midi device */
	unsigned char name[80];		/* name of this raw midi device */
	unsigned char reserved[64];	/* reserved for future use */
};

struct snd_rawmidi_output_info {
	unsigned int switches;	/* count of switches */
	unsigned char reserved[64];
};

struct snd_rawmidi_input_info {
	unsigned int switches;	/* count of switches */
	unsigned char reserved[64];
};

struct snd_rawmidi_switch {
	unsigned int switchn;	/* switch # (filled by application) */
	unsigned char name[32];	/* identification of switch (from driver) */
	unsigned int type;	/* look to SND_MIXER_SW_TYPE_XXXX */
	unsigned int low;	/* low range value */
	unsigned int high;	/* high range value */
	union {
		unsigned int enable;		/* 0 = off, 1 = on */
		unsigned char data8[32];	/* 8-bit data */
		unsigned short data16[16];	/* 16-bit data */
		unsigned int data32[8];		/* 32-bit data */
	} value;
	unsigned char reserved[32];
};

struct snd_rawmidi_output_params {
	int size;		/* requested queue size in bytes */
	int max;		/* maximum number of bytes in queue for wakeup */
	int room;		/* minumum number of bytes writeable for wakeup */
	unsigned char reserved[16];	/* reserved for future use */
};

struct snd_rawmidi_input_params {
	int size;		/* requested queue size in bytes */
	int min;		/* minimum number of bytes fragments for wakeup */
	unsigned char reserved[16];	/* reserved for future use */
};

struct snd_rawmidi_output_status {
	int size;		/* real queue size */
	int count;		/* number of bytes writeable without blocking */
	int queue;		/* number of bytes in queue */
	unsigned char reserved[16];	/* reserved for future use */
};

struct snd_rawmidi_input_status {
	int size;		/* real queue size */
	int count;		/* number of bytes readable without blocking */
	int free;		/* bytes in buffer still free */
	int overrun;		/* count of overruns from last status (in bytes) */
	unsigned char reserved[16];	/* reserved for future use */
};

#define SND_RAWMIDI_IOCTL_PVERSION	_IOR ( 'W', 0x00, int )
#define SND_RAWMIDI_IOCTL_INFO		_IOR ( 'W', 0x01, struct snd_rawmidi_info )
#define SND_RAWMIDI_IOCTL_OUTPUT_INFO	_IOR ( 'W', 0x02, struct snd_rawmidi_output_info )
#define SND_RAWMIDI_IOCTL_INPUT_INFO	_IOR ( 'W', 0x03, struct snd_rawmidi_input_info )
#define SND_RAWMIDI_IOCTL_OSWITCHES	_IOR ( 'W', 0x04, int )
#define SND_RAWMIDI_IOCTL_OSWITCH_READ	_IOR ( 'W', 0x05, struct snd_rawmidi_switch )
#define SND_RAWMIDI_IOCTL_OSWITCH_WRITE	_IOWR( 'W', 0x05, struct snd_rawmidi_switch )
#define SND_RAWMIDI_IOCTL_ISWITCHES	_IOR ( 'W', 0x06, int )
#define SND_RAWMIDI_IOCTL_ISWITCH_READ	_IOR ( 'W', 0x07, struct snd_rawmidi_switch )
#define SND_RAWMIDI_IOCTL_ISWITCH_WRITE	_IOWR( 'W', 0x07, struct snd_rawmidi_switch )
#define SND_RAWMIDI_IOCTL_OUTPUT_PARAMS	_IOWR( 'W', 0x10, struct snd_rawmidi_output_params )
#define SND_RAWMIDI_IOCTL_INPUT_PARAMS	_IOWR( 'W', 0x11, struct snd_rawmidi_input_params )
#define SND_RAWMIDI_IOCTL_OUTPUT_STATUS	_IOR ( 'W', 0x20, struct snd_rawmidi_output_status )
#define SND_RAWMIDI_IOCTL_INPUT_STATUS	_IOW ( 'W', 0x21, struct snd_rawmidi_input_status )
#define SND_RAWMIDI_IOCTL_DRAIN_OUTPUT	_IO  ( 'W', 0x30 )
#define SND_RAWMIDI_IOCTL_FLUSH_OUTPUT  _IO  ( 'W', 0x31 )
#define SND_RAWMIDI_IOCTL_FLUSH_INPUT	_IO  ( 'W', 0x32 )

/*
 *  Timer section - /dev/snd/timer
 */

#define SND_TIMER_VERSION		SND_PROTOCOL_VERSION(1, 0, 0)

#define SND_TIMER_MAX			1024
#define SND_TIMER_SYSTEM		0	/* system timer number */

#define SND_TIMER_STYPE_NONE		0
#define SND_TIMER_STYPE_APPLICATION	1
#define SND_TIMER_STYPE_SEQUENCER	2
#define SND_TIMER_STYPE_OSS_SEQUENCER	3

#define SND_TIMER_FLG_SLAVE		(1<<0)	/* cannot be controlled */

#define SND_TIMER_PSFLG_AUTO		(1<<0)	/* auto start */

struct snd_timer_general_info {
	int count;			/* count of global timers */
	char reserved[64];
};

struct snd_timer_select {
	int number;			/* timer number */
	int slave;			/* timer is slave */
	unsigned int slave_type;	/* slave type - SND_TIMER_STYPE_ */
	unsigned int slave_id;		/* slave identification */
	char reserved[32];
};

struct snd_timer_info {
	unsigned int flags;		/* timer flags - SND_MIXER_FLG_* */
	char id[32];			/* timer identificator */
	char name[80];			/* timer name */
	unsigned long ticks;		/* maximum ticks */
	unsigned long resolution;	/* average resolution */
	char reserved[64];
};

struct snd_timer_params {
	unsigned int flags;		/* flags - SND_MIXER_PSFLG_* */
	unsigned long ticks;		/* requested resolution in ticks */
	char reserved[64];
};

struct snd_timer_status {
	unsigned long resolution;	/* current resolution */
	unsigned long lost;		/* counter of master tick lost */
	unsigned long overrun;		/* count of read queue overruns */
	int queue;			/* queue size */
	char reserved[64];
};

#define SND_TIMER_IOCTL_PVERSION	_IOR ('T', 0x00, int)
#define SND_TIMER_IOCTL_GINFO		_IOW ('T', 0x01, snd_timer_general_info_t)
#define SND_TIMER_IOCTL_SELECT		_IOW ('T', 0x10, snd_timer_select_t)
#define SND_TIMER_IOCTL_INFO		_IOR ('T', 0x11, snd_timer_info_t)
#define SND_TIMER_IOCTL_PARAMS		_IOW ('T', 0x12, snd_timer_params_t)
#define SND_TIMER_IOCTL_STATUS		_IOW ('T', 0x13, snd_timer_status_t)
#define SND_TIMER_IOCTL_START		_IO  ('T', 0x20)
#define SND_TIMER_IOCTL_STOP		_IO  ('T', 0x21)
#define SND_TIMER_IOCTL_CONTINUE	_IO  ('T', 0x22)

struct snd_timer_read {
	unsigned long resolution;
	unsigned long ticks;
};

/*
 *
 */

#endif				/* __ASOUND_H */
