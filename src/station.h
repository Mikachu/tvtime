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

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * NTSC cable modes.
 */
#define NTSC_CABLE_MODE_NOMINAL 0
#define NTSC_CABLE_MODE_IRC     1
#define NTSC_CABLE_MODE_HRC     2

typedef struct station_mgr_s station_mgr_t;

station_mgr_t *station_new( const char *table, int us_cable_mode, int verbose );
void station_delete( station_mgr_t *mgr );

/**
 * Set the current channel.
 */
int station_set( station_mgr_t *mgr, int pos );
void station_next( station_mgr_t *mgr );
void station_prev( station_mgr_t *mgr );
void station_last( station_mgr_t *mgr );

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
 * The last channel we were at, before the current one.
 */
int station_get_prev_id( station_mgr_t *mgr );

/**
 * Add or update the channel list.
 */
int station_add( station_mgr_t *mgr, int pos, const char *band, const char *channel, const char *name );

/**
 * Add a whole band to the channel list.
 */
int station_add_band( station_mgr_t *mgr, const char *band );

/**
 * Activates or deactivates the current station from the list.
 */
int station_set_current_active( station_mgr_t *mgr, int active );

/**
 * Writes out the current station config.
 */
int station_writeconfig( station_mgr_t *mgr );

#ifdef __cplusplus
};
#endif
#endif // STATION_H_INCLUDED
