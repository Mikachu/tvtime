/**
 * This file contains code from ffmpeg, see http://ffmpeg.org/
 *
 * Originated in imgconvert.c: Misc image convertion routines
 * Copyright (c) 2001, 2002, 2003 Fabrice Bellard.
 *
 * tvtime port Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedy.h"
#include "deinterlace.h"

/**
 * The MPEG2 spec uses a slightly harsher filter, they specify
 * [-1 8 2 8 -1].  ffmpeg uses a similar filter but with more of
 * a tendancy to blur than to use the local information.  The
 * filter taps here are: [-1 4 2 4 -1].
 */

static void deinterlace_line( unsigned char *dst, unsigned char *lum_m4,
                              unsigned char *lum_m3, unsigned char *lum_m2,
                              unsigned char *lum_m1, unsigned char *lum, int size )
{
    /**
     * C implementation.
    int sum;

    for(;size > 0;size--) {
        sum = -lum_m4[0];
        sum += lum_m3[0] << 2;
        sum += lum_m2[0] << 1;
        sum += lum_m1[0] << 2;
        sum += -lum[0];
        dst[0] = (sum + 4) >> 3; // This needs to be clipped at 0 and 255: cm[(sum + 4) >> 3];
        lum_m4++;
        lum_m3++;
        lum_m2++;
        lum_m1++;
        lum++;
        dst++;
    }
    */

    mmx_t rounder;

    rounder.uw[0]=4;
    rounder.uw[1]=4;
    rounder.uw[2]=4;
    rounder.uw[3]=4;
    pxor_r2r(mm7,mm7);
    movq_m2r(rounder,mm6);

    for (;size > 3; size-=4) {
        movd_m2r(lum_m4[0],mm0);
        movd_m2r(lum_m3[0],mm1);
        movd_m2r(lum_m2[0],mm2);
        movd_m2r(lum_m1[0],mm3);
        movd_m2r(lum[0],mm4);
        punpcklbw_r2r(mm7,mm0);
        punpcklbw_r2r(mm7,mm1);
        punpcklbw_r2r(mm7,mm2);
        punpcklbw_r2r(mm7,mm3);
        punpcklbw_r2r(mm7,mm4);
        paddw_r2r(mm3,mm1);
        psllw_i2r(1,mm2);
        paddw_r2r(mm4,mm0);
        psllw_i2r(2,mm1);// 2
        paddw_r2r(mm6,mm2);
        paddw_r2r(mm2,mm1);
        psubusw_r2r(mm0,mm1);
        psrlw_i2r(3,mm1); // 3
        packuswb_r2r(mm7,mm1);
        movd_r2m(mm1,dst[0]);
        lum_m4+=4;
        lum_m3+=4;
        lum_m2+=4;
        lum_m1+=4;
        lum+=4;
        dst+=4;
    }
    emms();
}


/**
 * The commented-out method below that uses the bottom_field member is more
 * like the filter as specified in the MPEG2 spec, but it doesn't seem to
 * have the desired effect.
 */

static void deinterlace_scanline_vfir( unsigned char *output,
                                       deinterlace_scanline_data_t *data,
                                       int width )
{
    deinterlace_line( output, data->tt1, data->t0, data->m1, data->b0, data->bb1, width*2 );
    // blit_packed422_scanline( output, data->m1, width );
}

static void copy_scanline( unsigned char *output,
                           deinterlace_scanline_data_t *data,
                           int width )
{
    blit_packed422_scanline( output, data->m0, width );
    /*
    if( data->bottom_field ) {
        deinterlace_line( output, data->tt2, data->t1, data->m2, data->b1, data->bb2, width*2 );
    } else {
        deinterlace_line( output, data->tt0, data->t1, data->m0, data->b1, data->bb0, width*2 );
    }
    */
}


static deinterlace_method_t vfirmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Vertical deinterlace filter",
    "Vertical FIR",
    1,
    0,
    0,
    0,
    0,
    1,
    deinterlace_scanline_vfir,
    copy_scanline,
    0
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void vfir_plugin_init( void )
#endif
{
    register_deinterlace_method( &vfirmethod );
}

