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
#include "videotools.h"

struct video_correction_s
{
    int source_black_level;
    int target_black_level;
    int source_white_level;
    int target_white_level;
    double luma_correction;
    unsigned char *luma_table;
    unsigned char *chroma_table;
    unsigned char *temp_scanline_data;
};

void create_packed422_from_planar422_scanline( unsigned char *output, unsigned char *luma,
                                               unsigned char *cb, unsigned char *cr, int width )
{
    unsigned int *o = (unsigned int *) output;

    for( width /= 2; width; --width ) {
        *o = (*cr << 24) | (*(luma + 1) << 16) | (*cb << 8) | (*luma);
        o++;
        luma += 2;
        cb++;
        cr++;
    }
}

void blit_colour_packed4444_scanline( unsigned char *output, int width,
                                      int alpha, int luma, int cb, int cr )
{
    int j;

    for( j = 0; j < width; j++ ) {
        *output++ = alpha;
        *output++ = luma;
        *output++ = cb;
        *output++ = cr;
    }
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

void blit_colour_packed422_scanline( unsigned char *output, int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;
    unsigned int *o = (unsigned int *) output;

    for( width /= 2; width; --width ) {
        *o++ = colour;
    }
}

void blit_colour_packed422( unsigned char *output, int width, int height, int stride, int luma, int cb, int cr )
{
    int i;

    for( i = 0; i < height; i++ ) {
        blit_colour_packed422_scanline( output + (i * stride), width, luma, cb, cr );
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

void interpolate_packed422_from_planar422_scanline( unsigned char *output,
                                                    unsigned char *topluma,
                                                    unsigned char *topcb,
                                                    unsigned char *topcr,
                                                    unsigned char *botluma,
                                                    unsigned char *botcb,
                                                    unsigned char *botcr,
                                                    int width )
{
    int i;

    for( i = 0; i < width; i += 2 ) {
        output[ (i*2)     ] = (topluma[ i ] + botluma[ i ]) >> 1;
        output[ (i*2) + 1 ] = ((topcb[ i/2 ] + botcb[ i/2 ] - 256) >> 1) + 128;
        output[ (i*2) + 2 ] = (topluma[ i+1 ] + botluma[ i+1 ]) >> 1;
        output[ (i*2) + 3 ] = ((topcr[ i/2 ] + botcr[ i/2 ] - 256) >> 1) + 128;
    }
}

void interpolate_packed422_scanline( unsigned char *output, unsigned char *top,
                                     unsigned char *bot, int width )
{
    int i;

    for( i = width*2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }
}

/**
 * The kernel for this filter comes from ffmpeg, http://ffmpeg.sf.net/
 * [-1 4 2 4 -1]
 * The whole codebase for using better filters needs to be improved though.
 */
void interpolate_packed422_scanline_filter( unsigned char *output,
                                            unsigned char *top2,
                                            unsigned char *top1,
                                            unsigned char *mid,
                                            unsigned char *bot1,
                                            unsigned char *bot2, int width )
{
    int i;

    for( i = width; i; --i ) {
        int temp;
        temp = ( ((*mid)<<1)
                      + (((*top1) + (*bot1))<<2)
                      - (*top2) - (*bot2) ) >>3;
        if( temp < 0 ) temp = 0;
        if( temp > 255 ) temp = 255;

        *output++ = temp;
        top2++; top1++; mid++; bot1++; bot2++;

        *output++ = *top1;
        top2++; top1++; mid++; bot1++; bot2++;
    }
}

void video_correction_packed422_field_to_frame_bot( video_correction_t *vc,
                                                    unsigned char *output,
                                                    int outstride,
                                                    unsigned char *field,
                                                    int fieldwidth,
                                                    int fieldheight,
                                                    int fieldstride )
{
    int i;

    /* Clear a scanline. */
    blit_colour_packed422_scanline( output, fieldwidth, 16, 128, 128 );
    output += outstride;

    /* Copy a scanline. */
    video_correction_correct_packed422_scanline( vc, output, field,
                                                 fieldwidth );
    field += fieldstride;
    output += outstride;

    for( i = 0; i < fieldheight - 1; i++ ) {
        /* Copy a scanline. */
        video_correction_correct_packed422_scanline( vc, output + outstride,
                                                     field, fieldwidth );

        /* Interpolate a scanline. */
        interpolate_packed422_scanline( output, output - outstride,
                                        output + outstride, fieldwidth );

        output += outstride * 2;
        field += fieldstride;
    }
}

void video_correction_packed422_field_to_frame_top( video_correction_t *vc,
                                                    unsigned char *output,
                                                    int outstride,
                                                    unsigned char *field,
                                                    int fieldwidth,
                                                    int fieldheight,
                                                    int fieldstride )
{
    int i;

    /* Copy a scanline. */
    video_correction_correct_packed422_scanline( vc, output,
                                                 field, fieldwidth );
    output += outstride;
    field += fieldstride;

    for( i = 0; i < fieldheight - 1; i++ ) {
        /* Copy a scanline. */
        video_correction_correct_packed422_scanline( vc, output + outstride,
                                                     field, fieldwidth );

        /* Interpolate a scanline. */
        interpolate_packed422_scanline( output, output - outstride,
                                        output + outstride, fieldwidth );

        output += outstride * 2;
        field += fieldstride;
    }

    /* Clear a scanline. */
    blit_colour_packed422_scanline( output, fieldwidth, 16, 128, 128 );
}

void packed422_field_to_frame_bot( unsigned char *output, int outstride,
                                   unsigned char *field, int fieldwidth,
                                   int fieldheight, int fieldstride )
{
    int i;

    /* Clear a scanline. */
    blit_colour_packed422_scanline( output, fieldwidth, 16, 128, 128 );
    output += outstride;

    /* Copy a scanline. */
    memcpy( output, field, fieldwidth*2 );
    field += fieldstride;
    output += outstride;

    for( i = 0; i < fieldheight - 1; i++ ) {
        /* Copy a scanline. */
        memcpy( output + outstride, field, fieldwidth*2 );

        /* Interpolate a scanline. */
        interpolate_packed422_scanline( output, output - outstride,
                                        output + outstride, fieldwidth );

        output += outstride * 2;
        field += fieldstride;
    }
}

void packed422_field_to_frame_top( unsigned char *output, int outstride,
                                   unsigned char *field, int fieldwidth,
                                   int fieldheight, int fieldstride )
{
    int i;

    /* Copy a scanline. */
    memcpy( output, field, fieldwidth*2 );
    output += outstride;
    field += fieldstride;

    for( i = 0; i < fieldheight - 1; i++ ) {
        /* Copy a scanline. */
        memcpy( output + outstride, field, fieldwidth*2 );

        /* Interpolate a scanline. */
        interpolate_packed422_scanline( output, output - outstride,
                                        output + outstride, fieldwidth );

        output += outstride * 2;
        field += fieldstride;
    }

    /* Clear a scanline. */
    blit_colour_packed422_scanline( output, fieldwidth, 16, 128, 128 );
}

void packed422_field_to_frame_bot_filter( unsigned char *output, int outstride,
                                          unsigned char *field, int fieldwidth,
                                          int fieldheight, int fieldstride )
{
    int i;

    /* Clear a scanline. */
    blit_colour_packed422_scanline( output, fieldwidth, 16, 128, 128 );
    output += outstride;

    /* Copy a scanline. */
    memcpy( output, field, fieldwidth*2 );
    field += fieldstride;
    output += outstride;

    for( i = 0; i < fieldheight - 1; i++ ) {
        unsigned char *top2, *top1, *mid, *bot1, *bot2;

        /* Copy a scanline. */
        memcpy( output + outstride, field, fieldwidth*2 );

        if( i > 2 && i < fieldheight - 3 ) {
            top2 = field - fieldstride - (fieldstride/2);
            top1 = field - fieldstride;
            mid  = field - (fieldstride/2);
            bot1 = field;
            bot2 = field + (fieldstride/2);
        } else {
            top2 = top1 = mid = bot1 = bot2 = field;
        }

        /* Interpolate a scanline. */
        interpolate_packed422_scanline_filter( output, top2, top1, mid,
                                               bot1, bot2, fieldwidth );

        output += outstride * 2;
        field += fieldstride;
    }
}

void packed422_field_to_frame_top_filter( unsigned char *output, int outstride,
                                          unsigned char *field, int fieldwidth,
                                          int fieldheight, int fieldstride )
{
    int i;

    /* Copy a scanline. */
    memcpy( output, field, fieldwidth*2 );
    field += fieldstride;
    output += outstride;

    for( i = 0; i < fieldheight - 1; i++ ) {
        unsigned char *top2, *top1, *mid, *bot1, *bot2;

        /* Copy a scanline. */
        memcpy( output + outstride, field, fieldwidth*2 );

        if( i > 2 && i < fieldheight - 3 ) {
            top2 = field - (fieldstride) - (fieldstride/2);
            top1 = field - (fieldstride);
            mid  = field - (fieldstride/2);
            bot1 = field;
            bot2 = field + (fieldstride/2);
        } else {
            top2 = top1 = mid = bot1 = bot2 = field;
        }

        /* Interpolate a scanline. */
        interpolate_packed422_scanline_filter( output, top2, top1, mid,
                                               bot1, bot2, fieldwidth );

        output += outstride * 2;
        field += fieldstride;
    }

    /* Clear a scanline. */
    blit_colour_packed422_scanline( output, fieldwidth, 16, 128, 128 );
}


static double intensity_to_voltage( video_correction_t *vc, double val )
{   
    /* Handle invalid values before doing a gamma transform. */
    if( val < 0.0 ) return 0.0;
    if( val > 1.0 ) return 1.0;

    return pow( val, 1.0 / vc->luma_correction );
}

static void video_correction_update_table( video_correction_t *vc )
{
    int i;

    for( i = 0; i < 256; i++ ) {
        if( i < vc->source_black_level ) {
            vc->luma_table[ i ] = vc->target_black_level;
        } else if( i > vc->source_white_level ) {
            vc->luma_table[ i ] = vc->target_white_level;
        } else {
            double curval, target;

            curval = ( (double) (i - vc->source_black_level) ) / ( (double) (vc->source_white_level - vc->source_black_level) );
            curval = intensity_to_voltage( vc, curval );
            target = curval * ( (double) (vc->target_white_level - vc->target_black_level) );
            vc->luma_table[ i ] = ( (int) (target + 0.5) ) + vc->target_black_level;
        }
    }
}

video_correction_t *video_correction_new( void )
{
    video_correction_t *vc = (video_correction_t *) malloc( sizeof( video_correction_t ) );
    if( !vc ) {
        return 0;
    }

    /* These are set this way to compensate for the bttv. */
    vc->source_black_level = 16;
    vc->source_white_level = 253;
    vc->target_black_level = 16;
    vc->target_white_level = 235;
    vc->luma_correction = 1.0;
    vc->luma_table = (unsigned char *) malloc( 256 );
    vc->chroma_table = (unsigned char *) malloc( 256 );
    vc->temp_scanline_data = (unsigned char *) malloc( 2048 * 2 );
    if( !vc->luma_table || !vc->chroma_table ) {
        if( vc->luma_table ) free( vc->luma_table );
        if( vc->chroma_table ) free( vc->chroma_table );
        free( vc );
        return 0;
    }
    video_correction_update_table( vc );

    return vc;
}

void video_correction_delete( video_correction_t *vc )
{
    free( vc->luma_table );
    free( vc->chroma_table );
    free( vc );
}

void video_correction_set_luma_power( video_correction_t *vc, double power )
{
    vc->luma_correction = power;
    video_correction_update_table( vc );
}

void video_correction_correct_luma_scanline( video_correction_t *vc, unsigned char *output, unsigned char *luma, int width )
{
    while( width-- ) {
        *output++ = vc->luma_table[ *luma++ ];
    }
}

void video_correction_correct_packed422_scanline( video_correction_t *vc,
                                                  unsigned char *output,
                                                  unsigned char *input, int width )
{
    while( width-- ) {
        *output++ = vc->luma_table[ *input++ ];
        *output++ = *input++;
    }
}

void composite_alphamask_packed422_scanline( unsigned char *output, unsigned char *input,
                                             unsigned char *mask, int width,
                                             int textluma, int textcb, int textcr, double textalpha )
{
    int i;

    if( textalpha > 1.0 ) textalpha = 1.0;
    if( textalpha < 0.0 ) textalpha = 0.0;

    for( i = 0; i < width; i++ ) {
        if( mask[ 0 ] ) {
            int a = (int) ( ( (double) mask[ 0 ] * textalpha ) + 0.5 );
            int tmp1, tmp2;

            tmp1 = (textluma - input[ 0 ]) * a;
            tmp2 = input[ 0 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *output = tmp2 & 0xff;

            if( ( i & 1 ) == 0 ) {
                tmp1 = (textcb - input[ 1 ]) * a;
                tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                output[ 1 ] = tmp2 & 0xff;

                tmp1 = (textcr - input[ 3 ]) * a;
                tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                output[ 3 ] = tmp2 & 0xff;
            }
        }
        mask++;
        output += 2;
        input += 2;
    }
}

void composite_alphamask_packed4444_scanline( unsigned char *output, unsigned char *input,
                                              unsigned char *mask, int width,
                                              int textluma, int textcb, int textcr )
{
    int i;

    for( i = 0; i < width; i++ ) {
        if( mask[ 0 ] ) {
            int a = mask[ 0 ];
            int tmp1, tmp2;

            tmp1 = (a - input[ 0 ]) * a;
            tmp2 = input[ 0 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            output[ 0 ] = tmp2 & 0xff;

            tmp1 = (textluma - input[ 1 ]) * a;
            tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            output[ 1 ] = tmp2 & 0xff;

            tmp1 = (textcb - input[ 2 ]) * a;
            tmp2 = input[ 2 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            output[ 2 ] = tmp2 & 0xff;

            tmp1 = (textcr - input[ 3 ]) * a;
            tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            output[ 3 ] = tmp2 & 0xff;
        }
        mask++;
        output += 4;
        input += 4;
    }
}

void composite_alphamask_packed4444( unsigned char *output, int owidth, int oheight, int ostride,
                                     unsigned char *mask, int mwidth, int mheight, int mstride,
                                     int luma, int cb, int cr, int xpos, int ypos )
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
            composite_alphamask_packed4444_scanline( output + ((dest_x) * 4),
                                                     output + ((dest_x) * 4),
                                                     mask + ((src_y + i)*mstride),
                                                     blit_w, luma, cb, cr );
            output += ostride;
        }
    }
}

void composite_packed4444_to_packed422_scanline( unsigned char *output, unsigned char *input,
                                                 unsigned char *foreground, int width )
{
    int i;

    for( i = 0; i < width; i++ ) {
        if( foreground[ 0 ] ) {
            int a = foreground[ 0 ];
            int tmp1, tmp2;

            tmp1 = (foreground[ 1 ] - input[ 0 ]) * a;
            tmp2 = input[ 0 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *output = tmp2 & 0xff;

            if( ( i & 1 ) == 0 ) {
                if( 1 ) { // i == 0 || i == (width-1) ) {
                    tmp1 = (foreground[ 2 ] - input[ 1 ]) * a;
                    tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 1 ] = tmp2 & 0xff;

                    tmp1 = (foreground[ 3 ] - input[ 3 ]) * a;
                    tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 3 ] = tmp2 & 0xff;
                } else {
                    //int cb = (( foreground[ 2 + 4 ] << 1 ) + foreground[ 2 - 0 ] + foreground[ 2 + 8 ])>>2;
                    //int cr = (( foreground[ 3 + 4 ] << 1 ) + foreground[ 3 - 0 ] + foreground[ 3 + 8 ])>>2;
                    //int cb = foreground[ 2 + 4 - (720*4)];
                    //int cr = foreground[ 3 + 4 - (720*4)];
                    int cb = foreground[ 2 ];
                    int cr = foreground[ 3 ];

                    tmp1 = (cb - input[ 1 ]) * a;
                    tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 1 ] = tmp2 & 0xff;

                    tmp1 = (cr - input[ 3 ]) * a;
                    tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 3 ] = tmp2 & 0xff;
                }
            }
        }
        foreground += 4;
        output += 2;
        input += 2;
    }
}

