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

/* Use a constant random seed for tests. */
const unsigned int seed = 2;

/* Test against NTSC resolution. */
const unsigned int width = 720;
const unsigned int height = 480;

/* Run for 1000 runs. */
const unsigned int numruns = 1000;

static const char *tests[] = {
   "blit_colour_packed422_scanline_c",
   "blit_colour_packed422_scanline_mmx",
   "blit_colour_packed422_scanline_mmxext",
};
const int numtests = ( sizeof( tests ) / sizeof( char * ) );

int main( int argc, char **argv )
{
    unsigned char *source422planar;
    unsigned char *dest422packed;
    uint64_t avg_sum = 0;
    uint64_t avg_count = 0;
    uint64_t before = 0;
    uint64_t after = 0;
    int testid = 0;
    int i;

    for( i = 0; i < numtests; i++ ) {
        fprintf( stderr, "timingtest: %2d: %s\n", i, tests[ i ] );
    }

    if( argc < 2 ) {
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
        fprintf( stderr, "timingtest: Results will be inaccurate.\n" );
    }

    /* Always use the same random seed. */
    fprintf( stderr, "timingtest: Random seed is %d.\n", seed );
    srandom( seed );

    /* For the logs. */
    fprintf( stderr, "timingtest: Resolution is %dx%d\n", width, height );

    source422planar = (unsigned char *) malloc( width * height * 2 );
    dest422packed = (unsigned char *) malloc( width * height * 2 );

    if( !source422planar || !dest422packed ) {
        fprintf( stderr, "timingtest: Can't allocate memory.\n" );
        return 1;
    }

    fprintf( stderr, "timingtest: Initializing source to random bytes...\n" );
    for( i = 0; i < width*height*2; i++ ) {
        //source422planar[ i ] = i % 256;
        source422planar[ i ] = random() % 256;
    }
    fprintf( stderr, "timingtest: Initialization complete.\n" );

    /* Sleep to let the system cool off. */
    usleep( 10000 );

    fprintf( stderr, "timingtest: Starting test:\n" );
    for( i = 0; i < numruns; i++ ) {

        /* Pause to let the scheduler run. */
        usleep( 20 );

        if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_c" ) ) {
            rdtscll( before );
            blit_colour_packed422_scanline_c( source422planar, width, 128, 128, 128 );
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_mmx" ) ) {
            rdtscll( before );
            blit_colour_packed422_scanline_mmx( source422planar, width, 128, 128, 128 );
            rdtscll( after );
        } else if( !strcmp( tests[ testid ], "blit_colour_packed422_scanline_mmxext" ) ) {
            rdtscll( before );
            blit_colour_packed422_scanline_mmxext( source422planar, width, 128, 128, 128 );
            rdtscll( after );
        }

        fprintf( stderr, "[%4d] Cycles: %7d\r", i, (int) (after - before) );
        avg_sum += (after - before);
        avg_count++;
    }

    fprintf( stderr, "\ntimingtest: %llu runs tested, average time was %llu cycles.\n",
             avg_count, (avg_sum/avg_count ) );

    free( source422planar );
    free( dest422packed );
    return 0;
}

