/**
 * Copyright (C) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "osdtools.h"
#include "tvtimeosd.h"
#include "commands.h"
#include "pulldown.h"

#define MENU_FADE_DELAY  100
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
    OSD_CHANNEL_NAME,
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
    osd_font_t *smallfont;
    osd_font_t *medfont;
    osd_font_t *bigfont;
    string_object_t strings[ OSD_MAX_STRING_OBJECTS ];

    osd_rect_t *databarbg;
    osd_rect_t *databar;
    int databar_ypos;
    int databar_xend;
    int databar_xstart;

    osd_list_t *list;
    int listpos_x;
    int listpos_y;

    osd_animation_t *film_logo;
    int film_logo_xpos;
    int film_logo_ypos;

    osd_animation_t *channel_logo;
    int channel_logo_xpos;
    int channel_logo_ypos;

    char channel_number_text[ 20 ];
    char channel_name_text[ 128 ];
    char tv_norm_text[ 20 ];
    char input_text[ 128 ];
    char freqtable_text[ 128 ];
    char audiomode_text[ 128 ];
    char deinterlace_text[ 128 ];
    char timeformat[ 128 ];

    char network_name[ 33 ];
    char network_call[ 7 ];
    const char *show_rating;
    const char *show_start;
    const char *show_length;
    const char *show_name;

    char hold_message[ 255 ];

    double framerate;
    int framerate_mode;
    int film_mode;
    int pulldown_mode;
    int mutestate;
    int hold;
};

const int top_size = 7;
const int left_size = 7;
const int bottom_size = 13;

tvtime_osd_t *tvtime_osd_new( int width, int height, double pixel_aspect,
                              unsigned int channel_rgb, unsigned int other_rgb )
{
    char *fontfile;
    char *logofile;

    tvtime_osd_t *osd = malloc( sizeof( tvtime_osd_t ) );
    if( !osd ) {
        return 0;
    }

    strcpy( osd->hold_message, "" );
    osd->framerate = 0.0;
    osd->framerate_mode = FRAMERATE_FULL;
    osd->film_mode = -1;
    osd->pulldown_mode = 0;
    osd->mutestate = 0;
    osd->hold = 0;
    osd->listpos_x = width / 2;
    osd->listpos_y = (height * 30) / 100;
    osd->databar_xend = (width * 90) / 100;

    memset( osd->channel_number_text, 0, sizeof( osd->channel_number_text ) );
    memset( osd->channel_name_text, 0, sizeof( osd->channel_name_text ) );
    memset( osd->tv_norm_text, 0, sizeof( osd->tv_norm_text ) );
    memset( osd->freqtable_text, 0, sizeof( osd->freqtable_text ) );
    memset( osd->audiomode_text, 0, sizeof( osd->audiomode_text ) );
    memset( osd->input_text, 0, sizeof( osd->input_text ) );
    memset( osd->deinterlace_text, 0, sizeof( osd->deinterlace_text ) );
    memset( osd->timeformat, 0, sizeof( osd->timeformat ) );

    fontfile = "FreeSansBold.ttf";
    logofile = "tvtimelogo";

    osd->databar = osd_rect_new();
    if( !osd->databar ) {
        free( osd );
        return 0;
    }
    osd->databarbg = osd_rect_new();
    if( !osd->databarbg ) {
        osd_rect_delete( osd->databar );
        free( osd );
        return 0;
    }

    osd->smallfont = osd_font_new( fontfile, 18, pixel_aspect );
    if( !osd->smallfont ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        free( osd );
        return 0;
    }
    osd->medfont = osd_font_new( fontfile, 30, pixel_aspect );
    if( !osd->medfont ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        osd_font_delete( osd->smallfont );
        free( osd );
        return 0;
    }
    osd->bigfont = osd_font_new( fontfile, 80, pixel_aspect );
    if( !osd->bigfont ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        osd_font_delete( osd->smallfont );
        osd_font_delete( osd->medfont );
        free( osd );
        return 0;
    }

    osd->list = osd_list_new( pixel_aspect );
    if( !osd->list ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        osd_font_delete( osd->smallfont );
        osd_font_delete( osd->medfont );
        osd_font_delete( osd->bigfont );
        free( osd );
        return 0;
    }

    osd->strings[ OSD_CHANNEL_NUM ].rightjustified = 0;
    osd->strings[ OSD_CHANNEL_NUM ].string = osd_string_new( osd->medfont );
    osd->strings[ OSD_CHANNEL_NAME ].rightjustified = 0;
    osd->strings[ OSD_CHANNEL_NAME ].string = osd_string_new( osd->bigfont );
    osd->strings[ OSD_TIME_STRING ].rightjustified = 1;
    osd->strings[ OSD_TIME_STRING ].string = osd_string_new( osd->medfont );
    osd->strings[ OSD_TUNER_INFO ].rightjustified = 0;
    osd->strings[ OSD_TUNER_INFO ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_SIGNAL_INFO ].rightjustified = 0;
    osd->strings[ OSD_SIGNAL_INFO ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_VOLUME_BAR ].rightjustified = 0;
    osd->strings[ OSD_VOLUME_BAR ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_DATA_BAR ].rightjustified = 0;
    osd->strings[ OSD_DATA_BAR ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_MUTED ].rightjustified = 0;
    osd->strings[ OSD_MUTED ].string = osd_string_new( osd->smallfont );

    osd->strings[ OSD_NETWORK_NAME ].rightjustified = 1;
    osd->strings[ OSD_NETWORK_NAME ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_NETWORK_CALL ].rightjustified = 1;
    osd->strings[ OSD_NETWORK_CALL ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_SHOW_NAME ].rightjustified = 0;
    osd->strings[ OSD_SHOW_NAME ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_SHOW_RATING ].rightjustified = 0;
    osd->strings[ OSD_SHOW_RATING ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_SHOW_START ].rightjustified = 0;
    osd->strings[ OSD_SHOW_START ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_SHOW_LENGTH ].rightjustified = 0;
    osd->strings[ OSD_SHOW_LENGTH ].string = osd_string_new( osd->smallfont );

    /* We create the logos, but it's ok if they fail to load. */
    osd->channel_logo = osd_animation_new( logofile, pixel_aspect, 256, 1 );
    osd->film_logo = osd_animation_new( "filmstrip", pixel_aspect, 256, 2 );

    if( !osd->strings[ OSD_CHANNEL_NUM ].string || !osd->strings[ OSD_TIME_STRING ].string ||
        !osd->strings[ OSD_TUNER_INFO ].string || !osd->strings[ OSD_SIGNAL_INFO ].string ||
        !osd->strings[ OSD_VOLUME_BAR ].string || !osd->strings[ OSD_DATA_BAR ].string ||
        !osd->strings[ OSD_MUTED ].string || !osd->strings[ OSD_SHOW_NAME ].string ||
        !osd->strings[ OSD_SHOW_RATING ].string || !osd->strings[ OSD_SHOW_START ].string ||
        !osd->strings[ OSD_SHOW_LENGTH ].string || !osd->strings[ OSD_NETWORK_NAME ].string ||
        !osd->strings[ OSD_NETWORK_CALL ].string || !osd->strings[ OSD_CHANNEL_NAME ].string ) {

        fprintf( stderr, "tvtimeosd: Can't create all OSD objects.\n" );
        tvtime_osd_delete( osd );
        return 0;
    }
    if( osd->channel_logo ) osd_animation_pause( osd->channel_logo, 1 );

    osd_string_set_colour_rgb( osd->strings[ OSD_TUNER_INFO ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_TUNER_INFO ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_TUNER_INFO ].string, "0", 0 );
    osd->strings[ OSD_TUNER_INFO ].xpos = (( width * left_size ) / 100) & ~1;
    osd->strings[ OSD_TUNER_INFO ].ypos = ( height * top_size ) / 100;

    osd_string_set_colour_rgb( osd->strings[ OSD_CHANNEL_NAME ].string,
                               (channel_rgb >> 16) & 0xff, (channel_rgb >> 8) & 0xff, (channel_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_CHANNEL_NAME ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NAME ].string, "0", 0 );
    osd->strings[ OSD_CHANNEL_NAME ].xpos = osd->strings[ OSD_TUNER_INFO ].xpos;
    osd->strings[ OSD_CHANNEL_NAME ].ypos = osd->strings[ OSD_TUNER_INFO ].ypos
                               + osd_string_get_height( osd->strings[ OSD_TUNER_INFO ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_CHANNEL_NUM ].string,
                               (channel_rgb >> 16) & 0xff, (channel_rgb >> 8) & 0xff, (channel_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_CHANNEL_NUM ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, "gY", 0 );
    osd->strings[ OSD_CHANNEL_NUM ].xpos = osd->strings[ OSD_TUNER_INFO ].xpos;
    osd->strings[ OSD_CHANNEL_NUM ].ypos = osd->strings[ OSD_CHANNEL_NAME ].ypos
                               + osd_string_get_height( osd->strings[ OSD_CHANNEL_NAME ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_SIGNAL_INFO ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_SIGNAL_INFO ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_SIGNAL_INFO ].string, "No signal", 0 );
    osd_string_set_hold( osd->strings[ OSD_SIGNAL_INFO ].string, 1 );
    osd->strings[ OSD_SIGNAL_INFO ].xpos = osd->strings[ OSD_TUNER_INFO ].xpos;
    osd->strings[ OSD_SIGNAL_INFO ].ypos = osd->strings[ OSD_CHANNEL_NUM ].ypos
                            + osd_string_get_height( osd->strings[ OSD_CHANNEL_NUM ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_TIME_STRING ].string,
                               (other_rgb >> 16) & 0xff, (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_TIME_STRING ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, "8", 0 );
    osd->strings[ OSD_TIME_STRING ].xpos = width - ((( width * left_size ) / 100) & ~1); // (( width * 65 ) / 100) & ~1;
    osd->strings[ OSD_TIME_STRING ].ypos = ( height * top_size ) / 100;

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

    osd->channel_logo_xpos = osd->strings[ OSD_TIME_STRING ].xpos;
    osd->channel_logo_ypos = osd->strings[ OSD_TIME_STRING ].ypos + osd_string_get_height( osd->strings[ OSD_TIME_STRING ].string ) + 2;

    osd->film_logo_xpos = osd->channel_logo_xpos - ( osd->channel_logo ? osd_animation_get_width( osd->channel_logo ) : 0 );
    osd->film_logo_ypos = osd->channel_logo_ypos;

    osd_string_set_colour_rgb( osd->strings[ OSD_NETWORK_NAME ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_NETWORK_NAME ].string, 1 );
    osd->strings[ OSD_NETWORK_NAME ].xpos = osd->strings[ OSD_TIME_STRING ].xpos;
    if( osd->channel_logo ) {
        osd->strings[ OSD_NETWORK_NAME ].ypos = osd->channel_logo_ypos + osd_animation_get_height( osd->channel_logo );
    } else {
        osd->strings[ OSD_NETWORK_NAME ].ypos = osd->strings[ OSD_TIME_STRING ].ypos + osd_string_get_height( osd->strings[ OSD_TIME_STRING ].string );
    }

    osd_string_set_colour_rgb( osd->strings[ OSD_NETWORK_CALL ].string,
                               (other_rgb >> 16) & 0xff, 
                               (other_rgb >> 8) & 0xff, (other_rgb & 0xff) );
    osd_string_show_border( osd->strings[ OSD_NETWORK_CALL ].string, 1 );
    osd->strings[ OSD_NETWORK_CALL ].xpos = osd->strings[ OSD_TIME_STRING ].xpos;
    osd->strings[ OSD_NETWORK_CALL ].ypos = osd->strings[ OSD_NETWORK_NAME ].ypos + osd_string_get_height( osd->strings[ OSD_MUTED ].string );

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
    if( osd->channel_logo ) osd_animation_delete( osd->channel_logo );
    if( osd->film_logo ) osd_animation_delete( osd->film_logo );
    osd_rect_delete( osd->databarbg );
    osd_rect_delete( osd->databar );
    osd_font_delete( osd->smallfont );
    osd_font_delete( osd->medfont );
    osd_font_delete( osd->bigfont );
    free( osd );
}

void tvtime_osd_set_pixel_aspect( tvtime_osd_t *osd, double pixel_aspect )
{
    int i;

    osd_font_set_pixel_aspect( osd->smallfont, pixel_aspect );
    osd_font_set_pixel_aspect( osd->medfont, pixel_aspect );
    osd_font_set_pixel_aspect( osd->bigfont, pixel_aspect );

    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        osd_string_rerender( osd->strings[ i ].string );
    }
}

