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

typedef struct osd_graphic_s osd_graphic_t;

typedef enum OSD_Shapes_e {
    OSD_Rect        = (1<<0),
    OSD_Circle      = (1<<1)
} OSD_Shape;
typedef struct osd_shape_s osd_shape_t;

/**
 * Creates a new string with the given truetype font at the given point size.
 * I don't understand point size vs pixel size, so, uh, whatever.  Maybe it's
 * just pixel height I have no clue.
 *
 * Right now we require the video width, height, and the aspect ratio.
 * Eventually I'll just take one parameter: pixel aspect, which would
 * make more sense.
 */
osd_string_t *osd_string_new( const char *fontfile, int fontsize,
                              int video_width, int video_height,
                              double video_aspect );
void osd_string_delete( osd_string_t *osds );
void osd_string_show_text( osd_string_t *osds, const char *text, int timeout );
int osd_string_visible( osd_string_t *osds );
void osd_string_set_timeout( osd_string_t *osds, int timeout );
void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr );
void osd_string_show_border( osd_string_t *osds, int show_border );
void osd_string_set_border_colour( osd_string_t *osds, int luma, int cb, int cr );
void osd_string_advance_frame( osd_string_t *osds );
void osd_string_composite_packed444( osd_string_t *osds, unsigned char *output,
                                     int width, int height, int stride,
                                     int xpos, int ypos );
void osd_string_composite_packed422( osd_string_t *osds, unsigned char *output,
                                     int width, int height, int stride,
                                     int xpos, int ypos, int rightjustified );

osd_shape_t *osd_shape_new( OSD_Shape shape_type, int shape_width, int shape_height );
void osd_shape_delete( osd_shape_t *osds );
void osd_shape_set_colour( osd_shape_t *osds, int luma, int cb, int cr );
void osd_shape_show_shape( osd_shape_t *osds, int timeout );
int osd_shape_visible( osd_shape_t *osds );
void osd_shape_advance_frame( osd_shape_t *osds );


osd_graphic_t *osd_graphic_new( const char *filename, int video_width,
                                int video_height, double aspect );
void osd_graphic_delete( osd_graphic_t *osdg );
void osd_graphic_show_graphic( osd_graphic_t *osdg, int timeout, int alpha );
int osd_graphic_visible( osd_graphic_t *osdg );
void osd_graphic_advance_frame( osd_graphic_t *osdg );
void osd_graphic_composite_packed444( osd_string_t *osds, 
                                      unsigned char *output,
                                      int width, int height, int stride,
                                      int xpos, int ypos );
void osd_graphic_composite_packed422( osd_graphic_t *osdg, 
                                      unsigned char *output,
                                      int width, int height, int stride,
                                      int xpos, int ypos );

#endif /* OSD_H_INCLUDED */
