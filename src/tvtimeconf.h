/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 * Copyright (c) 2003 Alexander Belov <asbel@mail.ru>
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

#ifndef TVTIMECONF_H_INCLUDED
#define TVTIMECONF_H_INCLUDED

#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct config_s config_t;

config_t *config_new( void );
void config_delete( config_t *ct );
int config_parse_tvtime_command_line( config_t *ct, int argc, char **argv );

int config_key_to_command( config_t *ct, int key );
int config_command_to_key( config_t *ct, int command );

int config_key_to_menu_command( config_t *ct, int key );
int config_menu_command_to_key( config_t *ct, int command );

int config_button_to_command( config_t *ct, int button );
int config_button_to_menu_command( config_t *ct, int button );

int config_get_verbose( config_t *ct );
int config_get_send_fields( config_t *ct );
const char *config_get_output_driver( config_t *ct );
int config_get_fullscreen_position( config_t *ct );
int config_get_debug( config_t *ct );
int config_get_outputheight( config_t *ct );
int config_get_useposition( config_t *ct );
int config_get_output_x( config_t *ct );
int config_get_output_y( config_t *ct );
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
unsigned int config_get_channel_text_rgb( config_t *ct );
unsigned int config_get_other_text_rgb( config_t *ct );
int config_get_fullscreen( config_t *ct );
int config_get_priority( config_t *ct );
uid_t config_get_uid( config_t *ct );
const char *config_get_deinterlace_method( config_t *ct );
int config_get_start_channel( config_t *ct );
int config_get_prev_channel( config_t *ct );
const char *config_get_screenshot_dir( config_t *ct );
const char *config_get_rvr_filename( config_t *ct );
int config_get_framerate_mode( config_t *ct );
int config_get_slave_mode( config_t *ct );
const char *config_get_mixer_device( config_t *ct );

double config_get_overscan( config_t *ct );

int config_get_check_freq_present( config_t *ct );

int config_get_usevbi( config_t *ct );
char *config_get_vbidev( config_t *ct );

int config_get_num_modes( config_t *ct );
config_t *config_get_mode_info( config_t *ct, int mode );

void config_save( config_t *ct, const char *name, const char *value );

#ifdef __cplusplus
};
#endif
#endif /* TVTIMECONF_H_INCLUDED */
