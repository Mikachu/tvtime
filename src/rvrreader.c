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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include "pulldown.h"
#include "rvrreader.h"

struct rvrreader_s
{
    int fd;
    ree_file_header_t *fhdr;
    ree_packet_t *pkt;
    int dataread;
    int curframe;
    uint8_t *framebuf[ 3 ];
    int curdecoded;
    uint8_t *decoded[ 3 ];
};

rvrreader_t *rvrreader_new( const char *filename )
{
    rvrreader_t *reader = (rvrreader_t *) malloc( sizeof( rvrreader_t ) );
    int width, height;

    if( !reader ) return 0;

    reader->fd = open( filename, O_RDONLY|O_LARGEFILE );
    if( reader->fd < 0 ) {
        fprintf( stderr, "rvrreader: Can't open file %s for reading.\n", filename );
        free( reader );
        return 0;
    }

    reader->fhdr = (ree_file_header_t *) malloc( sizeof( ree_file_header_t ) );
    if( !reader->fhdr ) {
        fprintf( stderr, "rvrreader: Can't allocate file header.\n" );
        close( reader->fd );
        free( reader );
        return 0;
    }

    reader->pkt = (ree_packet_t *) malloc( sizeof( ree_packet_t ) );
    if( !reader->pkt ) {
        fprintf( stderr, "rvrreader: Can't allocate data packet.\n" );
        free( reader->fhdr );
        close( reader->fd );
        free( reader );
        return 0;
    }

    /* Read in the file header. */
    read( reader->fd, reader->fhdr, sizeof( ree_file_header_t ) );
    lseek( reader->fd, reader->fhdr->headersize - sizeof( ree_file_header_t ), SEEK_CUR );

    reader->curframe = 0;

    width = rvrreader_get_width( reader );
    height = rvrreader_get_height( reader );
    reader->decoded[ 0 ] = (uint8_t *) malloc( width * height * 2 );
    reader->decoded[ 1 ] = (uint8_t *) malloc( width * height * 2 );
    reader->decoded[ 2 ] = (uint8_t *) malloc( width * height * 2 );
    reader->curdecoded = 0;

    if( !reader->decoded[ 0 ] || !reader->decoded[ 1 ] || !reader->decoded[ 2 ] ) {
        rvrreader_delete( reader );
        return 0;
    }

    reader->framebuf[ 0 ] = reader->decoded[ 0 ];
    reader->framebuf[ 1 ] = reader->decoded[ 1 ];
    reader->framebuf[ 2 ] = reader->decoded[ 2 ];

    /* Read first packet. */
    reader->dataread = 0;
    read( reader->fd, reader->pkt, sizeof( ree_packet_header_t ) );
    return reader;
}

void rvrreader_delete( rvrreader_t *reader )
{
    /* FIXME: Free all memory. */
    close( reader->fd );
    free( reader );
}

ree_file_header_t *rvrreader_get_fileheader( rvrreader_t *reader )
{
    return reader->fhdr;
}

int rvrreader_get_width( rvrreader_t *reader )
{
    return reader->fhdr->width;
}

int rvrreader_get_height( rvrreader_t *reader )
{
    return reader->fhdr->height;
}

static int rvrreader_next_packet( rvrreader_t *reader )
{
    int ret;
    if( !reader->dataread ) {
        lseek64( reader->fd, reader->pkt->hdr.payloadsize, SEEK_CUR );
    }
    reader->dataread = 0;

    ret = ( read( reader->fd, reader->pkt, sizeof( ree_packet_header_t ) ) == sizeof( ree_packet_header_t ) );
    if( ree_is_video_packet( reader->pkt ) ) {
        reader->curframe++;
    }
    return ret;
}

static ree_packet_t *rvrreader_cur_packet( rvrreader_t *reader )
{
    if( !reader->dataread ) {
        read( reader->fd, reader->pkt->data, reader->pkt->hdr.payloadsize );
        reader->dataread = 1;
    }
    return reader->pkt;
}

int rvrreader_next_frame( rvrreader_t *reader )
{
    while( rvrreader_next_packet( reader ) > 0 ) {
        if( ree_is_video_packet( reader->pkt ) ) {
            reader->curdecoded = (reader->curdecoded + 1) % 3;
            ree_decode_video_packet( rvrreader_cur_packet( reader ),
                                     reader->decoded[ reader->curdecoded ],
                                     rvrreader_get_width( reader ), rvrreader_get_height( reader ) );
            reader->framebuf[ 0 ] = reader->decoded[ reader->curdecoded ];
            reader->framebuf[ 1 ] = reader->decoded[ (reader->curdecoded + 3 - 1) % 3 ];
            reader->framebuf[ 2 ] = reader->decoded[ (reader->curdecoded + 3 - 2) % 3 ];
            return 1;
        }
    }
    return 0;
}

uint8_t *rvrreader_get_curframe( rvrreader_t *reader )
{
    return reader->framebuf[ 0 ];
}

uint8_t *rvrreader_get_lastframe( rvrreader_t *reader )
{
    return reader->framebuf[ 1 ];
}

uint8_t *rvrreader_get_secondlastframe( rvrreader_t *reader )
{
    return reader->framebuf[ 2 ];
}

