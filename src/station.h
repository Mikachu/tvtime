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

typedef struct station_mgr_s station_mgr_t;

station_mgr_t *station_init( config_t *ct );

/**
 * Set the current channel.
 */
int station_set( station_mgr_t *mgr, int pos );
void station_next( station_mgr_t *mgr );
void station_prev( station_mgr_t *mgr );

/**
 * Set the us cable mode (global).
 */
void station_toggle_us_cable_mode( station_mgr_t *mgr );

/**
 * Information about the current channel.
 */
int station_get_current_id( station_mgr_t *mgr );
const char *station_get_current_channel_name( station_mgr_t *mgr );
const char *station_get_current_band( station_mgr_t *mgr );
int station_get_current_frequency( station_mgr_t *mgr );
int station_get_current_active( station_mgr_t *mgr );

/**
 * Add or update the channel list.
 */
int station_add( station_mgr_t *mgr, int pos, const char *band, const char *channel, const char *name );
// adds a whole band
int station_add_band( station_mgr_t *mgr, const char *band );
// adds all channels with signal 
int station_scan_band( station_mgr_t *mgr, const char *band );
// (de)activates current station
int station_toggle_curr( station_mgr_t *mgr );
// scans all stations and (de)activates
int station_scan( station_mgr_t *mgr );

#ifdef __cplusplus
};
#endif
#endif // STATION_H_INCLUDED
