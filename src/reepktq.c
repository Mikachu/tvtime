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

#include <stdlib.h>
#include <sys/mman.h>
#include <pthread.h>
#include "reepktq.h"

struct reepktq_s
{
    int numframes;
    int framesize;

    int head;
    int tail;
    int size;

    int itemwaiting;

    uint8_t *data;
    pthread_mutex_t lock;
};

reepktq_t *reepktq_new( int numframes, int framesize )
{
    reepktq_t *pktq = (reepktq_t *) malloc( sizeof( reepktq_t ) );
    int i;

    if( !pktq ) return 0;

    pktq->numframes = numframes;
    pktq->framesize = framesize;
    pktq->tail = 0;
    pktq->head = 0;
    pktq->size = 0;
    pktq->itemwaiting = 0;
    pthread_mutex_init( &(pktq->lock), 0 );

    pktq->data = (uint8_t *) malloc( pktq->numframes * pktq->framesize );
    if( !pktq->data ) { free( pktq ); return 0; }

    for( i = 0; i < numframes * framesize; i++ ) pktq->data[ i ] = 0;
    mlockall( MCL_FUTURE );
    for( i = 0; i < numframes * framesize; i++ ) pktq->data[ i ] = 0;

    return pktq;
}

void reepktq_delete( reepktq_t *pktq )
{
    free( pktq->data );
    free( pktq );
}

uint8_t *reepktq_enqueue( reepktq_t *pktq )
{
    uint8_t *data = &(pktq->data[ pktq->tail * pktq->framesize ]);

    pthread_mutex_lock( &(pktq->lock) );
    if( pktq->size == pktq->numframes - 2 ) {
        pthread_mutex_unlock( &(pktq->lock) );
        return 0;
    }
    pktq->tail = ( pktq->tail + 1 ) % pktq->numframes;
    pktq->itemwaiting = 1;
    pktq->size++;
    pthread_mutex_unlock( &(pktq->lock) );
    return data;
}

int reepktq_queue_size( reepktq_t *pktq )
{
    return pktq->size;
}

void reepktq_complete_enqueue( reepktq_t *pktq )
{
    pthread_mutex_lock( &(pktq->lock) );
    pktq->itemwaiting = 0;
    pthread_mutex_unlock( &(pktq->lock) );
}

uint8_t *reepktq_head( reepktq_t *pktq )
{
    pthread_mutex_lock( &(pktq->lock) );
    if( !pktq->size || ( pktq->size == 1 && pktq->itemwaiting == 1 ) ) {
        pthread_mutex_unlock( &(pktq->lock) );
        return 0;
    }
    pthread_mutex_unlock( &(pktq->lock) );
    return &(pktq->data[ pktq->head * pktq->framesize ]);
}

void reepktq_dequeue( reepktq_t *pktq )
{
    pthread_mutex_lock( &(pktq->lock) );
    pktq->size--;
    pthread_mutex_unlock( &(pktq->lock) );
    pktq->head = ( pktq->head + 1 ) % pktq->numframes;
}

int reepktq_frame_size( reepktq_t *pktq )
{
    return pktq->framesize;
}

