
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "wine/driver.h"
#include "wine/mmreg.h"
#include "wine/ldt_keeper.h"
#include "wine/winbase.h"

#include "DS_Filter.h"
#include "DS_Deinterlace.h"
#include "DS_ApiCommon.h"

#include "dscalerplugin.h"
#include "deinterlace.h"

#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"

static DEINTERLACE_METHOD *di_tomsmocomp;

static void deinterlace_frame_di_tomsmocomp( unsigned char *output,
                                           deinterlace_frame_data_t *data,
                                           int bottom_field, int width, int height )
{
    TDeinterlaceInfo Info;
    TPicture Picture[ 8 ];
    int stride = (width*2);
    int i;

    Info.FieldHeight = height / 2;
    Info.FrameHeight = height;
    Info.FrameWidth = width;
    Info.InputPitch = stride*2;
    Info.LineLength = stride;
    Info.OverlayPitch = stride;
    Info.pMemcpy = dscaler_memcpy;

    if( bottom_field ) {
        Picture[ 0 ].pData = data->f0 + stride;
        Picture[ 0 ].Flags = PICTURE_INTERLACED_ODD;
        Picture[ 0 ].IsFirstInSeries = 0;

        Picture[ 1 ].pData = data->f0;
        Picture[ 1 ].Flags = PICTURE_INTERLACED_EVEN;
        Picture[ 1 ].IsFirstInSeries = 0;

        Picture[ 2 ].pData = data->f1 + stride;
        Picture[ 2 ].Flags = PICTURE_INTERLACED_ODD;
        Picture[ 2 ].IsFirstInSeries = 0;

        Picture[ 3 ].pData = data->f1;
        Picture[ 3 ].Flags = PICTURE_INTERLACED_EVEN;
        Picture[ 3 ].IsFirstInSeries = 0;

        Picture[ 4 ].pData = data->f2 + stride;
        Picture[ 4 ].Flags = PICTURE_INTERLACED_ODD;
        Picture[ 4 ].IsFirstInSeries = 0;

        Picture[ 5 ].pData = data->f2;
        Picture[ 5 ].Flags = PICTURE_INTERLACED_EVEN;
        Picture[ 5 ].IsFirstInSeries = 0;
    } else {
        Picture[ 0 ].pData = data->f0;
        Picture[ 0 ].Flags = PICTURE_INTERLACED_EVEN;
        Picture[ 0 ].IsFirstInSeries = 0;

        Picture[ 1 ].pData = data->f1 + stride;
        Picture[ 1 ].Flags = PICTURE_INTERLACED_ODD;
        Picture[ 1 ].IsFirstInSeries = 0;

        Picture[ 2 ].pData = data->f1;
        Picture[ 2 ].Flags = PICTURE_INTERLACED_EVEN;
        Picture[ 2 ].IsFirstInSeries = 0;

        Picture[ 3 ].pData = data->f2 + stride;
        Picture[ 3 ].Flags = PICTURE_INTERLACED_ODD;
        Picture[ 3 ].IsFirstInSeries = 0;

        Picture[ 4 ].pData = data->f2;
        Picture[ 4 ].Flags = PICTURE_INTERLACED_EVEN;
        Picture[ 4 ].IsFirstInSeries = 0;
    }

    Info.Overlay = output;
    for( i = 0; i < MAX_PICTURE_HISTORY; i++ ) {
        Info.PictureHistory[ i ] = &(Picture[ i ]);
    }

    di_tomsmocomp->pfnAlgorithm( &Info );
}

static deinterlace_method_t tomsmocompmethod =
{
    DEINTERLACE_PLUGIN_API_VERSION,
    "TomsMoComp (Dscaler DLL)",
    "TomsMoComp",
    4,
    MM_ACCEL_X86_MMXEXT,
    0,
    0,
    0,
    0,
    0,
    0,
    deinterlace_frame_di_tomsmocomp
};

#ifdef BUILD_TVTIME_PLUGINS
void deinterlace_plugin_init( void )
#else
void dscaler_tomsmocomp_plugin_init( void )
#endif
{
    di_tomsmocomp = load_dscaler_deinterlacer( "DI_TomsMoComp.dll" );
    if( di_tomsmocomp ) {
        register_deinterlace_method( &tomsmocompmethod );
    }
}

