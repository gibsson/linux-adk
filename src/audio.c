/*
 * audio.c
 *
 *  Audio abstraction layer.
 *  Based on aplay from alsa-utils.
 *
 *  Created on: Mar 19, 2010
 *      Author: Remi Lorriaux
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <assert.h>
#include <poll.h>
#include <alsa/asoundlib.h>
#include "buffer.h"
#include "audio.h"

/************************************************
 * CONSTANTS
 ************************************************/

#define DEVICE_CAPTURE		"default"
#define DEVICE_PLAYBACK		"default"

#define PARAM_FORMAT		SND_PCM_FORMAT_S16_LE
#define PARAM_CHANNELS		2
#define PARAM_RATE			44100

#define PARAM_BUFFER_TIME	100000
#define PARAM_BUFFER_FRAMES	0
#define PARAM_PERIOD_TIME	25000
#define PARAM_PERIOD_FRAMES	(0 * 256)

#define MAX_BUFFER_TIME		100000

#define POLL_TIMEOUT_MS		10

/************************************************
 * GLOBALS
 ************************************************/

static snd_pcm_t *read_handle = NULL;
static snd_pcm_t *write_handle = NULL;

#ifdef IMPLEMENT_POLL

static struct pollfd *read_pollfd = NULL;
static struct pollfd *write_pollfd = NULL;
static int read_pollcnt = 0;
static int write_pollcnt = 0;

#endif // IMPLEMENT_POLL

static snd_pcm_uframes_t chunk_size = 0;
static size_t bits_per_sample = 0, bits_per_frame = 0;
static size_t chunk_bytes = 0;

/************************************************
 * PRIVATE FUNCTIONS
 ************************************************/

static int set_params(snd_pcm_t * handle)
{
	snd_pcm_hw_params_t *params;
	snd_pcm_sw_params_t *swparams;
	int err;

	// Allocate space for parameters
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_sw_params_alloca(&swparams);

	// Load current settings
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		fprintf(stdout,
			"ERROR: Broken configuration for this PCM: no configurations available\n");
		goto error;
	}
	// Use interleaved frames
	err =
	    snd_pcm_hw_params_set_access(handle, params,
					 SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err < 0) {
		fprintf(stdout, "ERROR: Access type not available\n");
		goto error;
	}
	// Set sample format
	err = snd_pcm_hw_params_set_format(handle, params, PARAM_FORMAT);
	if (err < 0) {
		fprintf(stdout, "ERROR: Sample format non available\n");
		goto error;
	}
	// Set channels
	err = snd_pcm_hw_params_set_channels(handle, params, PARAM_CHANNELS);
	if (err < 0) {
		fprintf(stdout, "ERROR: Channels count not available\n");
		goto error;
	}
#if 0
	err = snd_pcm_hw_params_set_periods_min(handle, params, 2);
	assert(err >= 0);
#endif

	// Set sample rate
	unsigned int rate = PARAM_RATE;

	err = snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
	if (err < 0) {
		fprintf(stdout, "ERROR: Rate not available\n");
		goto error;
	}
	fprintf(stdout, "INFO: Set rate %d\n", rate);

	// Set buffers and periods

	unsigned int buffer_time = PARAM_BUFFER_TIME;
	snd_pcm_uframes_t buffer_frames = PARAM_BUFFER_FRAMES;

	if (buffer_time == 0 && buffer_frames == 0) {
		err =
		    snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time,
							  0);
		if (err < 0) {
			fprintf(stdout,
				"ERROR: snd_pcm_hw_params_get_buffer_time_max() failed\n");
			goto error;
		}

		if (buffer_time > MAX_BUFFER_TIME) {
			buffer_time = MAX_BUFFER_TIME;
		}
	}

	unsigned int period_time = PARAM_PERIOD_TIME;
	snd_pcm_uframes_t period_frames = PARAM_PERIOD_FRAMES;

	if (period_time == 0 && period_frames == 0) {
		if (buffer_time > 0) {
			period_time = buffer_time / 4;
		} else {
			period_frames = buffer_frames / 4;
		}
	}

	if (period_time > 0) {
		err = snd_pcm_hw_params_set_period_time_near(handle, params,
							     &period_time, 0);
		fprintf(stdout, "INFO: period_time = %d\n", period_time);
	} else {
		err = snd_pcm_hw_params_set_period_size_near(handle, params,
							     &period_frames, 0);
		fprintf(stdout, "INFO: period_frames = %d\n",
			(int)period_frames);
	}

	if (err < 0) {
		fprintf(stdout,
			"ERROR: snd_pcm_hw_params_set_period_xxx() failed\n");
		goto error;
	}

	if (buffer_time > 0) {
		err = snd_pcm_hw_params_set_buffer_time_near(handle, params,
							     &buffer_time, 0);
	} else {
		err = snd_pcm_hw_params_set_buffer_size_near(handle, params,
							     &buffer_frames);
	}

	if (err < 0) {
		fprintf(stdout,
			"ERROR: snd_pcm_hw_params_set_buffer_xxx() failed\n");
		goto error;
	}

	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		fprintf(stdout, "ERROR: Unable to install hw params\n");
		goto error;
	}

	snd_pcm_uframes_t buffer_size;

	snd_pcm_hw_params_get_period_size(params, &chunk_size, 0);
	snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
	snd_pcm_hw_params_get_buffer_time(params, &buffer_time, 0);
	if (chunk_size == buffer_size) {
		fprintf(stdout,
			"Can't use period equal to buffer size (%lu == %lu)",
			chunk_size, buffer_size);
		goto error;
	}

	fprintf(stdout, "INFO: buffer_time = %d\n", buffer_time);

	// Probably not important, but copy from aplay
