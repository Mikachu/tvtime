/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct commands_s commands_t;

#include "input.h"
#include "tvtimeosd.h"
#include "videoinput.h"
#include "tvtimeconf.h"
#include "videocorrection.h"
#include "menu.h"
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
    TVTIME_CHANNEL_UP,
    TVTIME_CHANNEL_DOWN,
    TVTIME_CHANNEL_PREV,
    TVTIME_LUMA_CORRECTION_TOGGLE,
    TVTIME_LUMA_UP,
    TVTIME_LUMA_DOWN,
    TVTIME_MIXER_MUTE,
    TVTIME_MIXER_UP,
    TVTIME_MIXER_DOWN,
    TVTIME_ENTER,
    TVTIME_CHANNEL_CHAR,

    TVTIME_TV_VIDEO,
    TVTIME_HUE_DOWN,
    TVTIME_HUE_UP,
    TVTIME_BRIGHT_DOWN,
    TVTIME_BRIGHT_UP,
    TVTIME_CONT_DOWN,
    TVTIME_CONT_UP,
    TVTIME_COLOUR_DOWN,
    TVTIME_COLOUR_UP,

    TVTIME_FINETUNE_DOWN,
    TVTIME_FINETUNE_UP,

    TVTIME_FREQLIST_DOWN,
    TVTIME_FREQLIST_UP,

    TVTIME_SHOW_BARS,
    TVTIME_DEBUG,

    TVTIME_FULLSCREEN,
    TVTIME_ASPECT,
    TVTIME_SCREENSHOT,
    TVTIME_DEINTERLACINGMODE,

    TVTIME_MENUMODE,
    TVTIME_DISPLAY_INFO,
    TVTIME_SHOW_CREDITS,

    TVTIME_TOGGLE_NTSC_CABLE_MODE,
    TVTIME_AUTO_ADJUST_PICT,
    TVTIME_SKIP_CHANNEL,
    TVTIME_TOGGLE_CONSOLE,
    TVTIME_SCROLL_CONSOLE_UP,
    TVTIME_SCROLL_CONSOLE_DOWN,
    TVTIME_TOGGLE_CC,

    TVTIME_TOGGLE_HALF_FRAMERATE,

    TVTIME_SCAN_CHANNELS,

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

    TVTIME_LAST
};

int tvtime_string_to_command( const char *str );
int tvtime_num_commands( void );
const char *tvtime_get_command( int pos );
int tvtime_get_command_id( int pos );

commands_t *commands_new( config_t *cfg, videoinput_t *vidin, 
                          station_mgr_t *mgr, tvtime_osd_t *osd,
                          video_correction_t *vc );
void commands_delete( commands_t *in );
void commands_handle( commands_t *in, int command, int arg );

int commands_quit( commands_t *in );
int commands_take_screenshot( commands_t *in );
int commands_print_debug( commands_t *in );
int commands_show_bars( commands_t *in );
int commands_toggle_fullscreen( commands_t *in );
int commands_toggle_aspect( commands_t *in );
int commands_toggle_deinterlacing_mode( commands_t *in );
int commands_toggle_menu( commands_t *in );
int commands_half_framerate( commands_t *in );
int commands_scan_channels( commands_t *in );
void commands_set_console( commands_t *in, console_t *con );
void commands_set_vbidata( commands_t *in, vbidata_t *con );

int commands_console_on( commands_t *in );
int commands_menu_on( commands_t *in );

void commands_set_menu( commands_t *in, menu_t *m );

void commands_next_frame( commands_t *in );

#ifdef __cplusplus
};
#endif
#endif /* COMMANDS_H_INCLUDED */
