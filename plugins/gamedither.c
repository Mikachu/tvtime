/**
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_past_linear_interp( unsigned char *output,
                                                     deinterlace_scanline_data_t *data,
                                                     int width )
{
    interpolate_packed422_scanline( output, data->t0, data->m1, width );
}

static void deinterlace_scanline_past_linear_copy( unsigned char *output,
                                                   deinterlace_scanline_data_t *data,
                                                   int width )
{
    interpolate_packed422_scanline( output, data->t1, data->m0, width );
}


static deinterlace_method_t gamedithermethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Video game dither",
    "Past linear",
    1,
    0,
    0,
    0,
    0,
    1,
    deinterlace_scanline_past_linear_interp,
    deinterlace_scanline_past_linear_copy,
    0
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void gamedither_plugin_init( void )
#endif
{
    register_deinterlace_method( &gamedithermethod );
}
