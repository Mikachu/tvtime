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

#include <stdlib.h>
#include "scope.h"

struct scope_s
{
    int width;
    int height;
    int id;
    uint8_t *scanline;
};

scope_t *scope_new( int width, int height )
{
    scope_t *scope = (scope_t *) malloc( sizeof( scope_t ) );
    if( !scope ) {
        return 0;
    }

    scope->width = width;
    scope->width = height;
    scope->id = 0;
    scope->scanline = 0;

    return scope;
}

void scope_delete( scope_t *scope )
{
    free( scope );
}

void scope_set_scanline( scope_t *scope, uint8_t *scanline, int id )
{
    scope->id = id;
    scope->scanline = scanline;
}

int scope_active_on_scanline( scope_t *scope, int scanline )
{
    if( scope->scanline && scanline >= (scope->height - 256) ) {
        return 1;
    }

    return 0;
}

void scope_composite_packed422_scanline( scope_t *scope,
                                         uint8_t *output,
                                         int width, int xpos, int scanline )
{
    if( scope->scanline && scanline >= (scope->height - 256) ) {
        uint8_t *cur = scope->scanline + (xpos*2);
        int val = scanline - (scope->height - 256);

        while( width-- ) {
            if( *cur == val ) {
                output[ 0 ] = 235;
            } else {
                output[ 0 ] = 16;
            }
            output[ 1 ] = 235;
            output += 2;
        }
    }
}


