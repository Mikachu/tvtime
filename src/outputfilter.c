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
#include "outputfilter.h"

struct outputfilter_s
{
    int width;
    int height;
    vbiscreen_t *vs;
};

outputfilter_t *outputfilter_new( int width, int height, double frameaspect, int verbose )
{
    outputfilter_t *of = malloc( sizeof( outputfilter_t ) );
    if( !of ) {
        return 0;
    }

    of->width = width;
    of->height = height;
    of->vs = vbiscreen_new( of->width, of->height, frameaspect, verbose );

    return of;
}

void outputfilter_delete( outputfilter_t *of )
{
    free( of );
}

vbiscreen_t *outputfilter_get_vbiscreen( outputfilter_t *of )
{
    return of->vs;
}

int outputfilter_active_on_scanline( outputfilter_t *of, int scanline )
{
    int active = 0;

    if( of->vs ) {
        active += vbiscreen_active_on_scanline( of->vs, scanline );
    }

    return active;
}

void outputfilter_composite_packed422_scanline( outputfilter_t *of,
                                                uint8_t *output,
                                                int width, int xpos,
                                                int scanline )
{
    if( of->vs ) {
        vbiscreen_composite_packed422_scanline( of->vs, output, width, xpos, scanline );
    }
}


