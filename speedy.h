/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This includes code from DScaler: http://deinterlace.sf.net/
 *
 * TwoFrame deinterlacing code:
 *   Copyright (c) 2000 Steven Grimm.  All rights reserved.
 * Greedy2Frame deinterlacing code:
 *   Copyright (c) 2000 John Adcock, Tom Barry, Steve Grimm.
 *   All rights reserved.
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

void interpolate_packed422_scanline_c( unsigned char *output,
                                       unsigned char *top,
                                       unsigned char *bot, int width );
void interpolate_packed422_scanline_mmxext( unsigned char *output,
                                            unsigned char *top,
                                            unsigned char *bot, int width );
void blit_colour_packed422_scanline_c( unsigned char *output,
                                       int width, int y, int cb, int cr );
void blit_colour_packed422_scanline_mmx( unsigned char *output,
                                         int width, int y, int cb, int cr );
void blit_colour_packed422_scanline_mmxext( unsigned char *output,
                                            int width, int y, int cb, int cr );
void blit_colour_packed4444_scanline_mmxext( unsigned char *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr );
void blit_colour_packed4444_scanline_mmx( unsigned char *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr );
void blit_colour_packed4444_scanline_c( unsigned char *output, int width,
                                        int alpha, int luma, int cb, int cr );
void blit_packed422_scanline_mmxext_xine( unsigned char *dest, const unsigned char *src, int width );
void blit_packed422_scanline_i386_linux( unsigned char *dest, const unsigned char *src, int width );
void blit_packed422_scanline_c( unsigned char *dest, const unsigned char *src, int width );

extern void (*interpolate_packed422_scanline)( unsigned char *output,
                                               unsigned char *top,
                                               unsigned char *bot, int width );
extern void (*blit_colour_packed422_scanline)( unsigned char *output,
                                               int width, int y, int cb, int cr );
extern void (*blit_colour_packed4444_scanline)( unsigned char *output,
                                                int width, int alpha, int luma,
                                                int cb, int cr );
extern void (*blit_packed422_scanline)( unsigned char *dest, const unsigned char *src, int width );

const char *speedy_get_deinterlacing_mode( void );
const char *speedy_next_deinterlacing_mode( void );
void setup_speedy_calls( void );
int speedy_get_accel( void );

#endif /* SPEEDY_H_INCLUDED */
