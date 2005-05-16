/**
 * Copyright (c) 2000 Tom Barry  All rights reserved.
 * mmx.h port copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This code is ported from DScaler: http://deinterlace.sf.net/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "deinterlace.h"
#include "speedtools.h"
#include "copyfunctions.h"

// This is a simple lightweight DeInterlace method that uses little CPU time
// but gives very good results for low or intermedite motion.
// It defers frames by one field, but that does not seem to produce noticeable
// lip sync problems.
//
// The method used is to take either the older or newer weave pixel depending
// upon which give the smaller comb factor, and then clip to avoid large damage
// when wrong.
//
// I'd intended this to be part of a larger more elaborate method added to 
// Blended Clip but this give too good results for the CPU to ignore here.

static void copy_scanline( uint8_t *output,
                           deinterlace_scanline_data_t *data,
                           int width )
{
    blit_packed422_scanline( output, data->m1, width );
}

static int GreedyMaxComb = 15;

static void deinterlace_greedy_packed422_scanline_mmxext( uint8_t *output,
                                                          deinterlace_scanline_data_t *data,
                                                          int width )
{
#ifdef ARCH_X86
    mmx_t MaxComb;
    uint8_t *m0 = data->m0;
    uint8_t *t1 = data->t1;
    uint8_t *b1 = data->b1;
    uint8_t *m2 = data->m2;

    // How badly do we let it weave? 0-255
    MaxComb.ub[ 0 ] = GreedyMaxComb;
    MaxComb.ub[ 1 ] = GreedyMaxComb;
    MaxComb.ub[ 2 ] = GreedyMaxComb;
    MaxComb.ub[ 3 ] = GreedyMaxComb;
    MaxComb.ub[ 4 ] = GreedyMaxComb;
    MaxComb.ub[ 5 ] = GreedyMaxComb;
    MaxComb.ub[ 6 ] = GreedyMaxComb;
    MaxComb.ub[ 7 ] = GreedyMaxComb;

    // L2 == m0
    // L1 == t1
    // L3 == b1
    // LP2 == m2

    width /= 4;
    while( width-- ) {
        movq_m2r( *t1, mm1 );    // L1
        movq_m2r( *m0, mm2 );    // L2
        movq_m2r( *b1, mm3 );    // L3
        movq_m2r( *m2, mm0 );    // LP2

        // average L1 and L3 leave result in mm4
        movq_r2r( mm1, mm4 );    // L1
        pavgb_r2r( mm3, mm4 );   // (L1 + L3)/2


        // get abs value of possible L2 comb
        movq_r2r( mm2, mm7 );    // L2
        psubusb_r2r( mm4, mm7 ); // L2 - avg
        movq_r2r( mm4, mm5 );    // avg
        psubusb_r2r( mm2, mm5 ); // avg - L2
        por_r2r( mm7, mm5 );     // abs(avg-L2)
        movq_r2r( mm4, mm6 );    // copy of avg for later


        // get abs value of possible LP2 comb
        movq_r2r( mm0, mm7 );    // LP2
        psubusb_r2r( mm4, mm7 ); // LP2 - avg
        psubusb_r2r( mm0, mm4 ); // avg - LP2
        por_r2r( mm7, mm4 );     // abs(avg-LP2)

        // use L2 or LP2 depending upon which makes smaller comb
        psubusb_r2r( mm5, mm4 ); // see if it goes to zero
        psubusb_r2r( mm5, mm5 ); // 0
        pcmpeqb_r2r( mm5, mm4 ); // if (mm4=0) then FF else 0
        pcmpeqb_r2r( mm4, mm5 ); // opposite of mm4

        // if Comb(LP2) <= Comb(L2) then mm4=ff, mm5=0 else mm4=0, mm5 = 55
        pand_r2r( mm2, mm5 );    // use L2 if mm5 == ff, else 0
        pand_r2r( mm0, mm4 );    // use LP2 if mm4 = ff, else 0
        por_r2r( mm5, mm4 );     // may the best win

        // Now lets clip our chosen value to be not outside of the range
        // of the high/low range L1-L3 by more than abs(L1-L3)
        // This allows some comb but limits the damages and also allows more
        // detail than a boring oversmoothed clip.

        movq_r2r( mm1, mm2 );    // copy L1
        psubusb_r2r( mm3, mm2 ); // - L3, with saturation
        paddusb_r2r( mm3, mm2 ); // now = Max(L1,L3)

        pcmpeqb_r2r( mm7, mm7 ); // all ffffffff
        psubusb_r2r( mm1, mm7 ); // - L1 
        paddusb_r2r( mm7, mm3 ); // add, may sat at fff..
        psubusb_r2r( mm7, mm3 ); // now = Min(L1,L3)

        // allow the value to be above the high or below the low by amt of MaxComb
        paddusb_m2r( MaxComb, mm2 );    // increase max by diff
        psubusb_m2r( MaxComb, mm3 );    // lower min by diff

        psubusb_r2r( mm3, mm4 );        // best - Min
        paddusb_r2r( mm3, mm4 );        // now = Max(best,Min(L1,L3)

        pcmpeqb_r2r( mm7, mm7 );        // all ffffffff
        psubusb_r2r( mm4, mm7 );        // - Max(best,Min(best,L3) 
        paddusb_r2r( mm7, mm2 );        // add may sat at FFF..
        psubusb_r2r( mm7, mm2 );        // now = Min( Max(best, Min(L1,L3), L2 )=L2 clipped

        movntq_r2m( mm2, *output );     // move in our clipped best

        // Advance to the next set of pixels.
        output += 8;
        m0 += 8;
        t1 += 8;
        b1 += 8;
        m2 += 8;
    }
    sfence();
    emms();
#endif
}

/**
 * The greedy deinterlacer introduces a one-field delay on the input.
 * From the diagrams in deinterlace.h, the field being deinterlaced is
 * always t-1.  For this reason, our copy_scanline method is used for
 * deinterlace_method_t's interpolate_scanline function, and the real
 * work is done in deinterlace_method_t's copy_scanline function.
 */

static deinterlace_method_t greedymethod =
{
    "Motion Adaptive: Simple Detection",
    "AdaptiveSimple",
    3,
    MM_ACCEL_X86_MMXEXT,
    0,
    1,
    copy_scanline,
    deinterlace_greedy_packed422_scanline_mmxext,
    0,
    1,
    { "Uses heuristics to detect motion in the input",
      "frames and reconstruct image detail where",
      "possible.  Use this for high quality output",
      "even on monitors set to an arbitrary refresh",
      "rate.",
      "",
      "Simple detection uses linear interpolation",
      "where motion is detected, using a two-field",
      "buffer.  This is the Greedy: Low Motion",
      "deinterlacer from DScaler." }
};

deinterlace_method_t *greedy_get_method( void )
{
    return &greedymethod;
}

