/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>.
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "station.h"
#include "mixer.h"
#include "input.h"
#include "commands.h"
#include "console.h"

#define NUM_FAVORITES 9
#define MAX_USER_MENUS 64

typedef struct command_names_s {
    const char *name;
    int command;
} command_names_t;

static command_names_t command_table[] = {

    { "AUTO_ADJUST_PICT", TVTIME_AUTO_ADJUST_PICT },
    { "AUTO_ADJUST_WINDOW", TVTIME_AUTO_ADJUST_WINDOW },

    { "BRIGHTNESS_DOWN", TVTIME_BRIGHTNESS_DOWN },
    { "BRIGHTNESS_UP", TVTIME_BRIGHTNESS_UP },

    { "CHANNEL_1", TVTIME_CHANNEL_1 },
    { "CHANNEL_2", TVTIME_CHANNEL_2 },
    { "CHANNEL_3", TVTIME_CHANNEL_3 },
    { "CHANNEL_4", TVTIME_CHANNEL_4 },
    { "CHANNEL_5", TVTIME_CHANNEL_5 },
    { "CHANNEL_6", TVTIME_CHANNEL_6 },
    { "CHANNEL_7", TVTIME_CHANNEL_7 },
    { "CHANNEL_8", TVTIME_CHANNEL_8 },
    { "CHANNEL_9", TVTIME_CHANNEL_9 },
    { "CHANNEL_0", TVTIME_CHANNEL_0 },

    { "CHANNEL_ACTIVATE_ALL", TVTIME_CHANNEL_ACTIVATE_ALL },
    { "CHANNEL_DEC", TVTIME_CHANNEL_DEC },
    { "CHANNEL_DOWN", TVTIME_CHANNEL_DEC },
    { "CHANNEL_INC", TVTIME_CHANNEL_INC },
    { "CHANNEL_PREV", TVTIME_CHANNEL_PREV },
    { "CHANNEL_RENUMBER", TVTIME_CHANNEL_RENUMBER },
    { "CHANNEL_SAVE_TUNING", TVTIME_CHANNEL_SAVE_TUNING },
    { "CHANNEL_SCAN", TVTIME_CHANNEL_SCAN },
    { "CHANNEL_SKIP", TVTIME_CHANNEL_SKIP },
    { "CHANNEL_UP", TVTIME_CHANNEL_INC },

    { "COLOUR_DOWN", TVTIME_COLOUR_DOWN },
    { "COLOUR_UP", TVTIME_COLOUR_UP },

    { "COLOR_DOWN", TVTIME_COLOUR_DOWN },
    { "COLOR_UP", TVTIME_COLOUR_UP },

    { "CONTRAST_DOWN", TVTIME_CONTRAST_DOWN },
    { "CONTRAST_UP", TVTIME_CONTRAST_UP },

    { "DISPLAY_INFO", TVTIME_DISPLAY_INFO },
    { "DISPLAY_MESSAGE", TVTIME_DISPLAY_MESSAGE },

    { "ENTER", TVTIME_ENTER },

    { "FINETUNE_DOWN", TVTIME_FINETUNE_DOWN },
    { "FINETUNE_UP", TVTIME_FINETUNE_UP },

    { "HUE_DOWN", TVTIME_HUE_DOWN },
    { "HUE_UP", TVTIME_HUE_UP },

    { "LUMA_DOWN", TVTIME_LUMA_DOWN },
    { "LUMA_UP", TVTIME_LUMA_UP },

    { "MENU_DOWN", TVTIME_MENU_DOWN },
    { "MENU_ENTER", TVTIME_MENU_ENTER },
    { "MENU_EXIT", TVTIME_MENU_EXIT },
    { "MENU_LEFT", TVTIME_MENU_LEFT },
    { "MENU_RIGHT", TVTIME_MENU_RIGHT },
    { "MENU_UP", TVTIME_MENU_UP },

    { "MIXER_DOWN", TVTIME_MIXER_DOWN },
    { "MIXER_TOGGLE_MUTE", TVTIME_MIXER_TOGGLE_MUTE },
    { "MIXER_UP", TVTIME_MIXER_UP },

    { "OVERSCAN_DOWN", TVTIME_OVERSCAN_DOWN },
    { "OVERSCAN_UP", TVTIME_OVERSCAN_UP },

    { "RESTART", TVTIME_RESTART },

    { "SCREENSHOT", TVTIME_SCREENSHOT },
    { "SCROLL_CONSOLE_DOWN", TVTIME_SCROLL_CONSOLE_DOWN },
    { "SCROLL_CONSOLE_UP", TVTIME_SCROLL_CONSOLE_UP },

    { "SHOW_DEINTERLACER_INFO", TVTIME_SHOW_DEINTERLACER_INFO },
    { "SHOW_MENU", TVTIME_SHOW_MENU },
    { "SHOW_STATS", TVTIME_SHOW_STATS },

    { "TOGGLE_ALWAYSONTOP", TVTIME_TOGGLE_ALWAYSONTOP },
    { "TOGGLE_ASPECT", TVTIME_TOGGLE_ASPECT },
    { "TOGGLE_AUDIO_MODE", TVTIME_TOGGLE_AUDIO_MODE },
    { "TOGGLE_BARS", TVTIME_TOGGLE_BARS },
    { "TOGGLE_CC", TVTIME_TOGGLE_CC },
    { "TOGGLE_COMPATIBLE_NORM", TVTIME_TOGGLE_COMPATIBLE_NORM },
    { "TOGGLE_CONSOLE", TVTIME_TOGGLE_CONSOLE },
    /* { "TOGGLE_CREDITS", TVTIME_TOGGLE_CREDITS }, Disabled for 0.9.8 */
    { "TOGGLE_DEINTERLACER", TVTIME_TOGGLE_DEINTERLACER },
    { "TOGGLE_FULLSCREEN", TVTIME_TOGGLE_FULLSCREEN },
    { "TOGGLE_FRAMERATE", TVTIME_TOGGLE_FRAMERATE },
    { "TOGGLE_INPUT", TVTIME_TOGGLE_INPUT },
    { "TOGGLE_LUMA_CORRECTION", TVTIME_TOGGLE_LUMA_CORRECTION },
    { "TOGGLE_MATTE", TVTIME_TOGGLE_MATTE },
    { "TOGGLE_MODE", TVTIME_TOGGLE_MODE },
    { "TOGGLE_MUTE", TVTIME_TOGGLE_MUTE },
    { "TOGGLE_NTSC_CABLE_MODE", TVTIME_TOGGLE_NTSC_CABLE_MODE },
    { "TOGGLE_PAUSE", TVTIME_TOGGLE_PAUSE },
    { "TOGGLE_PULLDOWN_DETECTION", TVTIME_TOGGLE_PULLDOWN_DETECTION },

    { "QUIT", TVTIME_QUIT },
};

enum menu_type
{
    MENU_REDIRECT,
    MENU_FAVORITES,
    MENU_DEINTERLACER,
    MENU_USER
};

