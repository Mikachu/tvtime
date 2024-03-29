/**
 * Copyright (C) 2002, 2004 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef OUTPUTAPI_H_INCLUDED
#define OUTPUTAPI_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
#include "input.h"

typedef struct output_api_s
{
    int (* init)( const char *user_geometry, int aspect, int squarepixel, int verbose );

    int (* set_input_size)( int inputwidth, int inputheight );

    /* Returns pointers to the next output buffer. */
    void (* lock_output_buffer)( void );
    uint8_t *(* get_output_buffer)( void );
    int (* get_output_stride)( void );
    int (* can_read_from_buffer)( void );
    void (* unlock_output_buffer)( void );

    /* Some useful functions for windowed outputs. */
    int (* is_exposed)( void );
    int (* get_visible_width)( void );
    int (* get_visible_height)( void );
    int (* is_fullscreen)( void );
    int (* is_alwaysontop)( void );

    /* Some support questions. */
    int (* is_fullscreen_supported)( void );
    int (* is_alwaysontop_supported)( void );
    int (* is_overscan_supported)( void );

    /* Output API for interlaced displays. */
    int (* is_interlaced)( void );
    void (* wait_for_sync)( int field );

    /* Output API for progressive displays. */
    int (* show_frame)( int x, int y, int width, int height );

    /* Functions for all outputs. */
    int (* toggle_aspect)( void );
    int (* toggle_alwaysontop)( void );
    int (* toggle_fullscreen)( int fullscreen_width, int fullscreen_height );
    void (* set_window_caption)( const char *caption );
    void (* update_xawtv_station)( int frequency, int channel_id,
                                   const char *channel_name );
    void (* update_server_time)( unsigned long timestamp );

    void (* set_window_position)( int x, int y );
    void (* set_window_height)( int window_height );
    void (* set_fullscreen_position)( int pos );
    void (* set_matte)( int width, int height );

    void (* poll_events)( input_t *in );
    void (* shutdown)( void );
} output_api_t;

#endif /* OUTPUTAPI_H_INCLUDED */
