/**
 * Copyright (C) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is a collection of useful video-related functions.  Some of
 * these depend on the speedy.[h,c] functions, or are implementations
 * of the full-frame versions.
 */

/**
 * Frame functions.
 */
void composite_alphamask_to_packed4444( uint8_t *output, int owidth,
                                        int oheight, int ostride,
                                        uint8_t *mask, int mwidth,
                                        int mheight, int mstride,
                                        int luma, int cb, int cr,
                                        int xpos, int ypos );
void composite_alphamask_alpha_to_packed4444( uint8_t *output, int owidth,
                                              int oheight, int ostride,
                                              uint8_t *mask, int mwidth,
                                              int mheight, int mstride,
                                              int luma, int cb, int cr, int alpha,
                                              int xpos, int ypos );
void composite_packed4444_to_packed422( uint8_t *output, int owidth,
                                        int oheight, int ostride,
                                        uint8_t *foreground, int fwidth,
                                        int fheight, int fstride,
                                        int xpos, int ypos );
void composite_packed4444_alpha_to_packed422( uint8_t *output,
                                              int owidth, int oheight,
                                              int ostride,
                                              uint8_t *foreground,
                                              int fwidth, int fheight,
                                              int fstride,
                                              int xpos, int ypos, int alpha );
void create_colourbars_packed444( uint8_t *output, int width, int height, int stride );
void blit_colour_packed4444( uint8_t *output, int width, int height,
                             int stride, int alpha, int luma, int cb, int cr );
void blit_colour_packed422( uint8_t *output, int width, int height,
                            int stride, int luma, int cb, int cr );
void crossfade_frame( uint8_t *output, uint8_t *src1, uint8_t *src2,
                      int width, int height, int outstride,
                      int src1stride, int src2stride, int pos );

#ifdef __cplusplus
};
#endif
#endif /* VIDEOTOOLS_H_INCLUDED */