typedef struct menu_names_s {
    const char *name;
    int menutype;
    const char *dest;
} menu_names_t;

static menu_names_t menu_table[] = {
    { "root", MENU_REDIRECT, "root-tuner" },
    { "favorites", MENU_FAVORITES, 0 },
    { "deinterlacer", MENU_DEINTERLACER, 0 },
    { "color", MENU_REDIRECT, "colour" }
};

static int tvtime_num_builtin_menus( void )
{
    return ( sizeof( menu_table ) / sizeof( menu_names_t ) );
}

static void set_redirect( const char *menu, const char *dest )
{
    int i;

    for( i = 0; i < tvtime_num_builtin_menus(); i++ ) {
        if( !strcasecmp( menu, menu_table[ i ].name ) ) {
            menu_table[ i ].dest = dest;
            return;
        }
    }
}

int tvtime_num_commands( void )
{
    return ( sizeof( command_table ) / sizeof( command_names_t ) );
}

const char *tvtime_get_command( int pos )
{
    return command_table[ pos ].name;
}

int tvtime_get_command_id( int pos )
{
    return command_table[ pos ].command;
}

int tvtime_string_to_command( const char *str )
{
    if( str ) {
        int i;

        for( i = 0; i < tvtime_num_commands(); i++ ) {
            if( !strcasecmp( str, tvtime_get_command( i ) ) ) {
                return tvtime_get_command_id( i );
            }
        }
    }

    return TVTIME_NOCOMMAND;
}

const char *tvtime_command_to_string( int command )
{
    int i;

    for( i = 0; i < tvtime_num_commands(); i++ ) {
        if( tvtime_get_command_id( i ) == command ) {
            return tvtime_get_command( i );
        }
    }

    return "ERROR";
}

int tvtime_is_menu_command( int command )
{
    return (command >= TVTIME_MENU_UP);
}

struct commands_s {
    config_t *cfg;
    videoinput_t *vidin;
    tvtime_osd_t *osd;
    char next_chan_buffer[ 5 ];
    int frame_counter;
    int digit_counter;
    int quit;
    int inputnum;

    int displayinfo;
    int screenshot;
    int printdebug;
    int showbars;
    int showdeinterlacerinfo;
    int togglefullscreen;
    int toggleaspect;
    int togglealwaysontop;
    int toggledeinterlacer;
    int togglepulldowndetection;
    int togglemode;
    int togglematte;
    int framerate;
    int scan_channels;
    int pause;
    int resizewindow;
    int restarttvtime;

    int delay;

    int change_channel;

    int renumbering;

    int apply_luma;
    int update_luma;
    double luma_power;

    double overscan;

    int console_on;
    int scrollconsole;
    console_t *console;

    vbidata_t *vbi;
    int capturemode;

    station_mgr_t *stationmgr;

    int curfavorite;
    int numfavorites;
    int favorites[ NUM_FAVORITES ];

    int menuactive;
    int curmenu;
    int curmenupos;
    int curmenusize;
    menu_t *curusermenu;
    menu_t *menus[ MAX_USER_MENUS ];

    menu_t *root_tuner;
    menu_t *root_notuner;
};

static void reinit_tuner( commands_t *cmd )
{
    /* Setup the tuner if available. */
    if( cmd->vbi ) {
        vbidata_reset( cmd->vbi );
        vbidata_capture_mode( cmd->vbi, cmd->capturemode );
    }

    set_redirect( "root", "root-notuner" );

    if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
        int norm;

        set_redirect( "root", "root-tuner" );

        videoinput_set_tuner_freq( cmd->vidin, station_get_current_frequency( cmd->stationmgr ) );

        norm = videoinput_get_norm_number( station_get_current_norm( cmd->stationmgr ) );
        if( norm >= 0 ) {
            videoinput_switch_to_compatible_norm( cmd->vidin, norm );
        }

        if( cmd->osd ) {
            char channel_display[ 20 ];
            snprintf( channel_display, sizeof( channel_display ), "%d",
                      station_get_current_id( cmd->stationmgr ) );
            tvtime_osd_set_norm( cmd->osd, videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            tvtime_osd_set_audio_mode( cmd->osd, videoinput_get_audio_mode_name( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) ) );
            tvtime_osd_set_freq_table( cmd->osd, station_get_current_band( cmd->stationmgr ) );
            tvtime_osd_set_channel_number( cmd->osd, channel_display );
            tvtime_osd_set_channel_name( cmd->osd, station_get_current_channel_name( cmd->stationmgr ) );
            tvtime_osd_set_network_call( cmd->osd, station_get_current_network_call_letters( cmd->stationmgr ) );
            tvtime_osd_set_network_name( cmd->osd, station_get_current_network_name( cmd->stationmgr ) );
            tvtime_osd_set_show_name( cmd->osd, "" );
            tvtime_osd_set_show_rating( cmd->osd, "" );
            tvtime_osd_set_show_start( cmd->osd, "" );
            tvtime_osd_set_show_length( cmd->osd, "" );
            tvtime_osd_show_info( cmd->osd );
        }
        cmd->frame_counter = 0;

    } else if( cmd->osd ) {
        tvtime_osd_set_audio_mode( cmd->osd, "" );
        tvtime_osd_set_freq_table( cmd->osd, "" );
        tvtime_osd_set_channel_number( cmd->osd, "" );
        tvtime_osd_set_channel_name( cmd->osd, "" );
        tvtime_osd_set_network_call( cmd->osd, "" );
        tvtime_osd_set_network_name( cmd->osd, "" );
        tvtime_osd_set_show_name( cmd->osd, "" );
        tvtime_osd_set_show_rating( cmd->osd, "" );
        tvtime_osd_set_show_start( cmd->osd, "" );
        tvtime_osd_set_show_length( cmd->osd, "" );
        tvtime_osd_show_info( cmd->osd );
    }
}

