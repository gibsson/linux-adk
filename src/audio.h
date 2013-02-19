/*
 * audio.h
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

#ifndef AUDIO_H_
#define AUDIO_H_

#include <sys/types.h>

#define PLAYBACK_ONLY

/************************************************
 * TYPES
 ************************************************/

// Audio parameters
typedef struct {

} audio_params_t;

/************************************************
 * FUNCTIONS
 ************************************************/

extern int audio_get_framesize(void);
#ifndef PLAYBACK_ONLY
extern int audio_read_avail(void);
extern int audio_read_poll(u_char * data, int bytes);
extern int audio_read(u_char * data, int bytes);
#endif
extern int audio_write_avail(void);
extern int audio_write(u_char * data, int bytes);
extern int audio_open(void);
extern int audio_close(void);

#endif /* AUDIO_H_ */
