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

#ifndef OSDTOOLS_H_INCLUDED
#define OSDTOOLS_H_INCLUDED

/**
 * This file defines some simple primitives needed for the on screen
 * display.  Each primitive can composite itself onto a packed 4:2:2
 * buffer.
 */

typedef struct osd_string_s osd_string_t;
typedef struct osd_databars_s osd_databars_t;
typedef struct osd_graphic_s osd_graphic_t;
typedef struct osd_shape_s osd_shape_t;

typedef enum OSD_Shapes_e {
    OSD_Rect        = (1<<0),
    OSD_Circle      = (1<<1)
} OSD_Shape;

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
int osd_string_get_width( osd_string_t *osds );
int osd_string_get_height( osd_string_t *osds );
void osd_string_set_hold( osd_string_t *osds, int hold );
void osd_string_set_timeout( osd_string_t *osds, int timeout );
void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr );
void osd_string_set_colour_rgb( osd_string_t *osds, int r, int g, int b );
void osd_string_show_border( osd_string_t *osds, int show_border );
void osd_string_set_border_colour( osd_string_t *osds, int luma,
                                   int cb, int cr );
void osd_string_advance_frame( osd_string_t *osds );
int osd_string_get_frames_left( osd_string_t *osds );

/**
 * Composites a scanline of the string on top of the background and writes to
 * the output buffer.  The output and background pointers are allowed to be
 * the same.
 */
void osd_string_composite_packed422_scanline( osd_string_t *osds,
                                              unsigned char *output,
                                              unsigned char *background,
                                              int width, int xpos,
                                              int scanline );

/**
 * databars, values are out of 128.
 */
osd_databars_t *osd_databars_new( int width );
void osd_databars_delete( osd_databars_t *osdd );
void osd_databars_set_colour( osd_databars_t *osdd, int alpha, int luma,
                              int cb, int cr );
void osd_databars_advance_frame( osd_databars_t *osdd );
int osd_databars_get_frames_left( osd_databars_t *osdd );
void osd_databars_prerender( osd_databars_t *osdd, int num_filled );
void osd_databars_show_bar( osd_databars_t *osdd, int num_filled, int frames );
void osd_databars_composite_packed422_scanline( osd_databars_t *osdd,
                                                unsigned char *output,
                                                unsigned char *background,
                                                int width );

osd_shape_t *osd_shape_new( OSD_Shape shape_type, int video_width,
                            int video_height, int shape_width,
                            int shape_height, double aspect, int alpha );

void osd_shape_delete( osd_shape_t *osds );
void osd_shape_set_colour( osd_shape_t *osds, int luma, int cb, int cr );
void osd_shape_show_shape( osd_shape_t *osds, int timeout );
int osd_shape_visible( osd_shape_t *osds );
void osd_shape_set_timeout( osd_shape_t *osds, int timeout );
void osd_shape_advance_frame( osd_shape_t *osds );
void osd_shape_composite_packed422( osd_shape_t *osds, 
                                    unsigned char *output,
                                    int width, int height, int stride,
                                    int xpos, int ypos );
void osd_shape_composite_packed422_scanline( osd_shape_t *osds,
                                             unsigned char *output,
                                             unsigned char *background,
                                             int width, int xpos,
                                             int scanline );


osd_graphic_t *osd_graphic_new( const char *filename, int video_width,
                                int video_height, double aspect, int alpha );
void osd_graphic_delete( osd_graphic_t *osdg );
int osd_graphic_get_width( osd_graphic_t *osdg );
int osd_graphic_get_height( osd_graphic_t *osdg );
void osd_graphic_show_graphic( osd_graphic_t *osdg, int timeout );
void osd_graphic_set_timeout( osd_graphic_t *osdg, int timeout );
int osd_graphic_visible( osd_graphic_t *osdg );
void osd_graphic_advance_frame( osd_graphic_t *osdg );
void osd_graphic_composite_packed422_scanline( osd_graphic_t *osdg,
                                               unsigned char *output,
                                               unsigned char *background,
                                               int width, int xpos,
                                               int scanline );

#endif /* OSDTOOLS_H_INCLUDED */
