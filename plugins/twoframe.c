/**
 * Copyright (c) 2000 Steven Grimm.  All rights reserved.
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

/**
 * Deinterlace the latest field, attempting to weave wherever it won't cause
 * visible artifacts.
 *
 * The data from the most recently captured field is always copied to the overlay
 * verbatim.  For the data from the previous field, the following algorithm is
 * applied to each pixel.
 *
 * We use the following notation for the top, middle, and bottom pixels
 * of concern:
 *
 * Field 1 | Field 2 | Field 3 | Field 4 |
 *         |   T0    |         |   T1    | scanline we copied in last iteration
 *   M0    |         |    M1   |         | intermediate scanline from alternate field
 *         |   B0    |         |   B1    | scanline we just copied
 *
 * We will weave M1 into the image if any of the following is true:
 *   - M1 is similar to either B1 or T1.  This indicates that no weave
 *     artifacts would be visible.  The SpatialTolerance setting controls
 *     how far apart the luminances can be before pixels are considered
 *     non-similar.
 *   - T1 and B1 and M1 are old.  In that case any weave artifact that
 *     appears isn't due to fast motion, since it was there in the previous
 *     frame too.  By "old" I mean similar to their counterparts in the
 *     previous frame; TemporalTolerance controls the maximum squared
 *     luminance difference above which a pixel is considered "new".
 *
 * Pixels are processed 4 at a time using MMX instructions.
 *
 * SQUARING NOTE:
 * We square luminance differences to amplify the effects of large
 * differences and to avoid dealing with negative differences.  Unfortunately,
 * we can't compare the square of difference directly against a threshold,
 * thanks to the lack of an MMX unsigned compare instruction.  The
 * problem is that if we had two pixels with luminance 0 and 255,
 * the difference squared would be 65025, which is a negative
 * 16-bit signed value and would thus compare less than a threshold.
 * We get around this by dividing all the luminance values by two before
 * squaring them; this results in an effective maximum luminance
 * difference of 127, whose square (16129) is safely comparable.
 */

static int TwoFrameTemporalTolerance = 300;
static int TwoFrameSpatialTolerance = 600;

