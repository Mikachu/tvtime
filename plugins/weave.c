/**
 * Pure weave deinterlacing plugin.
 *
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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
#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_weave( uint8_t *output,
                                        deinterlace_scanline_data_t *data,
                                        int width )
{
    blit_packed422_scanline( output, data->m1, width );
}

static void copy_scanline( uint8_t *output,
                           deinterlace_scanline_data_t *data,
                           int width )
{
    blit_packed422_scanline( output, data->m0, width );
}


static deinterlace_method_t weavemethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Weave Last Field",
    "Weave",
    2,
    0,
    0,
    0,
    0,
    1,
    deinterlace_scanline_weave,
    copy_scanline,
    0
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void weave_plugin_init( void )
#endif
{
    register_deinterlace_method( &weavemethod );
}

