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

config_t *config_new( const char *filename );
int config_init( config_t *ct );
int config_dump( config_t *ct );

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

#endif /* CONFIG_H_INCLUDED */
