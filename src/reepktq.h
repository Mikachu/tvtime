/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef REEPKTQ_H_INCLUDED
#define REEPKTQ_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reepktq_s reepktq_t;

/**
 * Create a new queue with the given frame size and number of frames.
 * This will allocate all the memory, and mlockall() it.
 */
reepktq_t *reepktq_new( int numframes, int framesize );

/**
 * Delete (free) the queue.
 */
void reepktq_delete( reepktq_t *pktq );

/**
 * Get a pointer to the next enqueue buffer.  Returns 0 if the queue
 * is full.
 */
uint8_t *reepktq_enqueue( reepktq_t *pktq );

/**
 * Finish enquing an entry.  Increments the next pointer.
 */
void reepktq_complete_enqueue( reepktq_t *pktq );

/**
 * Returns a pointer to the head of the queue.  Returns 0 if no entries
 * are in the queue.
 */
uint8_t *reepktq_head( reepktq_t *pktq );

/**
 * Increments the head pointer for the queue.
 */
void reepktq_dequeue( reepktq_t *pktq );

/**
 * Returns the size of the frames in the queue.
 */
int reepktq_frame_size( reepktq_t *pktq );

int reepktq_queue_size( reepktq_t *pktq );

#ifdef __cplusplus
};
#endif
#endif /* REEPKTQ_H_INCLUDED */
