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

#ifndef LEETFT_H_INCLUDED
#define LEETFT_H_INCLUDED

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

void ft_font_render( ft_font_t *font, unsigned char *output, const char *text,
                     int *width, int *height, int outsize );

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
void ft_string_set_text( ft_string_t *efs, const char *text );

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
unsigned char *ft_string_get_buffer( ft_string_t *efs );

#endif /* LEETFT_H_INCLUDED */
