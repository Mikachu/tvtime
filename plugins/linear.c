
#include <stdio.h>
#include "speedy.h"
#include "deinterlace.h"

static void deinterlace_scanline_linear( unsigned char *output,
                                         unsigned char *t1, unsigned char *m1,
                                         unsigned char *b1,
                                         unsigned char *t0, unsigned char *m0,
                                         unsigned char *b0, int width )
{
    interpolate_packed422_scanline( output, t1, b1, width );
}

static void copy_scanline( unsigned char *output, unsigned char *m2,
                           unsigned char *t1, unsigned char *m1,
                           unsigned char *b1, unsigned char *t0,
                           unsigned char *b0, int width )
{
    blit_packed422_scanline( output, m2, width );
}


static deinterlace_method_t linearmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Linear interpolation",
    "Linear",
    1,
    0,
    0,
    0,
    deinterlace_scanline_linear,
    copy_scanline
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void linear_plugin_init( void )
#endif
{
    register_deinterlace_method( &linearmethod );
}