static void deinterlace_twoframe_packed422_scanline_mmxext( unsigned char *output,
                                                            unsigned char *t1,
                                                            unsigned char *m1,
                                                            unsigned char *b1,
                                                            unsigned char *t0,
                                                            unsigned char *m0,
                                                            unsigned char *b0,
                                                            int width )
{
    const int64_t YMask = 0x00ff00ff00ff00ff;
    const int64_t qwAllOnes = 0xffffffffffffffff;
    int64_t qwBobbedPixels;
    int64_t qwSpatialTolerance;
    int64_t qwTemporalTolerance;

    // divide by 4 because of squaring behavior.
    qwSpatialTolerance = ((int64_t) TwoFrameSpatialTolerance) / 4;
    qwSpatialTolerance += (qwSpatialTolerance << 48) + (qwSpatialTolerance << 32)
                                                     + (qwSpatialTolerance << 16);
    qwTemporalTolerance = ((int64_t) TwoFrameTemporalTolerance) / 4;
    qwTemporalTolerance += (qwTemporalTolerance << 48) + (qwTemporalTolerance << 32)
                                                       + (qwTemporalTolerance << 16);

    // TODO: Just bob any extra pixels.

    width /= 4;
    while( width-- ) {
        movq_m2r( *t1, mm0 );          // mm0 = T1
        movq_m2r( *b1, mm1 );          // mm1 = B1
        movq_m2r( *m1, mm2 );          // mm2 = M1

        // Average T1 and B1 so we can do interpolated bobbing if we bob onto T1.
        movq_r2r( mm1, mm7 );          // mm7 = B1
        pavgb_r2r( mm0, mm7 );         // mm7 = (T1+B1/2)

        movq_r2m( mm7, qwBobbedPixels );

        // Now that we've averaged them, we no longer care about the chroma
        // values of T1 and B1 (all our comparisons are luminance-only).
        pand_m2r( YMask, mm0 );         // mm0 = luminance(T1)
        pand_m2r( YMask, mm1 );         // mm1 = luminance(B1)

        // Find out whether M1 is new.  "New" means the square of the
        // luminance difference between M1 and M0 is less than the temporal
        // tolerance.
        //
        movq_r2r( mm2, mm7 );           // mm7 = M1
        movq_m2r( *m0, mm4 );           // mm4 = M0
        pand_m2r( YMask, mm7 );         // mm7 = luminance(M1)
        movq_r2r( mm7, mm6 );           // mm6 = luminance(M1)     used below
        pand_m2r( YMask, mm4 );         // mm4 = luminance(M0)
        psubsw_r2r( mm4, mm7 );         // mm7 = M1 - M0
        psraw_i2r( 1, mm7 );            // mm7 = M1 - M0 (see SQUARING NOTE above)
        pmullw_r2r( mm7, mm7 );         // mm7 = (M1 - M0) ^ 2
        pcmpgtw_m2r( qwTemporalTolerance, mm7 );
                                        // mm7 = 0xffff where (M1 - M0) ^ 2 > threshold, 0x0000 otherwise


        // Find out how different T1 and M1 are.
        movq_r2r( mm0, mm3 );           // mm3 = T1
        psubsw_r2r( mm6, mm3 );         // mm3 = T1 - M1
        psraw_i2r( 1, mm3 );            // mm3 = T1 - M1 (see SQUARING NOTE above)
        pmullw_r2r( mm3, mm3 );         // mm3 = (T1 - M1) ^ 2
        pcmpgtw_m2r( qwSpatialTolerance, mm3 );
                                        // mm3 = 0xffff where (T1 - M1) ^ 2 > threshold, 0x0000 otherwise


        // Find out how different B1 and M1 are.
        movq_r2r( mm1, mm4 );           // mm4 = B1
        psubsw_r2r( mm6, mm4 );         // mm4 = B1 - M1
        psraw_i2r( 1, mm4 );            // mm4 = B1 - M1 (see SQUARING NOTE above)
        pmullw_r2r( mm4, mm4 );         // mm4 = (B1 - M1) ^ 2
        pcmpgtw_m2r( qwSpatialTolerance, mm4 );
                                        // mm4 = 0xffff where (B1 - M1) ^ 2 > threshold, 0x0000 otherwise


        // We care about cases where M1 is different from both T1 and B1.
        pand_r2r( mm4, mm3 );           // mm3 = 0xffff where M1 is different from T1 and B1, 0x0000 otherwise

        // Find out whether T1 is new.
        movq_r2r( mm0, mm4 );           // mm4 = T1
        movq_m2r( *t0, mm5 );           // mm5 = T0
        pand_m2r( YMask, mm5 );         // mm5 = luminance(T0)
        psubsw_r2r( mm5, mm4 );         // mm4 = T1 - T0
        psraw_i2r( 1, mm4 );            // mm4 = T1 - T0 (see SQUARING NOTE above)
        pmullw_r2r( mm4, mm4 );         // mm4 = (T1 - T0) ^ 2 / 4
        pcmpgtw_m2r( qwTemporalTolerance, mm4 );
                                        // mm4 = 0xffff where (T1 - T0) ^ 2 > threshold, 0x0000 otherwise

        // Find out whether B1 is new.
        movq_r2r( mm1, mm5 );           // mm5 = B1
        movq_m2r( *b0, mm6 );           // mm6 = B0
        pand_m2r( YMask, mm6 );         // mm6 = luminance(B0)
        psubsw_r2r( mm6, mm5 );         // mm5 = B1 - B0
        psraw_i2r( 1, mm5 );            // mm5 = B1 - B0 (see SQUARING NOTE above)
        pmullw_r2r( mm5, mm5 );         // mm5 = (B1 - B0) ^ 2
        pcmpgtw_m2r( qwTemporalTolerance, mm5 );
                                        // mm5 = 0xffff where (B1 - B0) ^ 2 > threshold, 0x0000 otherwise

        // We care about cases where M1 is old and either T1 or B1 is old.
        por_r2r( mm5, mm4 );            // mm4 = 0xffff where T1 or B1 is new
        por_r2r( mm7, mm4 );            // mm4 = 0xffff where T1 or B1 or M1 is new
        movq_m2r( qwAllOnes, mm6 );     // mm6 = 0xffffffffffffffff
        pxor_r2r( mm6, mm4 );           // mm4 = 0xffff where T1 and B1 and M1 are old

        // Pick up the interpolated (T1+B1)/2 pixels.
        movq_m2r( qwBobbedPixels, mm1 );// mm1 = (T1 + B1) / 2

        // At this point:
        //  mm1 = (T1+B1)/2
        //  mm2 = M1
        //  mm3 = mask, 0xffff where M1 is different from both T1 and B1
        //  mm4 = mask, 0xffff where T1 and B1 and M1 are old
        //  mm6 = 0xffffffffffffffff
        //
        // Now figure out where we're going to weave and where we're going to bob.
        // We'll weave if all pixels are old or M1 isn't different from both its
        // neighbors.
        pxor_r2r( mm6, mm3 );           // mm3 = 0xffff where M1 is the same as either T1 or B1
        por_r2r( mm4, mm3 );            // mm3 = 0xffff where M1 and T1 and B1 are old or M1 = T1 or B1
        pand_r2r( mm3, mm2 );           // mm2 = woven data where T1 or B1 isn't new or they're different
        pandn_r2r( mm1, mm3 );          // mm3 = bobbed data where T1 or B1 is new and they're similar
        por_r2r( mm2, mm3 );            // mm3 = finished pixels

        // Put the pixels in place.
        movntq_r2m( mm3, *output );

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

static deinterlace_setting_t settings[] =
{
    {
        "2 Frame Spatial Tolerance",
        SETTING_SLIDER,
        &TwoFrameSpatialTolerance,
        600, 0, 5000, 10,
        0
    },
    {
        "2 Frame Temporal Tolerance",
        SETTING_SLIDER,
        &TwoFrameTemporalTolerance,
        300, 0, 5000, 10,
        0
    }
};

static deinterlace_method_t twoframe =
{
    "TwoFrame (DScaler)",
    "2-Frame",
    4,
    MM_ACCEL_X86_MMXEXT,
    2,
    settings,
    deinterlace_twoframe_packed422_scanline_mmxext
};

void deinterlace_plugin_init( void )
{
    fprintf( stderr, "twoframe: Registering 2-Frame deinterlacing algorithm (DScaler).\n" );
    register_deinterlace_method( &twoframe );
}

