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

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "osdtools.h"
#include "tvtimeosd.h"
#include "credits.h"

struct tvtime_osd_s
{
    osd_string_t *channel_number;
    int channel_number_xpos;
    int channel_number_ypos;

    osd_string_t *tuner_info;
    int tuner_info_xpos;
    int tuner_info_ypos;

    osd_string_t *signal_info;
    int signal_info_xpos;
    int signal_info_ypos;

    osd_string_t *volume_bar;
    int volume_bar_xpos;
    int volume_bar_ypos;

    osd_string_t *data_bar;
    int data_bar_xpos;
    int data_bar_ypos;

    osd_string_t *muted;
    int muted_xpos;
    int muted_ypos;

    osd_string_t *channel_info;
    int channel_info_xpos;
    int channel_info_ypos;

    osd_graphic_t *channel_logo;
    int channel_logo_xpos;
    int channel_logo_ypos;

    int ismuted;
    int hassignal;

    char channel_number_text[ 20 ];
    char tv_norm_text[ 20 ];
    char input_text[ 128 ];
    char freqtable_text[ 128 ];
    char deinterlace_text[ 128 ];
    char timeformat[ 128 ];

    credits_t *credits;
    int show_credits;
};

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect )
{
    char *fontfile;
    char *logofile;
    tvtime_osd_t *osd = (tvtime_osd_t *) malloc( sizeof( tvtime_osd_t ) );
    if( !osd ) {
        return 0;
    }

    osd->channel_number = 0;
    osd->channel_info = 0;
    osd->volume_bar = 0;
    osd->data_bar = 0;
    osd->muted = 0;
    osd->hassignal = 0;
    osd->channel_logo = 0;

    memset( osd->channel_number_text, 0, sizeof( osd->channel_number_text ) );
    memset( osd->tv_norm_text, 0, sizeof( osd->tv_norm_text ) );
    memset( osd->freqtable_text, 0, sizeof( osd->freqtable_text ) );
    memset( osd->input_text, 0, sizeof( osd->input_text ) );
    memset( osd->deinterlace_text, 0, sizeof( osd->deinterlace_text ) );
    memset( osd->timeformat, 0, sizeof( osd->timeformat ) );

    fontfile = DATADIR "/FreeSansBold.ttf";
    logofile = DATADIR "/testlogo.png";

    osd->credits = credits_new( "credits.png", height );
    osd->show_credits = 0;

    osd->channel_number = osd_string_new( fontfile, 80, width, height, frameaspect );
    if( !osd->channel_number ) {
        fontfile = "./FreeSansBold.ttf";
        logofile = "./testlogo.png";
        osd->channel_number = osd_string_new( fontfile, 80, width, height, frameaspect );
    }
    osd->channel_info = osd_string_new( fontfile, 30, width, height, frameaspect );
    osd->tuner_info = osd_string_new( fontfile, 20, width, height, frameaspect );
    osd->signal_info = osd_string_new( fontfile, 20, width, height, frameaspect );
    osd->volume_bar = osd_string_new( fontfile, 15, width, height, frameaspect );
    osd->data_bar = osd_string_new( fontfile, 15, width, height, frameaspect );
    osd->muted = osd_string_new( fontfile, 15, width, height, frameaspect );
    osd->channel_logo = osd_graphic_new( logofile, width, height, frameaspect, 256 );

    if( !osd->channel_number || !osd->channel_info || !osd->tuner_info ||
        !osd->signal_info || !osd->volume_bar || !osd->data_bar ||
        !osd->muted || !osd->channel_logo ) {
        fprintf( stderr, "tvtimeosd: Can't create all OSD objects.\n" );
        tvtime_osd_delete( osd );
        return 0;
    }

    osd_string_set_colour_rgb( osd->tuner_info, 245, 222, 179 );
    osd_string_show_text( osd->tuner_info, "0", 1 );
    osd_string_show_border( osd->tuner_info, 1 );
    osd->tuner_info_xpos = ( width * 5 ) / 100;
    osd->tuner_info_ypos = ( height * 5 ) / 100;

    osd_string_set_colour( osd->channel_number, 220, 12, 155 );
    osd_string_show_text( osd->channel_number, "0", 1 );
    osd_string_show_border( osd->channel_number, 1 );
    osd->channel_number_xpos = osd->tuner_info_xpos;
    osd->channel_number_ypos = osd->tuner_info_ypos + osd_string_get_height( osd->tuner_info );

    osd_string_set_colour_rgb( osd->signal_info, 245, 222, 179 );
    osd_string_show_text( osd->signal_info, "No signal", 100 );
    osd_string_show_border( osd->signal_info, 1 );
    osd->signal_info_xpos = osd->tuner_info_xpos;
    osd->signal_info_ypos = osd->channel_number_ypos + osd_string_get_height( osd->channel_number );

    osd_string_set_colour( osd->channel_info, 220, 12, 155 );
    osd_string_set_colour_rgb( osd->channel_info, 245, 222, 179 );
    osd_string_show_border( osd->channel_info, 1 );
    osd->channel_info_xpos = ( width * 60 ) / 100;
    osd->channel_info_ypos = ( height * 5 ) / 100;

    osd_string_set_colour_rgb( osd->volume_bar, 245, 222, 179 );
    osd->volume_bar_xpos = ( width * 5 ) / 100;
    osd->volume_bar_ypos = ( height * 90 ) / 100;

    osd_string_set_colour_rgb( osd->data_bar, 245, 222, 179 );
    osd->data_bar_xpos = ( width * 5 ) / 100;
    osd->data_bar_ypos = ( height * 90 ) / 100;

    osd_string_set_colour_rgb( osd->muted, 245, 222, 179 );
    osd_string_show_text( osd->muted, "Mute", 100 );
    osd->muted_xpos = ( width * 5 ) / 100;
    osd->muted_ypos = ( height * 90 ) / 100;
    osd->ismuted = 0;

    osd->channel_logo_xpos = ( width * 60 ) / 100;
    osd->channel_logo_ypos = ( height * 14 ) / 100;

    return osd;
}

