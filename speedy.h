/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This file is subject to the terms of the GNU General Public License as
 * published by the Free Software Foundation.  A copy of this license is
 * included with this software distribution in the file COPYING.  If you
 * do not have a copy, you may obtain a copy by writing to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#ifndef SPEEDY_H_INCLUDED
#define SPEEDY_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speedy is a collection of optimized functions plus their C fallbacks.
 * This includes a simple system to select which functions to use
 * at runtime.
 *
 * The optimizations are done with the help of the mmx.h system, from
 * libmpeg2 by Michel Lespinasse and Aaron Holtzman.
 */


/**
 * Interpolates a packed 4:2:2 scanline using linear interpolation.
 */
void interpolate_packed422_scanline_c( unsigned char *output,
                                       unsigned char *top,
                                       unsigned char *bot, int width );
void interpolate_packed422_scanline_mmxext( unsigned char *output,
                                            unsigned char *top,
                                            unsigned char *bot, int width );

/**
 * Blits a colour to a packed 4:2:2 scanline.
 */
void blit_colour_packed422_scanline_c( unsigned char *output,
                                       int width, int y, int cb, int cr );
void blit_colour_packed422_scanline_mmx( unsigned char *output,
                                         int width, int y, int cb, int cr );
void blit_colour_packed422_scanline_mmxext( unsigned char *output,
                                            int width, int y, int cb, int cr );

/**
 * Blits a colour to a packed 4:4:4:4 scanline.  I use luma/cb/cr instead of
 * RGB but this will of course work for either.
 */
void blit_colour_packed4444_scanline_mmxext( unsigned char *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr );
void blit_colour_packed4444_scanline_mmx( unsigned char *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr );
void blit_colour_packed4444_scanline_c( unsigned char *output, int width,
                                        int alpha, int luma, int cb, int cr );

/**
 * Scanline blitter for packed 4:2:2 scanlines.  This implementation uses
 * the fast memcpy code from xine which got it from mplayer.
 */
void blit_packed422_scanline_mmxext_xine( unsigned char *dest,
                                          const unsigned char *src, int width );
void blit_packed422_scanline_i386_linux( unsigned char *dest,
                                         const unsigned char *src, int width );
void blit_packed422_scanline_c( unsigned char *dest, const unsigned char *src,
                                int width );

/**
 * Here are the function pointers which will be initialized to point at the
 * fastest available version of the above after a call to setup_speedy_calls().
 */
extern void (*interpolate_packed422_scanline)( unsigned char *output,
                                               unsigned char *top,
                                               unsigned char *bot, int width );
extern void (*blit_colour_packed422_scanline)( unsigned char *output,
                                               int width, int y, int cb, int cr );
extern void (*blit_colour_packed4444_scanline)( unsigned char *output,
                                                int width, int alpha, int luma,
                                                int cb, int cr );
extern void (*blit_packed422_scanline)( unsigned char *dest, const unsigned char *src, int width );

/**
 * Sets up the function pointers to point at the fastest function available.
 */
void setup_speedy_calls( void );

/**
 * Returns a bitfield of what accellerations are available.  See mm_accel.h.
 */
int speedy_get_accel( void );

#ifdef __cplusplus
};
#endif
#endif /* SPEEDY_H_INCLUDED */
