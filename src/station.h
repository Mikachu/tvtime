/**
 * Copyright (c) 2003 Achim Schneider <batchall@mordor.ch>
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

#ifndef STATION_H_INCLUDED
#define STATION_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The station manager object handles the list of stations for the
 * tuner.  This gives us the frequencies, handles renumbering, and
 * stores user information about the channel like its name and call
 * letters.
 *
 * Station information is stored in the stationlist.xml file, and
 * changes made at runtime are saved.
 */

/** 
 * NTSC cable modes.
 */
#define NTSC_CABLE_MODE_STANDARD 0
#define NTSC_CABLE_MODE_IRC      1
#define NTSC_CABLE_MODE_HRC      2

typedef struct station_mgr_s station_mgr_t;

/**
 * Creates a new station manager object.  This will look up the given
 * 'norm' and 'table' in the stationlist.xml.  If not found, it will use
 * the default list for the table name.  Tables with defaults are:
 *
 * us-cable, us-broadcast, japan-cable, japan-broadcast, russia, europe,
 * france, australia, australia-optus, newzealand
 */
station_mgr_t *station_new( const char *norm, const char *table,
                            int us_cable_mode, int verbose );

/**
 * This will free the station manager object, but will not write any
 * unsaved changes to the stationlist file.
 */
void station_delete( station_mgr_t *mgr );

/**
 * Is this a clean startup, that is, was there no entry in the
 * stationlist for this frequency table?  If so, tvtime will start its
 * channel scanner.
 */
int station_is_new_install( station_mgr_t *mgr );

/**
 * Set the current channel directly to the given position.
 */
int station_set( station_mgr_t *mgr, int pos );

/**
 * Change up one channel in the list.
 */
void station_up( station_mgr_t *mgr );

/**
 * Change down one channel in the list.
 */
void station_down( station_mgr_t *mgr );

/**
 * Change to the last channel we were at before the current one.
 */
void station_prev( station_mgr_t *mgr );

/**
 * Set the us cable mode.
 */
void station_toggle_us_cable_mode( station_mgr_t *mgr );

/**
 * Information about the current channel.
 */
int station_get_current_id( station_mgr_t *mgr );
int station_get_current_pos( station_mgr_t *mgr );
const char *station_get_current_channel_name( station_mgr_t *mgr );
const char *station_get_current_band( station_mgr_t *mgr );
int station_get_current_frequency( station_mgr_t *mgr );
int station_get_current_active( station_mgr_t *mgr );

/**
 * The last channel we were at, before the current one.
 */
int station_get_prev_id( station_mgr_t *mgr );

/**
 * Returns how many stations there are in the list.
 */
int station_get_num_stations( station_mgr_t *mgr );

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
 * Re-activates all channels.
 */
void station_activate_all_channels( station_mgr_t *mgr );

/**
 * Remap the current channel to the new position.
 */
int station_remap( station_mgr_t *mgr, int pos );

/**
 * Writes out the current station config.
 */
int station_writeconfig( station_mgr_t *mgr );

#ifdef __cplusplus
};
#endif
#endif // STATION_H_INCLUDED
