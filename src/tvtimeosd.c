/**
 * Copyright (C) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include "commands.h"

#define OSD_FADE_DELAY  60

typedef struct string_object_s
{
    osd_string_t *string;
    int xpos;
    int ypos;
    int rightjustified;
} string_object_t;

enum osd_objects
{
    OSD_CHANNEL_NUM = 0,
    OSD_TUNER_INFO,
    OSD_SIGNAL_INFO,
    OSD_VOLUME_BAR,
    OSD_DATA_BAR,
    OSD_MUTED,
    OSD_TIME_STRING,

    /* XDS data */
    OSD_NETWORK_NAME,
    OSD_NETWORK_CALL,
    OSD_SHOW_NAME,
    OSD_SHOW_RATING,
    OSD_SHOW_START,
    OSD_SHOW_LENGTH,

    OSD_MAX_STRING_OBJECTS
};

struct tvtime_osd_s
{
    string_object_t *strings;

    osd_graphic_t *channel_logo;
    int channel_logo_xpos;
    int channel_logo_ypos;

    char channel_number_text[ 20 ];
    char tv_norm_text[ 20 ];
    char input_text[ 128 ];
    char freqtable_text[ 128 ];
    char audiomode_text[ 128 ];
    char deinterlace_text[ 128 ];
    char timeformat[ 128 ];

    const char *network_name;
    const char *network_call;
    const char *show_rating;
    const char *show_start;
    const char *show_length;
    const char *show_name;

    char hold_message[ 255 ];

    double framerate;
    int framerate_mode;

    credits_t *credits;
    int show_credits;
};

void tvtime_osd_set_hold_message( tvtime_osd_t* osd, const char *str )
{
    sprintf( osd->hold_message, "%s", str );
}

void tvtime_osd_set_network_name( tvtime_osd_t* osd, const char *str )
{
    char fullstr[ 128 ];
    strcpy( fullstr, "Network Name: " );
    strncat( fullstr, str, 127 );
    osd->network_name = str;
    osd_string_show_text( osd->strings[ OSD_NETWORK_NAME ].string, 
                          str, 
                          osd_string_get_frames_left( osd->strings[ OSD_NETWORK_NAME ].string ) );
}

void tvtime_osd_set_show_name( tvtime_osd_t* osd, const char *str )
{
    char fullstr[ 128 ];
    strcpy( fullstr, "Show Name: " );
    strncat( fullstr, str, 127 );

    osd->show_name = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_NAME ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_NAME ].string ) );
}

void tvtime_osd_set_network_call( tvtime_osd_t* osd, const char *str )
{
    char fullstr[ 128 ];
    strcpy( fullstr, "Call Letters: " );
    strncat( fullstr, str, 127 );

    osd->network_call = str;
    osd_string_show_text( osd->strings[ OSD_NETWORK_CALL ].string, 
                          str, 
                          osd_string_get_frames_left( osd->strings[ OSD_NETWORK_CALL ].string ) );
}

void tvtime_osd_set_show_rating( tvtime_osd_t* osd, const char *str )
{
    char fullstr[ 128 ];
    strcpy( fullstr, "Show Rating: " );
    strncat( fullstr, str, 127 );
    osd->show_rating = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_RATING ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_RATING ].string ) );
}

void tvtime_osd_set_show_start( tvtime_osd_t* osd, const char *str )
{
    char fullstr[ 128 ];
    strcpy( fullstr, "Start Time: " );
    strncat( fullstr, str, 127 );
    osd->show_start = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_START ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_START ].string ) );
}

void tvtime_osd_set_show_length( tvtime_osd_t* osd, const char *str )
{
    char fullstr[ 128 ];
    strcpy( fullstr, "Elapsed/Length: " );
    strncat( fullstr, str, 127 );
    osd->show_length = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_LENGTH ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_LENGTH ].string ) );
}

