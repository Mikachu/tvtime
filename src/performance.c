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
#include <string.h>
#include <sys/time.h>
#include "performance.h"

#define DROP_HISTORY_SIZE        10
#define NUM_TO_AVERAGE          100 

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

struct performance_s
{
    struct timeval lastframetime;

    struct timeval acquired_input;
    struct timeval show_bot;
    struct timeval constructed_top;
    struct timeval wait_for_bot;
    struct timeval show_top;
    struct timeval constructed_bot;

    int frames_dropped;
    int fieldtime;

    int time_bot_to_top;
    int time_top_to_bot;

    int total_blits;
    int curblit;
    double total_blit_time[ NUM_TO_AVERAGE ];
    double total_blitting_ms;
    
    int drop_history[ DROP_HISTORY_SIZE ];
    int drop_pos;

    int drop_reset;
};

performance_t *performance_new( int fieldtimeus )
{
    performance_t *perf = malloc( sizeof( performance_t ) );
    if( !perf ) {
        return 0;
    }

    perf->fieldtime = fieldtimeus;
    perf->time_bot_to_top = 0;
    perf->time_top_to_bot = 0;
    perf->drop_reset = 0;
    perf->total_blits = 0;
    perf->curblit = 0;
    perf->total_blitting_ms = 0.0;
    memset( perf->total_blit_time, 0, sizeof( perf->total_blit_time ) );
    gettimeofday( &perf->lastframetime, 0 );
    gettimeofday( &perf->acquired_input, 0 );
    gettimeofday( &perf->show_bot, 0 );
    gettimeofday( &perf->constructed_top, 0 );
    gettimeofday( &perf->wait_for_bot, 0 );
    gettimeofday( &perf->show_top, 0 );
    gettimeofday( &perf->constructed_bot, 0 );

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

void performance_checkpoint_show_bot_field( performance_t *perf )
{
    gettimeofday( &perf->show_bot, 0 );
    perf->time_top_to_bot = timediff( &perf->show_bot, &perf->show_top );
     
    perf->total_blitting_ms -= perf->total_blit_time[ perf->curblit ];
    perf->total_blit_time[ perf->curblit ] = ((double) timediff( &perf->show_bot, &perf->acquired_input )) / 1000.0;
    perf->total_blitting_ms += perf->total_blit_time[ perf->curblit ];
    perf->curblit = (perf->curblit + 1) % NUM_TO_AVERAGE;
    perf->total_blits++;
}

void performance_checkpoint_constructed_top_field( performance_t *perf )
{
    gettimeofday( &perf->constructed_top, 0 );
}

void performance_checkpoint_wait_for_bot_field( performance_t *perf )
{
    gettimeofday( &perf->wait_for_bot, 0 );
}

void performance_checkpoint_show_top_field( performance_t *perf )
{
    gettimeofday( &perf->show_top, 0 );
    perf->time_bot_to_top = timediff( &perf->show_top, &perf->show_bot );
    
    perf->total_blitting_ms -= perf->total_blit_time[ perf->curblit ];
    perf->total_blit_time[ perf->curblit ] = ((double) timediff( &perf->show_top, &perf->wait_for_bot )) / 1000.0;
    perf->total_blitting_ms += perf->total_blit_time[ perf->curblit ];
    perf->curblit = (perf->curblit + 1) % NUM_TO_AVERAGE;
    perf->total_blits++;
}

void performance_checkpoint_constructed_bot_field( performance_t *perf )
{
    gettimeofday( &perf->constructed_bot, 0 );
}

int performance_get_usecs_since_frame_acquired( performance_t *perf )
{
    struct timeval now;
    gettimeofday( &now, 0 );
    return timediff( &now, &perf->acquired_input );
}

void performance_print_last_frame_stats( performance_t *perf, int framesize )
{
    double acquire = ((double) timediff( &perf->acquired_input, &perf->constructed_bot )) / 1000.0;
    double show_bot = ((double) timediff( &perf->show_bot, &perf->lastframetime )) / 1000.0;
    double constructed_top = ((double) timediff( &perf->constructed_top, &perf->show_bot )) / 1000.0;
    double wait_for_bot = ((double) timediff( &perf->wait_for_bot, &perf->constructed_top )) / 1000.0;
    double show_top = ((double) timediff( &perf->show_top, &perf->wait_for_bot )) / 1000.0;
    double constructed_bot = ((double) timediff( &perf->constructed_bot, &perf->show_top )) / 1000.0;
    double cycle_time = ((double) timediff( &perf->acquired_input, &perf->lastframetime )) / 1000.0;
    double nummeasured = NUM_TO_AVERAGE;
    double avgblittime;

    //double blit_time = show_bot;
    //if( show_bot > show_top ) blit_time = show_top;

    if( nummeasured > perf->total_blits ) {
        nummeasured = perf->total_blits;
    }
    avgblittime = perf->total_blitting_ms / nummeasured;

    fprintf( stderr, "tvtime: acquire %5.2fms, show bot %5.2fms, build top %5.2fms\n"
                     "tvtime: waitbot %5.2fms, show top %5.2fms, build bot %5.2fms\n"
		     "tvtime: total_blits: %d, average blit time: %.2f\n",
             acquire, show_bot, constructed_top, wait_for_bot, show_top, constructed_bot,
             perf->total_blits, avgblittime );

    fprintf( stderr, "tvtime: System->video blit %.2fMB/sec, used %.2f%% CPU to deinterlace.\n",
             ( ((double) framesize) / avgblittime ) * ( 1000.0 / ( 1024.0 * 1024.0 ) ),
             ( ( constructed_top + constructed_bot ) / cycle_time ) * 100.0 );
    fprintf( stderr, "tvtime: Last frame times top-to-bot: %5.2f, bot-to-top: %5.2f\n",
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
        fprintf( stderr, "tvtime: Frame drop detected (%2.2fms between consecutive frames.\n",
            ((double) timediff( &perf->acquired_input, &perf->lastframetime)) / 1000.0 );
        performance_print_last_frame_stats( perf, framesize );
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

