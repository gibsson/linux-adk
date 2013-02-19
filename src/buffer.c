/*
 * buffer.c
 *
 * Basic implementation of a circular buffer.
 * Can be shared between several threads (USES LOCKS!)
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "buffer.h"

/************************************************
 * CONSTANTS
 ************************************************/

// Magic number contained in the buffer handles to check if
// they are valid (debug)
#define MAGIC_NUMBER	0x12345678UL

// TODO: better implementation
#define SLEEP_TIME	1000

/************************************************
 * PUBLIC API
 ************************************************/

int buffer_read(buffer_handle_t * handle, void *dst, int count)
{
	assert(handle);
	assert(handle->magic == MAGIC_NUMBER);
	assert(handle->buffer);
	assert(dst);

	if (count < 0)
		return 0;

	// LOCK
	pthread_mutex_lock(&handle->mutex);

	// Cannot read more than what is available
	count = (count < handle->read_avail) ? count : handle->read_avail;
	int over = handle->read_pos + count - handle->buffer_size;

	if (over > 0) {
		memcpy(dst, handle->buffer + handle->read_pos, count - over);
		// Roll back from the beginning of the buffer
		memcpy(dst + count - over, handle->buffer, over);
		handle->read_pos = over;
	} else {
		memcpy(dst, handle->buffer + handle->read_pos, count);
		handle->read_pos += count;
	}

	handle->write_avail += count;
	handle->read_avail -= count;

	// UNLOCK
	pthread_mutex_unlock(&handle->mutex);

	return count;
}

int buffer_write(buffer_handle_t * handle, void *src, int count)
{
	assert(handle);
	assert(handle->magic == MAGIC_NUMBER);
	assert(handle->buffer);
	assert(src);

	if (count < 0)
		return 0;

	// LOCK
	pthread_mutex_lock(&handle->mutex);

	// Cannot write more than what is available
	if (count > handle->write_avail) {
		fprintf(stdout, "Cannot write more than what is available\n");
	}
	count = (count < handle->write_avail) ? count : handle->write_avail;

	int over = handle->write_pos + count - handle->buffer_size;

	if (over > 0) {
		memcpy(handle->buffer + handle->write_pos, src, count - over);
		// Roll back from the beginning of the buffer
		memcpy(handle->buffer, (char *)src + count - over, over);
		handle->write_pos = over;
	} else {
		memcpy(handle->buffer + handle->write_pos, src, count);
		handle->write_pos += count;
	}

	handle->write_avail -= count;
	handle->read_avail += count;

	// UNLOCK
	pthread_mutex_unlock(&handle->mutex);

	return count;
}

int buffer_read_avail(buffer_handle_t * handle)
{
	assert(handle);

	pthread_mutex_lock(&handle->mutex);
	int val = handle->read_avail;
	pthread_mutex_unlock(&handle->mutex);

	return val;
}

int buffer_write_avail(buffer_handle_t * handle)
{
	assert(handle);

	pthread_mutex_lock(&handle->mutex);
	int val = handle->write_avail;
	pthread_mutex_unlock(&handle->mutex);

	return val;
}

void buffer_read_wait(buffer_handle_t * handle)
{
	assert(handle);

	// TODO: better implementation: use condition variable
	usleep(SLEEP_TIME);
}

void buffer_write_wait(buffer_handle_t * handle)
{
	assert(handle);

	// TODO: better implementation: use condition variable
	usleep(SLEEP_TIME);
}

buffer_handle_t *buffer_create(int size)
{
	if (size <= 0) {
		return NULL;
	}
	// Allocate handle
	buffer_handle_t *handle =
	    (buffer_handle_t *) malloc(sizeof(buffer_handle_t));
	if (!handle) {
		fprintf(stdout, "ERROR: failed to allocate handle\n");
		goto error;
	}
	// Initialize members
	handle->magic = MAGIC_NUMBER;
	handle->buffer_size = size;
	handle->read_pos = 0;
	handle->write_pos = 0;
	handle->read_avail = 0;
	handle->write_avail = size;

	if (pthread_mutex_init(&handle->mutex, NULL)) {
		goto error;
	}
	// Allocate buffer
	handle->buffer = (char *)malloc(size);
	if (!(handle->buffer)) {
		fprintf(stdout, "ERROR: failed to allocate buffer\n");
		goto error;
	}

	return handle;

error:
	if (handle)
		free(handle);

	return NULL;
}

void buffer_free(buffer_handle_t * handle)
{
	if (!handle)
		return;

	assert(handle->magic == MAGIC_NUMBER);

	if (handle->buffer) {
		free(handle->buffer);
	}

	free(handle);
}