commands_t *commands_new( config_t *cfg, videoinput_t *vidin,
                          station_mgr_t *mgr, tvtime_osd_t *osd,
                          int fieldtime )
{
    commands_t *cmd = malloc( sizeof( struct commands_s ) );
    char string[ 128 ];
    menu_t *menu;

    if( !cmd ) {
        return 0;
    }

    /* Number of frames to wait for next channel digit. */
    cmd->delay = 1000000 / fieldtime;

    cmd->cfg = cfg;
    cmd->vidin = vidin;
    cmd->osd = osd;
    cmd->stationmgr = mgr;
    cmd->frame_counter = 0;
    cmd->digit_counter = 0;

    cmd->displayinfo = 0;

    cmd->quit = 0;
    cmd->showbars = 0;
    cmd->showdeinterlacerinfo = 0;
    cmd->printdebug = 0;
    cmd->screenshot = 0;
    cmd->togglefullscreen = 0;
    cmd->toggleaspect = 0;
    cmd->togglealwaysontop = 0;
    cmd->toggledeinterlacer = 0;
    cmd->togglepulldowndetection = 0;
    cmd->togglemode = 0;
    cmd->togglematte = 0;
    cmd->framerate = FRAMERATE_FULL;
    cmd->console_on = 0;
    cmd->scrollconsole = 0;
    cmd->scan_channels = 0;
    cmd->pause = 0;
    cmd->change_channel = 0;
    cmd->renumbering = 0;
    cmd->resizewindow = 0;
    cmd->restarttvtime = 0;

    cmd->apply_luma = config_get_apply_luma_correction( cfg );
    cmd->update_luma = 0;
    cmd->luma_power = config_get_luma_correction( cfg );

    if( vidin && !videoinput_is_bttv( vidin ) && cmd->apply_luma ) {
        cmd->apply_luma = 0;
        fprintf( stderr, "commands: Input isn't from a bt8x8, disabling luma correction.\n" );
    }

    if( cmd->luma_power < 0.0 || cmd->luma_power > 10.0 ) {
        fprintf( stderr, "commands: Luma correction value out of range. Using 1.0.\n" );
        cmd->luma_power = 1.0;
    }

    cmd->overscan = config_get_overscan( cfg );
    if( cmd->overscan > 0.4 ) cmd->overscan = 0.4; if( cmd->overscan < 0.0 ) cmd->overscan = 0.0;

    cmd->console = 0;
    cmd->vbi = 0;
    cmd->capturemode = CAPTURE_OFF;

    cmd->numfavorites = 0;
    cmd->curfavorite = 0;

    cmd->menuactive = 0;
    cmd->curmenu = MENU_FAVORITES;
    cmd->curmenupos = 0;
    cmd->curusermenu = 0;
    memset( cmd->menus, 0, sizeof( cmd->menus ) );

    menu = menu_new( "root-tuner" );
    menu_set_text( menu, 0, "Setup" );
    sprintf( string, "%c%c%c  Station management", 0xee, 0x80, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_left_command( menu, 1, TVTIME_MENU_EXIT, 0 );
    menu_set_text( menu, 2, "Input configuration" );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 2, TVTIME_MENU_EXIT, 0 );
    menu_set_text( menu, 3, "Picture settings" );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 3, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Video processing", 0xee, 0x80, 0x9f );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, 4, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Exit", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    menu_set_right_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    menu_set_left_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    commands_add_menu( cmd, menu );

    menu = menu_new( "root-notuner" );
    menu_set_text( menu, 0, "Setup" );
    menu_set_text( menu, 1, "Input configuration" );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 1, TVTIME_MENU_EXIT, 0 );
    menu_set_text( menu, 2, "Picture settings" );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 2, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Video processing", 0xee, 0x80, 0x9f );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, 3, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Exit", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_MENU_EXIT, 0 );
    menu_set_right_command( menu, 4, TVTIME_MENU_EXIT, 0 );
    menu_set_left_command( menu, 4, TVTIME_MENU_EXIT, 0 );
    commands_add_menu( cmd, menu );

    menu = menu_new( "stations" );
    menu_set_text( menu, 0, "Setup - Station management" );
    menu_set_text( menu, 1, "Renumber current channel" );
    menu_set_enter_command( menu, 1, TVTIME_CHANNEL_RENUMBER, "" );
    menu_set_right_command( menu, 1, TVTIME_CHANNEL_RENUMBER, "" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    menu_set_text( menu, 2, "Rename current channel" );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "rename" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "rename" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    menu_set_text( menu, 3, "Station favorites" );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "favorites" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "favorites" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    menu_set_text( menu, 4, "Scan channels for signal" );
    menu_set_enter_command( menu, 4, TVTIME_CHANNEL_SCAN, "" );
    menu_set_right_command( menu, 4, TVTIME_CHANNEL_SCAN, "" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "input" );
    menu_set_text( menu, 0, "Setup - Input configuration" );
    menu_set_text( menu, 1, "Device setup" );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "device" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "device" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Data services", 0xee, 0x80, 0x9a );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "dataservices" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "dataservices" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Restart with new settings", 0xee, 0x80, 0x9c );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_RESTART, "" );
    menu_set_right_command( menu, 3, TVTIME_RESTART, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "processing" );
    menu_set_text( menu, 0, "Setup - Video processing" );
    menu_set_text( menu, 1, "Deinterlacer configuration" );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "device" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "device" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    menu_set_text( menu, 2, "Attempted framerate" );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "dataservices" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "dataservices" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    menu_set_text( menu, 3, "Input filters" );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "filters" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "filters" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "picture" );
    menu_set_text( menu, 0, "Setup - Picture" );
    sprintf( string, "%c%c%c  Brightness", 0xe2, 0x98, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "brightness" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "brightness" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Contrast", 0xe2, 0x97, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "contrast" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "contrast" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Colour", 0xe2, 0x98, 0xB0 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "colour" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "colour" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Hue", 0xe2, 0x97, 0xaf );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "hue" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "hue" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "brightness" );
    menu_set_text( menu, 0, "Setup - Picture - Brightness" );
    sprintf( string, "%c%c%c Adjust %c%c%c",
             0xe2, 0x97, 0x80,
             0xe2, 0x96, 0xb6 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_NOCOMMAND, "" );
    menu_set_right_command( menu, 1, TVTIME_BRIGHTNESS_UP, "" );
    menu_set_left_command( menu, 1, TVTIME_BRIGHTNESS_DOWN, "" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "contrast" );
    menu_set_text( menu, 0, "Setup - Picture - Contrast" );
    sprintf( string, "%c%c%c Adjust %c%c%c",
             0xe2, 0x97, 0x80,
             0xe2, 0x96, 0xb6 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_NOCOMMAND, "" );
    menu_set_right_command( menu, 1, TVTIME_CONTRAST_UP, "" );
    menu_set_left_command( menu, 1, TVTIME_CONTRAST_DOWN, "" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "colour" );
    menu_set_text( menu, 0, "Setup - Picture - Colour" );
    sprintf( string, "%c%c%c Adjust %c%c%c",
             0xe2, 0x97, 0x80,
             0xe2, 0x96, 0xb6 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_NOCOMMAND, "" );
    menu_set_right_command( menu, 1, TVTIME_COLOUR_UP, "" );
    menu_set_left_command( menu, 1, TVTIME_COLOUR_DOWN, "" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "hue" );
    menu_set_text( menu, 0, "Setup - Picture - Hue" );
    sprintf( string, "%c%c%c Adjust %c%c%c",
             0xe2, 0x97, 0x80,
             0xe2, 0x96, 0xb6 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_NOCOMMAND, "" );
    menu_set_right_command( menu, 1, TVTIME_HUE_UP, "" );
    menu_set_left_command( menu, 1, TVTIME_HUE_DOWN, "" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );

    reinit_tuner( cmd );

    return cmd;
}

void commands_delete( commands_t *cmd )
{
    int i;

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( cmd->menus[ i ] ) {
            menu_delete( cmd->menus[ i ] );
        }
    }
    free( cmd );
}

