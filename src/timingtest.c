/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include "rtctimer.h"
#include "videotools.h"
#include "speedy.h"
#include "leetft.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

/* What font to test text rendering with. */
static const char *leeft_font = "../data/FreeSansBold.ttf";
const unsigned int leeft_size = 30;
const double leetft_aspect = 1.125;

/* Use a constant random seed for tests. */
const unsigned int seed = 2;

/* Test against NTSC resolution. */
const unsigned int width = 720;
const unsigned int height = 480;

/* Run for 512 runs. */
const unsigned int numruns = 512;

static const char *tests[] = {
   "blit_colour_packed422_scanline_c 720x480 frame",
   "blit_colour_packed422_scanline_mmx 720x480 frame",
   "blit_colour_packed422_scanline_mmxext 720x480 frame",
   "blit_packed422_scanline_c 720x480 frame",
   "blit_packed422_scanline_mmx 720x480 frame",
   "blit_packed422_scanline_mmxext 720x480 frame",
   "interpolate_packed422_scanline_c 720x480 frame",
   "interpolate_packed422_scanline_mmx 720x480 frame",
   "interpolate_packed422_scanline_mmxext 720x480 frame",
   "blend_packed422_scanline_c 720x480 120/256 frame",
   "blend_packed422_scanline_mmxext 720x480 120/256 frame",
   "comb_factor_packed422_scanline 720x480 frame",
   "diff_factor_packed422_scanline_c 720x480 frame",
   "diff_factor_packed422_scanline_mmx 720x480 frame",
   "leetft_render_test_string",
   "packed444_to_rgb24_rec601_scanline",
   "packed444_to_rgb24_rec601_reference_scanline"
};
const int numtests = ( sizeof( tests ) / sizeof( char * ) );

