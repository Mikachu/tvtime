/**
 * Copyright (C) 2004 Billy Biggs <vektor@dumbterm.net>
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "dscalerapi.h"
#include "deinterlace.h"
#include "speedy.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mm_accel.h"
#include "tomsmocomp.h"

static void deinterlace_frame_di_tomsmocomp( uint8_t *output, int outstride,
                                             deinterlace_frame_data_t *data,
                                             int bottom_field, int width, int height )
{
    TDeinterlaceInfo Info;
    TPicture Picture[ 8 ];
    int stride = (width*2);
    int i;

    Info.FieldHeight = height / 2;
    Info.FrameHeight = height;
    Info.FrameWidth = width;
    Info.InputPitch = stride*2;
    Info.LineLength = stride;
    Info.OverlayPitch = outstride;
    Info.pMemcpy = speedy_memcpy;

    if( bottom_field ) {
        Picture[ 0 ].pData = data->f0 + stride;
        Picture[ 0 ].Flags = PICTURE_INTERLACED_ODD;

        Picture[ 1 ].pData = data->f0;
        Picture[ 1 ].Flags = PICTURE_INTERLACED_EVEN;

        Picture[ 2 ].pData = data->f1 + stride;
        Picture[ 2 ].Flags = PICTURE_INTERLACED_ODD;

        Picture[ 3 ].pData = data->f1;
        Picture[ 3 ].Flags = PICTURE_INTERLACED_EVEN;

        Picture[ 4 ].pData = data->f2 + stride;
        Picture[ 4 ].Flags = PICTURE_INTERLACED_ODD;

        Picture[ 5 ].pData = data->f2;
        Picture[ 5 ].Flags = PICTURE_INTERLACED_EVEN;
    } else {
        Picture[ 0 ].pData = data->f0;
        Picture[ 0 ].Flags = PICTURE_INTERLACED_EVEN;

        Picture[ 1 ].pData = data->f1 + stride;
        Picture[ 1 ].Flags = PICTURE_INTERLACED_ODD;

        Picture[ 2 ].pData = data->f1;
        Picture[ 2 ].Flags = PICTURE_INTERLACED_EVEN;

        Picture[ 3 ].pData = data->f2 + stride;
        Picture[ 3 ].Flags = PICTURE_INTERLACED_ODD;

        Picture[ 4 ].pData = data->f2;
        Picture[ 4 ].Flags = PICTURE_INTERLACED_EVEN;
    }

    Info.Overlay = output;
    for( i = 0; i < MAX_PICTURE_HISTORY; i++ ) {
        Info.PictureHistory[ i ] = &(Picture[ i ]);
    }

    if( mm_accel() & MM_ACCEL_X86_MMXEXT ) {
        tomsmocomp_filter_sse( &Info );
    } else if( mm_accel() & MM_ACCEL_X86_3DNOW ) {
        tomsmocomp_filter_3dnow( &Info );
    } else {
        tomsmocomp_filter_mmx( &Info );
    }
}

static deinterlace_method_t tomsmocompmethod =
{
    "Motion Adaptive: Motion Search",
    "AdaptiveSearch",
    4,
    MM_ACCEL_X86_MMX,
    0,
    0,
    0,
    0,
    0,
    0,
    deinterlace_frame_di_tomsmocomp,
    { "Uses heuristics to detect motion in the input",
      "frames and reconstruct image detail where",
      "possible.  Use this for high quality output",
      "even on monitors set to an arbitrary refresh",
      "rate.",
      "",
      "Motion search mode finds and follows motion",
      "vectors for accurate interpolation.  This is",
      "the TomsMoComp deinterlacer from DScaler.",
      "" }
};

deinterlace_method_t *dscaler_tomsmocomp_get_method( void )
{
    tomsmocomp_init();
    return &tomsmocompmethod;
}

