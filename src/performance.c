/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "performance.h"

#define DROP_HISTORY_SIZE	10

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

struct performance_s
{
    struct timeval lastfieldtime;
    struct timeval lastframetime;

    struct timeval acquired_input;

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

    int drop_history[ DROP_HISTORY_SIZE ];
    int drop_pos;

    int drop_reset;
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
    perf->drop_reset = 0;
    gettimeofday( &perf->lastfieldtime, 0 );
    gettimeofday( &perf->lastframetime, 0 );
    gettimeofday( &perf->acquired_input, 0 );
    gettimeofday( &perf->constructed_top, 0 );
    gettimeofday( &perf->delay_blit_top, 0 );
    gettimeofday( &perf->blit_top_start, 0 );
    gettimeofday( &perf->blit_top_end, 0 );
    gettimeofday( &perf->constructed_bot, 0 );
    gettimeofday( &perf->delay_blit_bot, 0 );
    gettimeofday( &perf->blit_bot_start, 0 );
    gettimeofday( &perf->blit_bot_end, 0 );
    memset( perf->drop_history, 0, sizeof( perf->drop_history ) );
    perf->drop_pos = 0;
    return perf;
}

void performance_delete( performance_t *perf )
{
    free( perf );
}

void performance_checkpoint_acquired_input_frame( performance_t *perf )
{
    int dropdiff;

    perf->lastframetime = perf->acquired_input;
    gettimeofday( &perf->acquired_input, 0 );

    dropdiff = timediff( &perf->acquired_input, &perf->lastframetime ) - (perf->fieldtime * 3);
    if( dropdiff >= 0 ) {
        perf->drop_history[ perf->drop_pos ] = ( dropdiff / (perf->fieldtime*2) ) + 2;
    } else {
        perf->drop_history[ perf->drop_pos ] = 1;
    }
    perf->drop_pos = ( perf->drop_pos + 1 ) % DROP_HISTORY_SIZE;
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

int performance_get_usecs_since_frame_acquired( performance_t *perf )
{
    struct timeval now;
    gettimeofday( &now, 0 );
    return timediff( &now, &perf->acquired_input );
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

void performance_print_last_frame_stats( performance_t *perf, int framesize )
{
    double acquire = ((double) timediff( &perf->acquired_input, &perf->blit_bot_end )) / 1000.0;
    double build_top = ((double) timediff( &perf->constructed_top, &perf->lastframetime )) / 1000.0;
    double wait_top = ((double) timediff( &perf->delay_blit_top, &perf->constructed_top )) / 1000.0;
    double blit_top = ((double) timediff( &perf->blit_top_end, &perf->blit_top_start )) / 1000.0;
    double build_bot = ((double) timediff( &perf->constructed_bot, &perf->blit_top_end )) / 1000.0;
    double wait_bot = ((double) timediff( &perf->delay_blit_bot, &perf->constructed_bot )) / 1000.0;
    double blit_bot = ((double) timediff( &perf->blit_bot_end, &perf->blit_bot_start )) / 1000.0;

    fprintf( stderr, "tvtime: acquire %5.2fms, build top %5.2fms, wait top %5.2fms, blit top %5.2fms\n"
                     "tvtime:                  build bot %5.2fms, wait bot %5.2fms, blit bot %5.2fms\n",
             acquire, build_top, wait_top, blit_top, build_bot, wait_bot, blit_bot );
    fprintf( stderr, "tvtime: system->video memory speed approximately %.2fMB/sec\n",
             ( ( (double) framesize ) / blit_top ) * ( 1000.0 / ( 1024.0 * 1024.0 ) ) );
    fprintf( stderr, "tvtime: Using approximately %.2f%% CPU to deinterlace.\n",
             ( ( build_top + build_bot ) / (acquire+build_top+wait_top+blit_top+build_bot+wait_bot+blit_bot) ) * 100.0 );
    fprintf( stderr, "tvtime: top-to-bot: %5.2f, bot-to-top: %5.2f\n",
             (double) perf->time_top_to_bot / 1000.0,
             (double) perf->time_bot_to_top / 1000.0 );
}

void performance_print_frame_drops( performance_t *perf, int framesize )
{
    /* Ignore the first frame drop after a reset. */
    if( !perf->drop_reset ) {
        perf->drop_reset = 1;
        return;
    }

    if( timediff( &perf->acquired_input, &perf->lastframetime ) > (perf->fieldtime * 3) ) {
        double build_top = ((double) timediff( &perf->constructed_top, &perf->lastframetime )) / 1000.0;
        double blit_top = ((double) timediff( &perf->blit_top_end, &perf->blit_top_start )) / 1000.0;
        double build_bot = ((double) timediff( &perf->constructed_bot, &perf->blit_top_end )) / 1000.0;
        double blit_bot = ((double) timediff( &perf->blit_bot_end, &perf->blit_bot_start )) / 1000.0;

        fprintf( stderr, "tvtime: Frame drop detected (%2.2fms between consecutive frames.\n",
            ((double) timediff( &perf->acquired_input, &perf->lastframetime)) / 1000.0 );
        performance_print_last_frame_stats( perf, framesize );

        if( build_top >= blit_top && build_top >= build_bot && build_top >= blit_bot ) {
            fprintf( stderr, "tvtime: Took %2.2fms to build a field.\n",
                     build_top );
        }
        if( blit_top >= build_top && blit_top >= build_bot && blit_top >= blit_bot ) {
            fprintf( stderr, "tvtime: Took %2.2fms to blit a field.\n",
                     blit_top );
        }
        if( build_bot >= build_top && build_bot >= blit_top && build_bot >= blit_bot ) {
            fprintf( stderr, "tvtime: Took %2.2fms to build a field.\n",
                     build_bot );
        }
        if( blit_bot >= build_top && blit_bot >= blit_top && blit_bot >= build_bot ) {
            fprintf( stderr, "tvtime: Took %2.2fms to blit a field.\n",
                     blit_bot );
        }
    }
}

double performance_get_percentage_dropped( performance_t *perf )
{
    int cur = 0;
    int i;

    for( i = 0; i < DROP_HISTORY_SIZE; i++ ) {
        cur += perf->drop_history[ i ];
    }

    return 1.0 - ( ( (double) DROP_HISTORY_SIZE ) / ( (double) cur ) );
}

