/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef PNGOUTPUT_H_INCLUDED
#define PNGOUTPUT_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pngoutput_s pngoutput_t;

/**
 * Creates a new PNG output file at the given width and height.  The
 * gamma should be the intended
 */
pngoutput_t *pngoutput_new( const char *filename, int width, int height,
                            int alpha, double gamma, int compression );

/**
 * Closes the PNG file, finishing the write.
 */
void pngoutput_delete( pngoutput_t *pngoutput );

/**
 * Writes a scanline (of the appropriate width) to the PNG file.
 */
void pngoutput_scanline( pngoutput_t *pngoutput, uint8_t *scanline );

#ifdef __cplusplus
};
#endif
#endif /* PNGOUTPUT_H_INCLUDED */
