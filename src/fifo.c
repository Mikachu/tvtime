/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "commands.h"
#include "fifo.h"

struct fifo_s {
    int fd;
    char buf[ 256 ];
    int bufpos;
    char *filename;
    char arg[ 256 ];
};

fifo_t *fifo_new( const char *filename )
{
    fifo_t *fifo = (fifo_t *) malloc( sizeof( struct fifo_s ) );

    if( !fifo ) {
        return 0;
    }

    memset( fifo->buf, 0, sizeof( fifo->buf ) );
    fifo->bufpos = 0;

    fifo->fd = open( filename, O_RDONLY | O_NONBLOCK );
    if( fifo->fd == -1 ) {

        /* attempt to create it */
        if( mkfifo( filename, S_IREAD | S_IWRITE ) == -1 ) {
            /* failed */
            fprintf( stderr, "fifo: Couldn't create %s as a fifo: %s\n",
                     filename, strerror( errno ) );
            free( fifo );
            return 0;
        }
        fifo->fd = open( filename, O_RDONLY | O_NONBLOCK );
        if( fifo->fd == -1 ) {
            free( fifo );
            return 0;
        }
    }

    fifo->filename = strdup( filename );

    /* our fifo is open for reading */
    return fifo;
}

static char *fifo_next_line( fifo_t *fifo )
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

int fifo_get_next_command( fifo_t *fifo )
{
    char *str = fifo_next_line( fifo );
    fifo->arg[0] = '\0';
    if( str ) {
        int cmd;
        int i;

        for( i = 0;; i++ ) {
            if( !str[ i ] ) break;
            if( isspace( str[ i ] ) ) {
                str[ i ] = '\0';
                memcpy(fifo->arg, str + i + 1, 255-i);
            }
        }

        cmd = tvtime_string_to_command( str );

        memset( fifo->buf, 0, sizeof( fifo->buf ) );
        fifo->bufpos = 0;
    
        return cmd;
    }

    return TVTIME_NOCOMMAND;
}

const char *fifo_get_arguments( fifo_t *fifo )
{
    return fifo->arg;
}

void fifo_delete( fifo_t *fifo )
{
    free( fifo->filename );
    close( fifo->fd );
    free( fifo );
}

const char *fifo_get_filename( fifo_t *fifo )
{
    if( !fifo ) return 0;
    return fifo->filename;
}

