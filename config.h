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

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

typedef struct config_s config_t;

/**
 * Input commands for keymap
 */
enum tvtime_commands
{
    TVTIME_NOCOMMAND     = 0,
    TVTIME_QUIT          = (1<<0),
    TVTIME_CHANNEL_UP    = (1<<1),
    TVTIME_CHANNEL_DOWN  = (1<<2),
    TVTIME_LUMA_UP       = (1<<3),
    TVTIME_LUMA_DOWN     = (1<<4),
    TVTIME_MIXER_MUTE    = (1<<5),
    TVTIME_MIXER_UP      = (1<<6),
    TVTIME_MIXER_DOWN    = (1<<7),
    TVTIME_ENTER         = (1<<8),
    TVTIME_CHANNEL_CHAR  = (1<<9),

    TVTIME_HUE_DOWN      = (1<<10),
    TVTIME_HUE_UP        = (1<<11),
    TVTIME_BRIGHT_DOWN   = (1<<12),
    TVTIME_BRIGHT_UP     = (1<<13),
    TVTIME_CONT_DOWN     = (1<<14),
    TVTIME_CONT_UP       = (1<<15),
    TVTIME_COLOUR_DOWN   = (1<<16),
    TVTIME_COLOUR_UP     = (1<<17),

    TVTIME_SHOW_BARS     = (1<<18),
    TVTIME_SHOW_TEST     = (1<<19),
    TVTIME_DEBUG         = (1<<20)
};
/**
 * Update this when adding to tvtime_commands
 */
#define KEYMAP_SIZE 22


config_t *config_new( int argc, char **argv );
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

#endif /* CONFIG_H_INCLUDED */