void tvtime_osd_hold( tvtime_osd_t *osd, int hold )
{
    int i;

    osd->hold = hold;
    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        osd_string_set_hold( osd->strings[ i ].string, hold );
    }
}

void tvtime_osd_set_norm( tvtime_osd_t *osd, const char *norm )
{
    snprintf( osd->tv_norm_text, sizeof( osd->tv_norm_text ), "%s", norm );
}

void tvtime_osd_set_input( tvtime_osd_t *osd, const char *norm )
{
    snprintf( osd->input_text, sizeof( osd->input_text ), "%s", norm );
}

void tvtime_osd_set_freq_table( tvtime_osd_t *osd, const char *freqtable )
{
    snprintf( osd->freqtable_text, sizeof( osd->freqtable_text ), "%s", freqtable );
}

void tvtime_osd_set_audio_mode( tvtime_osd_t *osd, const char *audiomode )
{
    snprintf( osd->audiomode_text, sizeof( osd->audiomode_text ), audiomode );
}

void tvtime_osd_set_channel_number( tvtime_osd_t *osd, const char *text )
{
    snprintf( osd->channel_number_text, sizeof( osd->channel_number_text ), "%s", text );
}

void tvtime_osd_set_channel_name( tvtime_osd_t *osd, const char *text )
{
    snprintf( osd->channel_name_text, sizeof( osd->channel_name_text ), "%s", text );
}

