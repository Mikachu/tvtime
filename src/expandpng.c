
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

