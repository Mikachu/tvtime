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
#include "outputfilter.h"

struct outputfilter_s
{
    int width;
    int height;
    vbiscreen_t *vs;
};

outputfilter_t *outputfilter_new( int width, int height, double frameaspect, int verbose )
{
    outputfilter_t *of = (outputfilter_t *) malloc( sizeof( outputfilter_t ) );
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
                                                unsigned char *output,
                                                int width, int xpos,
                                                int scanline )
{
    if( of->vs ) {
        vbiscreen_composite_packed422_scanline( of->vs, output, width, xpos, scanline );
    }
}


