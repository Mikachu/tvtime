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

#ifndef OSD_H_INCLUDED
#define OSD_H_INCLUDED

typedef struct osd_string_s osd_string_t;

/**
 * Creates a new string with the given truetype font at the given point size.
 * I don't understand point size vs pixel size, so, uh, whatever.  Maybe it's
 * just pixel height I have no clue.
 *
 * You have to pass in the video width and height to get the aspect ratio right.
 * I'll also need to pass in an option for 16:9 vs 4:3, but I'll do that later.
 */
osd_string_t *osd_string_new( const char *fontfile, int fontsize,
                              int video_width, int video_height );
void osd_string_delete( osd_string_t *osds );
void osd_string_show_text( osd_string_t *osds, const char *text, int timeout );
int osd_string_visible( osd_string_t *osds );
void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr );
void osd_string_advance_frame( osd_string_t *osds );
void osd_string_composite_packed422( osd_string_t *osds, unsigned char *output,
                                     int width, int height, int stride,
                                     int xpos, int ypos, int rightjustified );

#endif /* OSD_H_INCLUDED */
