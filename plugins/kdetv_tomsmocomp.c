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

static int  Search_Effort_MMX_0();
static int  Search_Effort_MMX_1();
static int  Search_Effort_MMX_3();
static int  Search_Effort_MMX_5();
static int  Search_Effort_MMX_9();
static int  Search_Effort_MMX_11();
static int  Search_Effort_MMX_13();
static int  Search_Effort_MMX_15();
static int  Search_Effort_MMX_19();
static int  Search_Effort_MMX_21();
static int  Search_Effort_MMX_Max();

static int  Search_Effort_MMX_0_SB();
static int  Search_Effort_MMX_1_SB();
static int  Search_Effort_MMX_3_SB();
static int  Search_Effort_MMX_5_SB();
static int  Search_Effort_MMX_9_SB();
static int  Search_Effort_MMX_11_SB();
static int  Search_Effort_MMX_13_SB();
static int  Search_Effort_MMX_15_SB();
static int  Search_Effort_MMX_19_SB();
static int  Search_Effort_MMX_21_SB();
static int  Search_Effort_MMX_Max_SB();

static int  Search_Effort_3DNOW_0();
static int  Search_Effort_3DNOW_1();
static int  Search_Effort_3DNOW_3();
static int  Search_Effort_3DNOW_5();
static int  Search_Effort_3DNOW_9();
static int  Search_Effort_3DNOW_11();
static int  Search_Effort_3DNOW_13();
static int  Search_Effort_3DNOW_15();
static int  Search_Effort_3DNOW_19();
static int  Search_Effort_3DNOW_21();
static int  Search_Effort_3DNOW_Max();

static int  Search_Effort_3DNOW_0_SB();
static int  Search_Effort_3DNOW_1_SB();
static int  Search_Effort_3DNOW_3_SB();
static int  Search_Effort_3DNOW_5_SB();
static int  Search_Effort_3DNOW_9_SB();
static int  Search_Effort_3DNOW_11_SB();
static int  Search_Effort_3DNOW_13_SB();
static int  Search_Effort_3DNOW_15_SB();
static int  Search_Effort_3DNOW_19_SB();
static int  Search_Effort_3DNOW_21_SB();
static int  Search_Effort_3DNOW_Max_SB();

static int  Search_Effort_SSE_0();
static int  Search_Effort_SSE_1();
static int  Search_Effort_SSE_3();
static int  Search_Effort_SSE_5();
static int  Search_Effort_SSE_9();
static int  Search_Effort_SSE_11();
static int  Search_Effort_SSE_13();
static int  Search_Effort_SSE_15();
static int  Search_Effort_SSE_19();
static int  Search_Effort_SSE_21();
static int  Search_Effort_SSE_Max();

static int  Search_Effort_SSE_0_SB();
static int  Search_Effort_SSE_1_SB();
static int  Search_Effort_SSE_3_SB();
static int  Search_Effort_SSE_5_SB();
static int  Search_Effort_SSE_9_SB();
static int  Search_Effort_SSE_11_SB();
static int  Search_Effort_SSE_13_SB();
static int  Search_Effort_SSE_15_SB();
static int  Search_Effort_SSE_19_SB();
static int  Search_Effort_SSE_21_SB();
static int  Search_Effort_SSE_Max_SB();

static long SearchEffort;
static int UseStrangeBob;

static MEMCPY_FUNC* pMyMemcpy;
static int IsOdd;
static const unsigned char* pWeaveSrc;
static const unsigned char* pWeaveSrcP;
static unsigned char* pWeaveDest;
static const unsigned char* pCopySrc;
static const unsigned char* pCopySrcP;
static unsigned char* pCopyDest;
static int src_pitch;
static int dst_pitch;
static int rowsize;
static int FldHeight;

static int Fieldcopy(void *dest, const void *src, size_t count, 
                     int rows, int dst_pitch, int src_pitch)
{
    unsigned char* pDest = (unsigned char*) dest;
    unsigned char* pSrc = (unsigned char*) src;
    int i;

    for (i=0; i < rows; i++) {
        pMyMemcpy(pDest, pSrc, count);
        pSrc += src_pitch;
        pDest += dst_pitch;
    }
    return 0;
}


#include "tomsmocomp/tomsmocompfilter_sse.cpp"
#include "tomsmocomp/tomsmocompfilter_mmx.cpp"
#include "tomsmocomp/tomsmocompfilter_3dnow.cpp"

#define SearchEffortDefault 5
#define UseStrangeBobDefault 0

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
        filterDScaler_SSE( &Info );
    } else if( mm_accel() & MM_ACCEL_X86_3DNOW ) {
        filterDScaler_3DNOW( &Info );
    } else {
        filterDScaler_MMX( &Info );
    }
}

static deinterlace_method_t tomsmocompmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
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

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void dscaler_tomsmocomp_plugin_init( void )
#endif
{
    register_deinterlace_method( &tomsmocompmethod );
    SearchEffort  = SearchEffortDefault;
    UseStrangeBob = UseStrangeBobDefault;
}

