
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include "videotools.h"

static inline void get_time( int64_t *const ptime )
{
   asm volatile ("rdtsc" : "=A" (*ptime));
}

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

int main( int argc, char **argv )
{
    unsigned char *source422planar;
    unsigned char *dest422packed;
    int width, height;
    int avg_sum = 0;
    int avg_count = 0;
    int i = 0;
    int64_t before;
    int64_t after;
    video_correction_t *vc = video_correction_new();

    width = 720;
    height = 480;

    source422planar = (unsigned char *) malloc( width * height * 2 );
    dest422packed = (unsigned char *) malloc( width * height * 2 );

    for( i = 0; i < width*height*2; i++ ) {
        source422planar[ i ] = i % 256;
    }
    usleep( 10000 );
    for( i = 0; i < 100; i++ ) {
        struct timeval time_before;
        struct timeval time_after;
        usleep( 20 );
        gettimeofday( &time_before, 0 );
        get_time( &before );
        video_correction_planar422_field_to_packed422_frame( vc, dest422packed,
            source422planar, source422planar + (width*height),
            source422planar + (width*height) + (width/2 * height), 0, width*2, width, width, height );
        get_time( &after );
        gettimeofday( &time_after, 0 );
        fprintf( stderr, "top: %10d\n", timediff( &time_after, &time_before ) );
        avg_sum += timediff( &time_after, &time_before );
        avg_count++;
        usleep( 20 );
        gettimeofday( &time_before, 0 );
        get_time( &before );
        video_correction_planar422_field_to_packed422_frame( vc, dest422packed,
            source422planar + width, source422planar + (width*height) + (width/2),
            source422planar + (width*height) + (width/2 * height) + (width/2), 1, width*2, width, width, height );
        get_time( &after );
        gettimeofday( &time_after, 0 );
        fprintf( stderr, "bot: %10d\n", timediff( &time_after, &time_before ) );
        avg_sum += timediff( &time_after, &time_before );
        avg_count++;
    }
    fprintf( stderr, "%d tested, average was %f.\n", avg_count, ((double) avg_sum) / ((double) avg_count) );

    free( source422planar );
    free( dest422packed );
    return 0;
}

