
#include "osd.h"
#include "tvtimeosd.h"

struct tvtime_osd_s
{
    osd_string_t *channel_number;
    osd_string_t *volume_bar;
    osd_string_t *muted_osd;
};

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect )
{
    tvtime_osd_t *osd = (tvtime_osd_t *) malloc( sizeof( tvtime_osd_t ) );
    if( !osd ) {
        return 0;
    }

    return osd;
}

void tvtime_osd_delete( tvtime_osd_t *osd )
{
    free( osd );
}

