
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

    /* Handle last few pixels. */
    if( width & 0x0f ) {
        for( i = (width & 0x0f)*2; i; --i ) {
            *output++ = ((*top++) + (*bot++)) >> 1;
        }
    }

    emms();
}

void blit_colour_packed422_scanline_c( unsigned char *output,
                                       int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;
    unsigned int *o = (unsigned int *) output;

    for( width /= 2; width; --width ) {
        *o++ = colour;
    }
}

void blit_colour_packed422_scanline_mmx( unsigned char *output,
                                         int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 4; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

void blit_colour_packed422_scanline_mmxext( unsigned char *output,
                                            int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 4; width; --width ) {
        movntq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

void blit_colour_packed4444_scanline_c( unsigned char *output, int width,
                                        int alpha, int luma, int cb, int cr )
{
    int j;

    for( j = 0; j < width; j++ ) {
        *output++ = alpha;
        *output++ = luma;
        *output++ = cb;
        *output++ = cr;
    }
}

void blit_colour_packed4444_scanline_mmx( unsigned char *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr )
{
    int colour = cr << 24 | cb << 16 | luma << 8 | alpha;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 2; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

void blit_colour_packed4444_scanline_mmxext( unsigned char *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr )
{
    int colour = cr << 24 | cb << 16 | luma << 8 | alpha;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 2; width; --width ) {
        movntq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}


void setup_speedy_calls( void )
{
    uint32_t accel = mm_accel();

    interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    blit_colour_packed422_scanline = blit_colour_packed422_scanline_c;
    blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_c;

    if( accel & MM_ACCEL_X86_MMXEXT ) {
        fprintf( stderr, "tvtime: Using MMXEXT optimized functions.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmxext;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmxext;
    } else if( accel & MM_ACCEL_X86_MMX ) {
        fprintf( stderr, "tvtime: Using MMX optimized functions.\n" );
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmx;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmx;
    } else {
        fprintf( stderr, "tvtime: No optimizations detected, "
                         "using C fallbacks.\n" );
    }
}

