/**
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

#include <stdlib.h>
#include <png.h>
#include "pnginput.h"

struct pnginput_s
{
    FILE *f;
    png_structp png_ptr;
    png_infop info_ptr;
    png_infop end_info;
    int has_alpha;
};

pnginput_t *pnginput_new( const char *filename )
{
    pnginput_t *pnginput = (pnginput_t *) malloc( sizeof( pnginput_t ) );
    png_uint_32 width, height;
    int bit_depth, colour_type, channels, rowbytes;
    double gamma;

    if( !pnginput ) return 0;

    pnginput->png_ptr = 0;
    pnginput->info_ptr = 0;
    pnginput->end_info = 0;

    pnginput->f = fopen( filename, "rb" );
    if( !pnginput->f ) {
        fprintf( stderr, "pnginput: Can't open input file %s\n", filename );
        pnginput_delete( pnginput );
        return 0;
    }

    pnginput->png_ptr = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    if( !pnginput->png_ptr ) {
        fprintf( stderr, "pnginput: Can't open PNG write struct.\n" );
        pnginput_delete( pnginput );
        return 0;
    }

    pnginput->info_ptr = png_create_info_struct( pnginput->png_ptr );
    if( !pnginput->info_ptr ) {
        png_destroy_read_struct( &(pnginput->png_ptr), 0, 0 );
        pnginput->png_ptr = 0;
        fprintf( stderr, "pnginput: Can't open PNG info struct.\n" );
        pnginput_delete( pnginput );
        return 0;
    }

    pnginput->end_info = png_create_info_struct( pnginput->png_ptr );
    if( !pnginput->end_info ) {
        png_destroy_read_struct( &(pnginput->png_ptr), &(pnginput->info_ptr), 0 );
        pnginput->png_ptr = 0;
        pnginput->info_ptr = 0;
        fprintf( stderr, "pnginput: Can't create PNG info struct.\n" );
        pnginput_delete( pnginput );
        return 0;
    }

    png_init_io( pnginput->png_ptr, pnginput->f );

    /* So paletted pngs work... Need to detect about alpha still though.. */
    png_set_expand( pnginput->png_ptr );

    png_read_png( pnginput->png_ptr, pnginput->info_ptr,
                  PNG_TRANSFORM_IDENTITY, 0 );

    width = png_get_image_width( pnginput->png_ptr, pnginput->info_ptr );
    height = png_get_image_height( pnginput->png_ptr, pnginput->info_ptr );
    bit_depth = png_get_bit_depth( pnginput->png_ptr, pnginput->info_ptr );
    colour_type = png_get_color_type( pnginput->png_ptr, pnginput->info_ptr );
    channels = png_get_channels( pnginput->png_ptr, pnginput->info_ptr );
    rowbytes = png_get_rowbytes( pnginput->png_ptr, pnginput->info_ptr );

    png_get_gAMA( pnginput->png_ptr, pnginput->info_ptr, &gamma );

    if( colour_type == PNG_COLOR_TYPE_GRAY_ALPHA ||
        colour_type == PNG_COLOR_TYPE_RGB_ALPHA ) {
        pnginput->has_alpha = 1;
    } else {
        pnginput->has_alpha = 0;
    }

/*
    fprintf( stderr, "width %u, height %u, depth %d, "
                     "colour %d, rowbytes %d, gamma %f\n",
             width, height, bit_depth, colour_type, rowbytes, gamma );
*/

    return pnginput;
}

void pnginput_delete( pnginput_t *pnginput )
{
    if( pnginput->png_ptr && pnginput->info_ptr ) {
        png_destroy_read_struct( &(pnginput->png_ptr), &(pnginput->info_ptr), 0 );
    }
    if( pnginput->f ) {
        fclose( pnginput->f );
    }
    free( pnginput );
}

unsigned char *pnginput_get_scanline( pnginput_t *pnginput, int num )
{
    return png_get_rows( pnginput->png_ptr, pnginput->info_ptr )[ num ];
}

unsigned int pnginput_get_width( pnginput_t *pnginput )
{
    return png_get_image_width( pnginput->png_ptr, pnginput->info_ptr );
}

unsigned int pnginput_get_height( pnginput_t *pnginput )
{
    return png_get_image_height( pnginput->png_ptr, pnginput->info_ptr );
}

int pnginput_has_alpha( pnginput_t *pnginput )
{
    return pnginput->has_alpha;
}
