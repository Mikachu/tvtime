/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef CREDITS_H_INCLUDED
#define CREDITS_H_INCLUDED

typedef struct credits_s credits_t;

credits_t *credits_new( const char *filename, int output_height );
void credits_delete( void );
void credits_restart( credits_t *credits, double speed );
void credits_composite_packed422_scanline( credits_t *credits,
                                           unsigned char *output,
                                           int width, int xpos,
                                           int scanline );
void credits_advance_frame( credits_t *credits );


#endif /* CREDITS_H_INCLUDED */
