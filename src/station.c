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

static station_info_t *first = 0;
static station_info_t *current = 0;
static int verbose = 0;
static int debug = 0;
static int us_cable_mode = 0;

const band_t *get_band( const char *band )
{
    const band_t *rp;
    
    for( rp = bands; rp < &bands[ numbands ]; ++rp ) {
        if( !strcasecmp( rp->name, band ) ) {
            return rp;
        }
    }

    return 0;
}

int isFreePos( int pos )
{
    station_info_t *rp = first;
    if( !rp ) return 1;

    do {
        if( pos == rp->pos ) return 0;
        rp = rp->next;
    } while ( rp != first );

    return 1;
}

int getNextPos( void )
{
    return first->prev->pos + 1;
}

station_info_t *newInfo( int pos, const char *name, const band_t *band, const band_entry_t *channel )
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

int insert( station_info_t *i )
{
    if( !first ) {
        first = i;
        first->next = first;
        first->prev = first;
    } else {
        station_info_t *rp = first;

        do {
            if( rp->pos == i->pos ) {
                fprintf( stderr, "station: Position %d already in use.\n", i->pos );
                return 0;
            }
            
            if( rp->pos > i->pos ) break;

            rp = rp->next;
        } while( rp != first );

        i->next = rp;
        i->prev = rp->prev;
        rp->prev->next = i;
        rp->prev = i;

        if( rp == first && i->pos < first->pos ) {
            first = i;
        }
    }

    return 1;
}

void station_dump( void )
{
    station_info_t *rp = first;
    if( !rp ) return;

    fprintf( stderr, "#\tBand\t\tChannel\tFreq\tName\n" );
    do {
        fprintf( stderr, "%d\t%s\t%s\t%d\t%s\n",rp->pos, rp->band->name,
            rp->channel->name, rp->channel->freq, rp->name);
        rp = rp->next;
    } while( rp != first );
}

int station_init( config_t *ct )
{
    debug = config_get_debug( ct );
    verbose = config_get_verbose( ct );

    // this is a config file parser  (no it isn't!) ;-)
/*
    station_add( 1, "vhf E2-E12", "e4", "ard" );
    station_add( 2, "VHF E2-E12", "E8", "zdf" );
    station_add( 3, "VHF E2-E12", "E6", "ndr" );
*/

    station_add_band( "us cable" );

    current = first;
    if( verbose ) {
        station_dump();
    }

    return 1;
}


int station_set( int pos )
{
    station_info_t *rp = first;
    if( !first ) return 0;

    do {
        if( rp->pos == pos ) {
            current = rp;
            return 1;
        }
        rp = rp->next;
    } while( rp != first );

    return 0;
}

void station_next( void )
{
    if( current ) {
        current = current->next;
    }
}

void station_prev( void )
{
    if( current ) {
        current = current->prev;
    }
}

int station_get_current_id( void )
{
    if( !current ) {
        return 0;
    } else {
        return current->pos;
    }
}

const char *station_get_current_channel_name( void )
{
    if( !current ) {
        return "No Frequency";
    } else {
        return current->name;
    }
}

const char *station_get_current_band( void )
{
    if( !current ) {
        return "No Band";
    } else {
        return current->band->name;
    }
}

void station_set_us_cable_mode( int us_cable )
{
    us_cable_mode = us_cable;
}

int station_get_current_frequency( void )
{
    if( !current ) {
        return 0;
    } else {
        if( current->band == us_cable_band ) {
            if( us_cable_mode == US_CABLE_HRC ) {
                return NTSC_CABLE_HRC( current->channel->freq );
            } else if( us_cable_mode == US_CABLE_IRC ) {
                return NTSC_CABLE_IRC( current->channel->freq );
            }
        }

        return current->channel->freq;
    }
}

int station_add( int pos, const char *bandname, const char *channel, const char *name )
{
    const band_t *band = get_band( bandname );
    const band_entry_t *entry;
    station_info_t *info;

    if( !band ) {
        return 0;
    }

    if( !isFreePos( pos ) ) pos = getNextPos();

    for( entry = band->channels; entry < &(band->channels[ band->count ]); entry++ ) {
        if( !strcasecmp( entry->name, channel ) ) {
            info = newInfo( pos, name, band, entry );
            if( info ) insert( info );
            return 1;
        }
    }
    return 0;
}

int station_add_band( const char *bandname )
{
    const band_t *band = get_band( bandname );
    int i;

    if( !band ) {
        return 0;
    }

    for( i = 0; i < band->count; i++ ) {
        station_info_t *info;
        int pos;

        if( !sscanf( band->channels[ i ].name, "%d", &pos ) || !isFreePos( pos ) ) {
            pos = getNextPos();
        }

        info = newInfo( pos, 0, band, &(band->channels[ i ]) );
        if( info ) insert( info );
    }

    return 1;
}

int station_scan( void )
{
    /* Billy:
     * I think the scanner should go through the current list of
     * stations and just mark the 'active' bit if there is signal
     * on the channel.
     */
    return 0;
}

int station_writeConfig( config_t *ct )
{
    return 0;
}

