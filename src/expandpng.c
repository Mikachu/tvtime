/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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
#include <stdlib.h>
#include "pnginput.h"
#include "pngoutput.h"

int main( int argc, char **argv )
{
    unsigned char *temp;
    pnginput_t *greyimage;
    pngoutput_t *outimage;
    int width, height, i;

    greyimage = pnginput_new( argv[ 1 ] );
    width = pnginput_get_width( greyimage );
    height = pnginput_get_height( greyimage );
    temp = (unsigned char *) malloc( width * 4 );

    outimage = pngoutput_new( argv[ 2 ], width, height, 1, 0.45 );

    for( i = 0; i < height; i++ ) {
        unsigned char *inscanline = pnginput_get_scanline( greyimage, i );
        int j;

        for( j = 0; j < width; j++ ) {
            temp[ (j*4) + 0 ] = 245; /* wheat */
            temp[ (j*4) + 1 ] = 222;
            temp[ (j*4) + 2 ] = 179;
            temp[ (j*4) + 3 ] = 0xff - inscanline[ j ];
        }
        pngoutput_scanline( outimage, temp );
    }

    pnginput_delete( greyimage );
    pngoutput_delete( outimage );
    return 0;
}

