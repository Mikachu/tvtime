/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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
#include "videocorrection.h"

struct video_correction_s
{
    int source_black_level;
    int source_white_level;
    int target_black_level;
    int target_white_level;

    int source_chroma_start;
    int source_chroma_end;
    int target_chroma_start;
    int target_chroma_end;

    double luma_correction;
    unsigned int *luma_table;
    unsigned int *chroma_table;
    unsigned char *temp_scanline_data;
};


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

/*
    for( i = 0; i < 256; i++ ) {
        if( i < vc->source_chroma_start ) {
            vc->chroma_table[ i ] = vc->target_chroma_start;
        } else if( i > vc->source_chroma_end ) {
            vc->chroma_table[ i ] = vc->target_chroma_end;
        } else {
            double curval, target;

            curval = ( (double) (i - vc->source_chroma_start) ) / ( (double) (vc->source_chroma_end - vc->source_chroma_start) );
            target = curval * ( (double) (vc->target_chroma_end - vc->target_chroma_start) );
            vc->chroma_table[ i ] = ( (int) (target + 0.5) ) + vc->target_chroma_start;
        }
    }
*/
}

video_correction_t *video_correction_new( int bt8x8_correction, int full_extent_correction )
{
    video_correction_t *vc = (video_correction_t *) malloc( sizeof( video_correction_t ) );
    if( !vc ) {
        return 0;
    }

    /* Support the broken bt8x8 extents, plus the bt8x8 in full luma mode */
    if( bt8x8_correction && !full_extent_correction ) {
        vc->source_black_level = 16;
        vc->source_white_level = 253;
        vc->source_chroma_start = 2;
        vc->source_chroma_end   = 253;
    } else if( bt8x8_correction && full_extent_correction ) {
        vc->source_black_level = 0;
        vc->source_white_level = 255;
        vc->source_chroma_start = 2;
        vc->source_chroma_end   = 253;
    } else if( !bt8x8_correction && full_extent_correction ) {
        vc->source_black_level = 0;
        vc->source_white_level = 255;
        vc->source_chroma_start = 0;
        vc->source_chroma_end   = 255;
    } else {
        vc->source_black_level = 16;
        vc->source_white_level = 235;
        vc->source_chroma_start = 16;
        vc->source_chroma_end   = 240;
    }

    /* We assume for now that we're always outputting to a Rec.601-compliant
     * video surface.  In the future maybe I can add an option for the broken
     * colourspace used by voodoo-based cards?
     */
    vc->target_black_level = 16;
    vc->target_white_level = 235;
    vc->target_chroma_start = 16;
    vc->target_chroma_end   = 240;

    vc->luma_correction = 1.0;
    vc->luma_table = (unsigned int *) malloc( 256 * sizeof( int ) );
    vc->chroma_table = (unsigned int *) malloc( 256* sizeof( int ) );
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
    free( vc->temp_scanline_data );
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
    unsigned int *table = vc->luma_table;
    while( width-- ) {
        *output++ = table[ *input++ ];
        *output++ = *input++;
    }
}

