
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "performance.h"

/**
 * Tolerance in usec for what we consider a frame drop.
 */
static int tolerance = 2000;

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}


struct performance_s
{
    struct timeval lastfieldtime;
    struct timeval lastframetime;

    struct timeval aquired_input;

    struct timeval constructed_top;
    struct timeval delay_blit_top;
    struct timeval blit_top_start;
    struct timeval blit_top_end;

    struct timeval constructed_bot;
    struct timeval delay_blit_bot;
    struct timeval blit_bot_start;
    struct timeval blit_bot_end;

    int frames_dropped;
    int fieldtime;

    int last_blit_time;

    int time_bot_to_top;
    int time_top_to_bot;
};

performance_t *performance_new( int fieldtimeus )
{
    performance_t *perf = (performance_t *) malloc( sizeof( performance_t ) );
    if( !perf ) {
        return 0;
    }
    perf->fieldtime = fieldtimeus;
    perf->last_blit_time = 0;
    perf->time_bot_to_top = 0;
    perf->time_top_to_bot = 0;
    gettimeofday( &perf->lastfieldtime, 0 );
    gettimeofday( &perf->lastframetime, 0 );
    gettimeofday( &perf->aquired_input, 0 );
    gettimeofday( &perf->constructed_top, 0 );
    gettimeofday( &perf->delay_blit_top, 0 );
    gettimeofday( &perf->blit_top_start, 0 );
    gettimeofday( &perf->blit_top_end, 0 );
    gettimeofday( &perf->constructed_bot, 0 );
    gettimeofday( &perf->delay_blit_bot, 0 );
    gettimeofday( &perf->blit_bot_start, 0 );
    gettimeofday( &perf->blit_bot_end, 0 );
    return perf;
}

void performance_delete( performance_t *perf )
{
    free( perf );
}

void performance_checkpoint_aquired_input_frame( performance_t *perf )
{
    perf->lastframetime = perf->aquired_input;
    gettimeofday( &perf->aquired_input, 0 );
}

void performance_checkpoint_constructed_top_field( performance_t *perf )
{
    gettimeofday( &perf->constructed_top, 0 );
}

void performance_checkpoint_delayed_blit_top_field( performance_t *perf )
{
    gettimeofday( &perf->delay_blit_top, 0 );
}

void performance_checkpoint_blit_top_field_start( performance_t *perf )
{
    gettimeofday( &perf->blit_top_start, 0 );
}

void performance_checkpoint_blit_top_field_end( performance_t *perf )
{
    gettimeofday( &perf->blit_top_end, 0 );
    perf->lastfieldtime = perf->blit_top_end;
    perf->last_blit_time = timediff( &perf->blit_top_end, &perf->blit_top_start );
    perf->time_bot_to_top = timediff( &perf->blit_top_end, &perf->blit_bot_end );
}

void performance_checkpoint_constructed_bot_field( performance_t *perf )
{
    gettimeofday( &perf->constructed_bot, 0 );
}

void performance_checkpoint_delayed_blit_bot_field( performance_t *perf )
{
    gettimeofday( &perf->delay_blit_bot, 0 );
}

void performance_checkpoint_blit_bot_field_start( performance_t *perf )
{
    gettimeofday( &perf->blit_bot_start, 0 );
}

void performance_checkpoint_blit_bot_field_end( performance_t *perf )
{
    gettimeofday( &perf->blit_bot_end, 0 );
    perf->lastfieldtime = perf->blit_bot_end;
    perf->last_blit_time = timediff( &perf->blit_bot_end, &perf->blit_bot_start );
    perf->time_top_to_bot = timediff( &perf->blit_bot_end, &perf->blit_top_end );
}

int performance_get_usecs_since_frame_aquired( performance_t *perf )
{
    struct timeval now;
    gettimeofday( &now, 0 );
    return timediff( &now, &perf->aquired_input );
}

int performance_get_usecs_since_last_field( performance_t *perf )
{
    struct timeval now;
    gettimeofday( &now, 0 );
    return timediff( &now, &perf->lastfieldtime );
}

int performance_get_usecs_of_last_blit( performance_t *perf )
{
    return perf->last_blit_time;
}

void performance_print_last_frame_stats( performance_t *perf )
{
    fprintf( stderr, "tvtime: aquire % 2.2fms, build top % 2.2fms, wait top % 2.2fms, blit top % 2.2fms\n"
                     "tvtime:                 build bot % 2.2fms, wait bot % 2.2fms, blit bot % 2.2fms\n",
             ((double) timediff( &perf->aquired_input, &perf->blit_bot_end )) / 1000.0,
             ((double) timediff( &perf->constructed_top, &perf->lastframetime )) / 1000.0,
             ((double) timediff( &perf->delay_blit_top, &perf->constructed_top )) / 1000.0,
             ((double) timediff( &perf->blit_top_end, &perf->blit_top_start )) / 1000.0,
             ((double) timediff( &perf->constructed_bot, &perf->blit_top_end )) / 1000.0,
             ((double) timediff( &perf->delay_blit_bot, &perf->constructed_bot )) / 1000.0,
             ((double) timediff( &perf->blit_bot_end, &perf->blit_bot_start )) / 1000.0 );
    fprintf( stderr, "tvtime: top-to-bot: % 2.2f, bot-to-top: % 2.2f\n",
             (double) perf->time_top_to_bot / 1000.0,
             (double) perf->time_bot_to_top / 1000.0 );
}

void performance_print_frame_drops( performance_t *perf )
{
    if( timediff( &perf->aquired_input, &perf->lastframetime ) > ( (perf->fieldtime * 2) + tolerance ) ) {
        fprintf( stderr, "tvtime: Frame drop detected (% 2.2fms between consecutive frames.\n",
            ((double) timediff( &perf->aquired_input, &perf->lastframetime)) / 1000.0 );
    }
}