#if 1
	size_t n = chunk_size;
#else
	if (avail_min < 0)
		n = chunk_size;
	else
		n = (double)rate *avail_min / 1000000;
#endif
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, n);

	snd_pcm_uframes_t start_threshold = buffer_size;
	err =
	    snd_pcm_sw_params_set_start_threshold(handle, swparams,
						  start_threshold);
	if (err < 0) {
		fprintf(stdout,
			"ERROR: snd_pcm_sw_params_set_start_threshold() failed\n");
		goto error;
	}

	snd_pcm_uframes_t stop_threshold = buffer_size;
	err =
	    snd_pcm_sw_params_set_stop_threshold(handle, swparams,
						 stop_threshold);
	if (err < 0) {
		fprintf(stdout,
			"ERROR: snd_pcm_sw_params_set_stop_threshold() failed\n");
		goto error;
	}

	bits_per_sample = snd_pcm_format_physical_width(PARAM_FORMAT);
	bits_per_frame = bits_per_sample * PARAM_CHANNELS;
	chunk_bytes = chunk_size * bits_per_frame / 8;

	return 0;

error:
	return -1;
}

static int xrun(snd_pcm_t * handle, int capture)
{
	snd_pcm_status_t *status;
	int res;

	snd_pcm_status_alloca(&status);
	if ((res = snd_pcm_status(handle, status)) < 0) {
		fprintf(stdout, "status error: %s", snd_strerror(res));
		return -1;
	}

	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
		if ((res = snd_pcm_prepare(handle)) < 0) {
			fprintf(stdout, "xrun: prepare error: %s",
				snd_strerror(res));
			return -1;
		}

		return 0;	// OK, data should be accepted again
	}

	if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
		if (capture) {
			fprintf(stdout,
				"capture stream format change? attempting to recover...\n\n");
			if ((res = snd_pcm_prepare(handle)) < 0) {
				fprintf(stdout,
					"xrun(DRAINING): prepare error: %s",
					snd_strerror(res));
				return -1;
			}
			return 0;
		}
	}

	fprintf(stdout, "read/write error, state = %s",
		snd_pcm_state_name(snd_pcm_status_get_state(status)));

	return -1;
}

#ifdef IMPLEMENT_POLL

static int wait_for_poll(snd_pcm_t * handle, struct pollfd *ufds,
			 unsigned int count)
{
	unsigned short revents;

	while (1) {
		int ret = poll(ufds, count, POLL_TIMEOUT_MS);
		if (ret == 0) {
			return -ETIMEDOUT;
		} else if (ret < 0) {
			return ret;
		}
		// Get returned events from poll descriptors
		snd_pcm_poll_descriptors_revents(handle, ufds, count, &revents);
		if (revents & POLLERR)
			return -EIO;
		if (revents & POLLOUT)
			return 0;
		if (revents & POLLIN)
			return 0;
	}
}

static int init_poll_descriptors(snd_pcm_t * handle, struct pollfd **ufds,
				 int *count)
{
	assert(handle);

	*count = snd_pcm_poll_descriptors_count(handle);
	if (*count <= 0) {
		fprintf(stdout, "ERROR: Invalid poll descriptors count\n");
		return *count;
	}

	*ufds = malloc(sizeof(struct pollfd) * (*count));
	if (*ufds == NULL) {
		fprintf(stdout, "ERROR: Not enough memory\n");
		return -ENOMEM;
	}

	int err = snd_pcm_poll_descriptors(handle, *ufds, *count);

	if (err < 0) {
		fprintf(stdout, "ERROR: Unable to obtain poll descriptors\n");
		return err;
	}

	return 1;
}

