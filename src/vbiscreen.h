/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 * Copyright (c) 2002 Doug Bell <drbell@users.sourceforge.net>.
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

#ifndef VBISCREEN_H_INCLUDED
#define VBISCREEN_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The VBI screen object is the closed caption renderer line 21 closed
 * captions in NTSC video streams.
 */

typedef struct vbiscreen_s vbiscreen_t;

vbiscreen_t *vbiscreen_new( int video_width, int video_height, 
                            double video_aspect, int verbose );
void vbiscreen_delete( vbiscreen_t *vs );
void vbiscreen_set_mode( vbiscreen_t *vs, int caption, int style);
void vbiscreen_new_caption( vbiscreen_t *vs, int indent, int ital,
                            unsigned int colour, int row );
void vbiscreen_tab( vbiscreen_t *vs, int cols );
void vbiscreen_delete_to_end( vbiscreen_t *vs );
void vbiscreen_backspace( vbiscreen_t *vs );
void vbiscreen_erase_displayed( vbiscreen_t *vs );
void vbiscreen_erase_non_displayed( vbiscreen_t *vs );
void vbiscreen_carriage_return( vbiscreen_t *vs );
void vbiscreen_end_of_caption( vbiscreen_t *vs );
void vbiscreen_print( vbiscreen_t *vs, char c1, char c2 );
int vbiscreen_active_on_scanline( vbiscreen_t *vs, int scanline );
void vbiscreen_composite_packed422_scanline( vbiscreen_t *vs,
                                             uint8_t *output,
                                             int width, int xpos, 
                                             int scanline );
void vbiscreen_dump_screen_text( vbiscreen_t *vs );
void vbiscreen_reset( vbiscreen_t *vs );

#ifdef __cplusplus
};
#endif
#endif /* VBISCREEN_H_INCLUDED */
