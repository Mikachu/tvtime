/**
 * Copyright (c) 2003 Achim Schneider <batchall@mordor.ch>
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

#ifndef STATION_H_INCLUDED
#define STATION_H_INCLUDED

#include "tvtimeconf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define US_CABLE_NOMINAL 	0
#define US_CABLE_HRC		1
#define US_CABLE_IRC		2

int station_init( config_t *ct );
int station_writeConfig( config_t *ct );

/**
 * Set the current channel.
 */
int station_set( int pos );
void station_next( void );
void station_prev( void );

/**
 * Set the us cable mode (global).
 */
void station_set_us_cable_mode( int us_cable );

/**
 * Information about the current channel.
 */
int station_get_current_id( void );
const char *station_get_current_channel_name( void );
const char *station_get_current_band( void );
int station_get_current_frequency( void );

/**
 * Add or update the channel list.
 */
int station_add( int pos, const char *band, const char *channel, const char *name );
int station_add_band( const char *band );
int station_scan( void );

#ifdef __cplusplus
};
#endif
#endif // STATION_H_INCLUDED