static void add_to_favorites( commands_t *cmd, int pos )
{
    int i;

    for( i = 0; i < NUM_FAVORITES; i++ ) {
        if( cmd->favorites[ i ] == pos ) return;
    }
    cmd->favorites[ cmd->curfavorite ] = pos;
    cmd->curfavorite = (cmd->curfavorite + 1) % NUM_FAVORITES;
    if( cmd->numfavorites < NUM_FAVORITES ) {
        cmd->numfavorites++;
    }
}

static void osd_list_audio_modes( tvtime_osd_t *osd, int ntsc, int curmode )
{
    tvtime_osd_list_set_lines( osd, ntsc ? 4 : 5 );
    tvtime_osd_list_set_text( osd, 0, "Preferred audio channel" );
    tvtime_osd_list_set_text( osd, 1, "Mono" );
    tvtime_osd_list_set_text( osd, 2, "Stereo" );
    tvtime_osd_list_set_text( osd, 3, ntsc ? "SAP" : "Primary Language" );
    if( !ntsc ) tvtime_osd_list_set_text( osd, 4, "Secondary Language" );
    if( curmode == VIDEOINPUT_MONO ) {
        tvtime_osd_list_set_hilight( osd, 1 );
    } else if( curmode == VIDEOINPUT_STEREO ) {
        tvtime_osd_list_set_hilight( osd, 2 );
    } else if( curmode == VIDEOINPUT_LANG1 || (ntsc && curmode == VIDEOINPUT_LANG2) ) {
        tvtime_osd_list_set_hilight( osd, 3 );
    } else if( curmode == VIDEOINPUT_LANG2 ) {
        tvtime_osd_list_set_hilight( osd, 4 );
    }
    tvtime_osd_show_list( osd, 1 );
}


/**
 * Hardcoded menus.
 */

static void menu_off( commands_t *cmd )
{
    tvtime_osd_list_hold( cmd->osd, 0 );
    tvtime_osd_show_list( cmd->osd, 0 );
    cmd->menuactive = 0;
}

static void menu_enter( commands_t *cmd )
{
    if( cmd->curmenu == MENU_FAVORITES ) {
        if( cmd->curmenupos == cmd->numfavorites ) {
            add_to_favorites( cmd, station_get_current_id( cmd->stationmgr ) );
        } else {
            if( cmd->curmenupos < cmd->numfavorites ) {
                station_set( cmd->stationmgr, cmd->favorites[ cmd->curmenupos ] );
                cmd->change_channel = 1;
            }
        }
        menu_off( cmd );
    } else if( cmd->curmenu == MENU_USER ) {
        int command = menu_get_enter_command( cmd->curusermenu, cmd->curmenupos + 1 );
        const char *argument = menu_get_enter_argument( cmd->curusermenu, cmd->curmenupos + 1 );

        /* I check for MENU_ENTER just to avoid a malicious infinite loop. */
        if( command != TVTIME_MENU_ENTER ) {
            commands_handle( cmd, command, argument );
        }
    }
}

static void menu_left( commands_t *cmd )
{
    if( cmd->curmenu == MENU_FAVORITES ) {
        commands_handle( cmd, TVTIME_SHOW_MENU, "stations" );
    } else if( cmd->curmenu == MENU_USER ) {
        int command = menu_get_left_command( cmd->curusermenu, cmd->curmenupos + 1 );
        const char *argument = menu_get_left_argument( cmd->curusermenu, cmd->curmenupos + 1 );

        /* I check for MENU_ENTER just to avoid a malicious infinite loop. */
        if( command != TVTIME_MENU_ENTER ) {
            commands_handle( cmd, command, argument );
        }
    }
}

static void menu_right( commands_t *cmd )
{
    if( cmd->curmenu == MENU_FAVORITES ) {
        menu_enter( cmd );
    } else if( cmd->curmenu == MENU_USER ) {
        int command = menu_get_right_command( cmd->curusermenu, cmd->curmenupos + 1 );
        const char *argument = menu_get_right_argument( cmd->curusermenu, cmd->curmenupos + 1 );

        /* I check for MENU_ENTER just to avoid a malicious infinite loop. */
        if( command != TVTIME_MENU_ENTER ) {
            commands_handle( cmd, command, argument );
        }
    }
}

static void display_current_menu( commands_t *cmd )
{
    int i;

    if( cmd->curmenu == MENU_FAVORITES ) {
        tvtime_osd_list_set_lines( cmd->osd, cmd->numfavorites + 3 );
        tvtime_osd_list_set_text( cmd->osd, 0, "Favorites" );
        for( i = 0; i < cmd->numfavorites; i++ ) {
            char text[ 32 ];
            sprintf( text, "%d", cmd->favorites[ i ] );
            tvtime_osd_list_set_text( cmd->osd, i + 1, text );
        }
        tvtime_osd_list_set_text( cmd->osd, cmd->numfavorites + 1, "Add current station" );
        tvtime_osd_list_set_text( cmd->osd, cmd->numfavorites + 2, "Exit" );
        cmd->curmenusize = cmd->numfavorites + 2;
    } else if( cmd->curmenu == MENU_USER && cmd->curusermenu ) {
        tvtime_osd_list_set_lines( cmd->osd, menu_get_num_lines( cmd->curusermenu ) );
        for( i = 0; i < menu_get_num_lines( cmd->curusermenu ); i++ ) {
            tvtime_osd_list_set_text( cmd->osd, i, menu_get_text( cmd->curusermenu, i ) );
        }
        cmd->curmenusize = menu_get_num_lines( cmd->curusermenu ) - 1;
    }

    tvtime_osd_list_set_hilight( cmd->osd, cmd->curmenupos + 1 );
    tvtime_osd_list_hold( cmd->osd, 1 );
    tvtime_osd_show_list( cmd->osd, 1 );
}

static int set_menu( commands_t *cmd, const char *menuname )
{
    int i;

    if( !menuname || !*menuname ) {
        return 0;
    }

    cmd->menuactive = 1;
    cmd->curusermenu = 0;

    for( i = 0; i < tvtime_num_builtin_menus(); i++ ) {
        if( !strcasecmp( menu_table[ i ].name, menuname ) ) {
            if( menu_table[ i ].menutype == MENU_REDIRECT ) {
                return set_menu( cmd, menu_table[ i ].dest );
            } else {
                cmd->curmenu = menu_table[ i ].menutype;
                cmd->curmenupos = 0;
                return 1;
            }
        }
    }

    cmd->curmenu = MENU_USER;
    cmd->curusermenu = 0;

    if( menuname && *menuname ) {
        for( i = 0; i < MAX_USER_MENUS; i++ ) {
            if( !cmd->menus[ i ] ) {
                break;
            }

            if( !strcasecmp( menuname, menu_get_name( cmd->menus[ i ] ) ) ) {
                cmd->curusermenu = cmd->menus[ i ];
                break;
            }
        }
    }

    if( cmd->curusermenu ) {
        cmd->curmenupos = menu_get_cursor( cmd->curusermenu );
        cmd->curmenusize = menu_get_num_lines( cmd->curusermenu ) - 1;
        return 1;
    }

    cmd->menuactive = 0;
    return 0;
}

