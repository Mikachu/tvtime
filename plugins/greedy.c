
// Copyright (c) 2000 Tom Barry  All rights reserved.


#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "deinterlace.h"
#include "speedtools.h"
#include "speedy.h"

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

static void copy_scanline( unsigned char *output, unsigned char *t1,
                           unsigned char *m1, unsigned char *b1,
                           unsigned char *t0, unsigned char *m0,
                           unsigned char *b0, int width )
{
    blit_packed422_scanline( output, m1, width );
}

static int GreedyMaxComb = 15;

static void deinterlace_greedy_packed422_scanline_mmxext( unsigned char *output, unsigned char *m2,
                                                          unsigned char *t1, unsigned char *m1,
                                                          unsigned char *b1, unsigned char *t0,
                                                          unsigned char *b0, int width )
{
    mmx_t MaxComb;

    // How badly do we let it weave? 0-255
    MaxComb.ub[ 0 ] = GreedyMaxComb;
    MaxComb.ub[ 1 ] = GreedyMaxComb;
    MaxComb.ub[ 2 ] = GreedyMaxComb;
    MaxComb.ub[ 3 ] = GreedyMaxComb;
    MaxComb.ub[ 4 ] = GreedyMaxComb;
    MaxComb.ub[ 5 ] = GreedyMaxComb;
    MaxComb.ub[ 6 ] = GreedyMaxComb;
    MaxComb.ub[ 7 ] = GreedyMaxComb;

    // L2 = m2;
    // L1 = t1;
    // L3 = b1;
    // LP2 = m1;

    PREFETCH_2048( t1 );
    PREFETCH_2048( m2 );
    PREFETCH_2048( b1 );
    PREFETCH_2048( m1 );

    width /= 4;
    while( width-- ) {
        movq_m2r( *t1, mm1 );    // L1
        movq_m2r( *m2, mm2 );    // L2
        movq_m2r( *b1, mm3 );    // L3
        movq_m2r( *m1, mm0 );    // LP2

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

        movq_r2m( mm2, *output );     // move in our clipped best

        // Advance to the next set of pixels.
        output += 8;
        t0 += 8;
        b0 += 8;
        t1 += 8;
        m1 += 8;
        b1 += 8;
        m2 += 8;
    }
    sfence();
    emms();
}

static deinterlace_setting_t settings[] =
{
    {
        "Greedy Max Comb",
        SETTING_SLIDER,
        &GreedyMaxComb,
        15, 0, 255, 1,
        0
    }
};

static deinterlace_method_t greedymethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Greedy - Low motion (DScaler)",
    "Greedy",
    3,
    MM_ACCEL_X86_MMXEXT,
    1,
    settings,
    copy_scanline,
    deinterlace_greedy_packed422_scanline_mmxext
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void greedy_plugin_init( void )
#endif
{
    register_deinterlace_method( &greedymethod );
}

