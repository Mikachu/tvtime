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
    int align;
} string_object_t;

enum osd_align
{
    OSD_LEFT,
    OSD_RIGHT,
    OSD_CENTRE
};

enum osd_objects
{
    OSD_CHANNEL_NUM = 0,
    OSD_CHANNEL_NAME,
    OSD_INPUT_NAME,
    OSD_TUNER_INFO,
    OSD_SIGNAL_INFO,
    OSD_MESSAGE1_BAR,
    OSD_MESSAGE2_BAR,
    OSD_DATA_VALUE,
    OSD_MUTED,
    OSD_TIME_STRING,
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

    int margin_left;
    int margin_right;
    int margin_top;
    int margin_bottom;
};

const int top_size = 7;
const int left_size = 7;
const int bottom_size = 13;

const int small_size_576 = 18;
const int med_size_576 = 30;
const int big_size_576 = 80;

tvtime_osd_t *tvtime_osd_new( int width, int height, double pixel_aspect,
                              unsigned int channel_rgb, unsigned int other_rgb )
{
    int channel_r = (channel_rgb >> 16) & 0xff;
    int channel_g = (channel_rgb >>  8) & 0xff;
    int channel_b = (channel_rgb      ) & 0xff;
    int other_r = (other_rgb >> 16) & 0xff;
    int other_g = (other_rgb >>  8) & 0xff;
    int other_b = (other_rgb      ) & 0xff;
    int smallsize, medsize, bigsize;
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

    osd->margin_left = ((width * left_size) / 100) & ~1;
    osd->margin_right = (width - ((width * left_size) / 100)) & ~1;
    osd->margin_top = (height * top_size) / 100;
    osd->margin_bottom = height - osd->margin_top;

    osd->listpos_x = width / 2;
    osd->listpos_y = (height * 30) / 100;
    osd->databar_xend = osd->margin_right;

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

    if( height == 576 ) {
        smallsize = small_size_576;
        medsize = med_size_576;
        bigsize = big_size_576;
    } else {
        smallsize = (int) (((((double) small_size_576) / 576.0) * ((double) height)) + 0.5);
        medsize = (int) (((((double) med_size_576) / 576.0) * ((double) height)) + 0.5);
        bigsize = (int) (((((double) big_size_576) / 576.0) * ((double) height)) + 0.5);
    }

    osd->smallfont = osd_font_new( fontfile, smallsize, pixel_aspect );
    if( !osd->smallfont ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        free( osd );
        return 0;
    }
    osd->medfont = osd_font_new( fontfile, medsize, pixel_aspect );
    if( !osd->medfont ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        osd_font_delete( osd->smallfont );
        free( osd );
        return 0;
    }
    osd->bigfont = osd_font_new( fontfile, bigsize, pixel_aspect );
    if( !osd->bigfont ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        osd_font_delete( osd->smallfont );
        osd_font_delete( osd->medfont );
        free( osd );
        return 0;
    }

    osd->list = osd_list_new( osd->smallfont, pixel_aspect );
    if( !osd->list ) {
        osd_rect_delete( osd->databarbg );
        osd_rect_delete( osd->databar );
        osd_font_delete( osd->smallfont );
        osd_font_delete( osd->medfont );
        osd_font_delete( osd->bigfont );
        free( osd );
        return 0;
    }

    osd->strings[ OSD_CHANNEL_NUM ].string = osd_string_new( osd->bigfont );
    osd->strings[ OSD_CHANNEL_NAME ].string = osd_string_new( osd->medfont );
    osd->strings[ OSD_TIME_STRING ].string = osd_string_new( osd->medfont );
    osd->strings[ OSD_INPUT_NAME ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_TUNER_INFO ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_SIGNAL_INFO ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_MESSAGE1_BAR ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_MESSAGE2_BAR ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_DATA_VALUE ].string = osd_string_new( osd->smallfont );
    osd->strings[ OSD_MUTED ].string = osd_string_new( osd->smallfont );

    /* We create the logos, but it's ok if they fail to load. */
    osd->channel_logo = 0; // osd_animation_new( logofile, pixel_aspect, 256, 1 );
    osd->film_logo = 0; // osd_animation_new( "filmstrip", pixel_aspect, 256, 2 );

    if( !osd->strings[ OSD_CHANNEL_NUM ].string || !osd->strings[ OSD_TIME_STRING ].string ||
        !osd->strings[ OSD_INPUT_NAME ].string ||
        !osd->strings[ OSD_TUNER_INFO ].string || !osd->strings[ OSD_SIGNAL_INFO ].string ||
        !osd->strings[ OSD_MESSAGE1_BAR ].string || !osd->strings[ OSD_MESSAGE2_BAR ].string ||
        !osd->strings[ OSD_DATA_VALUE ].string || !osd->strings[ OSD_MUTED ].string ) {

        fprintf( stderr, "tvtimeosd: Can't create all OSD objects.\n" );
        tvtime_osd_delete( osd );
        return 0;
    }
    if( osd->channel_logo ) osd_animation_pause( osd->channel_logo, 1 );

    /* Left top. */
    osd_string_set_colour_rgb( osd->strings[ OSD_CHANNEL_NUM ].string, channel_r, channel_g, channel_b );
    osd_string_show_border( osd->strings[ OSD_CHANNEL_NUM ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, "000", 0 );
    osd->strings[ OSD_CHANNEL_NUM ].align = OSD_LEFT;
    osd->strings[ OSD_CHANNEL_NUM ].xpos = osd->margin_left;
    osd->strings[ OSD_CHANNEL_NUM ].ypos = osd->margin_top;

    osd_string_set_colour_rgb( osd->strings[ OSD_CHANNEL_NAME ].string, channel_r, channel_g, channel_b );
    osd_string_show_border( osd->strings[ OSD_CHANNEL_NAME ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_CHANNEL_NAME ].string, "gY", 0 );
    osd->strings[ OSD_CHANNEL_NAME ].align = OSD_CENTRE;
    osd->strings[ OSD_CHANNEL_NAME ].xpos = width / 2;
    osd->strings[ OSD_CHANNEL_NAME ].ypos = osd->margin_top;

    osd_string_set_colour_rgb( osd->strings[ OSD_SIGNAL_INFO ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_SIGNAL_INFO ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_SIGNAL_INFO ].string, "No signal", 0 );
    osd_string_set_hold( osd->strings[ OSD_SIGNAL_INFO ].string, 1 );
    osd->strings[ OSD_SIGNAL_INFO ].align = OSD_CENTRE;
    osd->strings[ OSD_SIGNAL_INFO ].xpos = width / 2;
    osd->strings[ OSD_SIGNAL_INFO ].ypos = osd->strings[ OSD_CHANNEL_NAME ].ypos
                                         + osd_string_get_height( osd->strings[ OSD_CHANNEL_NAME ].string );


    /* Right top. */
    osd_string_set_colour_rgb( osd->strings[ OSD_TIME_STRING ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_TIME_STRING ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, "8", 0 );
    osd->strings[ OSD_TIME_STRING ].align = OSD_RIGHT;
    osd->strings[ OSD_TIME_STRING ].xpos = osd->margin_right;
    osd->strings[ OSD_TIME_STRING ].ypos = osd->margin_top;

    osd_string_set_colour_rgb( osd->strings[ OSD_INPUT_NAME ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_INPUT_NAME ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_INPUT_NAME ].string, "0g", 0 );
    osd->strings[ OSD_INPUT_NAME ].align = OSD_RIGHT;
    osd->strings[ OSD_INPUT_NAME ].xpos = osd->margin_right;
    osd->strings[ OSD_INPUT_NAME ].ypos = osd->strings[ OSD_TIME_STRING ].ypos
                                        + osd_string_get_height( osd->strings[ OSD_TIME_STRING ].string );

    osd_string_set_colour_rgb( osd->strings[ OSD_TUNER_INFO ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_TUNER_INFO ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_TUNER_INFO ].string, "0", 0 );
    osd->strings[ OSD_TUNER_INFO ].align = OSD_RIGHT;
    osd->strings[ OSD_TUNER_INFO ].xpos = osd->margin_right;
    osd->strings[ OSD_TUNER_INFO ].ypos = osd->strings[ OSD_INPUT_NAME ].ypos
                                        + osd_string_get_height( osd->strings[ OSD_INPUT_NAME ].string );


    /* Bottom. */
    osd_string_set_colour_rgb( osd->strings[ OSD_MESSAGE2_BAR ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_MESSAGE2_BAR ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_MESSAGE2_BAR ].string, "8", 0 );
    osd->strings[ OSD_MESSAGE2_BAR ].align = OSD_LEFT;
    osd->strings[ OSD_MESSAGE2_BAR ].xpos = osd->margin_left;
    osd->strings[ OSD_MESSAGE2_BAR ].ypos = osd->margin_bottom
                                          - osd_string_get_height( osd->strings[ OSD_MESSAGE2_BAR ].string );


    osd_string_set_colour_rgb( osd->strings[ OSD_DATA_VALUE ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_DATA_VALUE ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_DATA_VALUE ].string, "8", 0 );
    osd->strings[ OSD_DATA_VALUE ].align = OSD_RIGHT;
    osd->strings[ OSD_DATA_VALUE ].xpos = osd->margin_right;
    osd->strings[ OSD_DATA_VALUE ].ypos = osd->margin_bottom
                                        - osd_string_get_height( osd->strings[ OSD_DATA_VALUE ].string );


    osd_string_set_colour_rgb( osd->strings[ OSD_MESSAGE1_BAR ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_MESSAGE1_BAR ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_MESSAGE1_BAR ].string, "8", 0 );
    osd->strings[ OSD_MESSAGE1_BAR ].align = OSD_LEFT;
    osd->strings[ OSD_MESSAGE1_BAR ].xpos = osd->margin_left;
    osd->strings[ OSD_MESSAGE1_BAR ].ypos = osd->strings[ OSD_MESSAGE2_BAR ].ypos
                                          - osd_string_get_height( osd->strings[ OSD_MESSAGE1_BAR ].string );


    osd_string_set_colour_rgb( osd->strings[ OSD_MUTED ].string, other_r, other_g, other_b );
    osd_string_show_border( osd->strings[ OSD_MUTED ].string, 1 );
    osd_string_show_text( osd->strings[ OSD_MUTED ].string, "Mute", 0 );
    osd->strings[ OSD_MUTED ].align = OSD_LEFT;
    osd->strings[ OSD_MUTED ].xpos = osd->margin_left;
    osd->strings[ OSD_MUTED ].ypos = osd->strings[ OSD_MESSAGE1_BAR ].ypos
                                   - osd_string_get_height( osd->strings[ OSD_MESSAGE2_BAR ].string );


    /* Images. */
    osd->channel_logo_xpos = osd->margin_right;
    osd->channel_logo_ypos = osd->strings[ OSD_TUNER_INFO ].ypos
                           + osd_string_get_height( osd->strings[ OSD_TUNER_INFO ].string ) + 2;

    if( osd->channel_logo ) {
        osd->film_logo_xpos = osd->channel_logo_xpos - osd_animation_get_width( osd->channel_logo );
    } else {
        osd->film_logo_xpos = osd->margin_right;
    }
    osd->film_logo_ypos = osd->channel_logo_ypos;

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
    osd_list_delete( osd->list );
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
        if( i != OSD_MESSAGE1_BAR && i != OSD_MESSAGE2_BAR ) {
            osd_string_set_hold( osd->strings[ i ].string, hold );
        }
    }
    if( osd->channel_logo ) osd_animation_set_hold( osd->channel_logo, hold );
    if( osd->film_logo ) osd_animation_set_hold( osd->film_logo, hold );
}

void tvtime_osd_clear( tvtime_osd_t *osd )
{
    int i;

    for( i = 0; i < OSD_MAX_STRING_OBJECTS; i++ ) {
        osd_string_set_timeout( osd->strings[ i ].string, 0 );
    }
    if( osd->channel_logo ) osd_animation_set_timeout( osd->channel_logo, 0 );
    if( osd->film_logo ) osd_animation_set_timeout( osd->film_logo, 0 );
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
}

void tvtime_osd_set_show_name( tvtime_osd_t* osd, const char *str )
{
    osd->show_name = str;
}

void tvtime_osd_set_network_call( tvtime_osd_t* osd, const char *str )
{
    snprintf( osd->network_call, sizeof( osd->network_call ), "%s", str );
}

void tvtime_osd_set_show_rating( tvtime_osd_t* osd, const char *str )
{
    osd->show_rating = str;
}

void tvtime_osd_set_show_start( tvtime_osd_t* osd, const char *str )
{
    osd->show_start = str;
}

void tvtime_osd_set_show_length( tvtime_osd_t* osd, const char *str )
{
    osd->show_length = str;
}

void tvtime_osd_show_info( tvtime_osd_t *osd )
{
    char timestamp[ 50 ];
    time_t tm = time( 0 );
    char text[ 200 ];
    int delay = OSD_FADE_DELAY;
    struct tm *curtime = localtime( &tm );
    struct timeval tv;

    strftime( timestamp, 50, osd->timeformat, curtime );
    gettimeofday( &tv, 0 );
    if( osd->channel_logo ) osd_animation_seek( osd->channel_logo, ((double) tv.tv_usec) / (1000.0 * 1000.0) );

    osd_string_show_text( osd->strings[ OSD_CHANNEL_NUM ].string, osd->channel_number_text, delay );
    if( strcmp( osd->channel_number_text, osd->channel_name_text ) ) {
        osd_string_show_text( osd->strings[ OSD_CHANNEL_NAME ].string, osd->channel_name_text, delay );
    } else {
        osd_string_show_text( osd->strings[ OSD_CHANNEL_NAME ].string, "", delay );
    }
    osd_string_show_text( osd->strings[ OSD_TIME_STRING ].string, timestamp, delay );

    if( strlen( osd->freqtable_text ) ) {
        snprintf( text, sizeof( text ), "%s", osd->freqtable_text );
        osd_string_show_text( osd->strings[ OSD_TUNER_INFO ].string, text, delay );
    } else {
        osd_string_set_timeout( osd->strings[ OSD_TUNER_INFO ].string, 0 );
    }
    snprintf( text, sizeof( text ), "%s", osd->input_text );
    osd_string_show_text( osd->strings[ OSD_INPUT_NAME ].string, text, delay );

    if( *(osd->hold_message) ) {
        snprintf( text, sizeof( text ), "%s", osd->hold_message );
        osd_string_show_text( osd->strings[ OSD_MESSAGE1_BAR ].string, text, delay );
        osd_string_set_timeout( osd->strings[ OSD_MESSAGE2_BAR ].string, 0 );
        osd_string_set_timeout( osd->strings[ OSD_DATA_VALUE ].string, 0 );
        osd_rect_set_timeout( osd->databar, 0 );
        osd_rect_set_timeout( osd->databarbg, 0 );
    }

    /* Billy: What's up?  Are we ditching the logo for XDS? */
    if( osd->channel_logo ) osd_animation_set_timeout( osd->channel_logo, delay );
    if( osd->film_logo && osd->pulldown_mode ) osd_animation_set_timeout( osd->film_logo, delay );

    osd_string_set_timeout( osd->strings[ OSD_MUTED ].string, osd->mutestate ? delay : 0 );
}

int tvtime_osd_data_bar_visible( tvtime_osd_t *osd )
{
    return osd_string_visible( osd->strings[ OSD_MESSAGE1_BAR ].string );
}

void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage )
{
    if( !*(osd->hold_message ) ) {
        char bar[ 108 ];
        int maxwidth;

        sprintf( bar, "%s", barname );
        osd_string_show_text( osd->strings[ OSD_MESSAGE1_BAR ].string, bar, OSD_FADE_DELAY );
        osd_string_set_timeout( osd->strings[ OSD_MESSAGE2_BAR ].string, 0 );

        sprintf( bar, " %d", percentage );
        osd_string_show_text( osd->strings[ OSD_DATA_VALUE ].string, bar, OSD_FADE_DELAY );

        osd->databar_xstart = osd->strings[ OSD_MESSAGE2_BAR ].xpos;
        osd->databar_ypos = osd->strings[ OSD_MESSAGE2_BAR ].ypos + 4;
        maxwidth = osd->databar_xend
                 - osd_string_get_width( osd->strings[ OSD_DATA_VALUE ].string ) - osd->databar_xstart;
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
        osd_string_show_text( osd->strings[ OSD_MESSAGE1_BAR ].string, message, OSD_FADE_DELAY );
        osd_string_set_timeout( osd->strings[ OSD_MESSAGE2_BAR ].string, 0 );
        osd_string_set_timeout( osd->strings[ OSD_DATA_VALUE ].string, 0 );
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

void tvtime_osd_list_hold( tvtime_osd_t *osd, int hold )
{
    osd_list_set_hold( osd->list, hold );
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
        if( osd->channel_logo ) osd_animation_seek( osd->channel_logo, ((double) tv.tv_usec) / (1000.0 * 1000.0) );
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

                if( osd->strings[ i ].align == OSD_RIGHT ) {
                    startx = osd->strings[ i ].xpos - osd_string_get_width( osd->strings[ i ].string ) - xpos;
                } else if( osd->strings[ i ].align == OSD_CENTRE ) {
                    startx = osd->strings[ i ].xpos - (osd_string_get_width( osd->strings[ i ].string ) / 2) - xpos;
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
                                               strx, scanline - osd->strings[ OSD_MESSAGE2_BAR ].ypos );
        osd_rect_composite_packed422_scanline( osd->databar, output + (startx*2),
                                               output + (startx*2), width - startx,
                                               strx, scanline - osd->strings[ OSD_MESSAGE2_BAR ].ypos );
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

