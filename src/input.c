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
    int slave_mode;
    int quit;
};

input_t *input_new( config_t *cfg, commands_t *com, int verbose )
{
    input_t *in = malloc( sizeof( input_t ) );

    if( !in ) {
        fprintf( stderr, "input: Could not create new input object.\n" );
        return 0;
    }

    in->cfg = cfg;
    in->com = com;
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
    char argument[ 2 ];
    const char *curarg = argument;
    static int lastx = 0;
    static int lasty = 0;
    int tvtime_cmd = 0;
    int x, y, w, h;

    /* Once we've been told to quit, ignore all commands. */
    if( in->quit ) return;

    switch( command ) {
    case I_QUIT:
        in->quit = 1;
        commands_handle( in->com, TVTIME_QUIT, 0 );
        return;
        break;

    case I_MOUSEMOVE:
        lastx = arg >> 16;
        lasty = arg & 0xffff;
        commands_get_menu_bounding_box( in->com, &x, &y, &w, &h );
        if( lastx >= x && lastx < (x + w) &&
            lasty >= y && lasty < (y + h) ) {
            char temp[ 128 ];
            sprintf( temp, "%d %d", lastx, lasty );
            commands_handle( in->com, TVTIME_MOUSE_MOVE, temp );
        }
        return;

    case I_BUTTONRELEASE:
    case I_REMOTE:
    case I_KEYDOWN:
        if( command == I_REMOTE ) {
            tvtime_cmd = arg;
        } else if( command == I_BUTTONRELEASE ) {
            if( commands_in_menu( in->com ) ) {
                tvtime_cmd = config_button_to_menu_command( in->cfg, arg );

                if( tvtime_cmd == TVTIME_MENU_ENTER ) {
                    commands_get_menu_bounding_box( in->com, &x, &y, &w, &h );
                    if( lastx < x || lastx >= (x + w) ||
                        lasty < y || lasty >= (y + h) ) {
                        tvtime_cmd = TVTIME_MENU_EXIT;
                    }
                }

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
            if( in->slave_mode ) {
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

    *argument = '\0';
    commands_handle( in->com, tvtime_cmd, curarg );
}

void input_xawtv_command( input_t *in, int argc, char **argv )
{
    if( argc == 0 ) return;

    if( !strcasecmp( argv[ 0 ], "fullscreen" ) ) {
        commands_handle( in->com, TVTIME_TOGGLE_FULLSCREEN, "" );
    } else if( !strcasecmp( argv[ 0 ], "exit" ) ||
               !strcasecmp( argv[ 0 ], "bye" ) ||
               !strcasecmp( argv[ 0 ], "quit" ) ) {
        commands_handle( in->com, TVTIME_QUIT, "" );
    } else if( !strcasecmp( argv[ 0 ], "showtime" ) ) {
        commands_handle( in->com, TVTIME_DISPLAY_INFO, "" );
    } else if( !strcasecmp( argv[ 0 ], "message" ) ) {
        commands_handle( in->com, TVTIME_DISPLAY_MESSAGE, argv[ 1 ] );
    } else if( !strcasecmp( argv[ 0 ], "keypad" ) ) {
        commands_handle( in->com, TVTIME_KEY_EVENT, argv[ 1 ] );
    } else if( !strcasecmp( argv[ 0 ], "setchannel" ) && argc > 1 ) {
        if( !strcasecmp( argv[ 1 ], "next" ) ) {
            commands_handle( in->com, TVTIME_CHANNEL_INC, "" );
        } else if( !strcasecmp( argv[ 1 ], "prev" ) ) {
            commands_handle( in->com, TVTIME_CHANNEL_DEC, "" );
        } else if( !strcasecmp( argv[ 1 ], "back" ) ) {
            commands_handle( in->com, TVTIME_CHANNEL_PREV, "" );
        } else {
            commands_handle( in->com, TVTIME_SET_STATION, argv[ 1 ] );
        }
    } else if( !strcasecmp( argv[ 0 ], "setstation" ) && argc > 1 ) {
        if( !strcasecmp( argv[ 1 ], "next" ) ) {
            commands_handle( in->com, TVTIME_CHANNEL_INC, "" );
        } else if( !strcasecmp( argv[ 1 ], "prev" ) ) {
            commands_handle( in->com, TVTIME_CHANNEL_DEC, "" );
        } else {
            commands_handle( in->com, TVTIME_SET_STATION, argv[ 1 ] );
        }
    }
}

