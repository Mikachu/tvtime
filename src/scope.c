/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This file is subject to the terms of the GNU General Public License as
 * published by the Free Software Foundation.  A copy of this license is
 * included with this software distribution in the file COPYING.  If you
 * do not have a copy, you may obtain a copy by writing to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#include <stdlib.h>
#include "scope.h"

struct scope_s
{
    int width;
    int height;
    int id;
    unsigned char *scanline;
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

void scope_set_scanline( scope_t *scope, unsigned char *scanline, int id )
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
                                         unsigned char *output,
                                         int width, int xpos, int scanline )
{
    if( scope->scanline && scanline >= (scope->height - 256) ) {
        unsigned char *cur = scope->scanline + (xpos*2);
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


