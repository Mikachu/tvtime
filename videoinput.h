/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef VIDEOINPUT_H_INCLUDED
#define VIDEOINPUT_H_INCLUDED

#include <linux/videodev.h>  /* Include this for the VIDEO_MODE defines */

typedef struct videoinput_s videoinput_t;

/**
 * This object represents a video4linux capture device.  Frames are returned as
 * planar Y'CbCr image maps with 4:2:2 encoding: luma first, then cb, then cr.
 */

/**
 * Create a new input device from the given device name and which input number
 * (cable, composite1, composite2, etc) to use.
 */
videoinput_t *videoinput_new( const char *v4l_device, int inputnum,
                              int capwidth, int capheight, int palmode );

/**
 * Shut down the capture device.
 */
void videoinput_delete( videoinput_t *vidin );

/**
 * Returns the width of the input images.
 */
int videoinput_get_width( videoinput_t *vidin );

/**
 * Returns the height of the input images.
 */
int videoinput_get_height( videoinput_t *vidin );

/**
 * Returns a pointer to the next image buffer.  Also returns the last field
 * number captured.
 */
unsigned char *videoinput_next_image( videoinput_t *vidin );

/**
 * Returns the number of buffers we've got.
 */
int videoinput_get_num_frames( videoinput_t *vidin );

/**
 * Sets the tuner to tuner_number for the current channel. Also sets the tuners
 * mode to mode. Valid modes are VIDEO_MODE_PAL, VIDEO_MODE_NTSC, 
 * VIDEO_MODE_SECAM, VIDEO_MODE_AUTO (not sure if AUTO would have any effect)
 */
void videoinput_set_tuner( int tuner_number, int mode );

/**
 * Sets the frequency to tune in to. (ie. the station to watch)
 * Since frequencies.c specifies all freqs in KHz, we take that as input.
 * If the card expects frequency in MHz, this function handles that.
 */
void videoinput_set_tuner_freq( int freqKHz );

/**
 * Signal to the videoinput device that we're done reading the last frame, to
 * allow the hardware to use the buffer.
 */
void videoinput_free_last_frame( videoinput_t *vidin );
void videoinput_free_all_frames( videoinput_t *vidin );

#endif /* VIDEOINPUT_H_INCLUDED */
