
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

#ifndef DIFFCOMP_H_INCLUDED
#define DIFFCOMP_H_INCLUDED

/**
 * Lossless Differential coding (prediction coding) based compression
 * algorithm.  Based on an idea from the 'Huffyuv' codec by Ben
 * Rudiak-Gould.  See http://www.math.berkeley.edu/~benrg/huffyuv.html
 * for more information.
 *
 * Read the .c file for a more in-depth discussion of the technique.
 */

/**
 * Compress a plane (a single-channel 8bpp sample map) of an image from
 * src to dst.  Returns the number of bytes used of the destination
 * buffer.  Theoretical max size of the destination image is twice as
 * large as the input image, although this is very unlikely.
 */
int diffcomp_compress_plane( unsigned char *dst, unsigned char *src,
                             int width, int height );

/**
 * Decompresses a single plane from src to dst.  The width and height
 * provided must match those used to encode the image.
 */
void diffcomp_decompress_plane( unsigned char *dst, unsigned char *src,
                                int width, int height );

#endif /* DIFFCOMP_H_INCLUDED */