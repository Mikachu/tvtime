
#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedy.h"

void interpolate_packed422_scanline_c( unsigned char *output,
                                       unsigned char *top,
                                       unsigned char *bot, int width )
{
    int i;

    for( i = width*2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }
}


void interpolate_packed422_scanline_mmxext( unsigned char *output,
                                            unsigned char *top,
                                            unsigned char *bot, int width )
{
    int i;

    for( i = width/4; i; --i ) {
        movq_m2r( *bot, mm2 );
        movq_m2r( *top, mm3 );
        pavgb_r2r( mm3, mm2 );
        movntq_r2m( mm2, *output );
        output += 8;
        top += 8;
        bot += 8;
    }
    emms();
}

void setup_speedy_calls( void )
{
    uint32_t accel = mm_accel();

    if( accel & MM_ACCEL_X86_MMXEXT ) {
        fprintf( stderr, "tvtime: Using MMXEXT optimized functions.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
    } else if( accel & MM_ACCEL_X86_MMX ) {
        fprintf( stderr, "tvtime: Using MMX optimized functions.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    } else {
        fprintf( stderr, "tvtime: No optimizations detected, "
                         "using C fallbacks.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    }
}

