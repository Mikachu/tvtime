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

#include "input.h"

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
 * Function gets the width to scale to.
 * If aspect is 1 we run in 16:9 mode.
 */
int sdl_init( int width, int height, int outputwidth, int aspect );

/**
 * Returns a pointer to the next frame to be drawn.
 */
unsigned char *sdl_get_output( void );

/**
 * Display the given frame immediately.
 */
void sdl_show_frame( void );

/**
 * Toggle fullscreen mode.
 */
void sdl_toggle_fullscreen( void );

/**
 * Toggle display aspect ratio.
 */
void sdl_toggle_aspect( void );

/**
 * Perform the polling of events.  Sends signals to the input object.
 */
void sdl_poll_events( input_t *in );

/**
 * Shutdown SDL.
 */
void sdl_quit( void );

#endif /* SDLOUTPUT_H_INCLUDED */
