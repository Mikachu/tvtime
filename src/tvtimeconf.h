/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 * Copyright (C) 2003 Alexander Belov <asbel@mail.ru>
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

/**
 * The config object represents the data contained in the tvtime config
 * file and specified on the command line, and also provides an API for
 * writing information back to a config file.
 *
 * However, these are separate: the read data is immutable, and so
 * values written using this API may not be later queried by the
 * application.
 *
 * This object handles parsing the command line and therefore also
 * prints the usage message for tvtime, tvtime-command, tvtime-scanner
 * and tvtime-configure.
 */

typedef struct config_s config_t;

/**
 * Create a new config object.  This will initialize the internal data
 * structures and attempt to read the default config file from the
 * config file in $sysconfdir/tvtime/tvtime.xml and then override these
 * with the values in $HOME/.tvtime/tvtime.xml.
 */
config_t *config_new( void );

/**
 * Deletes the config file object, freeing all internal data and closing
 * the config file writer.
 */
void config_delete( config_t *ct );

/**
 * This is the API to the config file saving.  This writes the option
 * with the given name and new value to the config file in $HOME, or if
 * the --configfile parameter is used, to that file instead.
 */
void config_save( config_t *ct, const char *name, const char *value );

/**
 * Parses the command line for the 'tvtime' command.
 */
int config_parse_tvtime_command_line( config_t *ct, int argc, char **argv );

/**
 * Parses the command line for the 'tvtime-configure' command.
 */
int config_parse_tvtime_config_command_line( config_t *ct, int argc, char **argv );

/**
 * Parses the command line for the 'tvtime-scanner' command.
 */
int config_parse_tvtime_scanner_command_line( config_t *ct, int argc, char **argv );

/**
 * Returns the tvtime command corresponding to the given key event.
 */
int config_key_to_command( config_t *ct, int key );

/**
 * Returns the argument to give the tvtime command corresponding to the
 * given key event.
 */
const char *config_key_to_command_argument( config_t *ct, int key );

/**
 * Given a tvtime command, this returns the first bound key event listed
 * in the config file.
 */
int config_command_to_key( config_t *ct, int command );

/**
 * Returns the tvtime menu command corresponding to a key.  This is for
 * mapping keys when we're in menu mode, so that keys can be doubled-up
 * between menu and normal mode (for example, the arrow keys).
 */
int config_key_to_menu_command( config_t *ct, int key );

/**
 * Returns the argument to give the tvtime menu command corresponding to
 * the given key event.
 */
const char *config_key_to_menu_command_argument( config_t *ct, int key );

/**
 * Given a tvtime menu command, this returns the first bound key event
 * listed in the config file.
 */
int config_menu_command_to_key( config_t *ct, int command );

/**
 * Given a mouse button, this returns the tvtime command bound to that
 * button press event.
 */
int config_button_to_command( config_t *ct, int button );

/**
 * Given a mouse button, this returns the argument to give the tvtime
 * command bound to that button press event.
 */
const char *config_button_to_command_argument( config_t *ct, int button );

/**
 * Given a mouse button, this returns the tvtime menu command bound to
 * that button press event.
 */
int config_button_to_menu_command( config_t *ct, int button );

/**
 * Given a mouse button, this returns the argument to give the tvtime
 * menu command bound to that button press event.
 */
const char *config_button_to_menu_command_argument( config_t *ct, int button );

/**
 * The following represents all of the information stored in a tvtime
 * config file.
 */

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
const char *config_get_vbi_device( config_t *ct );
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
int config_get_save_restore_picture( config_t *ct );
int config_get_global_brightness( config_t *ct );
int config_get_global_contrast( config_t *ct );
int config_get_global_colour( config_t *ct );
int config_get_global_hue( config_t *ct );
const char *config_get_audio_mode( config_t *ct );
const char *config_get_xmltv_file( config_t *ct );
double config_get_overscan( config_t *ct );
int config_get_check_freq_present( config_t *ct );
int config_get_usexds( config_t *ct );
char *config_get_vbidev( config_t *ct );
int config_get_invert( config_t *ct );
int config_get_cc( config_t *ct );
int config_get_mirror( config_t *ct );

int config_get_num_modes( config_t *ct );
config_t *config_get_mode_info( config_t *ct, int mode );

#ifdef __cplusplus
};
#endif
#endif /* TVTIMECONF_H_INCLUDED */
