/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef CONSOLE_H_INCLUDED
#define CONSOLE_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
void console_setup_pipe( console_t *con, const char *pipename );
int console_scanf( console_t *con, char *format, ... );
void console_pipe_printf( console_t *con, char * format, ... );
void console_composite_packed422_scanline( console_t *con, uint8_t *output,
                                           int width, int xpos, int scanline );

#ifdef __cplusplus
};
#endif
#endif /* CONSOLE_H_INCLUDED */
