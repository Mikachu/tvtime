/**
 * Dummy plugin for 'scalerbob' support.
 *
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

static deinterlace_method_t scalerbobmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "XVideo Bob",
    "XVideo Bob",
    1,
    0,
    1,
    0,
    0,
    1,
    0,
    0,
    0
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void scalerbob_plugin_init( void )
#endif
{
    register_deinterlace_method( &scalerbobmethod );
}

