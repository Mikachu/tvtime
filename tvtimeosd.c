/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include "osd.h"
#include "tvtimeosd.h"

struct tvtime_osd_s
{
    osd_string_t *channel_number;
    osd_string_t *volume_bar;
    osd_string_t *data_bar;
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

    osd->data_bar = osd_string_new( "helr.ttf", 15, width,
                                    height, frameaspect );
    osd_string_set_colour( osd->data_bar, 200, 128, 128 );

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

void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage )
{
    char bar[ 108 ];
    memset( bar, 0, 108 );
    strcpy( bar, barname );
    memset( bar + 7, '|', percentage );
    osd_string_show_text( osd->data_bar, bar, 80 );
    osd_string_set_timeout( osd->volume_bar, 0 );
}

void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage )
{
    char bar[ 108 ];
    memset( bar, 0, 108 );
    strcpy( bar, "Volume " );
    memset( bar + 7, '|', percentage );
    osd_string_show_text( osd->volume_bar, bar, 80 );
    osd_string_set_timeout( osd->data_bar, 0 );
}

void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate )
{
    osd->muted = mutestate;
}

void tvtime_osd_composite_packed422( tvtime_osd_t *osd, unsigned char *output,
                                     int width, int height, int stride )
{
    osd_string_composite_packed422( osd->channel_number, output, width,
                                    height, stride, 50, 50, 0 );

    if( osd_string_visible( osd->data_bar ) ) {
        osd_string_composite_packed422( osd->data_bar, output, width, height,
                                        stride, 20, height - 40, 0 );
    } else if( osd->muted ) {
        osd_string_composite_packed422( osd->muted_osd, output, width, height,
                                        stride, 20, height - 40, 0 );
    } else if( osd_string_visible( osd->volume_bar ) ) {
        osd_string_composite_packed422( osd->volume_bar, output, width, height,
                                        stride, 20, height - 40, 0 );
    }
}

void tvtime_osd_advance_frame( tvtime_osd_t *osd )
{
    osd_string_advance_frame( osd->channel_number );
    osd_string_advance_frame( osd->volume_bar );
    osd_string_advance_frame( osd->data_bar );
}

