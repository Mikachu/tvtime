/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "tvtimeconf.h"
#include "fifo.h"

struct fifo_s {
    int fd;
    char buf[ 256 ];
    int bufpos;
    char *filename;
};

/* use NULL for givenname to use the default named pipe */
fifo_t *fifo_new( config_t *ct, char *givenname )
{
    char *filename = config_get_command_pipe( ct );
    fifo_t *fifo = (fifo_t *) malloc( sizeof( struct fifo_s ) );
    char temp_filename[ 1024 ];

    if( !fifo ) {
        return 0;
    }

    fifo->filename = 0;
    if( givenname ) filename = givenname;

    if( filename[ 0 ] == '~' && getenv( "HOME" ) ) {
        snprintf( temp_filename, sizeof( temp_filename ), "%s%s", getenv( "HOME" ), filename + 1 );
        temp_filename[ sizeof( temp_filename ) - 1 ] = '\0';
        filename = temp_filename;
    }

    fifo->fd = open( filename , O_RDONLY | O_NONBLOCK );
    if( fifo->fd == -1 ) {
        /* attempt to create it */
        if( mkfifo( filename, S_IREAD | S_IWRITE ) == -1 ) {
            /* failed */
            fprintf( stderr, "fifo: Couldn't create %s as a fifo: %s\n",
                     filename, strerror( errno ) );
            free( fifo );
            return 0;
        }
        fifo->fd = open( filename , O_RDONLY | O_NONBLOCK );
        if( fifo->fd == -1 ) {
            free( fifo );
            return 0;
        }
    }

    memset( fifo->buf, 0, sizeof( fifo->buf ) );
    fifo->bufpos = 0;
    fifo->filename = filename;

    /* our fifo is open for reading */
    return fifo;
}

char *fifo_next_line( fifo_t *fifo )
{
    char c;

    if( !fifo ) return 0;

    while( read( fifo->fd, &c, 1 ) > 0 ) {
        if( c == '\n' ) return fifo->buf;
        if( c == '\b' ) {
            if( fifo->bufpos == 0 ) continue;
            fifo->buf[ --fifo->bufpos ] = 0;
            continue;
        }
        if( fifo->bufpos < sizeof( fifo->buf ) - 1 ) 
            fifo->buf[ fifo->bufpos++ ] = c;
    }
    return 0;
}

int fifo_next_command( fifo_t *fifo )
{
    int cmd;
    char *str = 0;

    if( !fifo ) return TVTIME_NOCOMMAND;
    str = fifo_next_line( fifo );
    if( !str ) return TVTIME_NOCOMMAND;

    cmd = string_to_command( str );

    memset( fifo->buf, 0, sizeof( fifo->buf ) );
    fifo->bufpos = 0;
    
    return cmd;
}

void fifo_delete( fifo_t *fifo )
{
    if( !fifo ) return;
    close( fifo->fd );
    free( fifo );
}

char *fifo_get_filename( fifo_t *fifo )
{
    if( !fifo ) return 0;
    return fifo->filename;
}

