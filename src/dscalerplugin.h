
#ifndef DSCALERPLUGIN_H_INCLUDED
#define DSCALERPLUGIN_H_INCLUDED

#include "DS_Deinterlace.h"

FILTER_METHOD *load_dscaler_filter( char *filename );
void unload_dscaler_filter( FILTER_METHOD *method );

DEINTERLACE_METHOD *load_dscaler_deinterlacer( char *filename );
void unload_dscaler_deinterlacer( DEINTERLACE_METHOD *method );

#endif /* DSCALERPLUGIN_H_INCLUDED */
