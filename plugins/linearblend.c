/**
 * Linear blend deinterlacing plugin.  The algorithm for this filter is based
 * on the mythtv sources, which took it from the mplayer sources.
 *
 * The file is postprocess_template.c in mplayer, and is
 *
 *  Copyright (C) 2001-2002 Michael Niedermayer (michaelni@gmx.at)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedtools.h"
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_linear_blend( unsigned char *output,
                                        unsigned char *t1, unsigned char *m1,
                                        unsigned char *b1,
                                        unsigned char *t0, unsigned char *m0,
                                        unsigned char *b0, int width )
{
    int i;

    READ_PREFETCH_2048( t1 );
    READ_PREFETCH_2048( b1 );
    READ_PREFETCH_2048( m1 );

    // Get width in bytes.
    width *= 2;
    i = width / 8;
    width -= i * 8;

    pxor_r2r( mm7, mm7 );
    while( i-- ) {
        movd_m2r( *t1, mm0 );
        movd_m2r( *b1, mm1 );
        movd_m2r( *m1, mm2 );

        movd_m2r( *(t1+4), mm3 );
        movd_m2r( *(b1+4), mm4 );
        movd_m2r( *(m1+4), mm5 );

        punpcklbw_r2r( mm7, mm0 );
        punpcklbw_r2r( mm7, mm1 );
        punpcklbw_r2r( mm7, mm2 );

        punpcklbw_r2r( mm7, mm3 );
        punpcklbw_r2r( mm7, mm4 );
        punpcklbw_r2r( mm7, mm5 );

        psllw_i2r( 1, mm2 );
        psllw_i2r( 1, mm5 );
        paddw_r2r( mm0, mm2 );
        paddw_r2r( mm3, mm5 );
        paddw_r2r( mm1, mm2 );
        paddw_r2r( mm4, mm5 );
        psrlw_i2r( 2, mm2 );
        psrlw_i2r( 2, mm5 );
        packuswb_r2r( mm2, mm2 );
        packuswb_r2r( mm5, mm5 );

        movd_r2m( mm2, *output );
        movd_r2m( mm5, *(output+4) );
        output += 8;
        t1 += 8;
        b1 += 8;
        m1 += 8;
    }
    while( width-- ) {
        *output++ = (*t1++ + *b1++ + (2 * *m1++))>>2;
    }
    sfence();
    emms();
}

static void deinterlace_scanline_linear_blend2( unsigned char *output, unsigned char *m2,
                           unsigned char *t1, unsigned char *m1,
                           unsigned char *b1, unsigned char *t0,
                           unsigned char *b0, int width )
{
    int i;

    READ_PREFETCH_2048( t1 );
    READ_PREFETCH_2048( b1 );
    READ_PREFETCH_2048( m2 );

    // Get width in bytes.
    width *= 2;
    i = width / 8;
    width -= i * 8;

    pxor_r2r( mm7, mm7 );
    while( i-- ) {
        movd_m2r( *t1, mm0 );
        movd_m2r( *b1, mm1 );
        movd_m2r( *m2, mm2 );

        movd_m2r( *(t1+4), mm3 );
        movd_m2r( *(b1+4), mm4 );
        movd_m2r( *(m2+4), mm5 );

        punpcklbw_r2r( mm7, mm0 );
        punpcklbw_r2r( mm7, mm1 );
        punpcklbw_r2r( mm7, mm2 );

        punpcklbw_r2r( mm7, mm3 );
        punpcklbw_r2r( mm7, mm4 );
        punpcklbw_r2r( mm7, mm5 );

        psllw_i2r( 1, mm2 );
        psllw_i2r( 1, mm5 );
        paddw_r2r( mm0, mm2 );
        paddw_r2r( mm3, mm5 );
        paddw_r2r( mm1, mm2 );
        paddw_r2r( mm4, mm5 );
        psrlw_i2r( 2, mm2 );
        psrlw_i2r( 2, mm5 );
        packuswb_r2r( mm2, mm2 );
        packuswb_r2r( mm5, mm5 );

        movd_r2m( mm2, *output );
        movd_r2m( mm5, *(output+4) );
        output += 8;
        t1 += 8;
        b1 += 8;
        m2 += 8;
    }
    while( width-- ) {
        *output++ = (*t1++ + *b1++ + (2 * *m2++))>>2;
    }
    sfence();
    emms();
}


static deinterlace_method_t linearblendmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Linear Blend (idea from mplayer)",
    "LinearBlend",
    2,
    MM_ACCEL_X86_MMXEXT,
    0,
    0,
    deinterlace_scanline_linear_blend,
    deinterlace_scanline_linear_blend2
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void linearblend_plugin_init( void )
#endif
{
    register_deinterlace_method( &linearblendmethod );
}