const int top_size = 7;
const int left_size = 7;
const int bottom_size = 13;

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect,
                              unsigned int channel_rgb, unsigned int other_rgb )
{
    char *fontfile;
    char *logofile;
    char *creditsfile;

    tvtime_osd_t *osd = (tvtime_osd_t *) malloc( sizeof( tvtime_osd_t ) );
    if( !osd ) {
        return 0;
    }

    osd->strings = (string_object_t *) malloc( sizeof( string_object_t ) * OSD_MAX_STRING_OBJECTS );

    osd->channel_logo = 0;
    strcpy( osd->hold_message, "" );
    osd->framerate = 0.0;
    osd->framerate_mode = FRAMERATE_FULL;

    memset( osd->channel_number_text, 0, sizeof( osd->channel_number_text ) );
    memset( osd->tv_norm_text, 0, sizeof( osd->tv_norm_text ) );
    memset( osd->freqtable_text, 0, sizeof( osd->freqtable_text ) );
    memset( osd->audiomode_text, 0, sizeof( osd->audiomode_text ) );
    memset( osd->input_text, 0, sizeof( osd->input_text ) );
    memset( osd->deinterlace_text, 0, sizeof( osd->deinterlace_text ) );
    memset( osd->timeformat, 0, sizeof( osd->timeformat ) );

    fontfile = DATADIR "/FreeSansBold.ttf";
    logofile = DATADIR "/testlogo.png";
    creditsfile = DATADIR "/credits.png";

    osd->credits = credits_new( creditsfile, height );
    osd->show_credits = 0;
    if( !osd->credits ) {
        creditsfile = "../data/credits.png";
        osd->credits = credits_new( creditsfile, height );
    }

    osd->strings[ OSD_CHANNEL_NUM ].rightjustified = 0;
    osd->strings[ OSD_CHANNEL_NUM ].string = osd_string_new( fontfile, 80, width, height, frameaspect );
    if( !osd->strings[ OSD_CHANNEL_NUM ].string ) {
        fontfile = "../data/FreeSansBold.ttf";
        logofile = "../data/testlogo.png";
        osd->strings[ OSD_CHANNEL_NUM ].string = osd_string_new( fontfile, 80, width, height, frameaspect );
    }
    osd->strings[ OSD_TIME_STRING ].rightjustified = 1;
    osd->strings[ OSD_TIME_STRING ].string = osd_string_new( fontfile, 30, width, height, frameaspect );
    osd->strings[ OSD_TUNER_INFO ].rightjustified = 0;
    osd->strings[ OSD_TUNER_INFO ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_SIGNAL_INFO ].rightjustified = 0;
    osd->strings[ OSD_SIGNAL_INFO ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_VOLUME_BAR ].rightjustified = 0;
    osd->strings[ OSD_VOLUME_BAR ].string = osd_string_new( fontfile, 17, width, height, frameaspect );
    osd->strings[ OSD_DATA_BAR ].rightjustified = 0;
    osd->strings[ OSD_DATA_BAR ].string = osd_string_new( fontfile, 17, width, height, frameaspect );
    osd->strings[ OSD_MUTED ].rightjustified = 0;
    osd->strings[ OSD_MUTED ].string = osd_string_new( fontfile, 18, width, height, frameaspect );

    osd->strings[ OSD_NETWORK_NAME ].rightjustified = 1;
    osd->strings[ OSD_NETWORK_NAME ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_NETWORK_CALL ].rightjustified = 1;
    osd->strings[ OSD_NETWORK_CALL ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_SHOW_NAME ].rightjustified = 0;
    osd->strings[ OSD_SHOW_NAME ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_SHOW_RATING ].rightjustified = 0;
    osd->strings[ OSD_SHOW_RATING ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_SHOW_START ].rightjustified = 0;
    osd->strings[ OSD_SHOW_START ].string = osd_string_new( fontfile, 18, width, height, frameaspect );
    osd->strings[ OSD_SHOW_LENGTH ].rightjustified = 0;
    osd->strings[ OSD_SHOW_LENGTH ].string = osd_string_new( fontfile, 18, width, height, frameaspect );

    osd->channel_logo = osd_graphic_new( logofile, width, height, frameaspect, 256 );

    if( !osd->strings[ OSD_CHANNEL_NUM ].string || !osd->strings[ OSD_TIME_STRING ].string ||
        !osd->strings[ OSD_TUNER_INFO ].string || !osd->strings[ OSD_SIGNAL_INFO ].string ||
        !osd->strings[ OSD_VOLUME_BAR ].string || !osd->strings[ OSD_DATA_BAR ].string ||
        !osd->strings[ OSD_MUTED ].string || !osd->strings[ OSD_SHOW_NAME ].string || !osd->strings[ OSD_SHOW_RATING ].string || !osd->strings[ OSD_SHOW_START ].string || !osd->strings[ OSD_SHOW_LENGTH ].string || !osd->strings[ OSD_NETWORK_NAME ].string || !osd->strings[ OSD_NETWORK_CALL ].string || !osd->channel_logo ) {
        fprintf( stderr, "tvtimeosd: Can't create all OSD objects.\n" );
        tvtime_osd_delete( osd );
        return 0;
    }

    osd_string_set_colour_rgb( osd->strings[ OSD_TUNER_INFO ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_text( osd->strings[ OSD_TUNER_INFO ].string, "0", 0 );
    osd_string_show_border( osd->strings[ OSD_TUNER_INFO ].string, 1 );
    osd->strings[ OSD_TUNER_INFO ].xpos = (( width * left_size ) / 100) & ~1;
    osd->strings[ OSD_TUNER_INFO ].ypos = ( height * top_size ) / 100;

    osd_string_set_colour_rgb( osd->strings[ OSD_CHANNEL_NUM ].string,
                               (channel_rgb >> 16) & 0xff, (channel_rgb >> 8) & 0xff, (channel_rgb & 0xff) );
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, "0", 0 );
    osd_string_show_border( osd->strings[ OSD_CHANNEL_NUM ].string, 1 );
    osd->strings[ OSD_CHANNEL_NUM ].xpos = osd->strings[ OSD_TUNER_INFO ].xpos;
    osd->strings[ OSD_CHANNEL_NUM ].ypos = osd->strings[ OSD_TUNER_INFO ].xpos
                               + osd_string_get_height( osd->strings[ OSD_TUNER_INFO ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_SIGNAL_INFO ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_text( osd->strings[ OSD_SIGNAL_INFO ].string, "No signal", 0 );
    osd_string_show_border( osd->strings[ OSD_SIGNAL_INFO ].string, 1 );
    osd_string_set_hold( osd->strings[ OSD_MUTED ].string, 1 );
    osd->strings[ OSD_SIGNAL_INFO ].xpos = osd->strings[ OSD_TUNER_INFO ].xpos;
    osd->strings[ OSD_SIGNAL_INFO ].ypos = osd->strings[ OSD_CHANNEL_NUM ].ypos
                            + osd_string_get_height( osd->strings[ OSD_CHANNEL_NUM ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_TIME_STRING ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_TIME_STRING ].string, 1 );
    osd->strings[ OSD_TIME_STRING ].xpos = width - ((( width * left_size ) / 100) & ~1); // (( width * 65 ) / 100) & ~1;
    osd->strings[ OSD_TIME_STRING ].ypos = ( height * top_size ) / 100;
    osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, "8", 0 );

    osd_string_set_colour_rgb( osd->strings[ OSD_VOLUME_BAR ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_VOLUME_BAR ].string, 1 );
    osd->strings[ OSD_VOLUME_BAR ].xpos = (( width * left_size ) / 100) & ~1;
    osd->strings[ OSD_VOLUME_BAR ].ypos = ( height * (100 - bottom_size) ) / 100;

    osd_string_set_colour_rgb( osd->strings[ OSD_DATA_BAR ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_DATA_BAR ].string, 1 );
    osd->strings[ OSD_DATA_BAR ].xpos = osd->strings[ OSD_VOLUME_BAR ].xpos;
    osd->strings[ OSD_DATA_BAR ].ypos = ( height * (100 - bottom_size) ) / 100;

    osd_string_set_colour_rgb( osd->strings[ OSD_MUTED ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_MUTED ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_MUTED ].string, "Mute", 0 );
    osd_string_set_hold( osd->strings[ OSD_MUTED ].string, 1 );
    osd->strings[ OSD_MUTED ].xpos = osd->strings[ OSD_VOLUME_BAR ].xpos;
    osd->strings[ OSD_MUTED ].ypos = osd->strings[ OSD_VOLUME_BAR ].ypos
                                     - osd_string_get_height( osd->strings[ OSD_MUTED ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_SHOW_RATING ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_SHOW_RATING ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_SHOW_RATING ].string, "Not Rated ", 0 );
    osd->strings[ OSD_SHOW_RATING ].xpos = osd->strings[ OSD_DATA_BAR ].xpos;
    osd->strings[ OSD_SHOW_RATING ].ypos = osd->strings[ OSD_DATA_BAR ].ypos - osd_string_get_height( osd->strings[ OSD_MUTED ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_SHOW_NAME ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_SHOW_NAME ].string, 1 );
    osd->strings[ OSD_SHOW_NAME ].xpos = ( osd->strings[ OSD_DATA_BAR ].xpos + osd_string_get_width( osd->strings[ OSD_SHOW_RATING ].string ) ) & ~1;
    osd->strings[ OSD_SHOW_NAME ].ypos = osd->strings[ OSD_DATA_BAR ].ypos - osd_string_get_height( osd->strings[ OSD_MUTED ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_SHOW_START ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_SHOW_START ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_SHOW_START ].string, "00, WWW 00:00", 0 );
    osd->strings[ OSD_SHOW_START ].xpos = osd->strings[ OSD_DATA_BAR ].xpos;
    osd->strings[ OSD_SHOW_START ].ypos = osd->strings[ OSD_SHOW_NAME ].ypos - osd_string_get_height( osd->strings[ OSD_MUTED ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_SHOW_LENGTH ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_SHOW_LENGTH ].string, 1 );
    osd->strings[ OSD_SHOW_LENGTH ].xpos = ( osd->strings[ OSD_SHOW_START ].xpos + osd_string_get_width( osd->strings[ OSD_SHOW_START ].string ) ) & ~1;
    osd->strings[ OSD_SHOW_LENGTH ].ypos = osd->strings[ OSD_SHOW_START ].ypos;

    osd_string_set_colour_rgb( osd->strings[ OSD_NETWORK_NAME ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_NETWORK_NAME ].string, 1 );
    osd->strings[ OSD_NETWORK_NAME ].xpos = osd->strings[ OSD_TIME_STRING ].xpos;
    osd->strings[ OSD_NETWORK_NAME ].ypos = osd->strings[ OSD_TIME_STRING ].ypos + osd_string_get_height( osd->strings[ OSD_TIME_STRING ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_NETWORK_CALL ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_NETWORK_CALL ].string, 1 );
    osd->strings[ OSD_NETWORK_CALL ].xpos = osd->strings[ OSD_TIME_STRING ].xpos;
    osd->strings[ OSD_NETWORK_CALL ].ypos = osd->strings[ OSD_NETWORK_NAME ].ypos + osd_string_get_height( osd->strings[ OSD_MUTED ].string );


    /* remove these strings */
    osd_string_show_text( osd->strings[ OSD_SHOW_RATING ].string, "", 0 );
    osd_string_show_text( osd->strings[ OSD_SHOW_START ].string, "", 0 );

    osd->channel_logo_xpos = (( width * 60 ) / 100) & ~1;
    osd->channel_logo_ypos = ( height * 14 ) / 100;

    return osd;
}

void tvtime_osd_delete( tvtime_osd_t *osd )
{
    int i;

    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        if( osd->strings[ i ].string ) {
            osd_string_delete( osd->strings[ i ].string );
        }
    }
    free( osd->strings );
    if( osd->channel_logo ) osd_graphic_delete( osd->channel_logo );
    if( osd->credits ) credits_delete( osd->credits );
    free( osd );
}

void tvtime_osd_set_norm( tvtime_osd_t *osd, const char *norm )
{
    if( osd->tv_norm_text ) snprintf( osd->tv_norm_text, sizeof( osd->tv_norm_text ) - 1, "%s", norm );
}

void tvtime_osd_set_input( tvtime_osd_t *osd, const char *norm )
{
    if( osd->input_text ) snprintf( osd->input_text, sizeof( osd->input_text ) - 1, "%s", norm );
}

void tvtime_osd_set_freq_table( tvtime_osd_t *osd, const char *freqtable )
{
    if( osd->freqtable_text ) {
        snprintf( osd->freqtable_text, sizeof( osd->freqtable_text ) - 1, "%s", freqtable );
    }
}

void tvtime_osd_set_audio_mode( tvtime_osd_t *osd, const char *audiomode )
{
    if( osd->audiomode_text ) {
        snprintf( osd->audiomode_text, sizeof( osd->audiomode_text ) - 1, audiomode );
    }
}

void tvtime_osd_set_channel_number( tvtime_osd_t *osd, const char *norm )
{
    if( osd->channel_number_text ) {
        snprintf( osd->channel_number_text, sizeof( osd->channel_number_text ) - 1, "%s", norm );
    }
}

void tvtime_osd_set_deinterlace_method( tvtime_osd_t *osd, const char *method )
{
    snprintf( osd->deinterlace_text, sizeof( osd->deinterlace_text ) - 1, "%s", method );
}

void tvtime_osd_set_timeformat( tvtime_osd_t *osd, const char *format )
{
    snprintf( osd->timeformat, sizeof( osd->timeformat ) - 1, "%s", format );
}

void tvtime_osd_set_framerate( tvtime_osd_t *osd, double framerate, int mode )
{
    osd->framerate = framerate;
    osd->framerate_mode = mode;
}

void tvtime_osd_signal_present( tvtime_osd_t *osd, int signal )
{
    osd_string_set_timeout( osd->strings[ OSD_SIGNAL_INFO ].string, signal ? 0 : 100 );
}

void tvtime_osd_show_info( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    char text[ 200 ];
    int i;

    strftime( timestamp, 50, osd->timeformat, localtime( &tm ) );
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, osd->channel_number_text, OSD_FADE_DELAY );
    osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, timestamp, OSD_FADE_DELAY );

    if( strlen( osd->freqtable_text ) ) {
        sprintf( text, "%s [%s]: %s", osd->tv_norm_text, osd->audiomode_text, osd->freqtable_text );
    } else {
        sprintf( text, "%s", osd->tv_norm_text );
    }
    osd_string_show_text( osd->strings[ OSD_TUNER_INFO ].string, text, OSD_FADE_DELAY );

    if( *(osd->hold_message) ) {
        sprintf( text, "%s", osd->hold_message );
    } else {
        const char *mode = "";
        if( osd->framerate_mode == FRAMERATE_HALF_TFF ) {
            mode = " TFF";
        } else if( osd->framerate_mode == FRAMERATE_HALF_BFF ) {
            mode = " BFF";
        }
        sprintf( text, "%s - %s [%.2ffps%s]", osd->input_text,
                 osd->deinterlace_text, osd->framerate, mode );
    }
    osd_string_show_text( osd->strings[ OSD_DATA_BAR ].string, text, OSD_FADE_DELAY );
    osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );

    /** Billy: What's up?  Are we ditching the logo for XDS?
     *osd_graphic_show_graphic( osd->channel_logo, OSD_FADE_DELAY );
     *osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );
     */

    for( i = OSD_SHOW_NAME; i <= OSD_SHOW_LENGTH; i++ ) {
        osd_string_set_timeout( osd->strings[ i ].string, OSD_FADE_DELAY );
    }
}

