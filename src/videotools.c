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

#include <math.h>
#include <stdlib.h>
#include "speedy.h"
#include "videotools.h"

void blit_colour_packed4444( uint8_t *output, int width, int height,
                             int stride, int alpha, int luma, int cb, int cr )
{
    int i;

    for( i = 0; i < height; i++ ) {
        blit_colour_packed4444_scanline( output + (i * stride), width,
                                         alpha, luma, cb, cr );
    }
}

void blit_colour_packed422( uint8_t *output, int width, int height,
                            int stride, int luma, int cb, int cr )
{
    int i;

    for( i = 0; i < height; i++ ) {
        blit_colour_packed422_scanline( output + (i * stride), width,
                                        luma, cb, cr );
    }
}

void crossfade_frame( uint8_t *output, uint8_t *src1, uint8_t *src2,
                      int width, int height, int outstride,
                      int src1stride, int src2stride, int pos )
{
    while( height-- ) {
        blend_packed422_scanline( output, src1, src2, width, pos );
        output += outstride;
        src1 += src1stride;
        src2 += src2stride;
    }
}

void composite_alphamask_to_packed4444( uint8_t *output, int owidth,
                                        int oheight, int ostride,
                                        uint8_t *mask, int mwidth,
                                        int mheight, int mstride,
                                        int luma, int cb, int cr,
                                        int xpos, int ypos )
{
    int dest_x, dest_y, src_x, src_y, blit_w, blit_h;

    src_x = 0;
    src_y = 0;
    dest_x = xpos;
    dest_y = ypos;
    blit_w = mwidth;
    blit_h = mheight;

    if( dest_x < 0 ) {
        src_x = -dest_x;
        blit_w += dest_x;
        dest_x = 0;
    }
    if( dest_y < 0 ) {
        src_y = -dest_y;
        blit_h += dest_y;
        dest_y = 0;
    }
    if( dest_x + blit_w > owidth ) {
        blit_w = owidth - dest_x;
    }
    if( dest_y + blit_h > oheight ) {
        blit_h = oheight - dest_y;
    }

    if( blit_w > 0 && blit_h > 0 ) {
        int i;

        output += dest_y * ostride;
        for( i = 0; i < blit_h; i++ ) {
            composite_alphamask_to_packed4444_scanline( output + ((dest_x) * 4),
                                                        output + ((dest_x) * 4),
                                                        mask + ((src_y + i)*mstride) + src_x,
                                                        blit_w, luma, cb, cr );
            output += ostride;
        }
    }
}

void composite_alphamask_alpha_to_packed4444( uint8_t *output, int owidth,
                                              int oheight, int ostride,
                                              uint8_t *mask, int mwidth,
                                              int mheight, int mstride,
                                              int luma, int cb, int cr, int alpha,
                                              int xpos, int ypos )
{
    int dest_x, dest_y, src_x, src_y, blit_w, blit_h;

    src_x = 0;
    src_y = 0;
    dest_x = xpos;
    dest_y = ypos;
    blit_w = mwidth;
    blit_h = mheight;

    if( dest_x < 0 ) {
        src_x = -dest_x;
        blit_w += dest_x;
        dest_x = 0;
    }
    if( dest_y < 0 ) {
        src_y = -dest_y;
        blit_h += dest_y;
        dest_y = 0;
    }
    if( dest_x + blit_w > owidth ) {
        blit_w = owidth - dest_x;
    }
    if( dest_y + blit_h > oheight ) {
        blit_h = oheight - dest_y;
    }

    if( blit_w > 0 && blit_h > 0 ) {
        int i;

        output += dest_y * ostride;
        for( i = 0; i < blit_h; i++ ) {
            composite_alphamask_alpha_to_packed4444_scanline( output + ((dest_x) * 4),
                                                     output + ((dest_x) * 4),
                                                     mask + ((src_y + i)*mstride) + src_x,
                                                     blit_w, luma, cb, cr, alpha );
            output += ostride;
        }
    }
}

void composite_packed4444_to_packed422( uint8_t *output, int owidth,
                                        int oheight, int ostride,
                                        uint8_t *foreground, int fwidth,
                                        int fheight, int fstride,
                                        int xpos, int ypos )
{
    int dest_x, dest_y, src_x, src_y, blit_w, blit_h;

    src_x = 0;
    src_y = 0;
    dest_x = xpos;
    dest_y = ypos;
    blit_w = fwidth;
    blit_h = fheight;

    if( dest_x < 0 ) {
        src_x = -dest_x;
        blit_w += dest_x;
        dest_x = 0;
    }
    if( dest_y < 0 ) {
        src_y = -dest_y;
        blit_h += dest_y;
        dest_y = 0;
    }
    if( dest_x + blit_w > owidth ) {
        blit_w = owidth - dest_x;
    }
    if( dest_y + blit_h > oheight ) {
        blit_h = oheight - dest_y;
    }

    if( blit_w > 0 && blit_h > 0 ) {
        int i;

        output += dest_y * ostride;
        for( i = 0; i < blit_h; i++ ) {
            composite_packed4444_to_packed422_scanline( output + ((dest_x) * 2),
                                                        output + ((dest_x) * 2),
                                                        foreground + ((src_y + i) * fstride), blit_w );
            output += ostride;
        }
    }
}