void tvtime_osd_set_deinterlace_method( tvtime_osd_t *osd, const char *method )
{
    snprintf( osd->deinterlace_text, sizeof( osd->deinterlace_text ), "%s", method );
}

void tvtime_osd_set_film_mode( tvtime_osd_t *osd, int mode )
{
    osd->film_mode = mode;
    if( osd->film_logo ) osd_animation_pause( osd->film_logo, mode > 0 ? 0 : 1 );
}

void tvtime_osd_set_pulldown( tvtime_osd_t *osd, int mode )
{
    osd->pulldown_mode = mode;
    if( osd->film_logo ) {
        if( mode ) {
            osd_animation_set_timeout( osd->film_logo, osd_string_get_frames_left( osd->strings[ OSD_TIME_STRING ].string ) );
        } else {
            osd_animation_set_timeout( osd->film_logo, 0 );
        }
    }
}

void tvtime_osd_set_timeformat( tvtime_osd_t *osd, const char *format )
{
    snprintf( osd->timeformat, sizeof( osd->timeformat ), "%s", format );
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

void tvtime_osd_set_hold_message( tvtime_osd_t* osd, const char *str )
{
    snprintf( osd->hold_message, sizeof( osd->hold_message ), "%s", str );
}

void tvtime_osd_set_network_name( tvtime_osd_t* osd, const char *str )
{
    snprintf( osd->network_name, sizeof( osd->network_name ), "%s", str );
    osd_string_show_text( osd->strings[ OSD_NETWORK_NAME ].string, osd->network_name,
                          osd_string_get_frames_left( osd->strings[ OSD_NETWORK_NAME ].string ) );
}

void tvtime_osd_set_show_name( tvtime_osd_t* osd, const char *str )
{
    osd->show_name = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_NAME ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_NAME ].string ) );
}

