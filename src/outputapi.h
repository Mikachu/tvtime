/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef OUTPUTAPI_H_INCLUDED
#define OUTPUTAPI_H_INCLUDED

#include "input.h"

typedef struct output_api_s
{
    int (* init)( int inputwidth, int inputheight, int outputwidth, int aspect );

    /* Returns pointers to the next output buffer. */
    void (* lock_output_buffer)( void );
    unsigned char *(* get_output_buffer)( void );
    int (* get_output_stride)( void );
    void (* unlock_output_buffer)( void );

    /* Output API for interlaced displays. */
    int (* is_interlaced)( void );
    void (* wait_for_sync)( int field );

    /* Output API for progressive displays. */
    void (* show_frame)( void );

    /* Functions for all outputs. */
    int (* toggle_aspect)( void );
    int (* toggle_fullscreen)( void );
    void (* poll_events)( input_t *in );
    void (* shutdown)( void );
} output_api_t;

#endif /* OUTPUTAPI_H_INCLUDED */
