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
#include "osdtools.h"
#include "outputfilter.h"

struct outputfilter_s
{
    vbiscreen_t *vs;
	vtscreen_t *vts;
    tvtime_osd_t *osd;
    console_t *con;
    double pixelaspect;
};

outputfilter_t *outputfilter_new( void )
{
    outputfilter_t *of = malloc( sizeof( outputfilter_t ) );
    if( !of ) {
        return 0;
    }

    of->vs = 0;
    of->vts = 0;
    of->osd = 0;
    of->con = 0;

    return of;
}

void outputfilter_delete( outputfilter_t *of )
{
    free( of );
}

void outputfilter_set_vbiscreen( outputfilter_t *of, vbiscreen_t *vbiscreen )
{
    of->vs = vbiscreen;
}

void outputfilter_set_vtscreen( outputfilter_t *of, vtscreen_t *vts )
{
    of->vts= vts;
}

void outputfilter_set_osd( outputfilter_t *of, tvtime_osd_t *osd )
{
    of->osd = osd;
}

void outputfilter_set_console( outputfilter_t *of, console_t *console )
{
    of->con = console;
}

void outputfilter_set_pixel_aspect( outputfilter_t *of, double pixelaspect )
{
    of->pixelaspect = pixelaspect;
}

int outputfilter_active_on_scanline( outputfilter_t *of, int scanline )
{
    if( of->con || of->osd || of->vts ) { // FIXME
        return 1;
    } else if( of->vs ) {
        return vbiscreen_active_on_scanline( of->vs, scanline );
    }

    return 0;
}

void outputfilter_composite_packed422_scanline( outputfilter_t *of,
                                                uint8_t *output,
                                                int width, int xpos,
                                                int scanline )
{
    if( of->vs ) {
        vbiscreen_composite_packed422_scanline( of->vs, output, width, xpos, scanline );
    }
    if( of->vts ) {
        vtscreen_composite_packed422_scanline( of->vts, output, width, xpos, scanline );
    }
    if( of->osd ) {
        tvtime_osd_composite_packed422_scanline( of->osd, output, width, xpos, scanline );
    }
    if( of->con ) {
        console_composite_packed422_scanline( of->con, output, width, xpos, scanline );
    }
}