void tvtime_osd_delete( tvtime_osd_t *osd )
{
    if( osd->channel_number ) osd_string_delete( osd->channel_number );
    if( osd->channel_info ) osd_string_delete( osd->channel_info );
    if( osd->tuner_info ) osd_string_delete( osd->channel_info );
    if( osd->signal_info ) osd_string_delete( osd->channel_info );
    if( osd->volume_bar ) osd_string_delete( osd->volume_bar );
    if( osd->data_bar ) osd_string_delete( osd->data_bar );
    if( osd->muted ) osd_string_delete( osd->muted );
    if( osd->channel_logo ) osd_graphic_delete( osd->channel_logo );
    free( osd );
}

void tvtime_osd_set_norm( tvtime_osd_t *osd, const char *norm )
{
    snprintf( osd->tv_norm_text, sizeof( osd->tv_norm_text ) - 1, norm );
}

void tvtime_osd_set_input( tvtime_osd_t *osd, const char *norm )
{
    snprintf( osd->input_text, sizeof( osd->input_text ) - 1, norm );
}

void tvtime_osd_set_freq_table( tvtime_osd_t *osd, const char *freqtable )
{
    snprintf( osd->freqtable_text, sizeof( osd->freqtable_text ) - 1, freqtable );
}

void tvtime_osd_set_channel_number( tvtime_osd_t *osd, const char *norm )
{
    snprintf( osd->channel_number_text, sizeof( osd->channel_number_text ) - 1, norm );
}

void tvtime_osd_set_deinterlace_method( tvtime_osd_t *osd, const char *method )
{
    snprintf( osd->deinterlace_text, sizeof( osd->deinterlace_text ) - 1, method );
}

void tvtime_osd_set_timeformat( tvtime_osd_t *osd, const char *format )
{
    snprintf( osd->timeformat, sizeof( osd->timeformat ) - 1, format );
}

void tvtime_osd_signal_present( tvtime_osd_t *osd, int signal )
{
    osd->hassignal = signal;
}

