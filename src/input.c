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
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"

#ifdef HAVE_LIRC
#include <fcntl.h>
#include <lirc/lirc_client.h>
#endif

#include "tvtimeconf.h"
#include "input.h"
#include "commands.h"
#include "menu.h"
#include "console.h"

/* Number of frames to wait for next channel digit. */
#define CHANNEL_DELAY 100


/* Key names. */
typedef struct key_name_s key_name_t;

struct key_name_s {
    char *name;
    int key;
};

static key_name_t key_names[] = {
    { "Up", I_UP },
    { "Down", I_DOWN },
    { "Left", I_LEFT },
    { "Right", I_RIGHT },
    { "Insert", I_INSERT },
    { "Home", I_HOME },
    { "End", I_END },
    { "pgup", I_PGUP },
    { "pgdn", I_PGDN },
    { "pg up", I_PGUP },
    { "pg dn", I_PGDN },
    { "PageUp", I_PGUP },
    { "PageDown", I_PGDN },
    { "Page Up", I_PGUP },
    { "Page Down", I_PGDN },
    { "F1", I_F1 },
    { "F2", I_F2 },
    { "F3", I_F3 },
    { "F4", I_F4 },
    { "F5", I_F5 },
    { "F6", I_F6 },
    { "F7", I_F7 },
    { "F8", I_F8 },
    { "F9", I_F9 },
    { "F10", I_F10 },
    { "F11", I_F11 },
    { "F12", I_F12 },
    { "F13", I_F13 },
    { "F14", I_F14 },
    { "F15", I_F15 },
    { "Backspace", I_BACKSPACE },
    { "Escape", I_ESCAPE },
    { "Enter", I_ENTER },
    { "Print", I_PRINT },
    { "Menu", I_MENU },
    { 0, 0 }
};


int input_string_to_special_key( const char *str )
{
    int count;

    for( count = 0; key_names[ count ].name; count++ ) {
        if( !strcasecmp( str, key_names[ count ].name ) ) {
            return key_names[ count ].key;
        }
    }

    return 0;
}

const char *input_special_key_to_string( int key )
{
    int count;

    for( count = 0; key_names[ count ].name; count++ ) {
        if( key == key_names[ count ].key ) {
            return key_names[ count ].name;
        }
    }

    return 0;
}

struct input_s {
    config_t *cfg;
    commands_t *com;
    
    int menu_on;
    menu_t *menu;

    int console_on;
    console_t *console;

    int slave_mode;

    int quit;

    int lirc_used;
#ifdef HAVE_LIRC
    int lirc_fd;
    struct lirc_config *lirc_conf;
#endif
};

input_t *input_new( config_t *cfg, commands_t *com, console_t *con, menu_t *menu )
{
    input_t *in = (input_t *) malloc( sizeof( input_t ) );

    if( !in ) {
        fprintf( stderr, "input: Could not create new input object.\n" );
        return NULL;
    }

    in->cfg = cfg;
    in->menu = menu;
    in->com = com;
    in->menu_on = 0;
    in->console_on = 0;
    in->menu = menu;
    in->console = con;
    in->quit = 0;
    in->slave_mode = config_get_slave_mode( cfg );

    in->lirc_used = 0;

#ifdef HAVE_LIRC
    in->lirc_fd = lirc_init( "tvtime", 1 ); /* Initialize lirc and let it be verbose. */

    if( in->lirc_fd < 0 )
        fprintf( stderr, "tvtime: Can't connect to lircd. Lirc disabled.\n" );
    else {
        fcntl( in->lirc_fd, F_SETFL, O_NONBLOCK );
        if ( lirc_readconfig( NULL, &in->lirc_conf, NULL ) == 0 ) {
            in->lirc_used = 1;
        } else {
            fprintf( stderr, "tvtime: Can't read lirc config file. "
                     "Lirc disabled.\n" );
        }
    }
#endif

    return in;
}

void input_callback( input_t *in, InputEvent command, int arg )
{
    int tvtime_cmd, verbose;

    if( !in ) return;
    if( in->quit ) return;

    verbose = config_get_verbose( in->cfg );

    if( commands_menu_on( in->com ) && in->menu ) {
        if( menu_callback( in->menu, command, arg ) ) {
            return;
        }
    }

    switch( command ) {
    case I_QUIT:
        in->quit = 1;
        commands_handle( in->com, TVTIME_QUIT, arg );
        return;
        break;

    case I_BUTTONPRESS:
    case I_REMOTE:
    case I_KEYDOWN:
        if( command == I_REMOTE ) {
            tvtime_cmd = arg;
        } else if( command == I_BUTTONPRESS ) {
            tvtime_cmd = config_button_to_command( in->cfg, arg );
        } else {
            tvtime_cmd = config_key_to_command( in->cfg, arg );
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

    commands_handle( in->com, tvtime_cmd, arg );
}

#ifdef HAVE_LIRC
static void poll_lirc( input_t *in, struct lirc_config *lirc_conf )
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

    lirc_code2char( lirc_conf, code, &string );

    if( !string ) {
        /* No tvtime action for this code. */
        free( code );
        return;
    }

    cmd = tvtime_string_to_command( string );

    if( cmd != -1 ) {
        input_callback( in, I_REMOTE, cmd );
    } else {
        fprintf( stderr, "tvtime: Unknown lirc command: %s\n", string );
    }

    free( code );
}
#endif

void input_next_frame( input_t *in )
{
    if( in->lirc_used ) {
#ifdef HAVE_LIRC
        poll_lirc( in, in->lirc_conf );
#endif
    }
}

void input_delete( input_t *in )
{
    free( in );
}

