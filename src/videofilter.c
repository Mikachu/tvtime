/**
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "videocorrection.h"
#include "speedy.h"
#include "videofilter.h"

struct videofilter_s
{
    video_correction_t *vc;
    int bt8x8_correction;
    int uyvy_conversion;

    uint8_t tempscanline[ 2048 ];
};

videofilter_t *videofilter_new( void )
{
    videofilter_t *vf = malloc( sizeof( videofilter_t ) );
    if( !vf ) return 0;

    vf->vc = video_correction_new( 1, 0 );
    vf->bt8x8_correction = 0;
    vf->uyvy_conversion = 0;

    return vf;
}

void videofilter_delete( videofilter_t *vf )
{
    if( vf->vc ) video_correction_delete( vf->vc );
    free( vf );
}

void videofilter_set_bt8x8_correction( videofilter_t *vf, int correct )
{
    vf->bt8x8_correction = correct;
}

void videofilter_set_full_extent_correction( videofilter_t *vf, int correct )
{
}

void videofilter_set_luma_power( videofilter_t *vf, double power )
{
    if( vf->vc ) video_correction_set_luma_power( vf->vc, power );
}

void videofilter_enable_uyvy_conversion( videofilter_t *vf )
{
    vf->uyvy_conversion = 1;
}

int videofilter_active_on_scanline( videofilter_t *vf, int scanline )
{
    if( vf->uyvy_conversion || (vf->vc && vf->bt8x8_correction) ) {
        return 1;
    } else {
        return 0;
    }
}

void videofilter_packed422_scanline( videofilter_t *vf, uint8_t *data,
                                     int width, int xpos, int scanline )
{
    if( vf->uyvy_conversion ) {
        convert_uyvy_to_yuyv_scanline( data, data, width );
    }

    if( vf->vc && vf->bt8x8_correction ) {
        video_correction_correct_packed422_scanline( vf->vc, data, data, width );
        // filter_luma_121_packed422_inplace_scanline( data, width );
        // filter_luma_14641_packed422_inplace_scanline( data, width );
        // mirror_packed422_inplace_scanline( data, width );
        // halfmirror_packed422_inplace_scanline( data, width );
        // kill_chroma_packed422_inplace_scanline( data, width );
        // testing_packed422_inplace_scanline( data, width, scanline );
        // invert_colour_packed422_inplace_scanline( data, width );
    }
}

