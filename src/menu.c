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
#include <string.h>
#include <stdlib.h>
#include "menu.h"

#define MENU_MAX 16

struct menu_s
{
    char *name;
    char text[ MENU_MAX ][ 128 ];
    char arguments[ MENU_MAX ][ 128 ];
    int commands[ MENU_MAX ];
    int numlines;
    int cursor;
};

menu_t *menu_new( const char *name )
{
    menu_t *menu = malloc( sizeof( menu_t ) );
    if( !menu ) {
        return 0;
    }

    menu->numlines = 0;
    menu->cursor = 0;
    menu->name = strdup( name );
    if( !menu->name ) {
        free( menu );
        return 0;
    }

    return menu;
}

void menu_delete( menu_t *menu )
{
    free( menu );
}

void menu_set_text( menu_t *menu, int line, const char *text )
{
    snprintf( menu->text[ line ], sizeof( menu->text[ 0 ] ), "%s", text );
    if( line >= menu->numlines ) menu->numlines = line + 1;
}

void menu_set_command( menu_t *menu, int line, int command,
                       const char *argument )
{
    menu->commands[ line ] = command;
    snprintf( menu->arguments[ line ], sizeof( menu->arguments[ 0 ] ),
              "%s", argument );
}

void menu_set_cursor( menu_t *menu, int cursor )
{
    menu->cursor = cursor;
}

const char *menu_get_name( menu_t *menu )
{
    return menu->name;
}

int menu_get_num_lines( menu_t *menu )
{
    return menu->numlines;
}

const char *menu_get_text( menu_t *menu, int line )
{
    return menu->text[ line ];
}

int menu_get_command( menu_t *menu, int line )
{
    return menu->commands[ line ];
}

const char *menu_get_argument( menu_t *menu, int line )
{
    return menu->arguments[ line ];
}

int menu_get_cursor( menu_t *menu )
{
    return menu->cursor;
}

