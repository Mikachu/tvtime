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
#include <unistd.h>
#include "commands.h"
#include "utils.h"

int main( int argc, char **argv )
{
    config_t *cfg = config_new();
    FILE *fifo;
    int i;
    int nc = tvtime_num_commands();

    if( !cfg ) {
        fprintf( stderr, "tvtime-command: Can't initialize tvtime configuration, exiting.\n" );
        return 1;
    }

    if( argc < 2 ) {
        fprintf( stdout, "tvtime-command: Available commands:\n" );
        for( i = 0; i < (nc+1)/2; i++ ) {
            fprintf( stdout, "    %-25s  %-25s\n", tvtime_get_command( i ),
                ((nc+1)/2+i < nc ? tvtime_get_command((nc+1)/2+i) : ""));
        }
    }

    /* check if fifo can be written (tvtime running) */
    if( !get_tvtime_fifo( config_get_uid( cfg ) ) ) {
        fprintf( stderr, "tvtime-command: fifo cannot be found. tvtime not running?\n" );
        return 1;
    }

    i = open( get_tvtime_fifo( config_get_uid( cfg ) ), O_WRONLY | O_NONBLOCK);
    if (i < 0)
    {
        fprintf( stderr, "tvtime-command: fifo %s unwritable. tvtime not running?\n",
                 get_tvtime_fifo( config_get_uid( cfg ) ) );
        config_delete( cfg );
        return 1;
    }

    fifo = fdopen( i, "w" );
    if( !fifo ) {
        fprintf( stderr, "tvtime-command: Can't open fifo file %s.\n",
                get_tvtime_fifo( config_get_uid( cfg ) ) );
        config_delete( cfg );
        return 1;
    }

    if( argc >= 2 ) {
        for( i = 1; i < argc; i++ ) {
            int cmd = tvtime_string_to_command( argv[ i ] );
            if( cmd == TVTIME_NOCOMMAND ) {
                fprintf( stderr, "%s: Invalid command '%s'\n", argv[ 0 ], argv[ i ] );
            } else if( cmd == TVTIME_DISPLAY_MESSAGE || cmd == TVTIME_SCREENSHOT || cmd == TVTIME_KEY_EVENT ) {
                const char *name = tvtime_command_to_string( cmd );
                const char *arg = argv[ ++i ];
                fprintf( stdout, "%s: Sending command %s with argument %s.\n", argv[ 0 ], name, arg );
                fprintf( fifo, "%s %s\n", name, arg );
            } else {
                const char *name = tvtime_command_to_string( cmd );
                fprintf( stdout, "%s: Sending command %s.\n", argv[ 0 ], name );
                fprintf( fifo, "%s\n", name );
            }
        }
    }

    fclose( fifo );
    config_delete( cfg );
    return 0;
}

