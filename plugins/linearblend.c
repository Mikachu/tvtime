/**
 * Linear blend deinterlacing plugin.  The algorithm for this filter is based
 * on the mythtv sources, which took it from the mplayer sources.
 *
 * The file is postprocess_template.c in mplayer, and is
 *
 *  Copyright (C) 2001-2002 Michael Niedermayer (michaelni@gmx.at)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_linear_blend( unsigned char *output,
                                        unsigned char *t1, unsigned char *m1,
                                        unsigned char *b1,
                                        unsigned char *t0, unsigned char *m0,
                                        unsigned char *b0, int width )
{
    width *= 2;
    while( width-- ) {
        *output++ = (*t1++ + *b1++ + (2 * *m1++))>>2;
    }
}

static void deinterlace_scanline_linear_blend2( unsigned char *output, unsigned char *m2,
                           unsigned char *t1, unsigned char *m1,
                           unsigned char *b1, unsigned char *t0,
                           unsigned char *b0, int width )
{
    width *= 2;
    while( width-- ) {
        *output++ = (*t1++ + *b1++ + (2 * *m2++))>>2;
    }
}


static deinterlace_method_t linearblendmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Linear Blend (idea from mplayer)",
    "LinearBlend",
    1,
    0,
    0,
    0,
    deinterlace_scanline_linear_blend,
    deinterlace_scanline_linear_blend2
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void linearblend_plugin_init( void )
#endif
{
    register_deinterlace_method( &linearblendmethod );
}