void commands_add_menu( commands_t *cmd, menu_t *menu )
{
    int i;

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( !cmd->menus[ i ] ) {
            cmd->menus[ i ] = menu;
            return;
        }
    }
}

void commands_clear_menu_positions( commands_t *cmd )
{
    int i;

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( cmd->menus[ i ] ) {
            menu_set_cursor( cmd->menus[ i ], 0 );
        } else {
            return;
        }
    }
}

int commands_in_menu( commands_t *cmd )
{
    return cmd->menuactive;
}

void commands_handle( commands_t *cmd, int tvtime_cmd, const char *arg )
{
    int volume;

    if( tvtime_cmd == TVTIME_NOCOMMAND ) return;

    /*
    if( cmd->menuactive && !tvtime_is_menu_command( tvtime_cmd ) && cmd->curusermenu ) {
        menu_off( cmd );
    }
    */

    if( cmd->menuactive && tvtime_is_menu_command( tvtime_cmd ) ) {
        int x, y, line;
        switch( tvtime_cmd ) {
        case TVTIME_MENU_EXIT:
            menu_off( cmd );
            break;
        case TVTIME_MENU_UP:
            cmd->curmenupos = (cmd->curmenupos + cmd->curmenusize - 1) % (cmd->curmenusize);
            if( cmd->curusermenu ) menu_set_cursor( cmd->curusermenu, cmd->curmenupos );
            display_current_menu( cmd );
            break;
        case TVTIME_MENU_DOWN:
            cmd->curmenupos = (cmd->curmenupos + 1) % (cmd->curmenusize);
            if( cmd->curusermenu ) menu_set_cursor( cmd->curusermenu, cmd->curmenupos );
            display_current_menu( cmd );
            break;
        case TVTIME_MENU_LEFT: menu_left( cmd ); break;
        case TVTIME_MENU_RIGHT: menu_right( cmd ); break;
        case TVTIME_MENU_ENTER: menu_enter( cmd ); break;
        case TVTIME_SHOW_MENU:
            if( set_menu( cmd, arg ) ) {
                display_current_menu( cmd );
            } else {
                menu_off( cmd );
            }
            break;
        case TVTIME_MOUSE_MOVE:
            sscanf( arg, "%d %d", &x, &y );
            line = tvtime_osd_list_get_line_pos( cmd->osd, y );
            if( line > 0 ) {
                cmd->curmenupos = (line - 1);
                if( cmd->curusermenu ) menu_set_cursor( cmd->curusermenu, cmd->curmenupos );
                display_current_menu( cmd );
            }
            break;
        }
        return;
    }

    switch( tvtime_cmd ) {
    case TVTIME_QUIT:
        cmd->quit = 1;
        break;

    case TVTIME_SHOW_MENU:
        if( cmd->osd ) {
            commands_clear_menu_positions( cmd );
            if( set_menu( cmd, arg ) || set_menu( cmd, "root" ) ) {
                display_current_menu( cmd );
            }
        }
        break;

    case TVTIME_SHOW_DEINTERLACER_INFO:
        cmd->showdeinterlacerinfo = 1;
        break;

    case TVTIME_SHOW_STATS:
        cmd->printdebug = 1;
        break;

    case TVTIME_RESTART:
        cmd->restarttvtime = 1;
        break;

    case TVTIME_SCREENSHOT:
        cmd->screenshot = 1;
        break;

    case TVTIME_TOGGLE_BARS:
        cmd->showbars = !cmd->showbars;
        break;

    case TVTIME_TOGGLE_FULLSCREEN:
        cmd->togglefullscreen = 1;
        break;

    case TVTIME_SCROLL_CONSOLE_UP:
    case TVTIME_SCROLL_CONSOLE_DOWN:
        if( cmd->console_on )
            console_scroll_n( cmd->console, (tvtime_cmd == TVTIME_SCROLL_CONSOLE_UP) ? -1 : 1 );
        break;
            
    case TVTIME_TOGGLE_FRAMERATE:
        cmd->framerate = (cmd->framerate + 1) % FRAMERATE_MAX;
        break;

    case TVTIME_TOGGLE_CONSOLE:
        cmd->console_on = !cmd->console_on;
        console_toggle_console( cmd->console );
        break;

    case TVTIME_CHANNEL_SKIP:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            station_set_current_active( cmd->stationmgr, !station_get_current_active( cmd->stationmgr ) );
            if( cmd->osd ) {
                if( station_get_current_active( cmd->stationmgr ) ) {
                    tvtime_osd_show_message( cmd->osd, "Channel active in list." );
                } else {
                    tvtime_osd_show_message( cmd->osd, "Channel disabled from list." );
                }
            }
            station_writeconfig( cmd->stationmgr );
        }
    break;
            
    case TVTIME_TOGGLE_ASPECT:
        cmd->toggleaspect = 1;
        break;

    case TVTIME_TOGGLE_ALWAYSONTOP:
        cmd->togglealwaysontop = 1;
        break;

    case TVTIME_CHANNEL_SAVE_TUNING:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            char freq[ 32 ];
            int pos;

            snprintf( freq, sizeof( freq ), "%f", ((double) videoinput_get_tuner_freq( cmd->vidin )) / 1000.0 );
            pos = station_add( cmd->stationmgr, 0, "Custom", freq, 0 );
            station_writeconfig( cmd->stationmgr );
            station_set( cmd->stationmgr, pos );
            cmd->change_channel = 1;
        }
        break;

    case TVTIME_CHANNEL_ACTIVATE_ALL:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "All channels re-activated." );
            station_activate_all_channels( cmd->stationmgr );
            station_writeconfig( cmd->stationmgr );
        }
        break;

    case TVTIME_CHANNEL_RENUMBER:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            /* If we're scanning and suddenly want to renumber, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            /* Accept input of the destination channel. */
            if( cmd->digit_counter == 0 ) memset( cmd->next_chan_buffer, 0, 5 );
            cmd->frame_counter = cmd->delay;
            cmd->renumbering = 1;
            if( cmd->osd ) {
                char message[ 256 ];
                snprintf( message, sizeof( message ),
                          "Remapping %d.  Enter new channel number.",
                          station_get_current_id( cmd->stationmgr ) );
                tvtime_osd_set_hold_message( cmd->osd, message );
            }
        }
        break;

    case TVTIME_CHANNEL_SCAN:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            if( !config_get_check_freq_present( cmd->cfg ) ) {
                if( cmd->osd ) {
                    tvtime_osd_show_message( cmd->osd, "Scanner unavailable: Signal checking disabled." );
                }
            } else {
                cmd->scan_channels = !cmd->scan_channels;

                if( cmd->scan_channels && cmd->renumbering ) {
                    memset( cmd->next_chan_buffer, 0, 5 );
                    cmd->digit_counter = 0;
                    cmd->frame_counter = 0;
                    if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
                    cmd->renumbering = 0;
                }

                if( cmd->osd ) {
                    if( cmd->scan_channels ) {
                        int keycode = config_command_to_key( cmd->cfg, TVTIME_CHANNEL_SCAN );
                        if( keycode ) {
                            const char *special = input_special_key_to_string( keycode );
                            char message[ 256 ];

                            if( special ) {
                                snprintf( message, sizeof( message ), "Scanning (hit %s to stop).", special );
                            } else {
                                snprintf( message, sizeof( message ), "Scanning (hit %c to stop).", keycode );
                            }
                            tvtime_osd_set_hold_message( cmd->osd, message );
                            tvtime_osd_show_info( cmd->osd );
                        }
                    } else {
                        tvtime_osd_set_hold_message( cmd->osd, "" );
                        tvtime_osd_show_info( cmd->osd );
                    }
                }
            }
        }
        break;

    case TVTIME_TOGGLE_CC:
        if( cmd->vbi ) {
            vbidata_capture_mode( cmd->vbi, cmd->capturemode ? CAPTURE_OFF : CAPTURE_CC1 );
            if( cmd->capturemode ) {
                if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "Closed Captioning Disabled." );
                cmd->capturemode = CAPTURE_OFF;
            } else {
                if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "Closed Captioning Enabled." );
                cmd->capturemode = CAPTURE_CC1;
            }
        } else {
            if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "No VBI device configured for CC decoding." );
        }
        break;

    case TVTIME_TOGGLE_COMPATIBLE_NORM:
        videoinput_switch_to_next_compatible_norm( cmd->vidin );
        if( videoinput_has_tuner( cmd->vidin ) ) {
            station_set_current_norm( cmd->stationmgr, videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            station_writeconfig( cmd->stationmgr );
        }
        if( cmd->osd ) {
            tvtime_osd_set_norm( cmd->osd, videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_DISPLAY_MESSAGE:
        if( cmd->osd && arg ) tvtime_osd_show_message( cmd->osd, arg );
        break;

    case TVTIME_DISPLAY_INFO:
        cmd->displayinfo = !cmd->displayinfo;
        if( cmd->osd ) {
            if( cmd->displayinfo ) {
                tvtime_osd_hold( cmd->osd, 1 );
                tvtime_osd_show_info( cmd->osd );
            } else {
                tvtime_osd_hold( cmd->osd, 0 );
                tvtime_osd_clear( cmd->osd );
            }
        }
        break;

    case TVTIME_TOGGLE_CREDITS:
        break;

    case TVTIME_TOGGLE_AUDIO_MODE:
        if( cmd->vidin ) {
            videoinput_set_audio_mode( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) << 1 );
            if( cmd->osd ) {
                osd_list_audio_modes( cmd->osd, videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC,
                                      videoinput_get_audio_mode( cmd->vidin ) );
                tvtime_osd_set_audio_mode( cmd->osd, videoinput_get_audio_mode_name( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) ) );
                tvtime_osd_show_info( cmd->osd );
            }
        }
        break;

    case TVTIME_TOGGLE_DEINTERLACER:
        cmd->toggledeinterlacer = 1;
        break;

    case TVTIME_TOGGLE_PULLDOWN_DETECTION:
        cmd->togglepulldowndetection = 1;
        break;

    case TVTIME_CHANNEL_CHAR:
        if( arg && isdigit( arg[ 0 ] ) && cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            /* Decode the input char from commands.  */
            if( cmd->digit_counter == 0 ) memset( cmd->next_chan_buffer, 0, 5 );
            cmd->next_chan_buffer[ cmd->digit_counter ] = arg[ 0 ];
            cmd->digit_counter++;
            cmd->frame_counter = cmd->delay;

            /**
             * Send an enter command if we type more
             * digits than there are channels.
             */
            if( cmd->digit_counter > 0 && (station_get_max_position( cmd->stationmgr ) < 10) ) {
                commands_handle( cmd, TVTIME_ENTER, 0 );
            } else if( cmd->digit_counter > 1 && (station_get_max_position( cmd->stationmgr ) < 100) ) {
                commands_handle( cmd, TVTIME_ENTER, 0 );
            } else if( cmd->digit_counter > 2 ) {
                commands_handle( cmd, TVTIME_ENTER, 0 );
            }
        }
        break;

    case TVTIME_TOGGLE_LUMA_CORRECTION:
        cmd->apply_luma = !cmd->apply_luma;
        if( cmd->osd ) {
            if( cmd->apply_luma ) {
                tvtime_osd_show_message( cmd->osd, "Luma correction enabled." );
                config_save( cmd->cfg, "ApplyLumaCorrection", "1" );
            } else {
                tvtime_osd_show_message( cmd->osd, "Luma correction disabled." );
                config_save( cmd->cfg, "ApplyLumaCorrection", "0" );
            }
        }
        break;

    case TVTIME_OVERSCAN_UP:
    case TVTIME_OVERSCAN_DOWN:
        cmd->overscan = cmd->overscan + ( (tvtime_cmd == TVTIME_OVERSCAN_UP) ? 0.0025 : -0.0025 );
        if( cmd->overscan > 0.4 ) cmd->overscan = 0.4; if( cmd->overscan < 0.0 ) cmd->overscan = 0.0;

        if( cmd->osd ) {
            char message[ 200 ];
            snprintf( message, sizeof( message ), "Overscan: %.1f%%",
                      cmd->overscan * 2.0 * 100.0 );
            tvtime_osd_show_message( cmd->osd, message );
        }
        break;

    case TVTIME_LUMA_UP:
    case TVTIME_LUMA_DOWN:
        if( !cmd->apply_luma ) {
            fprintf( stderr, "tvtime: Luma correction disabled.  "
                     "Run with -c to use it.\n" );
        } else {
            char message[ 200 ];
            cmd->luma_power = cmd->luma_power + ( (tvtime_cmd == TVTIME_LUMA_UP) ? 0.1 : -0.1 );

            cmd->update_luma = 1;

            if( cmd->luma_power > 10.0 ) cmd->luma_power = 10.0;
            if( cmd->luma_power <  0.0 ) cmd->luma_power = 0.0;

            snprintf( message, sizeof( message ), "%.1f", cmd->luma_power );
            config_save( cmd->cfg, "LumaCorrection", message );
            if( cmd->osd ) {
                snprintf( message, sizeof( message ), "Luma correction value: %.1f",
                          cmd->luma_power );
                tvtime_osd_show_message( cmd->osd, message );
            }
        }
        break;

    case TVTIME_AUTO_ADJUST_PICT:
        if( cmd->vidin ) {
            videoinput_reset_default_settings( cmd->vidin );
            if( cmd->osd ) {
                tvtime_osd_show_message( cmd->osd, "Picture settings reset to defaults." );
            }
        }
        break;

    case TVTIME_AUTO_ADJUST_WINDOW:
        cmd->resizewindow = 1;
        break;

    case TVTIME_TOGGLE_NTSC_CABLE_MODE:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            station_toggle_us_cable_mode( cmd->stationmgr );
            cmd->change_channel = 1;
        }
        break;

    case TVTIME_FINETUNE_DOWN:
    case TVTIME_FINETUNE_UP:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            videoinput_set_tuner_freq( cmd->vidin, videoinput_get_tuner_freq( cmd->vidin ) +
                                       ( tvtime_cmd == TVTIME_FINETUNE_UP ? ((1000/16)+1) : -(1000/16) ) );

            if( cmd->vbi ) {
                vbidata_reset( cmd->vbi );
                vbidata_capture_mode( cmd->vbi, cmd->capturemode );
            }

            if( cmd->osd ) {
                char message[ 200 ];
                snprintf( message, sizeof( message ), "Tuning: %4.2fMhz.",
                          ((double) videoinput_get_tuner_freq( cmd->vidin )) / 1000.0 );
                tvtime_osd_show_message( cmd->osd, message );
            }
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_CHANNEL_INC: 
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /**
             * If we're scanning and the user hits a key, stop scanning.
             * arg will be 0 if the scanner has called us.
             */
            if( cmd->scan_channels && arg ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_inc( cmd->stationmgr );
            cmd->change_channel = 1;
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;
    case TVTIME_CHANNEL_DEC:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_dec( cmd->stationmgr );
            cmd->change_channel = 1;
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_CHANNEL_PREV:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_prev( cmd->stationmgr );
            cmd->change_channel = 1;
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_MIXER_TOGGLE_MUTE:
        mixer_mute( !mixer_ismute() );

        if( cmd->osd ) {
            tvtime_osd_show_data_bar( cmd->osd, "Volume", (mixer_get_volume()) & 0xff );
        }
        break;

    case TVTIME_MIXER_UP:
    case TVTIME_MIXER_DOWN:

        /* If the user hits the volume control, drop us out of mute mode. */
        if( cmd->vidin && videoinput_get_muted( cmd->vidin ) ) {
            commands_handle( cmd, TVTIME_TOGGLE_MUTE, 0 );
        }
        volume = mixer_set_volume( ( (tvtime_cmd == TVTIME_MIXER_UP) ? 1 : -1 ) );

        if( cmd->osd ) {
            tvtime_osd_show_data_bar( cmd->osd, "Volume", volume & 0xff );
        }
        break;

    case TVTIME_TOGGLE_MUTE:
        if( cmd->vidin ) {
            videoinput_mute( cmd->vidin, !videoinput_get_muted( cmd->vidin ) );
            if( cmd->osd ) {
                tvtime_osd_volume_muted( cmd->osd, videoinput_get_muted( cmd->vidin ) );
            }
        }
        break;

    case TVTIME_TOGGLE_INPUT:
        if( cmd->vidin ) {
            cmd->frame_counter = 0;

            if( cmd->renumbering ) {
                memset( cmd->next_chan_buffer, 0, sizeof( cmd->next_chan_buffer ) );
                commands_handle( cmd, TVTIME_ENTER, 0 );
            }

            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            videoinput_set_input_num( cmd->vidin, ( videoinput_get_input_num( cmd->vidin ) + 1 ) % videoinput_get_num_inputs( cmd->vidin ) );
            reinit_tuner( cmd );

            if( cmd->osd ) {
                tvtime_osd_set_input( cmd->osd, videoinput_get_input_name( cmd->vidin ) );
                tvtime_osd_show_info( cmd->osd );
            }
        }
        break;

    case TVTIME_HUE_UP:
    case TVTIME_HUE_DOWN:
        if( cmd->vidin ) {
            videoinput_set_hue_relative( cmd->vidin, (tvtime_cmd == TVTIME_HUE_UP) ? 1 : -1 );
            if( cmd->osd ) {
                tvtime_osd_show_data_bar( cmd->osd, "Hue", videoinput_get_hue( cmd->vidin ) );
            }
        }
        break;

    case TVTIME_BRIGHTNESS_UP: 
    case TVTIME_BRIGHTNESS_DOWN:
        if( cmd->vidin ) {
            videoinput_set_brightness_relative( cmd->vidin, (tvtime_cmd == TVTIME_BRIGHTNESS_UP) ? 1 : -1 );
            if( cmd->osd ) {
                tvtime_osd_show_data_bar( cmd->osd, "Brightness", videoinput_get_brightness( cmd->vidin ) );
            }
        }
        break;

    case TVTIME_CONTRAST_UP:
    case TVTIME_CONTRAST_DOWN:
        if( cmd->vidin ) {
            videoinput_set_contrast_relative( cmd->vidin, (tvtime_cmd == TVTIME_CONTRAST_UP) ? 1 : -1 );
            if( cmd->osd ) {
                tvtime_osd_show_data_bar( cmd->osd, "Contrast", videoinput_get_contrast( cmd->vidin ) );
            }
        }
        break;

    case TVTIME_COLOUR_UP:
    case TVTIME_COLOUR_DOWN:
        if( cmd->vidin ) {
            videoinput_set_colour_relative( cmd->vidin, (tvtime_cmd == TVTIME_COLOUR_UP) ? 1 : -1 );
            if( cmd->osd ) {
                tvtime_osd_show_data_bar( cmd->osd, "Colour", videoinput_get_colour( cmd->vidin ) );
            }
        }
        break;

    case TVTIME_ENTER:
        if( cmd->next_chan_buffer[ 0 ] ) {
            if( cmd->renumbering ) {
                station_remap( cmd->stationmgr, atoi( cmd->next_chan_buffer ) );
                station_writeconfig( cmd->stationmgr );
                cmd->renumbering = 0;
                if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
            }
            if( station_set( cmd->stationmgr, atoi( cmd->next_chan_buffer ) ) ) {
                cmd->change_channel = 1;
            } else {
                snprintf( cmd->next_chan_buffer, sizeof( cmd->next_chan_buffer ),
                          "%d", station_get_current_id( cmd->stationmgr ) );
                if( cmd->osd ) {
                    tvtime_osd_set_channel_number( cmd->osd, cmd->next_chan_buffer );
                    tvtime_osd_show_info( cmd->osd );
                }
            }
        } else {
            if( cmd->renumbering ) {
                cmd->renumbering = 0;
                if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
            }
            snprintf( cmd->next_chan_buffer, sizeof( cmd->next_chan_buffer ),
                      "%d", station_get_current_id( cmd->stationmgr ) );
            if( cmd->osd ) {
                tvtime_osd_set_channel_number( cmd->osd, cmd->next_chan_buffer );
                tvtime_osd_show_info( cmd->osd );
            }
        }
        cmd->frame_counter = 0;
        break;

    case TVTIME_CHANNEL_1:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "1" );
        break;

    case TVTIME_CHANNEL_2:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "2" );
        break;

    case TVTIME_CHANNEL_3:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "3" );
        break;

    case TVTIME_CHANNEL_4:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "4" );
        break;

    case TVTIME_CHANNEL_5:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "5" );
        break;

    case TVTIME_CHANNEL_6:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "6" );
        break;

    case TVTIME_CHANNEL_7:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "7" );
        break;

    case TVTIME_CHANNEL_8:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "8" );
        break;

    case TVTIME_CHANNEL_9:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "9" );
        break;

    case TVTIME_CHANNEL_0:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "0" );
        break;

    case TVTIME_TOGGLE_PAUSE:
        cmd->pause = !(cmd->pause);
        if( cmd->osd ) tvtime_osd_show_message( cmd->osd, cmd->pause ? "Paused" : "Resumed" );
        break;

    case TVTIME_TOGGLE_MATTE:
        cmd->togglematte = 1;
        break;

    case TVTIME_TOGGLE_MODE:
        cmd->togglemode = 1;
        break;
    }
}

