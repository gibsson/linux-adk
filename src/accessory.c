/*
 * Linux ADK - accessory.c
 *
 * Copyright (C) 2013 - Gary Bisson <bisson.gary@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <libusb.h>

#include "linux-adk.h"
#ifndef WIN32
#include "hid.h"
#endif

void accessory_main(accessory_t * acc)
{
	int ret = 0;
#ifndef WIN32
	hid_device hid;

	/* In case of Audio/HID support */
	if (acc->pid >= AOA_AUDIO_PID) {
		/* Audio warning */
		printf("Device should now be recognized as valid ALSA card...\n");
		printf("  => arecord -l\n");

		/* HID handling */
		if(search_hid(&hid) == 0) {
			register_hid_callback(acc, &hid);
			send_hid_descriptor(acc, &hid);
		}
	}
#endif
	/* If we have an accessory interface */
	if ((acc->pid != AOA_AUDIO_ADB_PID) && (acc->pid != AOA_AUDIO_PID)) {
		uint8_t acc_buf[512];
		int transferred, i;
		int errors = 20;

		/* Claiming first (accessory )interface from the opened device */
		ret =
		    libusb_claim_interface(acc->handle,
					   AOA_ACCESSORY_INTERFACE);
		if (ret != 0) {
			printf("Error %d claiming interface...\n", ret);
			return;
		}

		/* Snooping loop; Display every data received from device */
		while (!stop_acc) {
			ret =
			    libusb_bulk_transfer(acc->handle,
						 AOA_ACCESSORY_EP_IN, acc_buf,
						 sizeof(acc_buf), &transferred,
						 200);
			if (ret < 0) {
				if (ret == LIBUSB_ERROR_TIMEOUT)
					continue;
				printf("bulk transfer error %d\n", ret);
				if (--errors == 0)
					break;
				else
					sleep(1);
			}

			printf("Received %d bytes\n", transferred);
			for (i = 0; i < transferred;) {
				printf("%#2.2x ", acc_buf[i++]);
				if (!(i % 8))
					printf("\n");
			}
			printf("\n");
		}
	}
#ifndef WIN32
	if ((acc->pid >= AOA_AUDIO_PID) && (hid.handle))
		pthread_join(hid.rx_thread, NULL);
#endif
}
