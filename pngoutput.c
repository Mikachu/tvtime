/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <pngoutput.h>

struct pngoutput_s
{
    FILE *f;
    png_structp png_ptr;
    png_infop info_ptr;
    int width, height;
    double gamma;
};

void pngoutput_header_16bit( pngoutput_t *pngoutput )
{
    png_set_gAMA( pngoutput->png_ptr, pngoutput->info_ptr, 1.0 );
    png_set_IHDR( pngoutput->png_ptr, pngoutput->info_ptr,
                  pngoutput->width, pngoutput->height, 16,
                  PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
    png_write_info( pngoutput->png_ptr, pngoutput->info_ptr );
}

void pngoutput_header_8bit( pngoutput_t *pngoutput )
{
    png_set_gAMA( pngoutput->png_ptr, pngoutput->info_ptr, pngoutput->gamma );
    png_set_sRGB( pngoutput->png_ptr, pngoutput->info_ptr,
                  PNG_sRGB_INTENT_RELATIVE );
    png_set_IHDR( pngoutput->png_ptr, pngoutput->info_ptr,
                  pngoutput->width, pngoutput->height, 8,
                  // PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                  PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );
    png_write_info( pngoutput->png_ptr, pngoutput->info_ptr );
}

pngoutput_t *pngoutput_new( const char *filename,
                            int width, int height,
                            double gamma )
{
    pngoutput_t *pngoutput = (pngoutput_t *) malloc( sizeof( pngoutput_t ) );

    if( !pngoutput ) return 0;

    pngoutput->f = 0;
    pngoutput->png_ptr = 0;
    pngoutput->info_ptr = 0;
    pngoutput->width = width;
    pngoutput->height = height;
    pngoutput->gamma = gamma;

    pngoutput->f = fopen( filename, "w" );
    if( !pngoutput->f ) {
        fprintf( stderr, "pngoutput: Can't open output file %s\n", filename );
        pngoutput_delete( pngoutput );
        return 0;
    }

    pngoutput->png_ptr = png_create_write_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
    if( !pngoutput->png_ptr ) {
        fprintf( stderr, "pngoutput: Can't open PNG write struct.\n" );
        pngoutput_delete( pngoutput );
        return 0;
    }

    pngoutput->info_ptr = png_create_info_struct( pngoutput->png_ptr );
    if( !pngoutput->info_ptr ) {
        png_destroy_write_struct( &(pngoutput->png_ptr), 0 );
        pngoutput->png_ptr = 0;
        fprintf( stderr, "pngoutput: Can't open PNG info struct.\n" );
        pngoutput_delete( pngoutput );
        return 0;
    }

    png_init_io( pngoutput->png_ptr, pngoutput->f );
    png_set_filter( pngoutput->png_ptr, 0, PNG_FILTER_PAETH );
    png_set_compression_level( pngoutput->png_ptr, Z_BEST_COMPRESSION );
    pngoutput_header_8bit( pngoutput );

    return pngoutput;
}

void pngoutput_delete( pngoutput_t *pngoutput )
{
    if( pngoutput->png_ptr && pngoutput->info_ptr ) {
        png_write_end( pngoutput->png_ptr, pngoutput->info_ptr );
        png_destroy_write_struct( &pngoutput->png_ptr, &pngoutput->info_ptr );
    }
    if( pngoutput->f ) {
        fclose( pngoutput->f );
    }
    free( pngoutput );
}

void pngoutput_scanline( pngoutput_t *pngoutput, unsigned char *scanline )
{
    png_write_row( pngoutput->png_ptr, scanline );
}

