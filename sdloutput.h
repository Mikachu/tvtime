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

#ifndef SDLOUTPUT_H_INCLUDED
#define SDLOUTPUT_H_INCLUDED

#include <SDL/SDL.h>

/**
 * Input commands.
 */
enum tvtime_commands
{
    TVTIME_NOCOMMAND     = 0,
    TVTIME_QUIT          = (1<<0),
    TVTIME_CHANNEL_UP    = (1<<1),
    TVTIME_CHANNEL_DOWN  = (1<<2),
    TVTIME_LUMA_UP       = (1<<3),
    TVTIME_LUMA_DOWN     = (1<<4),
    TVTIME_KP0           = (1<<5),
    TVTIME_KP1           = (1<<6),
    TVTIME_KP2           = (1<<7),
    TVTIME_KP3           = (1<<8),
    TVTIME_KP4           = (1<<9),
    TVTIME_KP5           = (1<<10),
    TVTIME_KP6           = (1<<11),
    TVTIME_KP7           = (1<<12),
    TVTIME_KP8           = (1<<13),
    TVTIME_KP9           = (1<<14),
    TVTIME_MIXER_MUTE    = (1<<15),
    TVTIME_MIXER_UP      = (1<<16),
    TVTIME_MIXER_DOWN    = (1<<17),
    TVTIME_DIGIT         = (1<<18),
    TVTIME_KP_ENTER      = (1<<19),
    TVTIME_CHANNEL_CHAR  = (1<<20),

    TVTIME_HUE_DOWN      = (1<<21),
    TVTIME_HUE_UP        = (1<<22),
    TVTIME_BRIGHT_DOWN   = (1<<23),
    TVTIME_BRIGHT_UP     = (1<<24),
    TVTIME_CONT_DOWN     = (1<<25),
    TVTIME_CONT_UP       = (1<<26),
    TVTIME_COLOUR_DOWN   = (1<<27),
    TVTIME_COLOUR_UP     = (1<<28),

    TVTIME_DEBUG         = (1<<31)
};

/**
 * Returns a pointer to the next frame to be drawn.
 */
unsigned char *sdl_get_output( void );

/**
 * For planar (4:2:0) output, here are the two chroma planes.
 */
unsigned char *sdl_get_cb( void );
unsigned char *sdl_get_cr( void );

/**
 * Functions to handle SDL output.
 *
 * We have to make sure all our calls to SDL occur in the same thread,
 * since otherwise SDL is not happy.  All of the other functions here
 * are for the render thread.
 */

/**
 * Initialize the SDL output device.  Provided is the width and height
 * that incoming frames will be provided at.  This width and height, of
 * course, having nothing to do with the output width and height, which
 * will be decided internally.
 *
 * If use420 is true then we'll open in 4:2:0 mode instead of 4:2:2 mode.
 * Function also gets the width to scale to.
 * If aspect is 1 we run in 16:9 mode.
 */
int sdl_init( int width, int height, int use420, int outputwidth, int aspect );

/**
 * Display the given frame immediately.
 */
void sdl_show_frame( void );

/**
 * Perform the polling of events.  Returns a bitfield of the commands
 * listed above.
 */
int sdl_poll_events( void );

#endif /* SDLOUTPUT_H_INCLUDED */
