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
#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedtools.h"
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_linear_blend( uint8_t *output,
                                               deinterlace_scanline_data_t *data,
                                               int width )
{
    uint8_t *t0 = data->t0;
    uint8_t *b0 = data->b0;
    uint8_t *m1 = data->m1;
#ifdef ARCH_X86
    int i;

    // Get width in bytes.
    width *= 2;
    i = width / 8;
    width -= i * 8;

    pxor_r2r( mm7, mm7 );
    while( i-- ) {
        movd_m2r( *t0, mm0 );
        movd_m2r( *b0, mm1 );
        movd_m2r( *m1, mm2 );

        movd_m2r( *(t0+4), mm3 );
        movd_m2r( *(b0+4), mm4 );
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
        t0 += 8;
        b0 += 8;
        m1 += 8;
    }
    while( width-- ) {
        *output++ = (*t0++ + *b0++ + (*m1++ << 1)) >> 2;
    }
    emms();
#else
    width *= 2;
    while( width-- ) {
        *output++ = (*t0++ + *b0++ + (*m1++ << 1)) >> 2;
    }
#endif
}

static void deinterlace_scanline_linear_blend2( uint8_t *output,
                                                deinterlace_scanline_data_t *data,
                                                int width )
{
    uint8_t *m0 = data->m0;
    uint8_t *t1 = data->t1;
    uint8_t *b1 = data->b1;
#ifdef ARCH_X86
    int i;

    // Get width in bytes.
    width *= 2;
    i = width / 8;
    width -= i * 8;

    pxor_r2r( mm7, mm7 );
    while( i-- ) {
        movd_m2r( *t1, mm0 );
        movd_m2r( *b1, mm1 );
        movd_m2r( *m0, mm2 );

        movd_m2r( *(t1+4), mm3 );
        movd_m2r( *(b1+4), mm4 );
        movd_m2r( *(m0+4), mm5 );

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
        m0 += 8;
    }
    while( width-- ) {
        *output++ = (*t1++ + *b1++ + (*m0++ << 1)) >> 2;
    }
    emms();
#else
    width *= 2;
    while( width-- ) {
        *output++ = (*t1++ + *b1++ + (*m0++ << 1)) >> 2;
    }
#endif
}


static deinterlace_method_t linearblendmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Blur: Temporal",
    "BlurTemporal",
    2,
#ifdef ARCH_X86
    MM_ACCEL_X86_MMX,
#else
    0,
#endif
    0,
    0,
    0,
    1,
    deinterlace_scanline_linear_blend,
    deinterlace_scanline_linear_blend2,
    0,
    { "Avoids flicker by blurring consecutive frames",
      "of input.  Use this if you want to run your",
      "monitor at an arbitrary refresh rate and not",
      "use much CPU, and are willing to sacrifice",
      "detail.",
      "",
      "Temporal mode evenly blurs content for least",
      "flicker, but with visible trails on fast motion.",
      "From the linear blend deinterlacer in mplayer.",
      "" }
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void linearblend_plugin_init( void )
#endif
{
    register_deinterlace_method( &linearblendmethod );
}

