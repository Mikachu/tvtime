/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef RTCTIMER_H_INCLUDED
#define RTCTIMER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Class for accurate timing support under operating systems supporting
 * the /dev/rtc real time clock device.  Under linux, the rtc device can
 * be used in conjunction with realtime (SCHED_FIFO) priority to allow
 * for a thread to wake up every millisecond.
 *
 * The real time clock is key to smooth video playback, since it
 * allows my video renderer thread to have +/- 1ms accuracy on my frame
 * blits.
 *
 * This class provides a timer where you tell it a frequency (in hz) and
 * then you can sleep until the next hz by calling rtctimer_next_tick().
 * The hz value given MUST be a power of 2.
 *
 * This code originally appeared in a C++ class in my project ttrk
 * (http://www.div8.net/ttrk/)
 */

typedef struct rtctimer_s rtctimer_t;

/**
 * Create a new timer object.  This does not initialize the clock to any
 * value.  The verbose flag indicates that we want to see error messages
 * to stderr.
 */
rtctimer_t *rtctimer_new( int verbose );

/**
 * Free the timer object.  This will stop the clock first if it is
 * active.
 */
void rtctimer_delete( rtctimer_t *rtctimer );

/**
 * Starts the clock running.  Returns 1 on success, 0 otherwise.
 */
int rtctimer_start_clock( rtctimer_t *rtctimer );

/**
 * Stops the clock.  Returns 1 on success, 0 otherwise.
 */
int rtctimer_stop_clock( rtctimer_t *rtctimer );

/**
 * Attempts to set the frequency of the clock to the given value in hz.
 * The hz value given MUST be a power of 2.  Returns 1 on success, 0
 * otherwise.
 */
int rtctimer_set_interval( rtctimer_t *rtctimer, int hz );

/**
 * Sleeps (using poll()) until the next tick.  This is guarenteed to
 * block, so if you're running with realtime priority, this should be
 * used to yield.
 */
int rtctimer_next_tick( rtctimer_t *rtctimer );

/**
 * Returns the current resolution in hz.
 */
int rtctimer_get_resolution( rtctimer_t *rtctimer );

/**
 * Returns the number of usecs in each tick.
 */
int rtctimer_get_usecs( rtctimer_t *rtctimer );

/**
 * Returns 1 if realtime (SCHED_FIFO) priority was gained, 0 if it is
 * unavailable (likely you're not root).
 */
int set_realtime_priority( int max );

#ifdef __cplusplus
};
#endif
#endif /* RTCTIMER_H_INCLUDED */