int tvtime_osd_data_bar_visible( tvtime_osd_t *osd )
{
    return osd_string_visible( osd->strings[ OSD_DATA_BAR ].string );
}

void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage )
{
    char bar[ 108 ];
    memset( bar, 0, 108 );
    strcpy( bar, barname );
    memset( bar + 7, '|', percentage );
    osd_string_show_text( osd->strings[ OSD_DATA_BAR ].string, bar, OSD_FADE_DELAY );
    osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );
}

void tvtime_osd_show_message( tvtime_osd_t *osd, const char *message )
{
    osd_string_show_text( osd->strings[ OSD_DATA_BAR ].string, message, OSD_FADE_DELAY );
    osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );
}

void tvtime_osd_show_channel_logo( tvtime_osd_t *osd )
{
    osd_graphic_show_graphic( osd->channel_logo, OSD_FADE_DELAY );
}

void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage )
{
    char bar[ 108 ];
    memset( bar, 0, 108 );
    strcpy( bar, "Volume " );
    memset( bar + 7, '|', percentage );
    osd_string_show_text( osd->strings[ OSD_VOLUME_BAR ].string, bar, OSD_FADE_DELAY );
    osd_string_set_timeout( osd->strings[ OSD_DATA_BAR ].string, 0 );
}

void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate )
{
    osd_string_set_timeout( osd->strings[ OSD_MUTED ].string, mutestate ? 100 : 0 );
}