void composite_packed4444_to_packed422( unsigned char *output, int owidth, int oheight, int ostride,
                                        unsigned char *foreground, int fwidth, int fheight, int fstride,
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

void composite_packed4444_alpha_to_packed422_scanline( unsigned char *output, unsigned char *input,
                                                       unsigned char *foreground, int width, int alpha )
{
    int i;

    for( i = 0; i < width; i++ ) {
        if( foreground[ 0 ] ) {
            int a = ((foreground[ 0 ]*alpha)+0x80)>>8;
            int tmp1, tmp2;

            tmp1 = (foreground[ 1 ] - input[ 0 ]) * a;
            tmp2 = input[ 0 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *output = tmp2 & 0xff;

            if( ( i & 1 ) == 0 ) {
                if( 1 ) { // i == 0 || i == (width-1) ) {
                    tmp1 = (foreground[ 2 ] - input[ 1 ]) * a;
                    tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 1 ] = tmp2 & 0xff;

                    tmp1 = (foreground[ 3 ] - input[ 3 ]) * a;
                    tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 3 ] = tmp2 & 0xff;
                } else {
                    //int cb = (( foreground[ 2 + 4 ] << 1 ) + foreground[ 2 - 0 ] + foreground[ 2 + 8 ])>>2;
                    //int cr = (( foreground[ 3 + 4 ] << 1 ) + foreground[ 3 - 0 ] + foreground[ 3 + 8 ])>>2;
                    //int cb = foreground[ 2 + 4 - (720*4) ];
                    //int cr = foreground[ 3 + 4 - (720*4) ];
                    int cb = foreground[ 2 ];
                    int cr = foreground[ 3 ];

                    tmp1 = (cb - input[ 1 ]) * a;
                    tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 1 ] = tmp2 & 0xff;

                    tmp1 = (cr - input[ 3 ]) * a;
                    tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                    output[ 3 ] = tmp2 & 0xff;
                }
            }
        }
        foreground += 4;
        output += 2;
        input += 2;
    }
}

