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
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "utils.h"
#include "input.h"

/* Number of frames to wait for next channel digit. */
#define CHANNEL_DELAY 100

struct input_s {
    config_t *cfg;
    commands_t *com;
    int console_on;
    console_t *console;
    int slave_mode;
    int quit;
};

input_t *input_new( config_t *cfg, commands_t *com, console_t *con, int verbose )
{
    input_t *in = malloc( sizeof( input_t ) );

    if( !in ) {
        fprintf( stderr, "input: Could not create new input object.\n" );
        return 0;
    }

    in->cfg = cfg;
    in->com = com;
    in->console_on = 0;
    in->console = con;
    in->quit = 0;
    in->slave_mode = config_get_slave_mode( cfg );

    return in;
}

void input_delete( input_t *in )
{
    free( in );
}

void input_callback( input_t *in, int command, int arg )
{
    char temp[ 128 ];
    char argument[ 2 ];
    const char *curarg = argument;
    int tvtime_cmd = 0;

    /* Once we've been told to quit, ignore all commands. */
    if( in->quit ) return;

    switch( command ) {
    case I_QUIT:
        in->quit = 1;
        commands_handle( in->com, TVTIME_QUIT, 0 );
        return;
        break;

    case I_MOUSEMOVE:
        sprintf( temp, "%d %d", arg >> 16, arg & 0xffff );
        commands_handle( in->com, TVTIME_MOUSE_MOVE, temp );
        return;

    case I_BUTTONRELEASE:
    case I_REMOTE:
    case I_KEYDOWN:
        if( command == I_REMOTE ) {
            tvtime_cmd = arg;
        } else if( command == I_BUTTONRELEASE ) {
            if( commands_in_menu( in->com ) ) {
                tvtime_cmd = config_button_to_menu_command( in->cfg, arg );
                if( config_button_to_menu_command_argument( in->cfg, arg ) ) {
                    curarg = config_button_to_menu_command_argument( in->cfg, arg );
                }
            } else {
                tvtime_cmd = config_button_to_command( in->cfg, arg );
                if( config_button_to_command_argument( in->cfg, arg ) ) {
                    curarg = config_button_to_command_argument( in->cfg, arg );
                }
            }
        } else {
            if( commands_in_menu( in->com ) ) {
                tvtime_cmd = config_key_to_menu_command( in->cfg, arg );
                if( config_key_to_menu_command_argument( in->cfg, arg ) ) {
                    curarg = config_key_to_menu_command_argument( in->cfg, arg );
                }
            } else {
                tvtime_cmd = config_key_to_command( in->cfg, arg );
                if( config_key_to_command_argument( in->cfg, arg ) ) {
                    curarg = config_key_to_command_argument( in->cfg, arg );
                }
            }
        }

        if( command == I_KEYDOWN ) {
            if( commands_console_on( in->com ) && in->console ) {
                char blah[2];
                blah[1] = '\0';

                if( tvtime_cmd == TVTIME_TOGGLE_CONSOLE ||
                    tvtime_cmd == TVTIME_SCROLL_CONSOLE_UP ||
                    tvtime_cmd == TVTIME_SCROLL_CONSOLE_DOWN )
                    break;

                blah[0] = arg & 0xFF;
                switch( blah[0] ) {
                case I_ENTER:
                    blah[0] = '\n';
                    break;

                default:
                    if( (arg & I_SHIFT) && isalpha(blah[0]) ) {
                        blah[0] ^= 0x20;
                    }
                    break;
                }
                console_pipe_printf( in->console, blah );
                console_printf( in->console, blah );
                return;
            } else if( in->slave_mode ) {
                const char *special = input_special_key_to_string( arg );
                if( special ) {
                    fprintf( stdout, "%s\n", special );
                    fflush( stdout );
                } else {
                    fprintf( stdout, "%c\n", arg & 0xff );
                    fflush( stdout );
                }
                return;
            }
        }
        break;

    default:
        return;
        break;
    }

    argument[ 1 ] = '\0';
    argument[ 0 ] = arg;
    commands_handle( in->com, tvtime_cmd, curarg );
}