void composite_packed4444_alpha_to_packed422( uint8_t *output,
                                              int owidth, int oheight,
                                              int ostride,
                                              uint8_t *foreground,
                                              int fwidth, int fheight,
                                              int fstride,
                                              int xpos, int ypos, int alpha )
{
    int dest_x, dest_y, src_x, src_y, blit_w, blit_h;

    src_x = 0;
    src_y = 0;
    dest_x = xpos;
    dest_y = ypos;
    blit_w = fwidth;
    blit_h = fheight;

    if( dest_x < 0 ) {
        src_x = -dest_x;
        blit_w += dest_x;
        dest_x = 0;
    }
    if( dest_y < 0 ) {
        src_y = -dest_y;
        blit_h += dest_y;
        dest_y = 0;
    }
    if( dest_x + blit_w > owidth ) {
        blit_w = owidth - dest_x;
    }
    if( dest_y + blit_h > oheight ) {
        blit_h = oheight - dest_y;
    }

    if( blit_w > 0 && blit_h > 0 ) {
        int i;

        output += dest_y * ostride;
        for( i = 0; i < blit_h; i++ ) {
            composite_packed4444_alpha_to_packed422_scanline( output + ((dest_x) * 2),
                                                              output + ((dest_x) * 2),
                                                              foreground + ((src_y + i) * fstride), blit_w, alpha );
            output += ostride;
        }
    }
}

/*
 * 100% SMPTE Color Bars:
 *
 *     top 2/3:  100% white, followed by 100% binary combinations
 *               of R', G', and B' with decreasing luma
 *
 * middle 1/12:  reverse order of above, but with 75% white and
 *               alternating black
 *
 *  bottom 1/4:  -I, 100% white, +Q, black, PLUGE, black,
 *                where PLUGE is (black - 4 IRE), black, (black + 4 IRE)
 *
 */

/* 100% white   */
/* 100% yellow  */
/* 100% cyan    */
/* 100% green   */
/* 100% magenta */
/* 100% red     */
/* 100% blue    */
static uint8_t rainbowRGB[] = {
  255, 255, 255,
  255, 255,   0,
    0, 255, 255,
    0, 255,   0,
  255,   0, 255,
  255,   0,   0,
    0,   0, 255 };
static uint8_t rainbowYCbCr[ 21 ];


/* 100% blue    */
/*      black   */
/* 100% magenta */
/*      black   */
/* 100% cyan    */
/*      black   */
/* 100% white   */
static uint8_t wobnairRGB[] = {
    0,   0, 255,
    0,   0,   0,
  255,   0, 255,
    0,   0,   0,
    0, 255, 255,
    0,   0,   0,
  255, 255, 255 };
static uint8_t wobnairYCbCr[ 21 ];



void create_colourbars_packed444( uint8_t *output, int width, int height, int stride )
{
    int i, x, y, w;
    int bnb_start;
    int pluge_start;
    int stripe_width;
    int pl_width;

    rgb24_to_packed444_rec601_scanline( rainbowYCbCr, rainbowRGB, 7 );
    rgb24_to_packed444_rec601_scanline( wobnairYCbCr, wobnairRGB, 7 );

    bnb_start = height * 2 / 3;
    pluge_start = height * 3 / 4;
    stripe_width = (width + 6) / 7;

    /* Top: Rainbow */
    for( y = 0; y < bnb_start; y++ ) {
        for( i = 0, x = 0; i < 7; i++ ) {
            for( w = 0; (w < stripe_width) && (x < width); w++, x++ ) {
                output[ (y*stride) + (x*3) + 0 ] = rainbowYCbCr[ (i*3) + 0 ];
                output[ (y*stride) + (x*3) + 1 ] = rainbowYCbCr[ (i*3) + 1 ];
                output[ (y*stride) + (x*3) + 2 ] = rainbowYCbCr[ (i*3) + 2 ];
            }
        }
    }


    /* Middle:  Wobnair */
    for(; y < pluge_start; y++ ) {
        for (i = 0, x = 0; i < 7; i++) {
            for (w = 0; (w < stripe_width) && (x < width); w++, x++) {
                output[ (y*stride) + (x*3) + 0 ] = wobnairYCbCr[ (i*3) + 0 ];
                output[ (y*stride) + (x*3) + 1 ] = wobnairYCbCr[ (i*3) + 1 ];
                output[ (y*stride) + (x*3) + 2 ] = wobnairYCbCr[ (i*3) + 2 ];
            }
        }
    }

    for(; y < height; y++ ) {
        /* Bottom:  PLUGE */
        pl_width = 5 * stripe_width / 4;
        /* -I -- [ 0 -1 0 ] == [ 0 0.54464 -0.83867 ] */
        for (x = 0; x < pl_width; x++) {
            output[ (y*stride) + (x*3) + 0 ] = 0;
            output[ (y*stride) + (x*3) + 1 ] = 189;
            output[ (y*stride) + (x*3) + 2 ] = 34;
        }
        /* white */
        for (; x < (2 * pl_width); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 235;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        /* +Q -- well, we use +Cr here */
        /* +Q -- [ 0 0 1 ] == [ 0 0.83867 0.54464 ] */
        for (; x < (3 * pl_width); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 0;
            output[ (y*stride) + (x*3) + 1 ] = 222;
            output[ (y*stride) + (x*3) + 2 ] = 189;
        }
        /* black */
        for (; x < (5 * stripe_width); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 16;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        /* black - 8 (3.75IRE) | black | black + 8  */
        for (; x < (5 * stripe_width) + (stripe_width / 3); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 8;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        for (; x < (5 * stripe_width) + (2 * (stripe_width / 3)); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 16;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        for (; x < (6 * stripe_width); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 24;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        /* black */
        for (; x < width; x++) {
            output[ (y*stride) + (x*3) + 0 ] = 16;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
    }
}

