/*
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

#ifndef __ASOUNDID_H
#define __ASOUNDID_H

/*
 *  Types of soundcards...
 *  Note: Don't assign new number to 100% clones...
 *  Note: Order shouldn't be preserved, but assigment must be!!!
 */

/* Gravis UltraSound */
#define SND_CARD_TYPE_GUS_CLASSIC	0x00000001
#define SND_CARD_TYPE_GUS_EXTREME	0x00000002
#define SND_CARD_TYPE_GUS_ACE		0x00000003
#define SND_CARD_TYPE_GUS_MAX		0x00000004
#define SND_CARD_TYPE_AMD_INTERWAVE	0x00000005
/* Sound Blaster */
#define SND_CARD_TYPE_SB_10		0x00000006
#define SND_CARD_TYPE_SB_20		0x00000007
#define SND_CARD_TYPE_SB_PRO		0x00000008
#define SND_CARD_TYPE_SB_16		0x00000009
#define SND_CARD_TYPE_SB_AWE		0x0000000a
/* Various */
#define SND_CARD_TYPE_ESS_ES1688	0x0000000b	/* ESS AudioDrive ESx688 */
#define SND_CARD_TYPE_OPL3_SA		0x0000000c	/* Yamaha OPL3 SA */
#define SND_CARD_TYPE_MOZART		0x0000000d	/* OAK Mozart */
#define SND_CARD_TYPE_S3_SONICVIBES	0x0000000e	/* S3 SonicVibes */
#define SND_CARD_TYPE_ENS1370		0x0000000f	/* Ensoniq ES1370 */
#define SND_CARD_TYPE_ENS1371		0x00000010	/* Ensoniq ES1371 */
#define SND_CARD_TYPE_CS4232		0x00000011	/* CS4232/CS4232A */
#define SND_CARD_TYPE_CS4236		0x00000012	/* CS4235/CS4236B/CS4237B/CS4238B/CS4239 */
#define SND_CARD_TYPE_AMD_INTERWAVE_STB	0x00000013	/* AMD InterWave + TEA6330T */
#define SND_CARD_TYPE_ESS_ES1938	0x00000014	/* ESS Solo-1 ES1938 */
#define SND_CARD_TYPE_ESS_ES1869	0x00000015	/* ESS AudioDrive ES1869 */
#define SND_CARD_TYPE_ENS4081		0x00000016      /* Ensoniq Vivo90 */
#define SND_CARD_TYPE_OPTI9XX		0x00000017	/* Opti 9xx chipset */
#define SND_CARD_TYPE_SERIAL		0x00000018	/* Serial MIDI driver */
/* --- */
#define SND_CARD_TYPE_LAST		0x00000017
  
#endif				/* __ASOUNDID_H */
