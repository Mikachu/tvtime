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

typedef struct config_s config_t;

/**
 * Input commands for keymap
 */
enum tvtime_commands
{
    TVTIME_NOCOMMAND,
    TVTIME_QUIT,
    TVTIME_CHANNEL_UP,
    TVTIME_CHANNEL_DOWN,
    TVTIME_LUMA_UP,
    TVTIME_LUMA_DOWN,
    TVTIME_MIXER_MUTE,
    TVTIME_MIXER_UP,
    TVTIME_MIXER_DOWN,
    TVTIME_ENTER,
    TVTIME_CHANNEL_CHAR,

    TVTIME_HUE_DOWN,
    TVTIME_HUE_UP,
    TVTIME_BRIGHT_DOWN,
    TVTIME_BRIGHT_UP,
    TVTIME_CONT_DOWN,
    TVTIME_CONT_UP,
    TVTIME_COLOUR_DOWN,
    TVTIME_COLOUR_UP,

    TVTIME_SHOW_BARS,
    TVTIME_SHOW_TEST,
    TVTIME_DEBUG,

    TVTIME_FULLSCREEN,
    TVTIME_ASPECT,
    TVTIME_SCREENSHOT,
    TVTIME_DEINTERLACINGMODE,

    TVTIME_MENUMODE,

    TVTIME_LAST
};


config_t *config_new( int argc, char **argv );
void config_delete( config_t *ct );
int config_dump( config_t *ct );
int config_key_to_command( config_t *ct, int key );

int config_get_verbose( config_t *ct );
int config_get_debug( config_t *ct );
int config_get_outputwidth( config_t *ct );
int config_get_inputwidth( config_t *ct );
int config_get_aspect( config_t *ct );
int config_get_inputnum( config_t *ct );
int config_get_tuner_number( config_t *ct );
int config_get_apply_luma_correction( config_t *ct );
double config_get_luma_correction( config_t *ct );
const char *config_get_v4l_device( config_t *ct );
const char *config_get_v4l_norm( config_t *ct );
const char *config_get_v4l_freq( config_t *ct );
const char *config_get_timeformat( config_t *ct );

void config_set_verbose( config_t *ct, int verbose );
void config_set_debug( config_t *ct, int debug );
void config_set_outputwidth( config_t *ct, int outputwidth );
void config_set_inputwidth( config_t *ct, int inputwidth );
void config_set_aspect( config_t *ct, int aspect );
void config_set_inputnum( config_t *ct, int inputnum );
void config_set_apply_luma_correction( config_t *ct, int apply_luma_correction );
void config_set_luma_correction( config_t *ct, double luma_correction );
void config_set_v4l_device( config_t *ct, const char *v4ldev );
void config_set_v4l_norm( config_t *ct, const char *v4lnorm );
void config_set_v4l_freq( config_t *ct, const char *v4lfreq );
void config_set_tuner_number( config_t *ct, int tuner_number );
void config_set_timeformat( config_t *ct, const char *format );

#endif /* TVTIMECONF_H_INCLUDED */
