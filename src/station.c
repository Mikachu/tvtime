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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "station.h"
#include "bands.h"

typedef struct station_info_s station_info_t;
struct station_info_s
{
    int pos;
    int active;
    char name[32];
    const band_t *band;
    const band_entry_t *channel;

    station_info_t *next;
    station_info_t *prev;
};

struct station_mgr_s
{
    station_info_t *first;
    station_info_t *current;
    int verbose;
    int debug;
    int us_cable_mode;
};

static const band_t *get_band( const char *band )
{
    const band_t *rp;
    
    for( rp = bands; rp < &bands[ numbands ]; ++rp ) {
        if( !strcasecmp( rp->name, band ) ) {
            return rp;
        }
    }

    return 0;
}

static int isFreePos( station_mgr_t *mgr, int pos )
{
    station_info_t *rp = mgr->first;
    if( !rp ) return 1;

    do {
        if( pos == rp->pos ) return 0;
        rp = rp->next;
    } while ( rp != mgr->first );

    return 1;
}

static int getNextPos( station_mgr_t *mgr )
{
    return mgr->first->prev->pos + 1;
}

static station_info_t *newInfo( int pos, const char *name, const band_t *band, const band_entry_t *channel )
{
    station_info_t *i;

    i = malloc( sizeof( station_info_t ) );
    if( i ) {
        i->pos = pos;
        i->active = 1;
        i->channel = channel;
        i->band = band;
        if( name ) {
            snprintf( i->name, 32, "%d [%s]", pos, name );
        } else {
            sprintf( i->name, "%d", pos );
        }
        // strncpy( i->name, name, 32 );
        i->name[ 31 ] = '\0';

        i->next = 0;
        i->prev = 0;
        return i;
    }

    return 0;
}

static int insert( station_mgr_t *mgr, station_info_t *i )
{
    if( !mgr->first ) {
        mgr->first = i;
        mgr->first->next = mgr->first;
        mgr->first->prev = mgr->first;
    } else {
        station_info_t *rp = mgr->first;

        do {
            if( rp->pos == i->pos ) {
                fprintf( stderr, "station: Position %d already in use.\n", i->pos );
                return 0;
            }
            
            if( rp->pos > i->pos ) break;

            rp = rp->next;
        } while( rp != mgr->first );

        i->next = rp;
        i->prev = rp->prev;
        rp->prev->next = i;
        rp->prev = i;

        if( rp == mgr->first && i->pos < mgr->first->pos ) {
            mgr->first = i;
        }
    }

    return 1;
}

static void station_dump( station_mgr_t *mgr )
{
    station_info_t *rp = mgr->first;
    if( !rp ) return;

    fprintf( stderr, "#\tBand\t\tChannel\tFreq\tName\n" );
    do {
        fprintf( stderr, "%d\t%s\t%s\t%d\t%s\n",rp->pos, rp->band->name,
            rp->channel->name, rp->channel->freq, rp->name);
        rp = rp->next;
    } while( rp != mgr->first );
}

station_mgr_t *station_init( config_t *ct )
{
    station_mgr_t *mgr = (station_mgr_t *) malloc( sizeof( station_mgr_t ) );
    const char *frequencies;

    if( !mgr ) return 0;

    mgr->debug = config_get_debug( ct );
    mgr->verbose = config_get_verbose( ct );
    mgr->first = 0;
    mgr->current = 0;
    mgr->us_cable_mode = 0;

    frequencies = config_get_v4l_freq( ct );

    // this is a config file parser  (no it isn't!) ;-)
/*
    station_add( 1, "vhf E2-E12", "e4", "ard" );
    station_add( 2, "VHF E2-E12", "E8", "zdf" );
    station_add( 3, "VHF E2-E12", "E6", "ndr" );
*/

    station_add_band( mgr, "us cable" );

    mgr->current = mgr->first;
    if( mgr->verbose ) {
        station_dump( mgr );
    }

    return mgr;
}


int station_set( station_mgr_t *mgr, int pos )
{
    station_info_t *rp = mgr->first;
    if( !mgr->first ) return 0;

    do {
        if( rp->pos == pos ) {
            mgr->current = rp;
            return 1;
        }
        rp = rp->next;
    } while( rp != mgr->first );

    return 0;
}

void station_next( station_mgr_t *mgr )
{
    if( mgr->current ) {
        mgr->current = mgr->current->next;
    }
}

void station_prev( station_mgr_t *mgr )
{
    if( mgr->current ) {
        mgr->current = mgr->current->prev;
    }
}

int station_get_current_id( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return 0;
    } else {
        return mgr->current->pos;
    }
}

const char *station_get_current_channel_name( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return "No Frequency";
    } else {
        return mgr->current->name;
    }
}

const char *station_get_current_band( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return "No Band";
    } else {
        return mgr->current->band->name;
    }
}

void station_set_us_cable_mode( station_mgr_t *mgr, int us_cable )
{
    mgr->us_cable_mode = us_cable;
}

int station_get_current_frequency( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return 0;
    } else {
        if( mgr->current->band == us_cable_band ) {
            if( mgr->us_cable_mode == US_CABLE_HRC ) {
                return NTSC_CABLE_HRC( mgr->current->channel->freq );
            } else if( mgr->us_cable_mode == US_CABLE_IRC ) {
                return NTSC_CABLE_IRC( mgr->current->channel->freq );
            }
        }

        return mgr->current->channel->freq;
    }
}

int station_add( station_mgr_t *mgr, int pos, const char *bandname, const char *channel, const char *name )
{
    const band_t *band = get_band( bandname );
    const band_entry_t *entry;
    station_info_t *info;

    if( !band ) {
        return 0;
    }

    if( !isFreePos( mgr, pos ) ) pos = getNextPos( mgr );

    for( entry = band->channels; entry < &(band->channels[ band->count ]); entry++ ) {
        if( !strcasecmp( entry->name, channel ) ) {
            info = newInfo( pos, name, band, entry );
            if( info ) insert( mgr, info );
            return 1;
        }
    }
    return 0;
}

int station_add_band( station_mgr_t *mgr, const char *bandname )
{
    const band_t *band = get_band( bandname );
    int i;

    if( !band ) {
        return 0;
    }

    for( i = 0; i < band->count; i++ ) {
        station_info_t *info;
        int pos;

        if( !sscanf( band->channels[ i ].name, "%d", &pos ) || !isFreePos( mgr, pos ) ) {
            pos = getNextPos( mgr );
        }

        info = newInfo( pos, 0, band, &(band->channels[ i ]) );
        if( info ) insert( mgr, info );
    }

    return 1;
}

int station_scan( station_mgr_t *mgr )
{
    /* Billy:
     * I think the scanner should go through the current list of
     * stations and just mark the 'active' bit if there is signal
     * on the channel.
     */
    /* Achim:
     * I think it should work like add_band, but only add if there
     * is a signal...
     */
    /* Billy:
     * I disagree.  I want to start with a list of all the US cable
     * frequencies, then use scan to disable all of the ones from
     * the active list that don't have signal, but still let me switch
     * to them manually.
     */
    return 0;
}

int station_writeConfig( config_t *ct )
{
    return 0;
}

