/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "utils.h"
#include "tvtimeconf.h"

int main( int argc, char **argv )
{
    int nc = tvtime_num_commands();
    config_t *cfg = config_new();
    FILE *fifo;
    char *fifofile;
    int i;

    if( !cfg ) {
        fputs( "tvtime-command: Cannot allocate memory.\n", stderr );
        return 1;
    }

    if( argc < 2 ) {
        fprintf( stdout, "\nAvailable commands:\n" );
        for( i = 0; i < (nc + 1)/2; i++ ) {
            fprintf( stdout, "    %-25s  %-25s\n", tvtime_get_command( i ),
               ((nc + 1)/2+i < nc ? tvtime_get_command((nc + 1)/2 + i) : "") );
        }
        return 0;
    }

    fifofile = get_tvtime_fifo_filename( config_get_uid( cfg ) );
    if( !fifofile ) {
        fputs( "tvtime-command: Cannot allocate memory.\n", stderr );
        config_delete( cfg );
        return 1;
    }

    /* Check if fifo can be written (tvtime running). */
    i = open( fifofile, O_WRONLY | O_NONBLOCK );
    if( i < 0 ) {
        fprintf( stderr, "tvtime-command: Cannot open %s: %s\n",
                 fifofile, strerror( errno ) );
        fprintf( stderr, "tvtime-command: tvtime not running?\n" );
        config_delete( cfg );
        free( fifofile );
        return 1;
    }

    fifo = fdopen( i, "w" );
    if( !fifo ) {
        fprintf( stderr, "tvtime-command: Cannot open %s: %s\n",
                 fifofile, strerror( errno ) );
        close( i );
        config_delete( cfg );
        free( fifofile );
        return 1;
    }

    for( i = 1; i < argc; i++ ) {
        int cmd = tvtime_string_to_command( argv[ i ] );
        if( cmd == TVTIME_NOCOMMAND ) {
            fprintf( stderr, "%s: Invalid command '%s'\n",
                     argv[ 0 ], argv[ i ] );
        } else if( tvtime_command_takes_arguments( cmd ) && (i + 1) < argc ) {
            const char *name = tvtime_command_to_string( cmd );
            const char *arg = argv[ ++i ];
            fprintf( stdout, "%s: Sending command %s with argument %s.\n",
                     argv[ 0 ], name, arg );
            fprintf( fifo, "%s %s\n", name, arg );
        } else {
            const char *name = tvtime_command_to_string( cmd );
            fprintf( stdout, "%s: Sending command %s.\n", argv[ 0 ], name );
            fprintf( fifo, "%s\n", name );
        }
    }

    fclose( fifo );
    config_delete( cfg );
    free( fifofile );
    return 0;
}

