
/**
 * Copyright (c) 2000 John Adcock.  All rights reserved.
 * Based on code from Virtual Dub Plug-in by Gunnar Thalin
 * mmx.h port copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This code is ported from DScaler: http://deinterlace.sf.net/
 *
 * This file is subject to the terms of the GNU General Public License as
 * published by the Free Software Foundation.  A copy of this license is
 * included with this software distribution in the file COPYING.  If you
 * do not have a copy, you may obtain a copy by writing to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "deinterlace.h"
#include "speedy.h"

static int EdgeDetect = 625;
static int JaggieThreshold = 73;

/**
 * DeinterlaceFieldBob
 *
 * Deinterlaces a field with a tendency to bob rather than weave.  Best for
 * high-motion scenes like sports.
 *
 * The algorithm for this was taken from the 
 * Deinterlace - area based Vitual Dub Plug-in by
 * Gunnar Thalin
 */
static void deinterlace_videobob_packed422_scanline_mmxext( unsigned char *output,
                                                            unsigned char *t1,
                                                            unsigned char *m1,
                                                            unsigned char *b1,
                                                            unsigned char *t0,
                                                            unsigned char *m0,
                                                            unsigned char *b0,
                                                            int width )
{
    int64_t qwEdgeDetect;
    int64_t qwThreshold;
    const int64_t YMask = 0x00ff00ff00ff00ff;

    qwEdgeDetect = EdgeDetect;
    qwEdgeDetect += (qwEdgeDetect << 48) + (qwEdgeDetect << 32) + (qwEdgeDetect << 16);
    qwThreshold = JaggieThreshold;
    qwThreshold += (qwThreshold << 48) + (qwThreshold << 32) + (qwThreshold << 16);

    width /= 4;
    while( width-- ) {
        movq_m2r( *t1, mm0 );
        movq_m2r( *m1, mm1 );
        movq_m2r( *b1, mm2 );

        // get intensities in mm3 - 4
        movq_r2r( mm0, mm3 );
        movq_r2r( mm1, mm4 );
        movq_r2r( mm2, mm5 );

        pand_m2r( YMask, mm3 );
        pand_m2r( YMask, mm4 );
        pand_m2r( YMask, mm5 );

        // get average in mm0
        pavgb_r2r( mm2, mm0 );


        // work out (O1 - E) * (O2 - E) / 2 - EdgeDetect * (O1 - O2) ^ 2 >> 12
        // result will be in mm6
        psrlw_i2r( 1, mm3 );
        psrlw_i2r( 1, mm4 );
        psrlw_i2r( 1, mm5 );

        movq_r2r( mm3, mm6 );
        psubw_r2r( mm4, mm6 );  // mm6 = O1 - E

        movq_r2r( mm5, mm7 );
        psubw_r2r( mm4, mm7 );  // mm7 = O2 - E

        pmullw_r2r( mm7, mm6 ); // mm0 = (O1 - E) * (O2 - E)

        movq_r2r( mm3, mm7 );
        psubw_r2r( mm5, mm7 );  // mm7 = (O1 - O2)
        pmullw_r2r( mm7, mm7 ); // mm7 = (O1 - O2) ^ 2
        psrlw_i2r( 12, mm7 );   // mm7 = (O1 - O2) ^ 2 >> 12
        pmullw_m2r( qwEdgeDetect, mm7 );
                                // mm7  = EdgeDetect * (O1 - O2) ^ 2 >> 12

        psubw_r2r( mm7, mm6 );  // mm6 is what we want

        pcmpgtw_m2r( qwThreshold, mm6 );
        movq_r2r( mm6, mm7 );
        pand_r2r( mm6, mm0 );
        pandn_r2r( mm1, mm7 );
        por_r2r( mm0, mm7 );

        movntq_r2m( mm7, *output );

        // Advance to the next set of pixels.
        output += 8;
        t0 += 8;
        m0 += 8;
        b0 += 8;
        t1 += 8;
        m1 += 8;
        b1 += 8;
    }
    sfence();
    emms();
}

static void copy_scanline( unsigned char *output, unsigned char *m2,
                           unsigned char *t1, unsigned char *m1,
                           unsigned char *b1, unsigned char *t0,
                           unsigned char *b0, int width )
{
    blit_packed422_scanline( output, m2, width );
}

static deinterlace_setting_t settings[] =
{
    {
        "Weave edge detect",
        SETTING_SLIDER,
        &EdgeDetect,
        625, 0, 10000, 5,
        0
    },
    {
        "Weave Jaggie Threshold",
        SETTING_SLIDER,
        &JaggieThreshold,
        73, 0, 5000, 5,
        0
    }
};

static deinterlace_method_t bobmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Bob (DScaler)",
    "Bob",
    2,
    MM_ACCEL_X86_MMXEXT,
    2,
    settings,
    deinterlace_videobob_packed422_scanline_mmxext,
    copy_scanline
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void videobob_plugin_init( void )
#endif
{
    fprintf( stderr, "videobob: Registering Video Bob "
             "deinterlacing algorithm (DScaler).\n" );
    register_deinterlace_method( &bobmethod );
}