#endif // IMPLEMENT_POLL

/************************************************
 * PUBLIC AUDIO INTERFACE
 ************************************************/

int audio_get_framesize(void)
{
	return bits_per_frame;
}

int audio_write_avail(void)
{
	snd_pcm_sframes_t frames = snd_pcm_avail_update(write_handle);

	if (frames < 0) {
		if (frames == -EPIPE) {
			fprintf(stdout, "WARNING: EPIPE\n\n");
			xrun(write_handle, 1);
		}

		frames = 0;
		fprintf(stdout, "ERROR: snd_pcm_avail_update() failed\n");
	}

	return (int)frames *bits_per_frame;
}

int audio_write(u_char * data, int bytes)
{
	ssize_t r;
	ssize_t bytes_written = 0;
	size_t count = bytes * 8 / bits_per_frame;

	if (!write_handle) {
		fprintf(stdout, "ERROR: playback device not open\n");
		return -1;
	}

	while (count > 0) {
		r = snd_pcm_writei(write_handle, data, count);
		if (r == -EAGAIN) {
			fprintf(stdout, "%s: -EAGAIN\n", __func__);
			snd_pcm_wait(write_handle, 1000);
		} else if (r == -EPIPE) {
			fprintf(stdout, "ERROR: EPIPE\n");
			xrun(write_handle, 1);
		} else if (r == -ESTRPIPE) {
			fprintf(stdout, "ERROR: ESTRPIPE\n");
// TODO: copy this from aplay (not needed in this test)
//                      suspend();
		} else if (r < 0) {
			fprintf(stdout, "ERROR: write error: %s",
				snd_strerror(r));
			return -1;
		}

		if (r > 0) {
			count -= r;
			data += r * bits_per_frame / 8;
			bytes_written += r * bits_per_frame / 8;
		}
	}

	return bytes_written;
}

#ifndef PLAYBACK_ONLY

int audio_read_avail(void)
{
	snd_pcm_sframes_t frames = snd_pcm_avail_update(read_handle);

	if (frames < 0) {
		if (frames == -EPIPE) {
			fprintf(stdout, "WARNING: EPIPE\n");
			xrun(read_handle, 0);
		}

		frames = 0;
		fprintf(stdout, "ERROR: snd_pcm_avail_update() failed\n");
	}

	return (int)frames *bits_per_frame;
}

int audio_read(u_char * data, int bytes)
{
	ssize_t r;
	size_t result = 0;
	size_t count = bytes * 8 / bits_per_frame;
	int bytes_read = 0;

	assert((int)(count * bits_per_frame / 8) == bytes);

	if (!data || ((int)count < 0)) {
		fprintf(stdout, "ERROR: invalid parameters\n");
		return -1;
	}

	if (!read_handle) {
		fprintf(stdout, "ERROR: capture device not open\n");
		return -1;
	}

	while (count > 0) {
		r = snd_pcm_readi(read_handle, data, count);
		if (r == -EAGAIN || (r >= 0 && (size_t) r < count)) {
			snd_pcm_wait(read_handle, 1000);
		} else if (r == -EPIPE) {
			fprintf(stdout, "ERROR: EPIPE\n");
			xrun(read_handle, 1);
		} else if (r == -ESTRPIPE) {
			fprintf(stdout, "ERROR: ESTRPIPE\n");
			// TODO: copy this from aplay (not needed in this test)
//                      suspend();
		} else if (r < 0) {
			fprintf(stdout, "ERROR: write error: %s",
				snd_strerror(r));
			return -1;
		}

		if (r > 0) {
			result += r;
			count -= r;
			bytes_read += r * bits_per_frame / 8;
			data += r * bits_per_frame / 8;
		}
	}

	return bytes_read;
}

#ifdef IMPLEMENT_POLL

