/**
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef XCOMMON_H_INCLUDED
#define XCOMMON_H_INCLUDED

#include <X11/Xlib.h>
#include "input.h"

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} area_t;

int xcommon_open_display( int aspect, int init_height, int verbose );
void xcommon_close_display( void );

Display *xcommon_get_display( void );
Window xcommon_get_output_window( void );
GC xcommon_get_gc( void );
int xcommon_is_fullscreen( void );
int xcommon_get_visible_width( void );
int xcommon_get_visible_height( void );

void xcommon_ping_screensaver( void );
area_t xcommon_get_video_area( void );
area_t xcommon_get_window_area( void );
void xcommon_clear_screen( void );
int xcommon_toggle_fullscreen( int fullscreen_width, int fullscreen_height );
int xcommon_toggle_aspect( void );
void xcommon_poll_events( input_t *in );
void xcommon_set_window_caption( const char *caption );
void xcommon_set_window_height( int window_height );
int xcommon_is_exposed( void );
void xcommon_set_colourkey( int colourkey );
void xcommon_frame_drawn( void );

#endif /* XCOMMON_H_INCLUDED */
