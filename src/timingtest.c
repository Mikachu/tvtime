/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <string.h>
#include "rtctimer.h"
#include "videotools.h"
#include "speedy.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

/* Include these because I'm lazy and don't want to disturb other code. */
char *program_name = "tvtime";
int dlevel = 5;

/* Use a constant random seed for tests. */
const unsigned int seed = 2;

/* Test against NTSC resolution. */
const unsigned int width = 720;
const unsigned int height = 480;

/* Run for 1000 runs. */
const unsigned int numruns = 4*60;

static const char *tests[] = {
   "blit_colour_packed422_scanline_c 720x480 frame",
   "blit_colour_packed422_scanline_mmxext 720x480 frame",
   "blit_packed422_scanline_c 720x480 frame",
   "blit_packed422_scanline_i386_linux 720x480 frame",
   "blit_packed422_scanline_mmxext_billy 720x480 frame",
   "interpolate_packed422_scanline_c 720x480 frame",
   "interpolate_packed422_scanline_mmxext 720x480 frame",
   "blend_packed422_scanline_c 720x480 120/256 frame",
   "blend_packed422_scanline_mmxext 720x480 120/256 frame",
   "comb_factor_packed422_scanline 720x480 frame" 
};
const int numtests = ( sizeof( tests ) / sizeof( char * ) );

int main( int argc, char **argv )
{
    unsigned char *source422packed;
    unsigned char *source422packed2;
    unsigned char *source4444packed;
    unsigned char *dest422packed;
    uint64_t avg_sum = 0;
    uint64_t avg_count = 0;
    uint64_t before = 0;
    uint64_t after = 0;
    int stride = width * 2;
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
  
    if( !set_realtime_priority( 0 ) ) {
        fprintf( stderr, "timingtest: Can't set realtime priority (need root).\n" );
        fprintf( stderr, "timingtest: Results would be inaccurate, exiting.\n" );
        return 1;
    }

    mhz = speedy_measure_cpu_mhz();
    fprintf( stderr, "timingtest: Using CPU MHz %.3f for calculations.\n", mhz );

    /* Always use the same random seed. */
    srandom( seed );

    source422packed = (unsigned char *) malloc( width * height * 2 );
    source422packed2 = (unsigned char *) malloc( width * height * 2 );
    source4444packed = (unsigned char *) malloc( width * height * 4 );
    dest422packed = (unsigned char *) malloc( width * height * 2 );

    if( !source422packed || !source422packed2 || !source4444packed || !dest422packed ) {
        fprintf( stderr, "timingtest: Can't allocate memory.\n" );
        return 1;
    }

    for( i = 0; i < width*height*2; i++ ) {
        //source422packed[ i ] = i % 256;
        source422packed[ i ] = random() % 256;
        source422packed2[ i ] = random() % 256;
    }
    for( i = 0; i < width*height*4; i++ ) {
        source4444packed[ i ] = random() % 256;
    }

    /* Sleep to let the system cool off. */
    usleep( 10000 );

    for( i = 0; i < numruns; i++ ) {
        int j;

        if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_colour_packed422_scanline_c( dest422packed + (stride*j), width, 128, 128, 128 );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_mmxext 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_colour_packed422_scanline_mmxext( dest422packed + (stride*j), width, 128, 128, 128 );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blit_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_packed422_scanline_c( dest422packed + (stride*j), source422packed + (stride*j), width );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blit_packed422_scanline_i386_linux 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_packed422_scanline_i386_linux( dest422packed + (stride*j), source422packed + (stride*j), width );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blit_packed422_scanline_mmxext_billy 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                blit_packed422_scanline_mmxext_billy( dest422packed + (stride*j), source422packed + (stride*j), width );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "interpolate_packed422_scanline_c 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                interpolate_packed422_scanline_c( dest422packed + (stride*j),
                                                  source422packed + (stride*j),
                                                  source422packed2 + (stride*j), width );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "interpolate_packed422_scanline_mmxext 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height; j++ ) {
                interpolate_packed422_scanline_mmxext( dest422packed + (stride*j),
                                                       source422packed + (stride*j),
                                                       source422packed2 + (stride*j), width );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blend_packed422_scanline_c 720x480 120/256 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height/2; j++ ) {
                blend_packed422_scanline_c( dest422packed + (stride*j),
                                            source422packed + (stride*j),
                                            source422packed2 + (stride*j), width, 120 );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blend_packed422_scanline_mmxext 720x480 120/256 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height/2; j++ ) {
                blend_packed422_scanline_mmxext( dest422packed + (stride*j),
                                                 source422packed + (stride*j),
                                                 source422packed2 + (stride*j), width, 120 );
            }
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "comb_factor_packed422_scanline 720x480 frame" ) ) {
            rdtscll( before );
            for( j = 0; j < height/2; j++ ) {
                comb_factor_packed422_scanline( source422packed + (stride*j*2),
                                                source422packed2 + (stride*(j+1)*2),
                                                source422packed + (stride*j*2) + (stride*2),
                                                width );
            }
            rdtscll( after );
        }

        fprintf( stderr, "[%4d] Cycles: %7d\r", i, (int) (after - before) );
        avg_sum += (after - before);
        avg_count++;
    }

    fprintf( stderr, "timingtest: %llu runs tested, average time was %llu cycles (%.2f ms).\n",
             avg_count, (avg_sum/avg_count ), ((double) (avg_sum/avg_count)) / (mhz * 1000.0) );

    free( source422packed );
    free( source422packed2 );
    free( dest422packed );
    return 0;
}