int main( int argc, char **argv )
{
    uint8_t *source422packed;
    uint8_t *source422packed2;
    uint8_t *source444packed;
    uint8_t *dest422packed;
    uint8_t *dest444packed;
    ft_font_t *font = 0;
    ft_string_t *fts = 0;
    unsigned int datasize = 0;
    uint64_t avg_sum = 0;
    uint64_t avg_count = 0;
    uint64_t before = 0;
    uint64_t after = 0;
    int stride422 = width * 2;
    int stride444 = width * 3;
    int testid = 0;
    double mhz;
    int i;

    setup_speedy_calls( 1 );

    if( argc < 2 ) {
        for( i = 0; i < numtests; i++ ) {
            fprintf( stderr, "timingtest: %2d: %s\n", i, tests[ i ] );
        }
        fprintf( stderr, "usage: timingtest <testid>\n" );
        return 1;
    }
    testid = atoi( argv[ 1 ] );

    if( testid >= numtests ) {
        fprintf( stderr, "timingtest: Test %d not found.\n", testid );
        return 1;
    } else {
        fprintf( stderr, "timingtest: Testing %s.\n", tests[ testid ] );
    }

    if( !strcmp( tests[ testid ], "leetft_render_test_string" ) ) {
        font = ft_font_new( leeft_font, leeft_size, leetft_aspect );
        if( !font ) {
            fprintf( stderr, "timingtest: Running leetft test, but font %s not found.\n", leeft_font );
            return 1;
        }
        fts = ft_string_new( font );
        if( !fts ) {
            fprintf( stderr, "timingtest: Can't create string object.\n" );
            return 1;
        }
    }
  
    if( !set_realtime_priority( 0 ) ) {
        fprintf( stderr, "timingtest: Can't set realtime priority (need root).\n" );
        fprintf( stderr, "timingtest: Results would be inaccurate, exiting.\n" );
        return 1;
    }

    mhz = speedy_measure_cpu_mhz();
    fprintf( stderr, "timingtest: Using CPU MHz %.3f for calculations.\n", mhz );

    /* Always use the same random seed. */
    srandom( seed );

    source422packed = malloc( width * height * 2 );
    source422packed2 = malloc( width * height * 2 );
    source444packed = malloc( width * height * 3 );
    dest422packed = malloc( width * height * 2 );
    dest444packed = malloc( width * height * 3 );

    if( !source422packed || !source422packed2 || !dest422packed || !dest444packed ) {
        fprintf( stderr, "timingtest: Can't allocate memory.\n" );
        return 1;
    }

    for( i = 0; i < width*height*2; i++ ) {
        //source422packed[ i ] = i % 256;
        source422packed[ i ] = random() % 256;
        source422packed2[ i ] = random() % 256;
    }

    /* Sleep to let the system cool off. */
    usleep( 10000 );

    for( i = 0; i < numruns; i++ ) {
        int j;

        if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_colour_packed422_scanline_c( dest422packed + (stride422*j), width, 128, 128, 128 );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_mmx 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_colour_packed422_scanline_mmx( dest422packed + (stride422*j), width, 128, 128, 128 );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_mmxext 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_colour_packed422_scanline_mmxext( dest422packed + (stride422*j), width, 128, 128, 128 );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blit_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_packed422_scanline_c( dest422packed + (stride422*j), source422packed + (stride422*j), width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blit_packed422_scanline_mmx 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_packed422_scanline_mmx( dest422packed + (stride422*j), source422packed + (stride422*j), width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blit_packed422_scanline_mmxext 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_packed422_scanline_mmxext( dest422packed + (stride422*j), source422packed + (stride422*j), width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "interpolate_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                interpolate_packed422_scanline_c( dest422packed + (stride422*j),
                                                  source422packed + (stride422*j),
                                                  source422packed2 + (stride422*j), width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "interpolate_packed422_scanline_mmx 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                interpolate_packed422_scanline_mmx( dest422packed + (stride422*j),
                                                    source422packed + (stride422*j),
                                                    source422packed2 + (stride422*j), width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "interpolate_packed422_scanline_mmxext 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                interpolate_packed422_scanline_mmxext( dest422packed + (stride422*j),
                                                       source422packed + (stride422*j),
                                                       source422packed2 + (stride422*j), width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blend_packed422_scanline_c 720x480 120/256 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height/2; j++ ) {
                blend_packed422_scanline_c( dest422packed + (stride422*j),
                                            source422packed + (stride422*j),
                                            source422packed2 + (stride422*j), width, 120 );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "blend_packed422_scanline_mmxext 720x480 120/256 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height/2; j++ ) {
                blend_packed422_scanline_mmxext( dest422packed + (stride422*j),
                                                 source422packed + (stride422*j),
                                                 source422packed2 + (stride422*j), width, 120 );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "comb_factor_packed422_scanline 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height/2; j++ ) {
                comb_factor_packed422_scanline( source422packed + (stride422*j*2),
                                                source422packed2 + (stride422*(j+1)*2),
                                                source422packed + (stride422*j*2) + (stride422*2),
                                                width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "diff_factor_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                diff_factor_packed422_scanline_c( source422packed + (stride422*j),
                                                  source422packed2 + (stride422*j),
                                                  width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "diff_factor_packed422_scanline_mmx 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                diff_factor_packed422_scanline_mmx( source422packed + (stride422*j),
                                                    source422packed2 + (stride422*j),
                                                    width );
            }
            rdtscll( after );
            datasize += width * height * 2;
        } else if( !strcmp( tests[ testid ], "leetft_render_test_string" ) ) {
            rdtscll( before );
            ft_string_set_text( fts, "The quick brown fox jumped over the lazy dog", 0 );
            rdtscll( after );
            datasize += ft_string_get_stride( fts ) * ft_string_get_height( fts );
        } else if( !strcmp( tests[ testid ], "packed444_to_rgb24_rec601_scanline" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                packed444_to_rgb24_rec601_scanline( dest444packed + (stride444*j),
                                                    source444packed + (stride444*j), width );
            }
            rdtscll( after );
            datasize += width * height * 3;
        } else if( !strcmp( tests[ testid ], "packed444_to_rgb24_rec601_reference_scanline" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                packed444_to_rgb24_rec601_reference_scanline( dest444packed + (stride444*j),
                                                              source444packed + (stride444*j), width );
            }
            rdtscll( after );
            datasize += width * height * 3;
        }

        fprintf( stderr, "[%4d] Cycles: %7d\r", i, (int) (after - before) );
        avg_sum += (after - before);
        avg_count++;
    }

    fprintf( stderr, "timingtest: %llu runs tested, average time was %llu cycles (%.2f ms), throughput %.2fMB/sec.\n",
             avg_count, (avg_sum/avg_count ), ((double) (avg_sum/avg_count)) / (mhz * 1000.0),
             (( (double) datasize / (  ((double) avg_sum) / (mhz * 1000.0) ) ) * 1000.0) / (1024.0 * 1024.0) );

    free( source422packed );
    free( source422packed2 );
    free( dest422packed );
    return 0;
}