void tvtime_osd_set_network_call( tvtime_osd_t* osd, const char *str )
{
    snprintf( osd->network_call, sizeof( osd->network_call ), "%s", str );
    osd_string_show_text( osd->strings[ OSD_NETWORK_CALL ].string, osd->network_call, 
                          osd_string_get_frames_left( osd->strings[ OSD_NETWORK_CALL ].string ) );
}

void tvtime_osd_set_show_rating( tvtime_osd_t* osd, const char *str )
{
    osd->show_rating = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_RATING ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_RATING ].string ) );
}

void tvtime_osd_set_show_start( tvtime_osd_t* osd, const char *str )
{
    osd->show_start = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_START ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_START ].string ) );
}

void tvtime_osd_set_show_length( tvtime_osd_t* osd, const char *str )
{
    osd->show_length = str;
    osd_string_show_text( osd->strings[ OSD_SHOW_LENGTH ].string, str, 
                          osd_string_get_frames_left( osd->strings[ OSD_SHOW_LENGTH ].string ) );
}

void tvtime_osd_show_info( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    char text[ 200 ];
    int delay = OSD_FADE_DELAY;
    struct tm *curtime = localtime( &tm );
    struct timeval tv;
    int i;

    strftime( timestamp, 50, osd->timeformat, curtime );
    gettimeofday( &tv, 0 );
    osd_animation_seek( osd->channel_logo, ((double) tv.tv_usec) / (1000.0 * 1000.0) );
    /* Yes, we're showing the name in the NUM spot and the number in the NAME spot. */
    /* I will fix this up when I get a chance. -Billy */
    if( strcmp( osd->channel_number_text, osd->channel_name_text ) ) {
        osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, osd->channel_name_text, delay );
    } else {
        osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, "", delay );
    }
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NAME ].string, osd->channel_number_text, delay );
    osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, timestamp, delay );

    if( strlen( osd->freqtable_text ) ) {
        snprintf( text, sizeof( text ), "%s: %s [%s]",
                  osd->input_text, osd->freqtable_text, osd->audiomode_text );
    } else {
        snprintf( text, sizeof( text ), "%s", osd->input_text );
    }
    osd_string_show_text( osd->strings[ OSD_TUNER_INFO ].string, text, delay );

