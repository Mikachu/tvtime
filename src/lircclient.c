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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "lircclient.h"

struct lirc_config;
static int (*lirc_init)( char *prog, int verbose ) = 0;
static int (*lirc_deinit)( void ) = 0;
static int (*lirc_readconfig)( char *file, struct lirc_config **config,
                               int (check)( char *s ) ) = 0;
static int (*lirc_nextcode)( char **code ) = 0;
static int (*lirc_code2char)( struct lirc_config *config,
                              char *code, char **string ) = 0;

static struct lirc_config *lirc_conf;
static int lirc_fd;

int lirc_open( void )
{
    void *lirc_library;

    lirc_library = dlopen( "liblirc_client.so.0", RTLD_LAZY );
 
    if( lirc_library ) { 
        lirc_init = (int (*)(char*,int)) dlsym( lirc_library, "lirc_init" );
        lirc_deinit = (int (*)(void)) dlsym( lirc_library, "lirc_deinit" );
        lirc_readconfig = (int (*)(char*,struct lirc_config **,int (char*))) dlsym( lirc_library, "lirc_readconfig" );
        lirc_nextcode = (int (*)(char**)) dlsym( lirc_library, "lirc_nextcode" );
        lirc_code2char = (int (*)(struct lirc_config *,char *,char**)) dlsym( lirc_library, "lirc_code2char" );
    }

    if( !lirc_library ) {
        fprintf( stderr, "lircclient: Library not found.\n" );
        return 0;
    } else if( !lirc_init || !lirc_deinit || !lirc_readconfig ||
               !lirc_nextcode || !lirc_code2char ) {
        fprintf( stderr, "lircclient: Library sucks.\n" );
        return 0;
    }

    /* Initialize lirc and let it be verbose. */
    lirc_fd = lirc_init( "tvtime", 1 );
    if( lirc_fd < 0 ) {
        fprintf( stderr, "lircclient: Can't connect to lircd. Lirc disabled.\n" );
    } else {
        fcntl( lirc_fd, F_SETFL, O_NONBLOCK );
        if( lirc_readconfig( 0, &lirc_conf, 0 ) == 0 ) {
            return 1;
        } else {
            fprintf( stderr, "tvtime: Can't read lirc config file. "
                     "Lirc disabled.\n" );
        }
    }

    return 0;
}

void lirc_shutdown( void )
{
    lirc_deinit();
}

void lirc_poll( commands_t *commands )
{
    char *code;
    char *string;
    int cmd;

    if( lirc_nextcode( &code ) != 0 ) {
        /* Can not connect to lircd. */
        return;
    }

    if( !code ) {
        /* No remote control code available. */
        return;
    }

    while( !lirc_code2char( lirc_conf, code, &string ) && string ) {
        char cmdstr[ 4096 ];
        char args[ 4096 ];
        int i;

        strncpy( cmdstr, string, sizeof( cmdstr ) );
        *args = 0;

        for( i = 0;; i++ ) {
            if( !cmdstr[ i ] ) break;
            if( isspace( cmdstr[ i ] ) ) {
                cmdstr[ i ] = '\0';
                strncpy( args, cmdstr +  i + 1, sizeof( args ) );
                break;
            }
        }

        cmd = tvtime_string_to_command( cmdstr );
        if( cmd != TVTIME_NOCOMMAND ) {
            commands_handle( commands, cmd, args );
        } else {
            fprintf( stderr, "tvtime: Unknown lirc command: %s\n", string );
        }
    }
    free( code );
}

