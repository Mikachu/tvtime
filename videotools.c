/**
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
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

void luma_plane_field_to_frame( unsigned char *output, unsigned char *input,
                                int width, int height, int stride, int bottom_field )
{
    int i, j;

    if( bottom_field ) {
        memset( output, 16, width );
        output += width;

        memcpy( output, input, width );
        output += width;
    }

    for( i = 0; i < (height / 2) - 1; i++ ) {
        unsigned char *c;

        if( !bottom_field ) {
            memcpy( output, input, width );
            output += width;
        }

        c = input;
        for( j = 0; j < width; j++ ) {
            *output = (*c + *(c+stride)) >> 1;
            output++;
            c++;
        }
        input += stride;

        if( bottom_field ) {
            memcpy( output, input, width );
            output += width;
        }
    }

    if( !bottom_field ) {
        memcpy( output, input, width );
        output += width;

        memset( output, 16, width );
    }
}

void chroma_plane_field_to_frame( unsigned char *output, unsigned char *input,
                                  int width, int height, int stride, int bottom_field )
{
    int i, j;

    if( bottom_field ) {
        memset( output, 128, width );
        output += width;

        memcpy( output, input, width );
        output += width;
    }

    for( i = 0; i < (height / 2) - 1; i++ ) {
        unsigned char *c;

        if( !bottom_field ) {
            memcpy( output, input, width );
            output += width;
        }

        c = input;
        for( j = 0; j < width; j++ ) {
            *output = ((*c + *(c+stride) - 256) >> 1) + 128;
            output++;
            c++;
        }
        input += stride;

        if( bottom_field ) {
            memcpy( output, input, width );
            output += width;
        }
    }

    if( !bottom_field ) {
        memcpy( output, input, width );
        output += width;

        memset( output, 128, width );
    }
}

static inline void clear_scanline_packed_422( unsigned char *output, int size )
{
    static int clear = 128 << 24 | 16 << 16 | 128 << 8 | 16;
    unsigned int *o = (unsigned int *) output;

    for( size /= 4; size; --size ) {
        *o++ = clear;
    }
}

static inline void copy_scanline_packed_422( unsigned char *output, unsigned char *luma,
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

static inline void interpolate_scanline_packed_422( unsigned char *output,
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


void video_correction_planar422_field_to_packed422_frame( video_correction_t *vc,
                                                          unsigned char *output,
                                                          unsigned char *fieldluma,
                                                          unsigned char *fieldcb,
                                                          unsigned char *fieldcr,
                                                          int bottom_field,
                                                          int lstride, int cstride,
                                                          int width, int height )
{
    int i;

    if( bottom_field ) {
        /* Clear a scanline. */
        clear_scanline_packed_422( output, width * 2 );
        output += width * 2;

        /* Copy a scanline. */
        video_correction_correct_luma_scanline( vc, vc->temp_scanline_data, fieldluma, width );
        copy_scanline_packed_422( output, vc->temp_scanline_data, fieldcb, fieldcr, width );
        output += width * 2;
    }

    for( i = 0; i < (height / 2) - 1; i++ ) {
        /* Correct top scanline. */
        video_correction_correct_luma_scanline( vc, vc->temp_scanline_data, fieldluma, width );

        if( !bottom_field ) {
            /* Copy a scanline. */
            copy_scanline_packed_422( output, vc->temp_scanline_data, fieldcb, fieldcr, width );
            output += width * 2;
        }

        /* Interpolate a scanline. */
        video_correction_correct_luma_scanline( vc, vc->temp_scanline_data + width, fieldluma + lstride, width );
        interpolate_scanline_packed_422( output, vc->temp_scanline_data, fieldcb, fieldcr,
                                         vc->temp_scanline_data + width, fieldcb + cstride, fieldcr + cstride,
                                         width );
        output += width * 2;

        fieldluma += lstride;
        fieldcb += cstride;
        fieldcr += cstride;

        if( bottom_field ) {
            /* Copy a scanline. */
            copy_scanline_packed_422( output, vc->temp_scanline_data + width, fieldcb, fieldcr, width );
            output += width * 2;
        }
    }

    if( !bottom_field ) {
        /* Copy a scanline. */
        video_correction_correct_luma_scanline( vc, vc->temp_scanline_data, fieldluma, width );
        copy_scanline_packed_422( output, vc->temp_scanline_data, fieldcb, fieldcr, width );
        output += width * 2;

        /* Clear a scanline. */
        clear_scanline_packed_422( output, width * 2 );
    }
}

void planar422_field_to_packed422_frame( unsigned char *output,
                                         unsigned char *fieldluma,
                                         unsigned char *fieldcb,
                                         unsigned char *fieldcr,
                                         int bottom_field,
                                         int lstride, int cstride,
                                         int width, int height )
{
    int i;

    if( bottom_field ) {
        /* Clear a scanline. */
        clear_scanline_packed_422( output, width * 2 );
        output += width * 2;

        /* Copy a scanline. */
        copy_scanline_packed_422( output, fieldluma, fieldcb, fieldcr, width );
        output += width * 2;
    }

    for( i = 0; i < (height / 2) - 1; i++ ) {
        if( !bottom_field ) {
            /* Copy a scanline. */
            copy_scanline_packed_422( output, fieldluma, fieldcb, fieldcr, width );
            output += width * 2;
        }

        /* Interpolate a scanline. */
        interpolate_scanline_packed_422( output, fieldluma, fieldcb, fieldcr,
                                         fieldluma + lstride, fieldcb + cstride, fieldcr + cstride,
                                         width );
        output += width * 2;

        fieldluma += lstride;
        fieldcb += cstride;
        fieldcr += cstride;

        if( bottom_field ) {
            /* Copy a scanline. */
            copy_scanline_packed_422( output, fieldluma, fieldcb, fieldcr, width );
            output += width * 2;
        }
    }

    if( !bottom_field ) {
        /* Copy a scanline. */
        copy_scanline_packed_422( output, fieldluma, fieldcb, fieldcr, width );
        output += width * 2;

        /* Clear a scanline. */
        clear_scanline_packed_422( output, width * 2 );
    }
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
    vc->temp_scanline_data = (unsigned char *) malloc( 720 * 2 );
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

void composite_textmask_packed422_scanline( unsigned char *output, unsigned char *input,
                                            unsigned char *textmask, int width,
                                            int textluma, int textcb, int textcr, double textalpha )
{
    int alpha_table[ 5 ];
    int i;

    if( textalpha > 1.0 ) textalpha = 1.0;
    if( textalpha < 0.0 ) textalpha = 0.0;
    for( i = 0; i < 5; i++ ) {
        alpha_table[ i ] = (int) ( ( ( ( ( (double) i ) * textalpha ) / 5.0 ) * 255.0 ) + 0.5 );
    }

    for( i = 0; i < width; i++ ) {
        if( textmask[ 0 ] ) {
            int a = alpha_table[ textmask[ 0 ] ];
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
        textmask++;
        output += 2;
        input += 2;
    }
}

const int filterkernel[] = { -1, 3, -6, 12, -24, 80, 80, -24, 12, -6, 3, -1 };
const int kernelsize = sizeof( filterkernel ) / sizeof( int );

void scanline_chroma422_to_chroma444_rec601( unsigned char *dest, unsigned char *src, int srcwidth )
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

