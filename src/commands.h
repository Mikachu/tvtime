/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct commands_s commands_t;

#include "tvtimeosd.h"
#include "videoinput.h"
#include "tvtimeconf.h"
#include "console.h"
#include "vbidata.h"
#include "station.h"

#define MAX_CMD_NAMELEN 64

/**
 * Input commands for keymap
 */
enum tvtime_commands
{
    TVTIME_NOCOMMAND,
    TVTIME_QUIT,
    TVTIME_CHANNEL_INC,
    TVTIME_CHANNEL_DEC,
    TVTIME_CHANNEL_PREV,
    TVTIME_TOGGLE_LUMA_CORRECTION,
    TVTIME_LUMA_UP,
    TVTIME_LUMA_DOWN,
    TVTIME_TOGGLE_MUTE,
    TVTIME_MIXER_UP,
    TVTIME_MIXER_DOWN,
    TVTIME_MIXER_TOGGLE_MUTE,
    TVTIME_ENTER,
    TVTIME_CHANNEL_CHAR,

    TVTIME_TOGGLE_INPUT,
    TVTIME_HUE_DOWN,
    TVTIME_HUE_UP,
    TVTIME_BRIGHTNESS_DOWN,
    TVTIME_BRIGHTNESS_UP,
    TVTIME_CONTRAST_DOWN,
    TVTIME_CONTRAST_UP,
    TVTIME_COLOUR_DOWN,
    TVTIME_COLOUR_UP,

    TVTIME_FINETUNE_DOWN,
    TVTIME_FINETUNE_UP,

    TVTIME_TOGGLE_BARS,
    TVTIME_SHOW_STATS,

    TVTIME_TOGGLE_FULLSCREEN,
    TVTIME_TOGGLE_ASPECT,
    TVTIME_SCREENSHOT,
    TVTIME_TOGGLE_DEINTERLACER,
    TVTIME_TOGGLE_PULLDOWN_DETECTION,

    TVTIME_MENUMODE,
    TVTIME_DISPLAY_INFO,
    TVTIME_TOGGLE_CREDITS,

    TVTIME_TOGGLE_NTSC_CABLE_MODE,
    TVTIME_AUTO_ADJUST_PICT,
    TVTIME_AUTO_ADJUST_WINDOW,
    TVTIME_CHANNEL_SKIP,
    TVTIME_TOGGLE_CONSOLE,
    TVTIME_SCROLL_CONSOLE_UP,
    TVTIME_SCROLL_CONSOLE_DOWN,
    TVTIME_TOGGLE_CC,

    TVTIME_TOGGLE_FRAMERATE,

    TVTIME_TOGGLE_AUDIO_MODE,

    TVTIME_CHANNEL_ACTIVATE_ALL,
    TVTIME_CHANNEL_SCAN,

    TVTIME_CHANNEL_RENUMBER,
    TVTIME_CHANNEL_SAVE_TUNING,

    TVTIME_OVERSCAN_UP,
    TVTIME_OVERSCAN_DOWN,

    TVTIME_CHANNEL_1,
    TVTIME_CHANNEL_2,
    TVTIME_CHANNEL_3,
    TVTIME_CHANNEL_4,
    TVTIME_CHANNEL_5,
    TVTIME_CHANNEL_6,
    TVTIME_CHANNEL_7,
    TVTIME_CHANNEL_8,
    TVTIME_CHANNEL_9,
    TVTIME_CHANNEL_0,

    TVTIME_TOGGLE_PAUSE,

    TVTIME_TOGGLE_ALWAYSONTOP,
    TVTIME_TOGGLE_MATTE,
    TVTIME_TOGGLE_MODE,
    TVTIME_TOGGLE_COMPATIBLE_NORM
};

enum framerate_mode
{
    FRAMERATE_FULL = 0,
    FRAMERATE_HALF_TFF = 1,
    FRAMERATE_HALF_BFF = 2,
    FRAMERATE_MAX = 3
};

int tvtime_string_to_command( const char *str );
const char *tvtime_command_to_string( int command );
int tvtime_num_commands( void );
const char *tvtime_get_command( int pos );
int tvtime_get_command_id( int pos );

commands_t *commands_new( config_t *cfg, videoinput_t *vidin, 
                          station_mgr_t *mgr, tvtime_osd_t *osd );
void commands_delete( commands_t *in );
void commands_handle( commands_t *in, int command, int arg );

int commands_quit( commands_t *in );
int commands_take_screenshot( commands_t *in );
int commands_print_debug( commands_t *in );
int commands_show_bars( commands_t *in );
int commands_toggle_fullscreen( commands_t *in );
int commands_toggle_aspect( commands_t *in );
int commands_toggle_deinterlacer( commands_t *in );
int commands_toggle_pulldown_detection( commands_t *in );
int commands_toggle_mode( commands_t *in );
int commands_toggle_matte( commands_t *in );
int commands_toggle_alwaysontop( commands_t *in );
int commands_apply_luma_correction( commands_t *in );
int commands_resize_window( commands_t *in );
int commands_update_luma_power( commands_t *in );
double commands_get_luma_power( commands_t *in );
double commands_get_overscan( commands_t *in );
double commands_get_overscan( commands_t *in );
int commands_get_framerate( commands_t *in );
void commands_set_framerate( commands_t *in, int framerate );
int commands_scan_channels( commands_t *in );
void commands_set_console( commands_t *in, console_t *con );
void commands_set_vbidata( commands_t *in, vbidata_t *con );

int commands_console_on( commands_t *in );

void commands_next_frame( commands_t *in );
int commands_pause( commands_t *in );

#ifdef __cplusplus
};
#endif
#endif /* COMMANDS_H_INCLUDED */
