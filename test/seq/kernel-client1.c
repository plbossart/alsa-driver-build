/*
 *   Test of kernel client interface for ALSA sequencer
 *   Copyright (c) 1998 by Frank van de Pol <F.K.W.van.de.Pol@inter.nl.net>
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

#define SND_MAIN_OBJECT_FILE
#include "driver.h"
#include "seq.h"

static int client;



/*
 *  REGISTRATION PART
 */



/* call-back function for event input */
static int event_input(snd_seq_event_t * ev, void *private_data)
{
	snd_printk("Call...\n");
	snd_printk(".......Event %d\n", ev->type);
//      snd_printk(".......Event %d,%d,%d to %d,%d,%d\n",
	//                      ev->event,ev->note,ev->velocity,
	//                      ev->queue,ev->client,ev->port);

	/* for testing, bounce the event to the next client */
	ev->dest.client += 1;
	/*... after 2 seconds */
	ev->flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_REL;
	ev->time.real.sec = 2;
	ev->time.real.nsec = 0;

	snd_seq_kernel_client_enqueue(client, ev);

	snd_printk(".......Back!\n");
	return 1; /* success */
}



/*
 *  INIT PART
 */





int init_module(void)
{

	snd_seq_client_callback_t callbacks;

	callbacks.input = event_input;

	client = snd_seq_register_kernel_client(&callbacks, NULL);

	/* set our name */
	snd_seq_kernel_client_ctl(client, SND_SEQ_IOCTL_SET_CLIENT_NAME, "Kernel test");

	return 0;
}


void cleanup_module(void)
{
	snd_seq_unregister_kernel_client(client);
}
