/**
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

/**
 * I just got Poynton's new book and this is just me playing around
 * with some of his filters.
 */

#include <stdio.h>
#include "speedy.h"
#include "deinterlace.h"

const int weight[] = {
 0, 0, 0, 0, 0, 0, 0, 0, // 128
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, // 96
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, // 64
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, // 32
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, // 128
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,

 0, 0, 0, 0, 0, 0, 0, 0, // 96
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,

 4, 4, 4, 4, 4, 4, 4, 4, // 64
 4, 4, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4,

 4, 4, 4, 4, 4, 4, 4, 4, // 32
 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8,

// ---------------------

 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8,
 8, 8, 8, 8, 8, 8, 8, 8,
 4, 4, 4, 4, 4, 4, 4, 4, // 32

 4, 4, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4,
 4, 4, 4, 4, 4, 4, 4, 4, // 64

 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, // 96

 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, // 128

 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, // 32

 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, // 64

 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, // 96

 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0, // 128
};

static void deinterlace_scanline_slow( unsigned char *output,
                                       unsigned char *t1, unsigned char *m1,
                                       unsigned char *b1,
                                       unsigned char *t0, unsigned char *m0,
                                       unsigned char *b0, int width )
{
    int drop = 2;
    int i;

    t1 += (drop - 2)*2;
    b1 += (drop - 2)*2;
    m1 += (drop - 2)*2;
    output += (drop - 2)*2;

    for( i = drop; i < width - drop; i++ ) {

        /* todo: rounding */

        unsigned int curfield = t1[ 0 ] + b1[ 0 ] + t1[ 8 ] + b1[ 8 ] +
                                ((t1[ 2 ] + t1[ 6 ] + b1[ 2 ] + b1[ 6 ])<<2) +
                                ((t1[ 4 ] + b1[ 4 ])*6);
        unsigned int both = (curfield + ((m1[ 0 ] + m1[ 8 ] +
                                         ((m1[ 2 ] + m1[ 6 ])<<2) +
                                         (m1[ 4 ]*6))<<1))>>6;
        unsigned int w;
        int diff;

        curfield = (curfield)>>5;
        diff = both - curfield + 256;
        w = weight[ diff ];

        if( w ) {
            output[ 4 ] = ((8 - w)*curfield + (w*m1[ 4 ])) >> 3;
        } else {
            output[ 4 ] = curfield;
        }

        /* Always average chroma for now. */
        output[ 5 ] = t1[ 5 ]; //(t1[ 5 ] + b1[ 5 ])>>1;
        output += 2;
        t1 += 2;
        b1 += 2;
        m1 += 2;
    }
}

static void copy_scanline( unsigned char *output, unsigned char *m2,
                           unsigned char *t1, unsigned char *m1,
                           unsigned char *b1, unsigned char *t0,
                           unsigned char *b0, int width )
{
    blit_packed422_scanline( output, m2, width );
}


static deinterlace_method_t simplemo =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Simple Motion-Adaptive",
    "SimpleMo",
    1,
    0,
    0,
    0,
    0,
    deinterlace_scanline_slow,
    copy_scanline
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void simplemo_plugin_init( void )
#endif
{
    register_deinterlace_method( &simplemo );
}

