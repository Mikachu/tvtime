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

#ifndef EFS_H_INCLUDED
#define EFS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Simple font object with a C API.  Basically just a wrapper for
 * the C++ TTFFont object from mythtv defined in ttfont.h.
 * 
 * You first create a font object at the given font size.  To be honest,
 * I don't know the units for the font size, it might be 'points' or
 * pixels high.  Then you use that font object to create strings
 * which are 1-byte-per-pixel alphamaps.
 */

typedef struct efont_s efont_t;
typedef struct efont_string_s efont_string_t;

/**
 * Creates a new font object from the font file.  Opened using Freetype2
 * which supports truetype fonts among other formats.  The fontsize I
 * have no idea about the units, I should find out.  The video width,
 * height, and aspect ratio are used to determine the dpi at which to
 * render the font.
 */
efont_t *efont_new( const char *fontfile, int fontsize, int video_width,
                    int video_height, double pixel_aspect );

/**
 * Frees the font object.  Make sure you have no strings still active.
 */
void efont_delete( efont_t *font );

/**
 * Creates a new string object for the given font.  You can reset the
 * string as many times as you like with new text.
 */
efont_string_t *efs_new( efont_t *font );

/**
 * Deletes the string object.
 */
void efs_delete( efont_string_t *efs );

/**
 * Sets the text of the string object.
 */
void efs_set_text( efont_string_t *efs, const char *text );

/**
 * Returns the width in pixels.
 */
int efs_get_width( efont_string_t *efs );

/**
 * Returns the height in pixels.
 */
int efs_get_height( efont_string_t *efs );

/**
 * Returns the number of bytes per scanline.
 */
int efs_get_stride( efont_string_t *efs );

/**
 * Returns the buffer.  The buffer is a 1-byte-per-pixel alphamap with
 * values from 0-255.
 */
unsigned char *efs_get_buffer( efont_string_t *efs );


#ifdef __cplusplus
};
#endif
#endif /* EFS_H_INCLUDED */
