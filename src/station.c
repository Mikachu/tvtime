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
#include <libxml/tree.h>
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
    int us_cable_mode;
    int is_us_cable;
    int last_channel;
    int new_install;
    char band_and_frequency[ 1024 ];
    char stationrc[255];
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
    if ( !mgr->first ) return 1;
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
            snprintf( i->name, 32, "%s", name );
        } else {
            sprintf( i->name, "%d", pos );
        }

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
        mgr->current= i;
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
        mgr->current= i;
    }

    return 1;
}

static void station_dump( station_mgr_t *mgr )
{
    station_info_t *rp = mgr->first;
    if( !rp ) return;

    fprintf( stderr, "#\tBand\t\tChannel\tFreq\tActive\tName\n" );
    do {
        fprintf( stderr, "%d\t%s\t%s\t%d\t%s\t%s\n",rp->pos, rp->band->name,
            rp->channel->name, rp->channel->freq, rp->active ? "true" : "false", rp->name);
        rp = rp->next;
    } while( rp != mgr->first );
}

int station_readconfig( station_mgr_t *mgr )
{
    xmlNodePtr cur;
    xmlDocPtr doc;
    xmlNodePtr station;

    doc = xmlParseFile( mgr->stationrc );
    if( !doc ) {
        return 0;
    }

    cur = xmlDocGetRootElement( doc );
    if( !cur ) {
        fprintf( stderr, "station: %s: Empty document.\n", mgr->stationrc );
        xmlFreeDoc( doc );
        return 0;
    }

    if( xmlStrcmp( cur->name, (const xmlChar *) "STATIONRC" ) ) {
        fprintf( stderr, "station: %s: document of the wrong type", mgr->stationrc );
        xmlFreeDoc( doc );
        return 0;
    }

    station = cur->xmlChildrenNode;
    while( station ) {
        if( !xmlStrcmp( station->name, (const xmlChar *) "station" ) ) {
            xmlChar *name = 0;
            xmlChar *active_s = 0;
            xmlChar *pos_s = 0;
            xmlChar *band = 0;
            xmlChar *channel = 0;
            int pos;

            cur = station->xmlChildrenNode;
            while( cur ) {
                if( !xmlStrcmp( cur->name, (const xmlChar *) "name" ) ) {
                    name = xmlNodeGetContent( cur->xmlChildrenNode );
                } else if( !xmlStrcmp( cur->name, (const xmlChar *) "active" ) ) {
                    active_s = xmlNodeGetContent( cur->xmlChildrenNode );
                } else if( !xmlStrcmp( cur->name, (const xmlChar *) "pos" ) ) {
                    pos_s = xmlNodeGetContent( cur->xmlChildrenNode );
                } else if( !xmlStrcmp( cur->name, (const xmlChar *) "band" ) ) {
                    band = xmlNodeGetContent( cur->xmlChildrenNode );
                } else if( !strcmp( cur->name, (const xmlChar *) "channel" ) ) {
                    channel = xmlNodeGetContent( cur->xmlChildrenNode );
                }

                cur = cur->next;
            }
             
            if( pos_s && ( 1 != sscanf( (char *) pos_s, "%d", &pos ) ) ) {
                pos = 0;
            }

            station_add( mgr, pos, (char *) band, (char *) channel, (char *) name );
            station_set_current_active( mgr, !( active_s && ( !xmlStrcmp( active_s, (const xmlChar *) "false" ) ) ) );
        }
        station = station->next;
    }
    xmlFreeDoc( doc );
    return 1;
}

