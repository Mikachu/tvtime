/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef HAVE_CONSOLE_H
#define HAVE_CONSOLE_H

#include "tvtimeconf.h"

typedef struct console_s console_t;

console_t *console_new( int x, int y, int cols, int rows,
                        int fontsize, int video_width, int video_height,
                        double video_aspect, unsigned int fgcolour );
void console_delete( console_t *con );
void console_printf( console_t *con, char *format, ... );
void console_gotoxy( console_t *con, int x, int y );

void console_setfg( console_t *con, unsigned int fg );
void console_setbg( console_t *con, unsigned int bg );
void console_toggle_console( console_t *con );
void console_scroll_n( console_t *con, int n );
void console_setup_pipe( console_t *con, char *pipename );
int console_scanf( console_t *con, char *format, ... );
void console_pipe_printf( console_t *con, char * format, ... );
void console_composite_packed422_scanline( console_t *con, 
                                           unsigned char *output,
                                           int width, int xpos, int scanline );

#endif
