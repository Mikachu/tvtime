/**
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
 * Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
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

#include <math.h>
#include <stdlib.h>
#include "speedy.h"
#include "videotools.h"

static inline unsigned char clip255( int x )
{
    if( x > 255 ) {
        return 255;
    } else if( x < 0 ) {
        return 0;
    } else {
        return x;
    }
}

/**
 * result = (1 - alpha)B + alpha*F
 *        =  B - alpha*B + alpha*F
 *        =  B + alpha*(F - B)
 */

static inline __attribute__ ((always_inline,const)) int multiply_alpha( int a, int r )
{
    int temp;
    temp = (r * a) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

void blit_colour_packed4444( unsigned char *output, int width, int height,
                             int stride, int alpha, int luma, int cb, int cr )
{
    int i;

    for( i = 0; i < height; i++ ) {
        blit_colour_packed4444_scanline( output + (i * stride), width,
                                         alpha, luma, cb, cr );
    }
}

void blit_colour_packed422( unsigned char *output, int width, int height,
                            int stride, int luma, int cb, int cr )
{
    int i;

    for( i = 0; i < height; i++ ) {
        blit_colour_packed422_scanline( output + (i * stride), width,
                                        luma, cb, cr );
    }
}

void cheap_packed444_to_packed422_scanline( unsigned char *output,
                                            unsigned char *input, int width )
{
    width /= 2;
    while( width-- ) {
        output[ 0 ] = input[ 0 ];
        output[ 1 ] = input[ 1 ];
        output[ 2 ] = input[ 3 ];
        output[ 3 ] = input[ 2 ];
        output += 4;
        input += 6;
    }
}

void cheap_packed422_to_packed444_scanline( unsigned char *output,
                                            unsigned char *input, int width )
{
    width /= 2;
    while( width-- ) {
        output[ 0 ] = input[ 0 ];
        output[ 1 ] = input[ 1 ];
        output[ 2 ] = input[ 3 ];
        output[ 3 ] = input[ 2 ];
        output[ 4 ] = input[ 1 ];
        output[ 5 ] = input[ 3 ];
        output += 6;
        input += 4;
    }
}

void composite_alphamask_to_packed4444( unsigned char *output, int owidth,
                                        int oheight, int ostride,
                                        unsigned char *mask, int mwidth,
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

void composite_alphamask_alpha_to_packed4444( unsigned char *output, int owidth,
                                              int oheight, int ostride,
                                              unsigned char *mask, int mwidth,
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

void composite_packed4444_to_packed422( unsigned char *output, int owidth,
                                        int oheight, int ostride,
                                        unsigned char *foreground, int fwidth,
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

void composite_packed4444_alpha_to_packed422( unsigned char *output,
                                              int owidth, int oheight,
                                              int ostride,
                                              unsigned char *foreground,
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

/**
 * Sub-pixel data bar renderer.  There are 128 bars.
 */
void composite_bars_packed4444_scanline( unsigned char *output,
                                         unsigned char *background, int width,
                                         int a, int luma, int cb, int cr,
                                         int percentage )
{
    /**
     * This is the size of both the bar and the spacing in between in subpixel
     * units out of 256.  Yes, as it so happens, that puts it equal to 'width'.
     */
    int barsize = ( width * 256 ) / 256;
    int i;

    /* We only need to composite the bar on the pixels that matter. */
    for( i = 0; i < percentage; i++ ) {
        int barstart = i * barsize * 2;
        int barend = barstart + barsize;
        int pixstart = barstart / 256;
        int pixend = barend / 256;
        int j;

        for( j = pixstart; j <= pixend; j++ ) {
            unsigned char *curout = output + (j*4);
            unsigned char *curin = background + (j*4);
            int curstart = j * 256;
            int curend = curstart + 256;
            int alpha;

            if( barstart > curstart ) curstart = barstart;
            if( barend < curend ) curend = barend;
            if( curend - curstart < 256 ) {
                alpha = ( ( curend - curstart ) * a ) / 256;
            } else {
                alpha = a;
            }

            curout[ 0 ] = curin[ 0 ] + multiply_alpha( alpha - curin[ 0 ], alpha );
            curout[ 1 ] = curin[ 1 ] + multiply_alpha( luma - curin[ 1 ], alpha );
            curout[ 2 ] = curin[ 2 ] + multiply_alpha( cb - curin[ 2 ], alpha );
            curout[ 3 ] = curin[ 3 ] + multiply_alpha( cr - curin[ 3 ], alpha );
        }
    }
}


const int filterkernel[] = { -1, 3, -6, 12, -24, 80, 80, -24, 12, -6, 3, -1 };
const int kernelsize = sizeof( filterkernel ) / sizeof( int );

void chroma422_to_chroma444_rec601_scanline( unsigned char *dest, unsigned char *src,
                                             int srcwidth )
{
    if( srcwidth ) {
        int halfksize = kernelsize / 2;
        int i;

        /* Handle the first few samples. */
        for( i = 0; i < halfksize; i++ ) {
            int sum = 0;
            int j;

            for( j = 0; j < kernelsize; j++ ) {
                int pos = (i - halfksize + 1) + j;

                if( pos >= 0 && pos < srcwidth ) {
                    sum += filterkernel[ j ] * src[ pos ];
                }
            }

            dest[ i*2 ] = src[ i ];
            dest[ i*2 + 1 ] = ( sum + 128 ) >> 8;
        }

        /* Handle samples where we have the full padding. */
        for( i = halfksize; i < srcwidth - halfksize; i++ ) {
            dest[ i*2 ] = src[ i ];
            dest[ i*2 + 1 ] = ((  (160*(src[i  ] + src[i+1]))
                                - ( 48*(src[i-1] + src[i+2]))
                                + ( 24*(src[i-2] + src[i+3]))
                                - ( 12*(src[i-3] + src[i+4]))
                                + (  6*(src[i-4] + src[i+5]))
                                - (  2*(src[i-5] + src[i+6]))) + 128) >> 8;
        }

        /* Handle last few samples. */
        for( i = srcwidth - halfksize; i < srcwidth; i++ ) {
            int sum = 0;
            int j;

            for( j = 0; j < kernelsize; j++ ) {
                int pos = (i - halfksize + 1) + j;

                if( pos >= 0 && pos < srcwidth ) {
                    sum += filterkernel[ j ] * src[ pos ];
                }
            }

            dest[ i*2 ] = src[ i ];
            dest[ i*2 + 1 ] = ( sum + 128 ) >> 8;
        }
    }
}

/**
 * These are from lavtools in mjpegtools:
 *
 * colorspace.c:  Routines to perform colorspace conversions.
 *
 *  Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define FP_BITS 18

/* precomputed tables */

static int Y_R[256];
static int Y_G[256];
static int Y_B[256];
static int Cb_R[256];
static int Cb_G[256];
static int Cb_B[256];
static int Cr_R[256];
static int Cr_G[256];
static int Cr_B[256];
static int conv_RY_inited = 0;

static int RGB_Y[256];
static int R_Cr[256];
static int G_Cb[256];
static int G_Cr[256];
static int B_Cb[256];
static int conv_YR_inited = 0;

static int myround(double n)
{
  if (n >= 0) 
    return (int)(n + 0.5);
  else
    return (int)(n - 0.5);
}

static void init_RGB_to_YCbCr_tables(void)
{
  int i;

  /*
   * Q_Z[i] =   (coefficient * i
   *             * (Q-excursion) / (Z-excursion) * fixed-point-factor)
   *
   * to one of each, add the following:
   *             + (fixed-point-factor / 2)         --- for rounding later
   *             + (Q-offset * fixed-point-factor)  --- to add the offset
   *             
   */
  for (i = 0; i < 256; i++) {
    Y_R[i] = myround(0.299 * (double)i 
		     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_G[i] = myround(0.587 * (double)i 
		     * 219.0 / 255.0 * (double)(1<<FP_BITS));
    Y_B[i] = myround((0.114 * (double)i 
		      * 219.0 / 255.0 * (double)(1<<FP_BITS))
		     + (double)(1<<(FP_BITS-1))
		     + (16.0 * (double)(1<<FP_BITS)));

    Cb_R[i] = myround(-0.168736 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cb_G[i] = myround(-0.331264 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cb_B[i] = myround((0.500 * (double)i 
		       * 224.0 / 255.0 * (double)(1<<FP_BITS))
		      + (double)(1<<(FP_BITS-1))
		      + (128.0 * (double)(1<<FP_BITS)));

    Cr_R[i] = myround(0.500 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cr_G[i] = myround(-0.418688 * (double)i 
		      * 224.0 / 255.0 * (double)(1<<FP_BITS));
    Cr_B[i] = myround((-0.081312 * (double)i 
		       * 224.0 / 255.0 * (double)(1<<FP_BITS))
		      + (double)(1<<(FP_BITS-1))
		      + (128.0 * (double)(1<<FP_BITS)));
  }
  conv_RY_inited = 1;
}

static void init_YCbCr_to_RGB_tables(void)
{
  int i;

  /*
   * Q_Z[i] =   (coefficient * i
   *             * (Q-excursion) / (Z-excursion) * fixed-point-factor)
   *
   * to one of each, add the following:
   *             + (fixed-point-factor / 2)         --- for rounding later
   *             + (Q-offset * fixed-point-factor)  --- to add the offset
   *             
   */

  /* clip Y values under 16 */
  for (i = 0; i < 16; i++) {
    RGB_Y[i] = myround((1.0 * (double)(16) 
		     * 255.0 / 219.0 * (double)(1<<FP_BITS))
		    + (double)(1<<(FP_BITS-1)));
  }
  for (i = 16; i < 236; i++) {
    RGB_Y[i] = myround((1.0 * (double)(i - 16) 
		     * 255.0 / 219.0 * (double)(1<<FP_BITS))
		    + (double)(1<<(FP_BITS-1)));
  }
  /* clip Y values above 235 */
  for (i = 236; i < 256; i++) {
    RGB_Y[i] = myround((1.0 * (double)(235) 
		     * 255.0 / 219.0 * (double)(1<<FP_BITS))
		    + (double)(1<<(FP_BITS-1)));
  }
    
  /* clip Cb/Cr values below 16 */	 
  for (i = 0; i < 16; i++) {
    R_Cr[i] = myround(1.402 * (double)(-112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    G_Cr[i] = myround(-0.714136 * (double)(-112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    G_Cb[i] = myround(-0.344136 * (double)(-112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    B_Cb[i] = myround(1.772 * (double)(-112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
  }
  for (i = 16; i < 241; i++) {
    R_Cr[i] = myround(1.402 * (double)(i - 128)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    G_Cr[i] = myround(-0.714136 * (double)(i - 128)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    G_Cb[i] = myround(-0.344136 * (double)(i - 128)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    B_Cb[i] = myround(1.772 * (double)(i - 128)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
  }
  /* clip Cb/Cr values above 240 */	 
  for (i = 241; i < 256; i++) {
    R_Cr[i] = myround(1.402 * (double)(112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    G_Cr[i] = myround(-0.714136 * (double)(112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    G_Cb[i] = myround(-0.344136 * (double)(i - 128)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
    B_Cb[i] = myround(1.772 * (double)(112)
		   * 255.0 / 224.0 * (double)(1<<FP_BITS));
  }
  conv_YR_inited = 1;
}

void rgb24_to_packed444_rec601_scanline( unsigned char *output,
                                         unsigned char *input, int width )
{
    if( !conv_RY_inited ) init_RGB_to_YCbCr_tables();

    while( width-- ) {
        int r = input[ 0 ];
        int g = input[ 1 ];
        int b = input[ 2 ];

        output[ 0 ] = (Y_R[ r ] + Y_G[ g ] + Y_B[ b ]) >> FP_BITS;
        output[ 1 ] = (Cb_R[ r ] + Cb_G[ g ] + Cb_B[ b ]) >> FP_BITS;
        output[ 2 ] = (Cr_R[ r ] + Cr_G[ g ] + Cr_B[ b ]) >> FP_BITS;
        output += 3;
        input += 3;
    }
}

void rgba32_to_packed4444_rec601_scanline( unsigned char *output,
                                           unsigned char *input, int width )
{
    if( !conv_RY_inited ) init_RGB_to_YCbCr_tables();

    while( width-- ) {
        int r = input[ 0 ];
        int g = input[ 1 ];
        int b = input[ 2 ];
        int a = input[ 3 ];
        
        output[ 0 ] = a;
        output[ 1 ] = (Y_R[ r ] + Y_G[ g ] + Y_B[ b ]) >> FP_BITS;
        output[ 2 ] = (Cb_R[ r ] + Cb_G[ g ] + Cb_B[ b ]) >> FP_BITS;
        output[ 3 ] = (Cr_R[ r ] + Cr_G[ g ] + Cr_B[ b ]) >> FP_BITS;
        output += 4;
        input += 4;
    }
}

void packed444_to_rgb24_rec601_scanline( unsigned char *output,
                                         unsigned char *input, int width )
{
    if( !conv_YR_inited ) init_YCbCr_to_RGB_tables();

    while( width-- ) {
        int luma = input[ 0 ];
        int cb = input[ 1 ];
        int cr = input[ 2 ];

        output[ 0 ] = clip255( (RGB_Y[ luma ] + R_Cr[ cr ]) >> FP_BITS );
        output[ 1 ] = clip255( (RGB_Y[ luma ] + G_Cb[ cb ] + G_Cr[cr]) >> FP_BITS );
        output[ 2 ] = clip255( (RGB_Y[ luma ] + B_Cb[ cb ]) >> FP_BITS );

        output += 3;
        input += 3;
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
static unsigned char rainbowRGB[] = {
  255, 255, 255,
  255, 255,   0,
    0, 255, 255,
    0, 255,   0,
  255,   0, 255,
  255,   0,   0,
    0,   0, 255 };
static unsigned char rainbowYCbCr[ 21 ];


/* 100% blue    */
/*      black   */
/* 100% magenta */
/*      black   */
/* 100% cyan    */
/*      black   */
/* 100% white   */
static unsigned char wobnairRGB[] = {
    0,   0, 255,
    0,   0,   0,
  255,   0, 255,
    0,   0,   0,
    0, 255, 255,
    0,   0,   0,
  255, 255, 255 };
static unsigned char wobnairYCbCr[ 21 ];



void create_colourbars_packed444( unsigned char *output,
                                  int width, int height, int stride )
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

