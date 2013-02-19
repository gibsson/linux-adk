/*
 * buffer.h
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

#ifndef BUFFER_H_
#define BUFFER_H_

#include <pthread.h>

/************************************************
 * TYPES
 ************************************************/

// Handle to a buffer structure
// Create with buffer_create(), destroy with buffer_free()
typedef struct {
	char *buffer;
	int buffer_size;
	int read_pos;
	int write_pos;
	int read_avail;
	int write_avail;
	pthread_mutex_t mutex;

	// Define a constant magic number to make sure the handle is
	// valid (debug). Will be asserted for every call.
	unsigned long magic;
} buffer_handle_t;

/************************************************
 * FUNCTIONS
 ************************************************/

extern int buffer_read(buffer_handle_t * handle, void *dst, int count);
extern int buffer_write(buffer_handle_t * handle, void *src, int count);
extern int buffer_read_avail(buffer_handle_t * handle);
extern int buffer_write_avail(buffer_handle_t * handle);
extern void buffer_read_wait(buffer_handle_t * handle);
extern void buffer_write_wait(buffer_handle_t * handle);
extern buffer_handle_t *buffer_create(int size);
extern void buffer_free(buffer_handle_t * handle);

#endif /* BUFFER_H_ */
