
#include <stdlib.h>
#include "osd.h"
#include "tvtimeosd.h"

struct tvtime_osd_s
{
    osd_string_t *channel_number;
    osd_string_t *volume_bar;
    osd_string_t *muted_osd;
    int muted;
};

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect )
{
    tvtime_osd_t *osd = (tvtime_osd_t *) malloc( sizeof( tvtime_osd_t ) );
    if( !osd ) {
        return 0;
    }

    osd->channel_number = osd_string_new( "helr.ttf", 80, width,
                                          height, frameaspect );
    osd_string_set_colour( osd->channel_number, 220, 12, 155 );
    osd_string_show_border( osd->channel_number, 1 );

    osd->volume_bar = osd_string_new( "helr.ttf", 15, width,
                                      height, frameaspect );
    osd_string_set_colour( osd->volume_bar, 200, 128, 128 );

    osd->muted_osd = osd_string_new( "helr.ttf", 15, width,
                                     height, frameaspect );
    osd_string_set_colour( osd->muted_osd, 200, 128, 128 );
    osd_string_show_text( osd->muted_osd, "Mute", 100 );
    osd->muted = 0;

    return osd;
}

void tvtime_osd_delete( tvtime_osd_t *osd )
{
    free( osd );
}

void tvtime_osd_show_channel_number( tvtime_osd_t *osd, const char *text )
{
    osd_string_show_text( osd->channel_number, text, 80 );
}

void tvtime_osd_show_bar( tvtime_osd_t *osd, const char *barname,
                          int percentage )
{
    char bar[ 108 ];
    memset( bar, 0, 108 );
    strcpy( bar, barname );
    memset( bar + 7, '|', percentage );
    osd_string_show_text( osd->volume_bar, bar, 80 );
}

void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate )
{
    osd->muted = mutestate;
}

void tvtime_osd_composite_packed422( tvtime_osd_t *osd, unsigned char *output,
                                     int width, int height, int stride )
{
}

void tvtime_osd_advance_frame( tvtime_osd_t *osd )
{
    osd_string_advance_frame( osd->channel_number );
    osd_string_advance_frame( osd->volume_bar );
}

