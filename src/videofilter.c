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
#include "speedy.h"
#include "videofilter.h"

struct videofilter_s
{
    int uyvy_conversion;
    int colour_invert;
    int mirror;
    int chromakill;

    uint8_t tempscanline[ 2048 ];
};

videofilter_t *videofilter_new( void )
{
    videofilter_t *vf = malloc( sizeof( videofilter_t ) );
    if( !vf ) return 0;

    vf->uyvy_conversion = 0;
    vf->colour_invert = 0;
    vf->mirror = 0;
    vf->chromakill = 0;

    return vf;
}

void videofilter_delete( videofilter_t *vf )
{
    free( vf );
}

void videofilter_set_colour_invert( videofilter_t *vf, int invert )
{
    vf->colour_invert = invert;
}

void videofilter_set_mirror( videofilter_t *vf, int mirror )
{
    vf->mirror = mirror;
}

void videofilter_set_chroma_kill( videofilter_t *vf, int chromakill )
{
    vf->chromakill = chromakill;
}

void videofilter_enable_uyvy_conversion( videofilter_t *vf )
{
    vf->uyvy_conversion = 1;
}

int videofilter_active_on_scanline( videofilter_t *vf, int scanline )
{
    if( vf->uyvy_conversion || vf->colour_invert || vf->mirror ||
        vf->chromakill ) {
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

    if( vf->colour_invert ) {
        invert_colour_packed422_inplace_scanline( data, width );
    }

    if( vf->mirror ) {
        mirror_packed422_inplace_scanline( data, width );
    }

    if( vf->chromakill ) {
        kill_chroma_packed422_inplace_scanline( data, width );
    }
}