void commands_next_frame( commands_t *cmd )
{
    /* Decrement the frame counter if user is typing digits */
    if( cmd->frame_counter > 0 ) {
        cmd->frame_counter--;
        if( cmd->frame_counter == 0 ) {
            /* Switch to the next channel if the countdown expires. */
            commands_handle( cmd, TVTIME_ENTER, 0 );
        }
    }

    if( cmd->frame_counter == 0 ) {
        memset( cmd->next_chan_buffer, 0, 5 );
        cmd->digit_counter = 0;
        if( cmd->renumbering ) {
            if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
            cmd->renumbering = 0;
        }
    }

    if( cmd->frame_counter > 0 && !(cmd->frame_counter % 5)) {
        char input_text[6];

        strcpy( input_text, cmd->next_chan_buffer );
        if( !(cmd->frame_counter % 10) ) {
            strcat( input_text, "_" );
        } else {
            strcat( input_text, " " );
        }
        if( cmd->osd ) {
            tvtime_osd_set_channel_number( cmd->osd, input_text );
            tvtime_osd_show_info( cmd->osd );
        }
    }

    if( cmd->change_channel ) {
        reinit_tuner( cmd );
        cmd->change_channel = 0;
    }

    if( cmd->vbi ) {
        if( *(vbidata_get_network_name( cmd->vbi )) ) {
            /* If the network name has changed, save it to the config file. */
            if( strcmp( station_get_current_network_name( cmd->stationmgr ),
                        vbidata_get_network_name( cmd->vbi ) ) ) {
                station_set_current_network_name( cmd->stationmgr,
                                                  vbidata_get_network_name( cmd->vbi ) );
                station_writeconfig( cmd->stationmgr );
            }

            if( cmd->osd ) {
                tvtime_osd_set_network_name( cmd->osd, station_get_current_network_name( cmd->stationmgr ) );
            }
        }

        if( *(vbidata_get_network_call_letters( cmd->vbi )) ) {
            /* If the call letters have changed, save them to the config file. */
            if( strcmp( station_get_current_network_call_letters( cmd->stationmgr ),
                        vbidata_get_network_call_letters( cmd->vbi ) ) ) {
                station_set_current_network_call_letters( cmd->stationmgr,
                                                          vbidata_get_network_call_letters( cmd->vbi ) );
                station_writeconfig( cmd->stationmgr );
            }

            if( cmd->osd ) {
                tvtime_osd_set_network_call( cmd->osd, station_get_current_network_call_letters( cmd->stationmgr ) );
            }
        }

        if( cmd->osd ) {
            tvtime_osd_set_show_name( cmd->osd, vbidata_get_program_name( cmd->vbi ) );
            tvtime_osd_set_show_rating( cmd->osd, vbidata_get_program_rating( cmd->vbi ) );
            tvtime_osd_set_show_start( cmd->osd, vbidata_get_program_start_time( cmd->vbi ) );
            tvtime_osd_set_show_length( cmd->osd, vbidata_get_program_length( cmd->vbi ) );
        }
    }

    cmd->printdebug = 0;
    cmd->showdeinterlacerinfo = 0;
    cmd->screenshot = 0;
    cmd->togglefullscreen = 0;
    cmd->toggleaspect = 0;
    cmd->togglealwaysontop = 0;
    cmd->toggledeinterlacer = 0;
    cmd->togglepulldowndetection = 0;
    cmd->togglemode = 0;
    cmd->togglematte = 0;
    cmd->update_luma = 0;
    cmd->resizewindow = 0;
}