/*
    if( *(osd->hold_message) ) {
        snprintf( text, sizeof( text ), "%s", osd->hold_message );
    } else {
        const char *rate_mode = "";
        const char *film_mode = "";
        if( osd->framerate_mode == FRAMERATE_HALF_TFF ) {
            rate_mode = " TFF";
        } else if( osd->framerate_mode == FRAMERATE_HALF_BFF ) {
            rate_mode = " BFF";
        }
        switch( osd->film_mode ) {
            case 0: film_mode = ", DFilm"; break;
            case PULLDOWN_SEQ_AA: film_mode = ", Film1"; break;
            case PULLDOWN_SEQ_AB: film_mode = ", Film2"; break;
            case PULLDOWN_SEQ_BC: film_mode = ", Film3"; break;
            case PULLDOWN_SEQ_CC: film_mode = ", Film4"; break;
            case PULLDOWN_SEQ_DD: film_mode = ", Film5"; break;
            default: film_mode = ""; break;
        }
        snprintf( text, sizeof( text ), "%s - %s [%.2ffps%s%s]", osd->input_text,
                  osd->deinterlace_text, osd->framerate, rate_mode, film_mode );
    }
    osd_string_show_text( osd->strings[ OSD_DATA_BAR ].string, text, delay );
    osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );
    osd_rect_set_timeout( osd->databar, 0 );
    osd_rect_set_timeout( osd->databarbg, 0 );
*/

    /* Billy: What's up?  Are we ditching the logo for XDS? */
    if( osd->channel_logo ) osd_animation_set_timeout( osd->channel_logo, delay );
    if( osd->film_logo && osd->pulldown_mode ) osd_animation_set_timeout( osd->film_logo, delay );

    for( i = OSD_NETWORK_NAME; i <= OSD_SHOW_LENGTH; i++ ) {
        osd_string_set_timeout( osd->strings[ i ].string, delay );
    }
    osd_string_set_timeout( osd->strings[ OSD_MUTED ].string, osd->mutestate ? delay : 0 );
}

int tvtime_osd_data_bar_visible( tvtime_osd_t *osd )
{
    return osd_string_visible( osd->strings[ OSD_DATA_BAR ].string );
}

