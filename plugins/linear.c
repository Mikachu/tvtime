
#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_linear( unsigned char *output, unsigned char *t1,
                                         unsigned char *m1, unsigned char *b1,
                                         unsigned char *t0, unsigned char *m0,
                                         unsigned char *b0, int width )
{
    interpolate_packed422_scanline( output, t1, b1, width );
}

static deinterlace_method_t linearmethod =
{
    "Linear interpolation",
    "Linear",
    1,
    0,
    0,
    0,
    deinterlace_scanline_linear
};

void deinterlace_plugin_init( void )
{
    fprintf( stderr, "linear: Registering linear interpolation deinterlacing algorithm.\n" );
    register_deinterlace_method( &linearmethod );
}