int commands_quit( commands_t *cmd )
{
    return cmd->quit;
}

int commands_print_debug( commands_t *cmd )
{
    return cmd->printdebug;
}

int commands_show_bars( commands_t *cmd )
{
    return cmd->showbars;
}

int commands_take_screenshot( commands_t *cmd )
{
    return cmd->screenshot;
}

int commands_toggle_fullscreen( commands_t *cmd )
{
    return cmd->togglefullscreen;
}

int commands_get_framerate( commands_t *cmd )
{
    return cmd->framerate;
}

int commands_toggle_aspect( commands_t *cmd )
{
    return cmd->toggleaspect;
}

int commands_toggle_alwaysontop( commands_t *cmd )
{
    return cmd->togglealwaysontop;
}

int commands_toggle_deinterlacer( commands_t *cmd )
{
    return cmd->toggledeinterlacer;
}

int commands_toggle_pulldown_detection( commands_t *cmd )
{
    return cmd->togglepulldowndetection;
}

int commands_toggle_mode( commands_t *cmd )
{
    return cmd->togglemode;
}

int commands_toggle_matte( commands_t *cmd )
{
    return cmd->togglematte;
}

void commands_set_console( commands_t *cmd, console_t *con )
{
    cmd->console = con;
}

void commands_set_vbidata( commands_t *cmd, vbidata_t *vbi )
{
    cmd->vbi = vbi;
}

int commands_console_on( commands_t *cmd )
{
    return cmd->console_on;
}

int commands_scan_channels( commands_t *cmd )
{
    return cmd->scan_channels;
}

int commands_pause( commands_t *cmd )
{
    return cmd->pause;
}

int commands_apply_luma_correction( commands_t *cmd )
{
    return cmd->apply_luma;
}

int commands_update_luma_power( commands_t *cmd )
{
    return cmd->update_luma;
}

double commands_get_luma_power( commands_t *cmd )
{
    return cmd->luma_power;
}

double commands_get_overscan( commands_t *cmd )
{
    return cmd->overscan;
}

int commands_resize_window( commands_t *cmd )
{
    return cmd->resizewindow;
}

void commands_set_framerate( commands_t *cmd, int framerate )
{
    cmd->framerate = framerate % FRAMERATE_MAX;
}

int commands_show_deinterlacer_info( commands_t *cmd )
{
    return cmd->showdeinterlacerinfo;
}

int commands_restart_tvtime( commands_t *cmd )
{
    return cmd->restarttvtime;
}

