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

#include "linux-adk.h"
#include "audio.h"
#include "buffer.h"

/* Handle to the circular buffer used by read&write threads */
static buffer_handle_t *buffer = NULL;

static void *thread_write(void *ptr);
static void audio_callback(struct libusb_transfer *transfer);

void accessory_main(accessory_t * acc)
{
	int ret = 0;
	pthread_t thread_audio_write;

	/* In case of Audio support */
	if (acc->pid >= AOA_AUDIO_PID) {
		int interface, endpoint, max_pkt_size;

		/* Depending on the product id the endpoint is not the same */
		if ((acc->pid == AOA_AUDIO_PID)
		    || (acc->pid == AOA_AUDIO_ADB_PID)) {
			endpoint = AOA_AUDIO_NO_APP_EP;
			interface = AOA_AUDIO_NO_APP_INTERFACE;
		} else {
			endpoint = AOA_AUDIO_EP;
			interface = AOA_AUDIO_INTERFACE;
		}

		ret = libusb_claim_interface(acc->handle, interface);
		if (ret != 0) {
			printf("Error %d claiming interface %d...\n", ret,
			       interface);
			goto accessory;
		}

		/* Initialize the circular buffer */
		buffer = buffer_create(AUDIO_BUFFER_SIZE);
		if (!buffer) {
			printf("Failed to allocate audio buffer\n");
			goto accessory;
		}

		ret = audio_open();
		if (ret != 0) {
			printf("Failed to open audio device (%d)\n", ret);
			goto accessory;
		}

		acc->transfer = libusb_alloc_transfer(AUDIO_NUM_ISO_PACKETS);
		if (!acc->transfer) {
			printf("Error running libusb_alloc_transfer!\n");
			goto accessory;
		}

		ret = libusb_set_interface_alt_setting(acc->handle, interface,
						       AUDIO_ALT_SETTING);
		if (ret) {
			printf("Error setting alt setting of interface %x\n",
			       interface);
			goto accessory;
		}

		max_pkt_size =
		    libusb_get_max_iso_packet_size(libusb_get_device
						   (acc->handle), endpoint);
		if ((max_pkt_size == LIBUSB_ERROR_NOT_FOUND)
		    || max_pkt_size == LIBUSB_ERROR_OTHER) {
			printf("Error getting max packet size\n");
		} else {
			printf("Max Packet size for endpoint %#2.2x = %d\n",
			       endpoint, max_pkt_size);
		}

		acc->transfer->dev_handle = acc->handle;
		acc->transfer->endpoint = endpoint;
		acc->transfer->type = LIBUSB_TRANSFER_TYPE_ISOCHRONOUS;
		acc->transfer->timeout = 0;	/* no timeout */
		acc->transfer->length = AUDIO_NUM_ISO_PACKETS * max_pkt_size;
		acc->transfer->buffer = malloc(acc->transfer->length);
		acc->transfer->callback = audio_callback;
		acc->transfer->num_iso_packets = AUDIO_NUM_ISO_PACKETS;

		libusb_set_iso_packet_lengths(acc->transfer, max_pkt_size);

		/* Create the thread that writes to the audio playback buffer */
		if (pthread_create(&thread_audio_write, NULL, thread_write,
				   NULL)) {
			printf("Failed to create audio_write thread\n");
			goto accessory;
		}

		ret = libusb_submit_transfer(acc->transfer);
		if (ret) {
			printf("Submit transfer failed\n");
		} else {
			printf("Audio transfer submitted successfully\n");
		}
	}

accessory:
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
		while (1) {
			ret =
			    libusb_bulk_transfer(acc->handle,
						 AOA_ACCESSORY_EP_IN, acc_buf,
						 sizeof(acc_buf), &transferred,
						 0);
			if (ret < 0) {
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

	if ((acc->aoa_version > 1) && (acc->pid > AOA_ACCESSORY_ADB_PID)) {
		pthread_join(thread_audio_write, NULL);
	}
}

static void *thread_write(void *ptr)
{
	int buffer_size = AUDIO_BUFFER_FRAMES * audio_get_framesize();

	u_char *data = (u_char *) malloc(buffer_size);
	if (!data) {
		printf("Failed to allocate memory\n");
		goto end;
	}

	/*
	 * Preloading
	 * This phase is important and will fill the output buffer.
	 * If you do not preload, you will get underruns.
	 * The value has been determined empirically.
	 */
	printf("Preloading...\n");
	while (buffer_read_avail(buffer) <= 0) {
		audio_write(data, buffer_size);
	}
	printf("Preloading done\n");

	printf("%s: created internal buffer of size = %i\n", __func__,
	       buffer_size);

	while (1) {
		int count = buffer_read_avail(buffer);
#ifdef AUDIO_DEBUG
		double percentage_full = count;
		percentage_full = (percentage_full / AUDIO_BUFFER_SIZE) * 100;
		printf("%s: buffer_read_avail = %i\n", __func__, count);
		printf("%s: buffer is %f percent full\n", __func__,
		       percentage_full);
#endif
		if (count <= 4410) {
			usleep(AUDIO_SLEEP_TIME);
#ifdef AUDIO_DEBUG
			printf("%s: wait for a full period's worth of data\n",
			       __func__);
#endif
			continue;
		}

		count = count > buffer_size ? buffer_size : count;

		/* Read from circular buffer */
		int done = 0;
		while (1) {
			done += buffer_read(buffer, data + done, count - done);
			if (done >= count) {
				break;
			} else {
				/*
				 * Wait until we have something to read from the circular buffer
				 * (i.e. the USB thread has written some data from the android device)
				 */
				usleep(AUDIO_SLEEP_TIME);
			}
		}

		/*
		 * Write to the audio device.
		 * Assume all the data will be written. This is guaranteed by
		 * the implementation of audio_write()
		 */
		int ret = audio_write(data, count);
		if (ret < 0) {
			printf("audio_write() failed\n");
			goto end;
		}
	}

end:
	if (data) {
		free(data);
		data = NULL;
	}

	printf("INFO: DONE\n");

	pthread_exit(NULL);
	return NULL;
}

static void audio_callback(struct libusb_transfer *transfer)
{
	int i;
	unsigned char *buf;
	unsigned int written;

#ifdef AUDIO_DEBUG
	printf("%s\n", __func__);
#endif
	for (i = 0; i < AUDIO_NUM_ISO_PACKETS; i++) {
		if (transfer->iso_packet_desc[i].status
		    == LIBUSB_TRANSFER_COMPLETED) {
			buf = libusb_get_iso_packet_buffer_simple(transfer, i);
#ifdef AUDIO_DEBUG
			printf("%s: packet of %dbytes received\n", __func__,
			       transfer->iso_packet_desc[i].actual_length);
#endif
			written = 0;
			while (1) {
				written += buffer_write(buffer,
							buf + written,
							transfer->
							iso_packet_desc[i].
							actual_length -
							written);
				if (written >=
				    transfer->iso_packet_desc[i].
				    actual_length) {
					break;
				} else {
					printf
					    ("Still writing packet[%i] to circular buffer ...\n",
					     i);
				}
			}
		} else {
			printf("Bad iso packet status: %i\n",
			       transfer->iso_packet_desc[i].status);
		}
	}
	libusb_submit_transfer(transfer);
}
