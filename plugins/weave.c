
/**
 * Pure weave deinterlacing plugin.
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

static deinterlace_method_t weavemethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "Weave last field",
    "Weave",
    2,
    0,
    0,
    0,
    deinterlace_scanline_weave
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void weave_plugin_init( void )
#endif
{
    fprintf( stderr, "linear: Registering weave deinterlacing algorithm.\n" );
    register_deinterlace_method( &weavemethod );
}