void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage )
{
    if( !*(osd->hold_message ) ) {
        char bar[ 108 ];
        int maxwidth;

        sprintf( bar, "%s (%d) ", barname, percentage );
        osd_string_show_text( osd->strings[ OSD_DATA_BAR ].string, bar, OSD_FADE_DELAY );
        osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );

        maxwidth = osd->databar_xend - (osd->strings[ OSD_DATA_BAR ].xpos + osd_string_get_width( osd->strings[ OSD_DATA_BAR ].string ));
        osd->databar_xstart = osd->strings[ OSD_DATA_BAR ].xpos + osd_string_get_width( osd->strings[ OSD_DATA_BAR ].string );
        osd->databar_ypos = osd->strings[ OSD_DATA_BAR ].ypos + 4;
        osd_rect_set_colour( osd->databar, 255, 255, 128, 128 );
        osd_rect_set_size( osd->databar, (maxwidth * percentage) / 100, 18 );
        osd_rect_set_timeout( osd->databar, OSD_FADE_DELAY );
        osd_rect_set_colour( osd->databarbg, 80, 80, 40, 40 );
        osd_rect_set_size( osd->databarbg, maxwidth, 18 );
        osd_rect_set_timeout( osd->databarbg, OSD_FADE_DELAY );
    }
}

void tvtime_osd_show_message( tvtime_osd_t *osd, const char *message )
{
    if( !*(osd->hold_message) ) {
        osd_string_show_text( osd->strings[ OSD_DATA_BAR ].string, message, OSD_FADE_DELAY );
        osd_string_set_timeout( osd->strings[ OSD_VOLUME_BAR ].string, 0 );
        osd_rect_set_timeout( osd->databar, 0 );
        osd_rect_set_timeout( osd->databarbg, 0 );
    }
}

void tvtime_osd_show_channel_logo( tvtime_osd_t *osd )
{
    if( osd->channel_logo ) {
        osd_animation_set_timeout( osd->channel_logo, OSD_FADE_DELAY );
    }
}

void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage )
{
    if( !*(osd->hold_message ) ) {
        char bar[ 108 ];
        int maxwidth;

        sprintf( bar, "Volume (%d) ", percentage );
        osd_string_show_text( osd->strings[ OSD_VOLUME_BAR ].string, bar, OSD_FADE_DELAY );
        osd_string_set_timeout( osd->strings[ OSD_DATA_BAR ].string, 0 );

        maxwidth = osd->databar_xend - (osd->strings[ OSD_VOLUME_BAR ].xpos + osd_string_get_width( osd->strings[ OSD_VOLUME_BAR ].string ));
        osd->databar_xstart = osd->strings[ OSD_VOLUME_BAR ].xpos + osd_string_get_width( osd->strings[ OSD_VOLUME_BAR ].string );
        osd->databar_ypos = osd->strings[ OSD_VOLUME_BAR ].ypos + 4;
        osd_rect_set_colour( osd->databar, 255, 255, 128, 128 );
        osd_rect_set_size( osd->databar, (maxwidth * percentage) / 100, 18 );
        osd_rect_set_timeout( osd->databar, OSD_FADE_DELAY );
        osd_rect_set_colour( osd->databarbg, 80, 80, 40, 40 );
        osd_rect_set_size( osd->databarbg, maxwidth, 18 );
        osd_rect_set_timeout( osd->databarbg, OSD_FADE_DELAY );
    }
}

void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate )
{
    osd->mutestate = mutestate;
    osd_string_set_timeout( osd->strings[ OSD_MUTED ].string, osd->mutestate ? 100 : 0 );
}

void tvtime_osd_show_list( tvtime_osd_t *osd, int showlist )
{
    if( showlist ) {
        osd_list_set_timeout( osd->list, MENU_FADE_DELAY );
    } else {
        osd_list_set_timeout( osd->list, 0 );
    }
}

int tvtime_osd_list_get_hilight( tvtime_osd_t *osd )
{
    return osd_list_get_hilight( osd->list );
}

int tvtime_osd_list_get_numlines( tvtime_osd_t *osd )
{
    return osd_list_get_numlines( osd->list );
}

void tvtime_osd_list_set_hilight( tvtime_osd_t *osd, int pos )
{
    osd_list_set_hilight( osd->list, pos );
}

void tvtime_osd_list_set_text( tvtime_osd_t *osd, int line, const char *text )
{
    osd_list_set_text( osd->list, line, text );
}

void tvtime_osd_list_set_lines( tvtime_osd_t *osd, int numlines )
{
    osd_list_set_lines( osd->list, numlines );
}

