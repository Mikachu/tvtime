
#include <stdio.h>
#include <stdlib.h>
#include "pnginput.h"
#include "videotools.h"
#include "speedy.h"
#include "credits.h"

struct credits_s
{
    unsigned char *data;
    int *blanks;
    int width;
    int height;
    int stride;

    int output_height;
    double vpos;

    double curspeed;
};

credits_t *credits_new( const char *filename, int output_height )
{
    credits_t *credits = (credits_t *) malloc( sizeof( credits_t ) );
    pnginput_t *pngin;
    int i;

    if( !credits ) {
        return 0;
    }

    pngin = pnginput_new( filename );
    if( !pngin ) {
        fprintf( stderr, "credits: Can't open credit roll '%s'.\n", filename );
        free( credits );
        return 0;
    }

    if( !pnginput_has_alpha( pngin ) ) {
        fprintf( stderr, "credits: Credit roll '%s' has no alpha channel.\n", filename );
        pnginput_delete( pngin );
        free( credits );
        return 0;
    }

    credits->output_height = output_height;
    credits->vpos = (double) -output_height;
    credits->width = pnginput_get_width( pngin );
    credits->height = pnginput_get_height( pngin );
    credits->stride = credits->width * 4;
    credits->curspeed = 0;
    credits->data = (unsigned char *) malloc( credits->stride * credits->height );
    credits->blanks = (int *) malloc( credits->height * sizeof( int ) );
    if( !credits->data || !credits->blanks ) {
        pnginput_delete( pngin );
        free( credits );
        return 0;
    }

    for( i = 0; i < credits->height; i++ ) {
        unsigned char *cur = credits->data + (i * credits->stride);
        unsigned char *scanline = pnginput_get_scanline( pngin, i );
        int j;

        rgba32_to_packed4444_rec601_scanline( cur, scanline, credits->width );
        premultiply_packed4444_scanline( cur, cur, credits->width );

        credits->blanks[ i ] = 1;
        for( j = 0; j < credits->width; j++ ) {
            if( cur[ j * 4 ] ) {
                credits->blanks[ i ] = 0;
                break;
            }
        }
    }

    pnginput_delete( pngin );
    return credits;
}

void credits_delete( void )
{
}

void credits_restart( credits_t *credits, double speed )
{
    credits->vpos = (double) -credits->output_height;
    credits->curspeed = speed;
}

void credits_advance_frame( credits_t *credits )
{
    if( credits->vpos < (double) credits->height ) {
        credits->vpos += credits->curspeed;
    }
}

void credits_composite_packed422_scanline( credits_t *credits,
                                           unsigned char *output,
                                           int width, int xpos,
                                           int scanline )
{
    int cur = ( (int) credits->vpos ) + scanline;

    if( cur >= 0 && cur < credits->height && !credits->blanks[ cur ] ) {
        unsigned char *input = credits->data + (cur * credits->stride);

        if( xpos < 0 ) {
            width += xpos;
            xpos = 0;
        }

        if( xpos < credits->width ) {
            if( (xpos + width) > credits->width ) {
                width = credits->width - xpos;
            }

            composite_packed4444_to_packed422_scanline( output, output,
                                                        input + (xpos * 4),
                                                        width );
        }
    }
}



