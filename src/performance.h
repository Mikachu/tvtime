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

#ifndef PERFORMANCE_H_INCLUDED
#define PERFORMANCE_H_INCLUDED

/**
 * This object provides timing data for tvtime, taking timestamps and
 * answering questions about how long things took.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct performance_s performance_t;

performance_t *performance_new( int fieldtimeus );
void performance_delete( performance_t *perf );

void performance_checkpoint_aquired_input_frame( performance_t *perf );

void performance_checkpoint_constructed_top_field( performance_t *perf );
void performance_checkpoint_delayed_blit_top_field( performance_t *perf );
void performance_checkpoint_blit_top_field_start( performance_t *perf );
void performance_checkpoint_blit_top_field_end( performance_t *perf );

void performance_checkpoint_constructed_bot_field( performance_t *perf );
void performance_checkpoint_delayed_blit_bot_field( performance_t *perf );

void performance_checkpoint_blit_bot_field_start( performance_t *perf );
void performance_checkpoint_blit_bot_field_end( performance_t *perf );

void performance_print_last_frame_stats( performance_t *perf, int framesize );
void performance_print_frame_drops( performance_t *perf, int framesize );

int performance_get_usecs_since_frame_aquired( performance_t *perf );
int performance_get_usecs_since_last_field( performance_t *perf );
int performance_get_usecs_of_last_blit( performance_t *perf );

double performance_get_percentage_dropped( performance_t *perf );

#ifdef __cplusplus
};
#endif
#endif /* PERFORMANCE_H_INCLUDED */