void tvtime_osd_advance_frame( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    int chinfo_left = 0;
    int i;

    if( (chinfo_left = osd_string_get_frames_left( osd->strings[ OSD_TIME_STRING ].string)) ) {
        struct tm *curtime = localtime( &tm );
        struct timeval tv;

        strftime( timestamp, 50, osd->timeformat, curtime );
        osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, timestamp, chinfo_left );
        gettimeofday( &tv, 0 );
        osd_animation_seek( osd->channel_logo, ((double) tv.tv_usec) / (1000.0 * 1000.0) );
    }

    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        osd_string_advance_frame( osd->strings[ i ].string );
    }

    if( osd->channel_logo ) osd_animation_advance_frame( osd->channel_logo );
    if( osd->film_logo ) osd_animation_advance_frame( osd->film_logo );
    osd_list_advance_frame( osd->list );
    osd_rect_advance_frame( osd->databar );
    osd_rect_advance_frame( osd->databarbg );
}

void tvtime_osd_composite_packed422_scanline( tvtime_osd_t *osd,
                                              uint8_t *output,
                                              int width, int xpos,
                                              int scanline )
{
    int i;

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

    if( osd_rect_visible( osd->databar ) && ( scanline >= osd->databar_ypos ) ) {
        int startx;
        int strx = 0;

        startx = osd->databar_xstart - xpos;

        if( startx < 0 ) {
            strx = -startx;
            startx = 0;
        }

        /* Make sure we start somewhere even. */
        startx = startx & ~1;

        osd_rect_composite_packed422_scanline( osd->databarbg, output + (startx*2),
                                               output + (startx*2), width - startx,
                                               strx, scanline - osd->strings[ OSD_VOLUME_BAR ].ypos );
        osd_rect_composite_packed422_scanline( osd->databar, output + (startx*2),
                                               output + (startx*2), width - startx,
                                               strx, scanline - osd->strings[ OSD_VOLUME_BAR ].ypos );
    }

    if( osd->channel_logo ) {
        if( osd_animation_visible( osd->channel_logo ) ) {
            if( scanline >= osd->channel_logo_ypos &&
                scanline < osd->channel_logo_ypos + osd_animation_get_height( osd->channel_logo ) ) {

                int startx = osd->channel_logo_xpos - osd_animation_get_width( osd->channel_logo ) - xpos;
                int strx = 0;
                if( startx < 0 ) {
                    strx = -startx;
                    startx = 0;
                }
                if( startx < width ) {
                    osd_animation_composite_packed422_scanline( osd->channel_logo,
                                                                output + (startx*2),
                                                                output + (startx*2),
                                                                width - startx,
                                                                strx,
                                                                scanline - osd->channel_logo_ypos );
                }
            }
        }
    }

    if( osd->film_logo ) {
        if( osd_animation_visible( osd->film_logo ) ) {
            if( scanline >= osd->film_logo_ypos &&
                scanline < osd->film_logo_ypos + osd_animation_get_height( osd->film_logo ) ) {

                int startx = osd->film_logo_xpos - osd_animation_get_width( osd->film_logo ) - xpos;
                int strx = 0;
                if( startx < 0 ) {
                    strx = -startx;
                    startx = 0;
                }
                if( startx < width ) {
                    osd_animation_composite_packed422_scanline( osd->film_logo,
                                                                output + (startx*2),
                                                                output + (startx*2),
                                                                width - startx,
                                                                strx,
                                                                scanline - osd->film_logo_ypos );
                }
            }
        }
    }


    if( osd_list_visible( osd->list ) && scanline >= osd->listpos_y ) {
        int listpos_x, startx;
        int strx = 0;

        listpos_x = osd->listpos_x - (osd_list_get_width( osd->list ) / 2);
        startx = listpos_x - xpos;

        if( startx < 0 ) {
            strx = -startx;
            startx = 0;
        }

        /* Make sure we start somewhere even. */
        startx = startx & ~1;

        if( startx < width ) {
            osd_list_composite_packed422_scanline( osd->list,
                                                   output + (startx*2),
                                                   output + (startx*2),
                                                   width - startx,
                                                   strx,
                                                   scanline - osd->listpos_y );
        }
    }
}

