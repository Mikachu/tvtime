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

#ifndef TVTIMECONF_H_INCLUDED
#define TVTIMECONF_H_INCLUDED

#include <sys/types.h>
#include <unistd.h>
#include "configsave.h"

typedef struct config_s config_t;
typedef struct tvtime_mode_settings_s tvtime_mode_settings_t;

struct tvtime_mode_settings_s
{
    const char *name;
    const char *deinterlacer;
    int fullscreen;
    int fullscreen_width;
    int fullscreen_height;
    int window_height;
    int framerate_mode;
};

config_t *config_new( int argc, char **argv );
void config_delete( config_t *ct );
int config_key_to_command( config_t *ct, int key );
int config_button_to_command( config_t *ct, int button );

int config_get_verbose( config_t *ct );
int config_get_debug( config_t *ct );
int config_get_outputheight( config_t *ct );
int config_get_inputwidth( config_t *ct );
int config_get_aspect( config_t *ct );
int config_get_inputnum( config_t *ct );
int config_get_apply_luma_correction( config_t *ct );
double config_get_luma_correction( config_t *ct );
const char *config_get_v4l_device( config_t *ct );
const char *config_get_v4l_norm( config_t *ct );
const char *config_get_v4l_freq( config_t *ct );
int config_get_ntsc_cable_mode( config_t *ct );
const char *config_get_timeformat( config_t *ct );
unsigned int config_get_menu_bg_rgb( config_t *ct );
unsigned int config_get_channel_text_rgb( config_t *ct );
unsigned int config_get_other_text_rgb( config_t *ct );
int config_get_fullscreen( config_t *ct );
int config_get_priority( config_t *ct );
uid_t config_get_uid( config_t *ct );
const char *config_get_command_pipe( config_t *ct );
int config_get_preferred_deinterlace_method( config_t *ct );
int config_get_start_channel( config_t *ct );
int config_get_prev_channel( config_t *ct );
int config_get_left_scanline_bias( config_t *ct );
int config_get_right_scanline_bias( config_t *ct );
const char *config_get_screenshot_dir( config_t *ct );
const char *config_get_rvr_filename( config_t *ct );
int config_get_framerate_mode( config_t *ct );

const char *config_get_config_filename( config_t *ct );

double config_get_horizontal_overscan( config_t *ct );
double config_get_vertical_overscan( config_t *ct );

int config_get_check_freq_present( config_t *ct );

int tvtime_string_to_command( const char *str );

int config_get_usevbi( config_t *ct );
char *config_get_vbidev( config_t *ct );

configsave_t *config_get_configsave( config_t *ct );

int config_get_num_modes( config_t *ct );
tvtime_mode_settings_t *config_get_mode_info( config_t *ct, int mode );

#endif /* TVTIMECONF_H_INCLUDED */