void tvtime_osd_show_info( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    char text[ 200 ];

    strftime( timestamp, 50, osd->timeformat, localtime( &tm ) );
    osd_string_show_text( osd->channel_number, osd->channel_number_text, 80 );
    osd_string_show_text( osd->channel_info, timestamp, 80 );

    sprintf( text, "%s%s%s", osd->tv_norm_text,
             strlen( osd->freqtable_text ) ? ": " : "", osd->freqtable_text );
    osd_string_show_text( osd->tuner_info, text, 80 );

    sprintf( text, "%s - %s", osd->input_text, osd->deinterlace_text );
    osd_string_show_text( osd->data_bar, text, 80 );

    osd_graphic_show_graphic( osd->channel_logo, 80 );
    osd_string_set_timeout( osd->volume_bar, 0 );
}

int tvtime_osd_data_bar_visible( tvtime_osd_t *osd )
{
    return osd_string_visible( osd->data_bar );
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

void tvtime_osd_show_message( tvtime_osd_t *osd, const char *message )
{
    osd_string_show_text( osd->data_bar, message, 80 );
    osd_string_set_timeout( osd->volume_bar, 0 );
}

void tvtime_osd_show_channel_logo( tvtime_osd_t *osd )
{
    osd_graphic_show_graphic( osd->channel_logo, 80 );
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
    osd->ismuted = mutestate;
}

void tvtime_osd_advance_frame( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    int chinfo_left = 0;

    osd_string_advance_frame( osd->channel_number );
    osd_string_advance_frame( osd->channel_info );
    osd_string_advance_frame( osd->tuner_info );

    if( (chinfo_left = osd_string_get_frames_left( osd->channel_info)) ) {
        strftime( timestamp, 50, osd->timeformat, localtime( &tm ) );
        osd_string_show_text( osd->channel_info, timestamp, chinfo_left );
    }

    osd_string_advance_frame( osd->volume_bar );
    osd_string_advance_frame( osd->data_bar );
    osd_graphic_advance_frame( osd->channel_logo );

    if( osd->credits ) {
        credits_advance_frame( osd->credits );
    }
}

void tvtime_osd_toggle_show_credits( tvtime_osd_t *osd )
{
    osd->show_credits = !osd->show_credits;
    if( osd->show_credits && osd->credits ) {
        credits_restart( osd->credits, 3.0 );
    }
}

int tvtime_osd_active_on_scanline( tvtime_osd_t *osd, int scanline )
{
    int bar_start, bar_end;

    if( osd_string_visible( osd->channel_number ) ) {
        int start = osd->channel_number_ypos;
        int end = start + osd_string_get_height( osd->channel_number );

        if( scanline >= start && scanline < end ) return 1;
    }

    if( osd_string_visible( osd->channel_info ) ) {
        int start = osd->channel_info_ypos;
        int end = start + osd_string_get_height( osd->channel_info );

        if( scanline >= start && scanline < end ) return 1;
    }

    if( osd_graphic_visible( osd->channel_logo ) ) {
        int start = osd->channel_logo_ypos;
        int end = start + osd_graphic_get_height( osd->channel_logo );

        if( scanline >= start && scanline < end ) return 1;
    }

    bar_start = osd->data_bar_ypos;
    if( scanline < bar_start ) return 0;

    /**
     * For the bottom info, the data bar has priority over the
     * muted indicator which has priority over the volume bar.
     */
    if( osd_string_visible( osd->data_bar ) ) {
        bar_end = bar_start + osd_string_get_height( osd->data_bar );

        if( scanline < bar_end ) return 1;
    } else if( osd->ismuted ) {
        bar_end = bar_start + osd_string_get_height( osd->muted );

        if( scanline < bar_end ) return 1;
    } else if( osd_string_visible( osd->volume_bar ) ) {
        bar_end = bar_start + osd_string_get_height( osd->volume_bar );

        if( scanline < bar_end ) return 1;
    }

    return 0;
}

void tvtime_osd_composite_packed422_scanline( tvtime_osd_t *osd,
                                              unsigned char *output,
                                              int width, int xpos,
                                              int scanline )
{
    if( osd->show_credits && osd->credits ) {
        credits_composite_packed422_scanline( osd->credits, output, width, xpos, scanline );
    }

    if( osd_string_visible( osd->channel_number ) ) {
        int start = osd->channel_number_ypos;
        int end = start + osd_string_get_height( osd->channel_number );

        if( scanline >= start && scanline < end ) {
            int startx = osd->channel_number_xpos - xpos;
            int strx = 0;

            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_string_composite_packed422_scanline( osd->channel_number,
                                                         output + (startx*2),
                                                         output + (startx*2),
                                                         width - startx,
                                                         strx,
                                                         scanline - osd->channel_number_ypos );
            }
        }
    }

    if( osd_string_visible( osd->channel_info ) ) {
        if( scanline >= osd->channel_info_ypos &&
            scanline < osd->channel_info_ypos + osd_string_get_height( osd->channel_info ) ) {

            int startx = osd->channel_info_xpos - xpos;
            int strx = 0;
            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_string_composite_packed422_scanline( osd->channel_info,
                                                         output + (startx*2),
                                                         output + (startx*2),
                                                         width - startx,
                                                         strx,
                                                         scanline - osd->channel_info_ypos );
            }
        }
    }

    if( osd_string_visible( osd->tuner_info ) ) {
        if( scanline >= osd->tuner_info_ypos &&
            scanline < osd->tuner_info_ypos + osd_string_get_height( osd->tuner_info ) ) {

            int startx = osd->tuner_info_xpos - xpos;
            int strx = 0;
            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_string_composite_packed422_scanline( osd->tuner_info,
                                                         output + (startx*2),
                                                         output + (startx*2),
                                                         width - startx,
                                                         strx,
                                                         scanline - osd->tuner_info_ypos );
            }
        }
    }

    if( !osd->hassignal ) {
        if( osd_string_visible( osd->signal_info ) ) {
            if( scanline >= osd->signal_info_ypos &&
                scanline < osd->signal_info_ypos + osd_string_get_height( osd->signal_info ) ) {

                int startx = osd->signal_info_xpos - xpos;
                int strx = 0;
                if( startx < 0 ) {
                    strx = -startx;
                    startx = 0;
                }
                if( startx < width ) {
                    osd_string_composite_packed422_scanline( osd->signal_info,
                                                             output + (startx*2),
                                                             output + (startx*2),
                                                             width - startx,
                                                             strx,
                                                             scanline - osd->signal_info_ypos );
                }
            }
        }
    }

    if( osd_graphic_visible( osd->channel_logo ) ) {
        if( scanline >= osd->channel_logo_ypos &&
            scanline < osd->channel_logo_ypos + osd_graphic_get_height( osd->channel_logo ) ) {

            int startx = osd->channel_logo_xpos - xpos;
            int strx = 0;
            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_graphic_composite_packed422_scanline( osd->channel_logo,
                                                          output + (startx*2),
                                                          output + (startx*2),
                                                          width - startx,
                                                          strx,
                                                          scanline - osd->channel_logo_ypos );
            }
        }
    }

    /**
     * For the bottom info, the data bar has priority over the
     * muted indicator which has priority over the volume bar.
     */
    if( osd_string_visible( osd->data_bar ) ) {
        if( scanline >= osd->data_bar_ypos &&
            scanline < osd->data_bar_ypos + osd_string_get_height( osd->data_bar ) ) {

            int startx = osd->data_bar_xpos - xpos;
            int strx = 0;
            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_string_composite_packed422_scanline( osd->data_bar,
                                                         output + (startx*2),
                                                         output + (startx*2),
                                                         width - startx,
                                                         strx,
                                                         scanline - osd->data_bar_ypos );
            }
        }
    } else if( osd->ismuted ) {
        if( scanline >= osd->muted_ypos &&
            scanline < osd->muted_ypos + osd_string_get_height( osd->muted ) ) {

            int startx = osd->muted_xpos - xpos;
            int strx = 0;
            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_string_composite_packed422_scanline( osd->muted,
                                                         output + (startx*2),
                                                         output + (startx*2),
                                                         width - startx,
                                                         strx,
                                                         scanline - osd->muted_ypos );
            }
        }
    } else if( osd_string_visible( osd->volume_bar ) ) {
        if( scanline >= osd->volume_bar_ypos &&
            scanline < osd->volume_bar_ypos + osd_string_get_height( osd->volume_bar ) ) {

            int startx = osd->volume_bar_xpos - xpos;
            int strx = 0;
            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_string_composite_packed422_scanline( osd->volume_bar,
                                                         output + (startx*2),
                                                         output + (startx*2),
                                                         width - startx,
                                                         strx,
                                                         scanline - osd->volume_bar_ypos );
            }
        }
    }
}

