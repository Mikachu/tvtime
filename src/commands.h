/**
 * Copyright (C) 2002, 2003 Doug Bell <drbell@users.sourceforge.net>
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
#include "menu.h"

commands_t *commands_new( config_t *cfg, videoinput_t *vidin, 
                          station_mgr_t *mgr, tvtime_osd_t *osd,
                          int fieldtime );
void commands_delete( commands_t *cmd );

void commands_handle( commands_t *cmd, int command, const char *arg );

void commands_add_menu( commands_t *cmd, menu_t *menu );
menu_t *commands_get_menu( commands_t *cmd, const char *menuname );
void commands_refresh_menu( commands_t *cmd );
int commands_menu_active( commands_t *cmd );
void commands_set_station_mgr( commands_t *cmd, station_mgr_t *mgr );

double commands_get_luma_power( commands_t *cmd );
double commands_get_overscan( commands_t *cmd );
double commands_get_overscan( commands_t *cmd );
int commands_get_framerate( commands_t *cmd );
void commands_set_framerate( commands_t *cmd, int framerate );
void commands_set_console( commands_t *cmd, console_t *con );
void commands_set_vbidata( commands_t *cmd, vbidata_t *con );
int commands_in_menu( commands_t *cmd );

int commands_console_on( commands_t *cmd );

void commands_set_half_size( commands_t *cmd, int halfsize );
void commands_next_frame( commands_t *cmd );
int commands_pause( commands_t *cmd );
int commands_check_freq_present( commands_t *cmd );
void commands_set_pulldown_alg( commands_t *cmd, int pulldown_alg );

/**
 * These are commands sent back to tvtime to tell it to do things.
 */
int commands_quit( commands_t *cmd );
int commands_toggle_fullscreen( commands_t *cmd );
int commands_toggle_aspect( commands_t *cmd );
int commands_toggle_deinterlacer( commands_t *cmd );
int commands_toggle_pulldown_detection( commands_t *cmd );
int commands_toggle_mode( commands_t *cmd );
int commands_toggle_matte( commands_t *cmd );
int commands_toggle_alwaysontop( commands_t *cmd );
int commands_show_bars( commands_t *cmd );
int commands_print_debug( commands_t *cmd );
int commands_take_screenshot( commands_t *cmd );
const char *commands_screenshot_filename( commands_t *cmd );
int commands_show_deinterlacer_info( commands_t *cmd );
int commands_scan_channels( commands_t *cmd );
int commands_resize_window( commands_t *cmd );
int commands_update_luma_power( commands_t *cmd );
int commands_restart_tvtime( commands_t *cmd );
int commands_apply_luma_correction( commands_t *cmd );
int commands_apply_colour_invert( commands_t *cmd );
int commands_apply_mirror( commands_t *cmd );
int commands_apply_chroma_kill( commands_t *cmd );
int commands_set_deinterlacer( commands_t *cmd );
const char *commands_get_new_deinterlacer( commands_t *cmd );
const char *commands_get_new_norm( commands_t *cmd );
int commands_get_new_input_width( commands_t *cmd );
int commands_get_global_brightness( commands_t *cmd );
int commands_get_global_contrast( commands_t *cmd );
int commands_get_global_colour( commands_t *cmd );
int commands_get_global_hue( commands_t *cmd );
int commands_set_freq_table( commands_t *cmd );
const char *commands_get_new_freq_table( commands_t *cmd );
int commands_sleeptimer( commands_t *cmd );
int commands_sleeptimer_do_shutdown( commands_t *cmd );
const char *commands_get_matte_mode( commands_t *cmd );
const char *commands_get_fs_pos( commands_t *cmd );
int commands_get_audio_boost( commands_t *cmd );

#ifdef __cplusplus
};
#endif
#endif /* COMMANDS_H_INCLUDED */
