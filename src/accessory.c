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
#include <unistd.h>

#include <libusb.h>
#include <SDL_image.h>

#include "linux-adk.h"
#include "hid.h"

static char *images[] = {
	"freescale.jpg",
	"blown-away-android.jpg",
	"droid.jpg",
	"freescale_round.jpg",
	"android_skateboard.jpg",
	"Wikinews_Reader.jpg",
	"sabrelite.jpg",
	"droid_happy.jpg",
	"angry_birds_android.jpg",
};

void accessory_main(accessory_t * acc)
{
	int ret = 0;
	hid_device hid;
#if 0
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
		int transferred;
		int errors = 20;
		int index = -1;
		SDL_Surface *screen;

		/* Claiming first (accessory )interface from the opened device */
		ret =
		    libusb_claim_interface(acc->handle,
					   AOA_ACCESSORY_INTERFACE);
		if (ret != 0) {
			printf("Error %d claiming interface...\n", ret);
			return;
		}

		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			printf("Couldn't initialise SDL: %i\n", SDL_GetError());
			return;
		}

		atexit(SDL_Quit);

		screen=SDL_SetVideoMode(1024, 768, 16, SDL_SWSURFACE);
		if (screen == NULL) {
			printf("Couldn't set video mode: %s\n", SDL_GetError());
			return;
		}

		/* Snooping loop; Display every data received from device */
		while (!stop_acc) {
			SDL_Surface *image = NULL;

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

			if (acc_buf[0] == 0x6E) {
				if (++index > 8)
					index = 0;
				printf("Next picture: %s\n", images[index]);
				image = IMG_Load(images[index]);
			} else if (acc_buf[0] == 0x70) {
				if (--index < 0)
					index = 8;
				printf("Previous picture: %s\n", images[index]);
				image = IMG_Load(images[index]);
			} else {
				acc_buf[transferred] = '\0';
				printf("%s\n", acc_buf);
			}

			if (image != NULL) {
				SDL_Rect rcDest = {0, 0, 0, 0};
				SDL_FillRect(screen, NULL, 0x000000);
				SDL_BlitSurface(image, NULL, screen, &rcDest);
				SDL_UpdateRects(screen, 1, &rcDest);
				SDL_FreeSurface(image);
			}

			SDL_Flip(screen);
		}
	}

	if ((acc->pid >= AOA_AUDIO_PID) && (hid.handle))
		pthread_join(hid.rx_thread, NULL);
}
