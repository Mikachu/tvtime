/**
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
 * Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
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

#ifndef VIDEOTOOLS_H_INCLUDED
#define VIDEOTOOLS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is a collection of useful video-related functions.  Some of
 * these depend on the speedy.[h,c] functions, or are implementations
 * of the full-frame versions.
 */

/**
 * Sub-pixel data bar renderer.  There are 128 bars.
 */
void composite_bars_packed4444_scanline( unsigned char *output,
                                         unsigned char *background, int width,
                                         int a, int luma, int cb, int cr,
                                         int percentage );


void packed444_to_rgb24_rec601_reference_scanline( unsigned char *output,
                                                   unsigned char *input, int width );
void packed444_to_rgb24_rec601_scanline( unsigned char *output,
                                         unsigned char *input, int width );
void rgb24_to_packed444_rec601_scanline( unsigned char *output,
                                         unsigned char *input, int width );
void rgba32_to_packed4444_rec601_scanline( unsigned char *output,
                                           unsigned char *input, int width );

/**
 * Frame functions.
 */
void composite_alphamask_to_packed4444( unsigned char *output, int owidth,
                                        int oheight, int ostride,
                                        unsigned char *mask, int mwidth,
                                        int mheight, int mstride,
                                        int luma, int cb, int cr,
                                        int xpos, int ypos );
void composite_alphamask_alpha_to_packed4444( unsigned char *output, int owidth,
                                              int oheight, int ostride,
                                              unsigned char *mask, int mwidth,
                                              int mheight, int mstride,
                                              int luma, int cb, int cr, int alpha,
                                              int xpos, int ypos );
void composite_packed4444_to_packed422( unsigned char *output, int owidth,
                                        int oheight, int ostride,
                                        unsigned char *foreground, int fwidth,
                                        int fheight, int fstride,
                                        int xpos, int ypos );
void composite_packed4444_alpha_to_packed422( unsigned char *output,
                                              int owidth, int oheight,
                                              int ostride,
                                              unsigned char *foreground,
                                              int fwidth, int fheight,
                                              int fstride,
                                              int xpos, int ypos, int alpha );
void create_colourbars_packed444( unsigned char *output,
                                  int width, int height, int stride );
void blit_colour_packed4444( unsigned char *output, int width, int height,
                             int stride, int alpha, int luma, int cb, int cr );
void blit_colour_packed422( unsigned char *output, int width, int height,
                            int stride, int luma, int cb, int cr );

#ifdef __cplusplus
};
#endif
#endif /* VIDEOTOOLS_H_INCLUDED */
