/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "commands.h"
#include "tvtimeconf.h"

/**
 * These are for the ogle code.
 */
char *program_name = "tvtimecommand";
int dlevel = 3;

int main( int argc, char **argv )
{
    config_t *cfg = config_new( 0, 0 );
    FILE *fifo;
    int i;

    if( !cfg ) {
        fprintf( stderr, "tvtime-command: Can't initialize tvtime configuration, exiting.\n" );
        return 1;
    }

    fifo = fopen( config_get_command_pipe( cfg ), "w" );
    if( !fifo ) {
        fprintf( stderr, "tvtime-command: Can't open fifo file %s.\n", config_get_command_pipe( cfg ) );
        config_delete( cfg );
        return 1;
    }

    if( argc < 2 ) {
        fprintf( stdout, "tvtime-command: Available commands:\n" );
        for( i = 0; i < tvtime_num_commands(); i++ ) {
            fprintf( stdout, "\t- %s\n", tvtime_get_command( i ) );
        }
    } else {
        for( i = 1; i < argc; i++ ) {
            int cmd = tvtime_string_to_command( argv[ i ] );
            if( cmd == TVTIME_NOCOMMAND ) {
                fprintf( stderr, "%s: Invalid command '%s'\n", argv[ 0 ], argv[ i ] );
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