station_mgr_t *station_new( const char *table, int us_cable_mode, int verbose )
{
    station_mgr_t *mgr = (station_mgr_t *) malloc( sizeof( station_mgr_t ) );
    const char *frequencies;

    if( !mgr ) return 0;

    strncpy( mgr->stationrc, getenv( "HOME" ), 235 );
    strncat( mgr->stationrc, "/.tvtime/stationrc", 255 );
    mgr->verbose = verbose;
    mgr->first = 0;
    mgr->current = 0;
    mgr->is_us_cable = 0;
    mgr->us_cable_mode = us_cable_mode;
    mgr->last_channel = 0;
    mgr->new_install = 0;

    if( !station_readconfig( mgr ) ) {
        fprintf( stderr, "station: Errors reading %s\n", mgr->stationrc );

        mgr->new_install = 1;
        frequencies = table;
        fprintf( stderr, "station: Adding frequency table %s, all channels active\n", frequencies );

        if( !strcasecmp( frequencies, "europe-west" ) || !strcasecmp( frequencies, "europe-east" ) ||
            !strcasecmp( frequencies, "europe-cable" ) ) {
            fprintf( stderr, "\n*** Frequency list %s has been changed to be called %s in tvtime 0.9.7.\n",
                     frequencies, "europe" );
            fprintf( stderr, "*** Please update your configuration.  Comments?  tvtime-devel@lists.sourceforge.net\n\n" );
            frequencies = "europe";
        }

        if( !strcasecmp( frequencies, "us-cable" ) ) {
            station_add_band( mgr, "us cable" );
            mgr->is_us_cable = 1;
        } else if( !strcasecmp( frequencies, "us-broadcast" ) ) {
            station_add_band( mgr, "us broadcast" );
        } else if( !strcasecmp( frequencies, "japan-cable" ) ) {
            station_add_band( mgr, "japan cable" );
        } else if( !strcasecmp( frequencies, "japan-broadcast" ) ) {
            station_add_band( mgr, "japan broadcast" );
        } else if( !strcasecmp( frequencies, "russia" ) ) {
            station_add_band( mgr, "vhf russia" );
            station_add_band( mgr, "uhf" );
            station_add_band( mgr, "vhf s1-s41" );
        } else if( !strcasecmp( frequencies, "europe" ) ) {
            station_add_band( mgr, "vhf e2-e12" );
            station_add_band( mgr, "vhf s1-s41" );
            station_add_band( mgr, "vhf misc" );
            station_add_band( mgr, "vhf russia" );
            station_add_band( mgr, "vhf italy" );
            station_add_band( mgr, "vhf ireland" );
            station_add_band( mgr, "uhf" );
        } else if( !strcasecmp( frequencies, "france" ) ) {
            station_add_band( mgr, "vhf france" );
            station_add_band( mgr, "vhf e2-e12" );
            station_add_band( mgr, "vhf s1-s41" );
            station_add_band( mgr, "uhf" );
        } else if( !strcasecmp( frequencies, "australia" ) ) {
            station_add_band( mgr, "vhf australia" );
            station_add_band( mgr, "vhf e2-e12" );
            station_add_band( mgr, "uhf" );
        } else if( !strcasecmp( frequencies, "newzealand" ) ) {
            /* Billy: It's unclear to me if we should keep this around. */
            station_add_band( mgr, "vhf e2-e12" );
            station_add_band( mgr, "uhf" );
        }
        mgr->current = mgr->first;
    }
    
    if( mgr->verbose ) {
        station_dump( mgr );
    }

    return mgr;
}

void station_delete( station_mgr_t *mgr )
{
    free( mgr );
}

int station_is_new_install( station_mgr_t *mgr )
{
    return mgr->new_install;
}

int station_set( station_mgr_t *mgr, int pos )
{
    station_info_t *rp = mgr->first;

    if( rp ) {
        do {
            if( rp->pos == pos ) {
                if( mgr->current == rp ) {
                    return 0;
                }
                mgr->last_channel = station_get_current_id( mgr );
                mgr->current = rp;
                return 1;
            }
            rp = rp->next;
        } while( rp != mgr->first );
    }

    return 0;
}

void station_next( station_mgr_t *mgr )
{
    mgr->last_channel = station_get_current_id( mgr );

    if( mgr->current ) {
        station_info_t *i= mgr->current;
        do {
            mgr->current = mgr->current->next;
        } while ( !mgr->current->active && mgr->current != i ); 
    }
}

void station_prev( station_mgr_t *mgr )
{
    mgr->last_channel = station_get_current_id( mgr );

    if( mgr->current ) {
        station_info_t *i= mgr->current;
        do {
            mgr->current = mgr->current->prev;
        } while ( !mgr->current->active && mgr->current != i );
    }
}

void station_last( station_mgr_t *mgr )
{
    station_set( mgr, mgr->last_channel );
}

int station_get_current_id( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return 0;
    } else {
        return mgr->current->pos;
    }
}

int station_get_prev_id( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return 0;
    } else {
        return mgr->last_channel;
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
        if( mgr->is_us_cable && mgr->us_cable_mode ) {
            sprintf( mgr->band_and_frequency, "%s [%s]: %s",
                     mgr->current->band->name,
                     mgr->us_cable_mode == NTSC_CABLE_MODE_HRC ? "HRC" : "IRC",
                     mgr->current->channel->name );
        } else {
            sprintf( mgr->band_and_frequency, "%s: %s",
                     mgr->current->band->name, mgr->current->channel->name );
        }
        return mgr->band_and_frequency;
    }
}

