/**
 * Copyright (C) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef OSDTOOLS_H_INCLUDED
#define OSDTOOLS_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This file defines some simple primitives needed for the on screen
 * display.  Each primitive can composite itself onto a packed 4:2:2
 * buffer.
 */

typedef struct osd_string_s osd_string_t;
typedef struct osd_font_s osd_font_t;
typedef struct osd_rect_s osd_rect_t;
typedef struct osd_animation_s osd_animation_t;
typedef struct osd_list_s osd_list_t;

/**
 * Creates a new string with the given truetype font at the given size.
 * Font size is in pixels.
 *
 * Right now we require the video width, height, and the aspect ratio.
 * Eventually I'll just take one parameter: pixel aspect, which would
 * make more sense.
 */
osd_font_t *osd_font_new( const char *fontfile, int fontsize, double pixel_aspect );
void osd_font_delete( osd_font_t *font );
void osd_font_set_pixel_aspect( osd_font_t *font, double pixel_aspect );

/**
 * Creates a new string for a given font, with a given maximum width
 * that it will be asked to render to (for its back buffer).
 */
osd_string_t *osd_string_new( osd_font_t *font );
void osd_string_delete( osd_string_t *osds );

/**
 * Asks for the given text to be shown.  If the text given matches our
 * current back buffer, it will not be re-rendered.  The maximum size of
 * a string is 256 characters.
 */
void osd_string_show_text( osd_string_t *osds, const char *text, int timeout );
int osd_string_visible( osd_string_t *osds );
int osd_string_get_width( osd_string_t *osds );
int osd_string_get_height( osd_string_t *osds );
int osd_string_get_ascent( osd_string_t *osds );
void osd_string_set_hold( osd_string_t *osds, int hold );
void osd_string_set_timeout( osd_string_t *osds, int timeout );
int osd_string_get_frames_left( osd_string_t *osds );
void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr );
void osd_string_set_colour_rgb( osd_string_t *osds, int r, int g, int b );
void osd_string_show_border( osd_string_t *osds, int show_border );
void osd_string_set_border_colour( osd_string_t *osds, int luma,
                                   int cb, int cr );
void osd_string_rerender( osd_string_t *osds );
void osd_string_advance_frame( osd_string_t *osds );

/**
 * Composites a scanline of the string on top of the background and writes to
 * the output buffer.  The output and background pointers are allowed to be
 * the same.
 */
void osd_string_composite_packed422_scanline( osd_string_t *osds,
                                              uint8_t *output,
                                              uint8_t *background,
                                              int width, int xpos,
                                              int scanline );

osd_rect_t *osd_rect_new( void );
void osd_rect_delete( osd_rect_t *osdr );
void osd_rect_set_colour( osd_rect_t *osdr, int alpha, int luma, int cb, int cr );
void osd_rect_set_colour_rgb( osd_rect_t *osdr, int alpha, int r, int g, int b );
void osd_rect_set_timeout( osd_rect_t *osdr, int timeout );
void osd_rect_advance_frame( osd_rect_t *osdr );
int osd_rect_visible( osd_rect_t *osdr );
void osd_rect_set_size( osd_rect_t *osdr, int width, int height );
void osd_rect_composite_packed422_scanline( osd_rect_t *osdr, uint8_t *output,
                                            uint8_t *background, int width, int xpos,
                                            int scanline );

osd_animation_t *osd_animation_new( const char *filename_base,
                                    double pixel_aspect, int alpha, int frametime );
void osd_animation_delete( osd_animation_t *osda );
int osd_animation_get_width( osd_animation_t *osda );
int osd_animation_get_height( osd_animation_t *osda );
void osd_animation_pause( osd_animation_t *osda, int pause );
void osd_animation_seek( osd_animation_t *osda, double pos );
void osd_animation_set_hold( osd_animation_t *osda, int hold );
void osd_animation_set_timeout( osd_animation_t *osda, int timeout );
int osd_animation_visible( osd_animation_t *osda );
void osd_animation_advance_frame( osd_animation_t *osda );
void osd_animation_composite_packed422_scanline( osd_animation_t *osda,
                                                 uint8_t *output,
                                                 uint8_t *background,
                                                 int width, int xpos,
                                                 int scanline );

osd_list_t *osd_list_new( osd_font_t *font, double pixel_aspect );
void osd_list_delete( osd_list_t *osdl );
int osd_list_get_hilight( osd_list_t *osdl );
int osd_list_get_numlines( osd_list_t *osdl );
void osd_list_set_hilight( osd_list_t *osdl, int pos );
void osd_list_set_text( osd_list_t *osdl, int line, const char *text );
void osd_list_set_lines( osd_list_t *osdl, int numlines );
void osd_list_set_timeout( osd_list_t *osdl, int timeout );
void osd_list_set_hold( osd_list_t *osdl, int hold );
void osd_list_set_minimum_width( osd_list_t *osdl, int minwidth );
int osd_list_get_width( osd_list_t *osdl );
int osd_list_visible( osd_list_t *osdl );
int osd_list_get_line_pos( osd_list_t *osdl, int y );
void osd_list_advance_frame( osd_list_t *osdl );
void osd_list_rerender( osd_list_t *osdl );
void osd_list_get_bounding_box( osd_list_t *osdl, int *x, int *y,
                                int *width, int *height );
void osd_list_composite_packed422_scanline( osd_list_t *osdl,
                                            uint8_t *output,
                                            uint8_t *background,
                                            int width, int xpos,
                                            int scanline );
int osd_list_set_multitext( osd_list_t *odsl, int cur, const char *text,
                            int numlines, int maxwidth );
#ifdef __cplusplus
};
#endif
#endif /* OSDTOOLS_H_INCLUDED */
