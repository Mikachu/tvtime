/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef REEPKTQ_H_INCLUDED
#define REEPKTQ_H_INCLUDED

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
unsigned char *reepktq_enqueue( reepktq_t *pktq );

/**
 * Finish enquing an entry.  Increments the next pointer.
 */
void reepktq_complete_enqueue( reepktq_t *pktq );

/**
 * Returns a pointer to the head of the queue.  Returns 0 if no entries
 * are in the queue.
 */
unsigned char *reepktq_head( reepktq_t *pktq );

/**
 * Increments the head pointer for the queue.
 */
void reepktq_dequeue( reepktq_t *pktq );

/**
 * Returns the size of the frames in the queue.
 */
int reepktq_frame_size( reepktq_t *pktq );

int reepktq_queue_size( reepktq_t *pktq );

#endif /* REEPKTQ_H_INCLUDED */
