/**
 * Pure weave deinterlacing plugin.
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

static void deinterlace_scanline_weave( unsigned char *output,
                                        unsigned char *t1, unsigned char *m1,
                                        unsigned char *b1,
                                        unsigned char *t0, unsigned char *m0,
                                        unsigned char *b0, int width )
{
    blit_packed422_scanline( output, m1, width );
}

static void copy_scanline( unsigned char *output, unsigned char *m2,
                           unsigned char *t1, unsigned char *m1,
                           unsigned char *b1, unsigned char *t0,
                           unsigned char *b0, int width )
{
    blit_packed422_scanline( output, m2, width );
}


static deinterlace_method_t weavemethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Weave last field",
    "Weave",
    2,
    0,
    0,
    0,
    deinterlace_scanline_weave,
    copy_scanline
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void weave_plugin_init( void )
#endif
{
    register_deinterlace_method( &weavemethod );
}

