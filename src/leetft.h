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

#ifndef LEETFT_H_INCLUDED
#define LEETFT_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simple font object.
 * 
 * You first create a font object at the given font size.  To be honest,
 * I don't know the units for the font size, it might be 'points' or
 * pixels high.  Then you use that font object to create strings
 * which are 1-byte-per-pixel alphamaps.
 */

typedef struct ft_font_s ft_font_t;
typedef struct ft_string_s ft_string_t;

/**
 * Creates a new font object from the font file.  Opened using Freetype2
 * which supports truetype fonts among other formats.  The fontsize I
 * have no idea about the units, I should find out.  The video width,
 * height, and aspect ratio are used to determine the dpi at which to
 * render the font.
 */
ft_font_t *ft_font_new( const char *file, int fontsize, double pixel_aspect );

/**
 * Frees the font object.  Make sure you have no strings still active.
 */
void ft_font_delete( ft_font_t *font );

/**
 * Updates the pixel aspect ratio of the font.
 */
void ft_font_set_pixel_aspect( ft_font_t *font, double pixel_aspect );

/**
 * Returns the size of the font.
 */
int ft_font_get_size( ft_font_t *font );

/**
 * Returns subpixel width for points.
 */
int ft_font_points_to_subpix_width( ft_font_t *font, int points );

/**
 * Renders a given string with this font to the given buffer.
 */
void ft_font_render( ft_font_t *font, uint8_t *output, const char *text,
                     int subpix_pos, int *width, int *height, int outsize );

/**
 * Creates a new string object for the given font.  You can reset the
 * string as many times as you like with new text.
 */
ft_string_t *ft_string_new( ft_font_t *font );

/**
 * Deletes the string object.
 */
void ft_string_delete( ft_string_t *efs );

/**
 * Sets the text of the string object.
 */
void ft_string_set_text( ft_string_t *efs, const char *text, int subpix_pos );

/**
 * Returns the width in pixels.
 */
int ft_string_get_width( ft_string_t *efs );

/**
 * Returns the height in pixels.
 */
int ft_string_get_height( ft_string_t *efs );

/**
 * Returns the number of bytes per scanline.
 */
int ft_string_get_stride( ft_string_t *efs );

/**
 * Returns the buffer.  The buffer is a 1-byte-per-pixel alphamap with
 * values from 0-255.
 */
uint8_t *ft_string_get_buffer( ft_string_t *efs );

#ifdef __cplusplus
};
#endif
#endif /* LEETFT_H_INCLUDED */
