/**
 * Copyright (C) 2001 Paul Davis <pbd@Op.Net>,
 *                    Billy Biggs <vektor@dumbterm.net>.
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
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <sched.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include "rtctimer.h"

struct rtctimer_s
{
    int rtc_fd;
    int current_hz;
    int rtc_running;
    int verbose;
    int usecs;
};

rtctimer_t *rtctimer_new( int verbose )
{
    rtctimer_t *rtctimer = (rtctimer_t *) malloc( sizeof( rtctimer_t ) );
    if( !rtctimer ) return 0;

    rtctimer->verbose = verbose;
    if( ( rtctimer->rtc_fd = open( "/dev/rtc", O_RDONLY ) ) < 0 ) {
        if( rtctimer->verbose ) {
            fprintf( stderr, "rtctimer: Cannot open /dev/rtc: %s\n",
                     strerror( errno ) );
        }
        free( rtctimer );
        return 0;
    }

    if( fcntl( rtctimer->rtc_fd, F_SETOWN, getpid() ) < 0 ) {
        if( rtctimer->verbose ) {
            fprintf( stderr, "rtctimer: cannot set ownership of "
                             "/dev/rtc: %s\n", strerror( errno ) );
        }
        close( rtctimer->rtc_fd );
        free( rtctimer );
        return 0;
    }

    rtctimer->rtc_running = 0;
    rtctimer->current_hz = 0;
    rtctimer->usecs = 0;
    return rtctimer;
}

void rtctimer_delete( rtctimer_t *rtctimer )
{
    rtctimer_stop_clock( rtctimer );
    close( rtctimer->rtc_fd );
    free( rtctimer );
}

int rtctimer_next_tick( rtctimer_t *rtctimer )
{
    unsigned long rtc_data;
    struct pollfd pfd;
    pfd.fd = rtctimer->rtc_fd;
    pfd.events = POLLIN | POLLERR;

again:
    if( poll( &pfd, 1, 100000 ) < 0 ) {
        if( errno == EINTR ) {
            /**
             * This happens mostly when run under gdb, or when
             * exiting due to a signal.
             */
            goto again;
        }

        if( rtctimer->verbose ) {
            fprintf( stderr, "rtctimer: poll call failed: %s\n",
                     strerror( errno ) );
        }
        return 0;
    }

    read( rtctimer->rtc_fd, &rtc_data, sizeof( rtc_data ) );
    return 1;
}

int rtctimer_set_interval( rtctimer_t *rtctimer, int hz )
{
    int restart;

    if( hz == rtctimer->current_hz ) {
        return 1;
    }
    
    restart = rtctimer_stop_clock( rtctimer );

    if( ioctl( rtctimer->rtc_fd, RTC_IRQP_SET, hz ) < 0 ) {
        if( rtctimer->verbose ) {
            fprintf( stderr, "rtctimer: Cannot set periodic interval: %s\n",
                     strerror( errno ) );
        }
        return 0;
    }

    rtctimer->current_hz = hz;
    rtctimer->usecs = (int) ( ( ( 1000.0 * 1000.0 ) / hz ) + 0.5 );
    fprintf( stderr, "usecs is %d\n", rtctimer->usecs );

    if( restart ) {
        rtctimer_start_clock( rtctimer );
    }

    return 1;
}

int rtctimer_start_clock( rtctimer_t *rtctimer )
{
    if( !rtctimer->rtc_running ) {
        if( ioctl( rtctimer->rtc_fd, RTC_PIE_ON, 0 ) < 0 ) {
            if( rtctimer->verbose ) {
                fprintf( stderr, "rtctimer: cannot start periodic "
                         "interrupts: %s\n", strerror( errno ) );
            }
            return 0;
        }
        rtctimer->rtc_running = 1;
    }
    return rtctimer->rtc_running;
}
    
int rtctimer_stop_clock( rtctimer_t *rtctimer )
{
    int was_running = rtctimer->rtc_running;

    if( rtctimer->rtc_running ) {
        if( ioctl( rtctimer->rtc_fd, RTC_PIE_OFF, 0 ) < 0 ) {
            if( rtctimer->verbose ) {
                fprintf( stderr, "rtctimer: cannot stop periodic "
                         "interrupts: %s\n", strerror( errno ) );
            }
            return rtctimer->rtc_running;
        }
        rtctimer->rtc_running = 0;
    }

    return was_running;
}

int rtctimer_get_resolution( rtctimer_t *rtctimer )
{
    return rtctimer->current_hz;
}

int rtctimer_get_usecs( rtctimer_t *rtctimer )
{
    return rtctimer->usecs;
}

int set_realtime_priority( int max )
{
    struct sched_param schp;

    memset( &schp, 0, sizeof( schp ) );
    if( max ) schp.sched_priority = sched_get_priority_max( SCHED_FIFO );
    else schp.sched_priority = sched_get_priority_max( SCHED_FIFO ) - 1;

    if( sched_setscheduler( 0, SCHED_FIFO, &schp ) != 0 ) {
        return 0;
    }

    return 1;
}