int audio_read_poll(u_char * data, int bytes)
{
	ssize_t r;
	size_t result = 0;
	size_t count = bytes * 8 / bits_per_frame;

	assert((count * bits_per_frame / 8) == bytes);

	if (!data || (count < 0)) {
		fprintf(stdout, "ERROR: invalid parameters\n");
		return -1;
	}

	if (!read_handle) {
		fprintf(stdout, "ERROR: capture device not open\n");
		return -1;
	}

	assert(read_pollfd);

	snd_pcm_sframes_t frames_avail = snd_pcm_avail_update(read_handle);

	// Poll if we do not have enough data in the buffer
	if (frames_avail < count) {
		int ret = wait_for_poll(read_handle, read_pollfd, read_pollcnt);
		if (ret == -ETIMEDOUT) {
			// TODO: remove trace
			fprintf(stdout, "INFO: timeout\n");
//                      return 0;
		} else if (ret < 0) {
			fprintf(stdout, "ERROR: poll error\n");
			return -1;
		}
		// Now that we've polled,
		frames_avail += snd_pcm_avail_update(read_handle);
		count = (count > frames_avail) ? frames_avail : count;
	}

	int bytes_read = 0;

	while (count > 0) {
		r = snd_pcm_readi(read_handle, data, count);
		if (r == -EAGAIN || (r >= 0 && (size_t) r < count)) {
			snd_pcm_wait(read_handle, 1000);
		} else if (r == -EPIPE) {
			fprintf(stdout, "ERROR: EPIPE\n");
			xrun(read_handle, 1);
		} else if (r == -ESTRPIPE) {
			fprintf(stdout, "ERROR: ESTRPIPE\n");
			// TODO: copy this from aplay (not needed in this test)
//                      suspend();
		} else if (r < 0) {
			fprintf(stdout, "ERROR: write error: %s",
				snd_strerror(r));
			return -1;
		}

		if (r > 0) {
			result += r;
			count -= r;
			bytes_read += r * bits_per_frame / 8;
			data += r * bits_per_frame / 8;
		}
	}

	return bytes_read;
}

#else // IMPLEMENT_POLL

int audio_read_poll(u_char * data, int bytes)
{
	// Function not implemented
	assert(0);
	return 0;
}

#endif // IMPLEMENT_POLL
#endif // PLAYBACK_ONLY

int audio_open(void)
{
	int err;

	// Make sure the handles have only been opened once
	if (read_handle || write_handle) {
		fprintf(stdout, "ERROR: handles already open\n");
		return 2;
	}
#ifndef PLAYBACK_ONLY
	// Open capture
	err =
	    snd_pcm_open(&read_handle, DEVICE_CAPTURE, SND_PCM_STREAM_CAPTURE,
			 0);
	if (err < 0) {
		fprintf(stdout, "ERROR: failed to open CAPTURE (%s)",
			snd_strerror(err));
		return 1;
	}
#endif
	// Open playback
	err = snd_pcm_open(&write_handle, DEVICE_PLAYBACK, SND_PCM_STREAM_PLAYBACK, 0);	//SND_PCM_NONBLOCK);
	if (err < 0) {
		fprintf(stdout, "ERROR: failed to open PLAYBACK (%s)",
			snd_strerror(err));
		return 1;
	}
#ifdef IMPLEMENT_POLL
#ifndef PLAYBACK_ONLY
	// Initialize the poll descriptors
	if (init_poll_descriptors(read_handle, &read_pollfd, &read_pollcnt) <=
	    0) {
		return 3;
	}
#endif
	if (init_poll_descriptors(write_handle, &write_pollfd, &write_pollcnt)
	    <= 0) {
		return 3;
	}
#ifndef PLAYBACK_ONLY
	assert(read_pollfd);
#endif
	assert(write_pollfd);
#endif // IMPLEMENT_POLL

	// Configure the device
#ifndef PLAYBACK_ONLY
	fprintf(stdout, "INFO: Configuring the capture device...\n");
	err = set_params(read_handle);
	if (err < 0) {
		fprintf(stdout,
			"ERROR: Failed to configure the playback device\n");
		return 2;
	}
#endif

	fprintf(stdout, "INFO: Configuring the playback device...\n");
	err = set_params(write_handle);
	if (err < 0) {
		fprintf(stdout,
			"ERROR: Failed to configure the capture device\n");
		return 2;
	}
#ifndef PLAYBACK_ONLY
	snd_pcm_prepare(read_handle);
#endif
	snd_pcm_prepare(write_handle);

	fprintf(stdout, "INFO: Done preparing\n");

#ifdef IMPLEMENT_POLL
	//TODO: dealloc read_pollfd and write_pollfd on error
#endif

	return 0;
}

int audio_close(void)
{
	// Close handles
	if (read_handle) {
		snd_pcm_close(read_handle);
		read_handle = NULL;
	}

	if (write_handle) {
		snd_pcm_close(write_handle);
		write_handle = NULL;
	}
#ifdef IMPLEMENT_POLL

	// Close poll descriptors
#ifndef PLAYBACK_ONLY
	if (read_pollfd) {
		free(read_pollfd);
		read_pollfd = NULL;
	}
#endif
	if (write_pollfd) {
		free(write_pollfd);
		write_pollfd = NULL;
	}
#endif // IMPLEMENT_POLL

	// Reinit globals
	chunk_size = 0;
	bits_per_sample = 0;
	bits_per_frame = 0;
	chunk_bytes = 0;

	return 0;
}