void composite_packed4444_alpha_to_packed422( unsigned char *output, int owidth, int oheight, int ostride,
                                              unsigned char *foreground, int fwidth, int fheight, int fstride,
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
 * Color Bars:
 *
 *     top 2/3:  75% white, followed by 75% binary combinations
 *                of R', G', and B' with decreasing luma
 *
 * middle 1/12:  reverse order of above, but with 75% white and
 *                alternating black
 *
 *  bottom 1/4:  -I, 100% white, +Q, black, PLUGE, black,
 *                where PLUGE is (black - 4 IRE), black, (black + 4 IRE)
 *
 */

/*  75% white   */
/*  75% yellow  */
/*  75% cyan    */
/*  75% green   */
/*  75% magenta */
/*  75% red     */
/*  75% blue    */
static unsigned char rainbowRGB[] = {
  191, 191, 191,
  191, 191,   0,
    0, 191, 191,
    0, 191,   0,
  191,   0, 191,
  191,   0,   0,
    0,   0, 191 };
static unsigned char rainbowYCbCr[ 21 ];


/*  75% blue    */
/*      black   */
/*  75% magenta */
/*      black   */
/*  75% cyan    */
/*      black   */
/*  75% white   */
static unsigned char wobnairRGB[] = {
    0,   0, 191,
    0,   0,   0,
  191,   0, 191,
    0,   0,   0,
    0, 191, 191,
    0,   0,   0,
  191, 191, 191 };
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
        /* -I -- well, we use -Cb here */
        for (x = 0; x < pl_width; x++) {
            output[ (y*stride) + (x*3) + 0 ] = 0;
            output[ (y*stride) + (x*3) + 1 ] = 16;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        /* white */
        for (; x < (2 * pl_width); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 235;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 128;
        }
        /* +Q -- well, we use +Cr here */
        for (; x < (3 * pl_width); x++) {
            output[ (y*stride) + (x*3) + 0 ] = 0;
            output[ (y*stride) + (x*3) + 1 ] = 128;
            output[ (y*stride) + (x*3) + 2 ] = 240;
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