void tvtime_osd_advance_frame( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    int chinfo_left = 0;
    int i;

    if( (chinfo_left = osd_string_get_frames_left( osd->strings[ OSD_TIME_STRING ].string)) ) {
        strftime( timestamp, 50, osd->timeformat, localtime( &tm ) );
        osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, timestamp, chinfo_left );
    }

    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        osd_string_advance_frame( osd->strings[ i ].string );
    }

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

void tvtime_osd_composite_packed422_scanline( tvtime_osd_t *osd,
                                              unsigned char *output,
                                              int width, int xpos,
                                              int scanline )
{
    int i;

    if( osd->show_credits && osd->credits ) {
        credits_composite_packed422_scanline( osd->credits, output, width, xpos, scanline );
    }

    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        if( osd_string_visible( osd->strings[ i ].string ) ) {
            int start = osd->strings[ i ].ypos;
            int end = start + osd_string_get_height( osd->strings[ i ].string );

            if( scanline >= start && scanline < end ) {
                int startx;
                int strx = 0;

                if( osd->strings[ i ].rightjustified ) {
                    startx = osd->strings[ i ].xpos - osd_string_get_width( osd->strings[ i ].string ) - xpos;
                } else {
                    startx = osd->strings[ i ].xpos - xpos;
                }

                if( startx < 0 ) {
                    strx = -startx;
                    startx = 0;
                }

                /* Make sure we start somewhere even. */
                startx = startx & ~1;

                if( startx < width ) {
                    osd_string_composite_packed422_scanline( osd->strings[ i ].string,
                                                             output + (startx*2),
                                                             output + (startx*2),
                                                             width - startx,
                                                             strx,
                                                             scanline - osd->strings[ i ].ypos );
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
}

