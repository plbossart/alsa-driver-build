/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

******************************************************************************/
// The following code came from boot4000.s.CLD 

#include "hpios.h"		// for  defn.

static u32 adwDspCode_Boot4000a[] = {

/**** P memory  ****/
/*  Length  */ 0x0000008B,
/*  Address */ 0x00000F60,
/*Type:P,X,Y*/ 0x00000004,
/* 00000F60 */ 0x00240000, 0x0004C4BB, 0x0004C4B1, 0x000003F8,
	    0x0061F400, 0x00000048, 0x0054F400, 0x000BF080, 0x0007598C,
	    0x0054F400,
/* 00000F6A */ 0x00000FDE, 0x0007618C, 0x0008F4B9, 0x00100739,
	    0x0008F4B7, 0x00800839, 0x0008F4B6, 0x0040080A, 0x0008F4B8,
	    0x00200739,
/* 00000F74 */ 0x0008F4BB, 0x001FFFE2, 0x0046F400, 0x00123456,
	    0x0060F400, 0x00100000, 0x0061F400, 0x00110000, 0x00000000,
	    0x00000000,
/* 00000F7E */ 0x00446100, 0x00466000, 0x000200C4, 0x0056E000,
	    0x00200055, 0x000D104A, 0x00000005, 0x002C0300, 0x000D10C0,
	    0x0000001F,
/* 00000F88 */ 0x0056E100, 0x00200055, 0x000D1042, 0x00000005,
	    0x002C0700, 0x000D10C0, 0x00000018, 0x00200013, 0x0020001B,
	    0x00209500,
/* 00000F92 */ 0x0007F08D, 0x00000102, 0x000140CD, 0x00004300,
	    0x000D104A, 0x0000000B, 0x000140CD, 0x00004600, 0x000D104A,
	    0x00000004,
/* 00000F9C */ 0x000D10C0, 0x00000008, 0x002C0600, 0x000D10C0,
	    0x00000006, 0x002C0200, 0x000D10C0, 0x00000003, 0x002C0400,
	    0x000C1E86,
/* 00000FA6 */ 0x00000000, 0x00084405, 0x00200042, 0x0008CC05,
	    0x00200013, 0x000A8982, 0x00000FAB, 0x00084F0B, 0x000140CD,
	    0x00FFFFFF,
/* 00000FB0 */ 0x000AF0AA, 0x00000FD8, 0x000A8982, 0x00000FB2,
	    0x0008500B, 0x00221100, 0x000A8982, 0x00000FB6, 0x00084E0B,
	    0x0006CD00,
/* 00000FBA */ 0x00000FD6, 0x000A8982, 0x00000FBB, 0x00014485,
	    0x000AF0A2, 0x00000FC3, 0x0008584B, 0x000AF080, 0x00000FD6,
	    0x00014185,
/* 00000FC4 */ 0x000AF0A2, 0x00000FC9, 0x0008588B, 0x000AF080,
	    0x00000FD6, 0x00014285, 0x000AF0A2, 0x00000FCD, 0x000858CB,
	    0x00014385,
/* 00000FCE */ 0x000AF0A2, 0x00000FD6, 0x000CFFA0, 0x00000005,
	    0x0008608B, 0x000D10C0, 0x00000003, 0x000858CB, 0x00000000,
	    0x000C0FAA,
/* 00000FD8 */ 0x00084E05, 0x000140C6, 0x00FFFFC7, 0x0008CC05,
	    0x000000B9, 0x000C0000, 0x0008F498, 0x00000000, 0x00000000,
	    0x00000000,
/* 00000FE2 */ 0x0008F485, 0x00000000, 0x00000000, 0x00000000,
	    0x00000084, 0x00000000, 0x00000000, 0x000AF080, 0x00FF0000,

/* End-Of-Array */ 0xFFFFFFFF
};

// static u32  * adwDspCode_Boot4000Arrays[]={&adwDspCode_Boot4000a[0]};

/* Final CRC=0x002D9C5B */