void station_toggle_us_cable_mode( station_mgr_t *mgr )
{
    mgr->us_cable_mode = (mgr->us_cable_mode + 1) % 3;
}

int station_get_current_frequency( station_mgr_t *mgr )
{
    if( !mgr->current ) {
        return 0;
    } else {
        if( mgr->current->band == us_cable_band ) {
            if( mgr->us_cable_mode == NTSC_CABLE_MODE_HRC ) {
                return NTSC_CABLE_HRC( mgr->current->channel->freq );
            } else if( mgr->us_cable_mode == NTSC_CABLE_MODE_IRC ) {
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

int station_set_current_active( station_mgr_t *mgr, int active )
{
    if( mgr->current ) {
        mgr->current->active = active;
        return 1;
    }
    return 0;
   
}

int station_get_current_active( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->active;
    }
    return 0;
}


int station_remove( station_mgr_t *mgr )
{ // untested
    station_info_t *i= mgr->current;
    if( !mgr->current ) return 0;
    i->next->prev= i->prev;
    i->prev->next= i->next;
    
    if( i == mgr->first ) {
        mgr->first= i->next;
    }
    
    if( i == mgr->current ) {
        mgr->current= i->next;
    }

    if( i == i->next ) {
        mgr->current= NULL;
        mgr->first= NULL;
    }
    free( i );
    return 1;
}

station_info_t *ripout( station_mgr_t *mgr, int pos ) {
    station_info_t *rp = mgr->first;

    do {
        if( pos == rp->pos ) break;
        rp = rp->next;
    } while( rp != mgr->first );

    rp->next->prev= rp->prev;
    rp->prev->next= rp->next;
    
    if( rp == mgr->first ) {
        mgr->first= rp->next;
    }
    
    if( rp == mgr->current ) {
        mgr->current= rp->next;
    }

    if( rp == rp->next ) {
        mgr->current= NULL;
        mgr->first= NULL;
    }

    return rp;
}


int station_remap( station_mgr_t *mgr, int pos )
{
    int res;
    if( !mgr->current ) return 0;
    if( pos == mgr->current->pos ) return 1;

    if( isFreePos( mgr, pos ) ) {
        station_info_t *i = ripout( mgr, mgr->current->pos );
        i->pos = pos;
        res= insert( mgr, i );

    } else {
        station_info_t *old = ripout( mgr, pos );
        station_info_t *i = ripout( mgr, mgr->current->pos );
        
        old->pos= i->pos;
        i->pos = pos;
        res= insert( mgr, old ) && insert( mgr, i );
    }
    if ( mgr->verbose ) {
        station_dump( mgr );
    }
    return res;
}

int station_writeconfig( station_mgr_t *mgr)
{
    xmlDocPtr doc;
    xmlNodePtr tree, subtree;
    char buf[ 255 ];
    station_info_t *rp = mgr->first;

    if( !rp ) {
        return 1;
    }

    doc = xmlNewDoc( (const xmlChar *) "1.0" );
    doc->children = xmlNewDocNode( doc, 0, (const xmlChar *) "STATIONRC", 0 );

    sprintf( buf, "%d", mgr->current->pos );

    do {
        tree = xmlNewChild( doc->children, 0, (const xmlChar *) "station", 0 );

        subtree = xmlNewChild( tree, 0, (const xmlChar *) "name", (const xmlChar *) rp->name );

        subtree = xmlNewChild( tree, 0, (const xmlChar *) "active", (const xmlChar *) (rp->active ? "true" : "false") );

        sprintf( buf, "%d", rp->pos );
        subtree = xmlNewChild( tree, 0, (const xmlChar *) "pos", (const xmlChar *) buf );

        subtree = xmlNewChild( tree, 0, (const xmlChar *) "band", (const xmlChar *) rp->band->name );
        subtree = xmlNewChild( tree, 0, (const xmlChar *) "channel", (const xmlChar *) rp->channel->name );
        rp = rp->next;
    } while( rp != mgr->first );

    strncpy( buf, getenv( "HOME" ), 235 );
    strncat( buf, "/.tvtime/stationrc", 255 );

    return xmlSaveFormatFile( buf, doc, 1);
}

// vim: set et
