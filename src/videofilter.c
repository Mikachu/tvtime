
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "videocorrection.h"
#include "speedy.h"
#include "videofilter.h"

struct videofilter_s
{
    video_correction_t *vc;
    int bt8x8_correction;

    unsigned char tempscanline[ 2048 ];
};

videofilter_t *videofilter_new( void )
{
    videofilter_t *vf = (videofilter_t *) malloc( sizeof( videofilter_t ) );
    if( !vf ) return 0;

    vf->vc = video_correction_new( 1, 0 );
    vf->bt8x8_correction = 0;

    return vf;
}

void videofilter_delete( videofilter_t *vf )
{
    if( vf->vc ) video_correction_delete( vf->vc );
    free( vf );
}

void videofilter_set_bt8x8_correction( videofilter_t *vf, int correct )
{
    vf->bt8x8_correction = correct;
}

void videofilter_set_full_extent_correction( videofilter_t *vf, int correct )
{
}

void videofilter_set_luma_power( videofilter_t *vf, double power )
{
    if( vf->vc ) video_correction_set_luma_power( vf->vc, power );
}

int videofilter_active_on_scanline( videofilter_t *vf, int scanline )
{
    if( vf->vc && vf->bt8x8_correction ) {
        return 1;
    } else {
        return 0;
    }
}

void videofilter_packed422_scanline( videofilter_t *vf, unsigned char *data,
                                     int width, int xpos, int scanline )
{
    if( vf->vc && vf->bt8x8_correction ) {
        // video_correction_correct_packed422_scanline( vf->vc, data, data, width );
        // filter_luma_121_packed422_inplace_scanline( data, width );
        // filter_luma_14641_packed422_inplace_scanline( data, width );
         mirror_packed422_inplace_scanline( data, width );
        // kill_chroma_packed422_inplace_scanline( data, width );
        // testing_packed422_inplace_scanline( data, width, scanline );
    }
}

