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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libxml/tree.h>
#include "station.h"
#include "bands.h"
#include "utils.h"

static band_t custom_band = { "Custom", 0, 0 };

typedef struct station_info_s station_info_t;
struct station_info_s
{
    int pos;
    int active;
    char name[ 32 ];
    char network_name[ 33 ];
    char network_call_letters[ 7 ];
    char norm[ 10 ];
    char xmltvid[ 128 ];
    const band_t *band;
    const band_entry_t *channel;
    int brightness;
    int contrast;
    int colour;
    int hue;
    int finetune;

    station_info_t *next;
    station_info_t *prev;
};

struct station_mgr_s
{
    station_info_t *first;
    station_info_t *current;
    int num_stations;
    int max_position;
    int verbose;
    int us_cable_mode;
    int last_channel;
    int new_install;
    char band_and_frequency[ 1024 ];
    char stationrc[ 255 ];
    char *norm;
    char *table;
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

    if( rp ) {
        do {
            if( pos == rp->pos ) return 0;
            rp = rp->next;
        } while ( rp != mgr->first );
    }

    return 1;
}

static int getNextPos( station_mgr_t *mgr )
{
    if( !mgr->first ) return 1;
    return mgr->first->prev->pos + 1;
}

static station_info_t *station_info_new( int pos, const char *name,
                                         const band_t *band,
                                         const band_entry_t *channel )
{
    station_info_t *i = malloc( sizeof( station_info_t ) );

    if( i ) {
        i->pos = pos;
        i->active = 1;
        i->channel = channel;
        i->band = band;
        memset( i->network_name, 0, sizeof( i->network_name ) );
        memset( i->network_call_letters, 0, sizeof( i->network_call_letters ) );
        memset( i->norm, 0, sizeof( i->norm ) );
        memset( i->xmltvid, 0, sizeof( i->xmltvid ) );
        i->brightness = -1;
        i->contrast = -1;
        i->colour = -1;
        i->hue = -1;
        i->finetune = 0;

        if( name ) {
            snprintf( i->name, sizeof( i->name ), "%s", name );
        } else {
            snprintf( i->name, sizeof( i->name ), "%d", pos );
        }

        i->next = 0;
        i->prev = 0;
        return i;
    }

    return 0;
}

static int insert( station_mgr_t *mgr, station_info_t *i, int allowdup )
{
    mgr->num_stations++;

    if( i->pos > mgr->max_position ) {
        mgr->max_position = i->pos;
    }

    if( !mgr->first ) {
        mgr->first = i;
        mgr->first->next = mgr->first;
        mgr->first->prev = mgr->first;
        mgr->current = i;
    } else {
        station_info_t *rp = mgr->first;

        do {
            if( rp->pos == i->pos ) {
                if( mgr->verbose ) {
                    fprintf( stderr, "station: Position %d already in use.\n",
                             i->pos );
                }
                return 0;
            }

            if( !allowdup ) {
                if( rp->channel->freq == i->channel->freq ) {
                    if( mgr->verbose ) {
                        fprintf( stderr, "station: Frequency %d already in use.\n",
                                 i->channel->freq );
                    }
                    return 0;
                }
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
        mgr->current = i;
    }

    return 1;
}

static xmlNodePtr find_list( xmlNodePtr node, const char *normname,
                             const char *tablename )
{
    while( node ) {
        if( !xmlStrcasecmp( node->name, BAD_CAST "list" ) ) {
            xmlChar *norm = xmlGetProp( node, BAD_CAST "norm" );

            if( norm && !xmlStrcasecmp( norm, BAD_CAST normname ) ) {
                xmlChar *table = xmlGetProp( node, BAD_CAST "frequencies" );
                if( table && !xmlStrcasecmp( table, BAD_CAST tablename ) ) {

                    xmlFree( norm );
                    xmlFree( table );
                    return node;
                }
                if( table ) xmlFree( table );
            }
            if( norm ) xmlFree( norm );
        }

        node = node->next;
    }

    return 0;
}

int station_readconfig( station_mgr_t *mgr )
{
    xmlNodePtr cur;
    xmlDocPtr doc;
    xmlNodePtr station;
    xmlNodePtr list;

    if( !file_is_openable_for_read( mgr->stationrc ) ) {
        /* There is no file to try to read, so forget it. */
        return 0;
    }

    if( mgr->verbose ) {
        fprintf( stderr, "station: Reading stationlist from %s\n",
                 mgr->stationrc );
    }
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

    if( xmlStrcasecmp( cur->name, BAD_CAST "stationlist" ) ) {
        fprintf( stderr, "station: %s: Document not a stationlist.\n",
                 mgr->stationrc );
        xmlFreeDoc( doc );
        return 0;
    }

    list = find_list( cur->xmlChildrenNode, mgr->norm, mgr->table );
    if( !list ) {
        fprintf( stderr, "station: %s: No station list for norm '%s' and frequencies '%s'.\n",
                 mgr->stationrc, mgr->norm, mgr->table );
        xmlFreeDoc( doc );
        return 0;
    }

    station = list->xmlChildrenNode;
    while( station ) {
        if( !xmlStrcasecmp( station->name, BAD_CAST "station" ) ) {
            xmlChar *name = xmlGetProp( station, BAD_CAST "name" );
            xmlChar *active_s = xmlGetProp( station, BAD_CAST "active" );
            xmlChar *pos_s = xmlGetProp( station, BAD_CAST "position" );
            xmlChar *band = xmlGetProp( station, BAD_CAST "band" );
            xmlChar *channel = xmlGetProp( station, BAD_CAST "channel" );
            xmlChar *network = xmlGetProp( station, BAD_CAST "network" );
            xmlChar *call = xmlGetProp( station, BAD_CAST "call" );
            xmlChar *norm = xmlGetProp( station, BAD_CAST "norm" );
            xmlChar *brightness = xmlGetProp( station, BAD_CAST "brightness" );
            xmlChar *contrast = xmlGetProp( station, BAD_CAST "contrast" );
            xmlChar *colour = xmlGetProp( station, BAD_CAST "colour" );
            xmlChar *hue = xmlGetProp( station, BAD_CAST "hue" );
            xmlChar *finetune = xmlGetProp( station, BAD_CAST "finetune" );
            xmlChar *xmltvid = xmlGetProp( station, BAD_CAST "xmltvid" );

            /* Only band and channel are required. */
            if( band && channel ) {
                int pos, active;

                if( !name ) name = channel;
                if( !pos_s || ( 1 != sscanf( (char *) pos_s, "%d", &pos ) ) ) {
                    pos = 0;
                }
                if( active_s ) {
                    active = atoi( (char *) active_s );
                } else {
                    active = 1;
                }
                station_add( mgr, pos, (char *) band, (char *) channel, (char *) name );
                station_set_current_active( mgr, active );
                if( network ) station_set_current_network_name( mgr, (char *) network );
                if( call ) station_set_current_network_call_letters( mgr, (char *) call );
                if( norm ) {
                    station_set_current_norm( mgr, (char *) norm );
                } else {
                    station_set_current_norm( mgr, mgr->norm );
                }
                if( brightness ) station_set_current_brightness( mgr, atoi( (char *) brightness ) );
                if( contrast ) station_set_current_contrast( mgr, atoi( (char *) contrast ) );
                if( colour ) station_set_current_colour( mgr, atoi( (char *) colour ) );
                if( hue ) station_set_current_hue( mgr, atoi( (char *) hue ) );
                if( finetune ) station_set_current_finetune( mgr, atoi( (char *) finetune ) );
                if( xmltvid ) station_set_current_xmltv_id( mgr, (char *) xmltvid );
            }

            if( xmltvid ) xmlFree( xmltvid );
            if( finetune ) xmlFree( finetune );
            if( brightness ) xmlFree( brightness );
            if( contrast ) xmlFree( contrast );
            if( colour ) xmlFree( colour );
            if( hue ) xmlFree( hue );
            if( norm ) xmlFree( norm );
            if( network ) xmlFree( network );
            if( call ) xmlFree( call );
            if( name ) xmlFree( name );
            if( active_s ) xmlFree( active_s );
            if( pos_s ) xmlFree( pos_s );
            if( band ) xmlFree( band );
            if( channel ) xmlFree( channel );
        }
        station = station->next;
    }
    xmlFreeDoc( doc );
    return 1;
}

station_mgr_t *station_new( const char *norm, const char *table, int us_cable_mode, int verbose )
{
    station_mgr_t *mgr = malloc( sizeof( station_mgr_t ) );
    const char *frequencies;

    if( !mgr ) return 0;

    strncpy( mgr->stationrc, getenv( "HOME" ), 235 );
    strncat( mgr->stationrc, "/.tvtime/stationlist.xml", 255 );
    mgr->verbose = verbose;
    mgr->first = 0;
    mgr->current = 0;
    mgr->us_cable_mode = us_cable_mode;
    mgr->last_channel = 0;
    mgr->new_install = 0;
    mgr->num_stations = 0;
    mgr->max_position = 0;
    mgr->norm = strdup( norm );
    if( !mgr->norm ) {
        free( mgr );
        return 0;
    }

    mgr->table = strdup( table );
    if( !mgr->table ) {
        free( mgr->norm );
        free( mgr );
        return 0;
    }

    if( !station_readconfig( mgr ) ) {
        mgr->new_install = 1;
        frequencies = table;

        if( verbose ) {
            fprintf( stderr, "station: Adding frequency table %s, all channels active\n",
                     frequencies );
        }

        if( !strcasecmp( frequencies, "europe-west" ) || !strcasecmp( frequencies, "europe-east" ) ||
            !strcasecmp( frequencies, "europe-cable" ) ) {
            fprintf( stderr, "\n*** Frequency list %s has been changed to be called %s in tvtime 0.9.7.\n",
                     frequencies, "europe" );
            fprintf( stderr, "*** Please update your configuration.  Comments?  tvtime-devel@lists.sourceforge.net\n\n" );
            frequencies = "europe";
        }

        if( !strcasecmp( frequencies, "us-cable" ) ) {
            station_add_band( mgr, "us cable" );
        } else if( !strcasecmp( frequencies, "us-cable100" ) ) {
            station_add_band( mgr, "us cable" );
            station_add_band( mgr, "us cable 100" );
        } else if( !strcasecmp( frequencies, "us-broadcast" ) ) {
            station_add_band( mgr, "us broadcast" );
        } else if( !strcasecmp( frequencies, "china-broadcast" ) ) {
            station_add_band( mgr, "china broadcast" );
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
            station_add_band( mgr, "uhf australia" );
        } else if( !strcasecmp( frequencies, "australia-optus" ) ) {
            station_add_band( mgr, "australia optus" );
        } else if( !strcasecmp( frequencies, "newzealand" ) ) {
            /* Billy: It's unclear to me if we should keep this around. */
            station_add_band( mgr, "vhf e2-e12" );
            station_add_band( mgr, "uhf" );
        }
        mgr->current = mgr->first;
    }
    
    return mgr;
}

void station_delete( station_mgr_t *mgr )
{
    station_info_t *cur = mgr->first;
    while( cur ) {
        station_info_t *next = cur->next;
        if( next == mgr->first ) next = 0;
        free( cur );
        cur = next;
    }
    free( mgr->table );
    free( mgr->norm );
    free( mgr );
}

int station_is_new_install( station_mgr_t *mgr )
{
    return mgr->new_install;
}

int station_get_num_stations( station_mgr_t *mgr )
{
    return mgr->num_stations;
}

int station_get_max_position( station_mgr_t *mgr )
{
    return mgr->max_position;
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

void station_inc( station_mgr_t *mgr )
{
    mgr->last_channel = station_get_current_id( mgr );

    if( mgr->current ) {
        station_info_t *i = mgr->current;
        do {
            mgr->current = mgr->current->next;
        } while ( !mgr->current->active && mgr->current != i ); 
    }
}

void station_dec( station_mgr_t *mgr )
{
    mgr->last_channel = station_get_current_id( mgr );

    if( mgr->current ) {
        station_info_t *i= mgr->current;
        do {
            mgr->current = mgr->current->prev;
        } while ( !mgr->current->active && mgr->current != i );
    }
}

void station_prev( station_mgr_t *mgr )
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

int station_get_current_pos( station_mgr_t *mgr )
{
    station_info_t *rp = mgr->first;
    int i = 0;

    while( rp && rp != mgr->current ) {
        rp = rp->next;
        i++;
    }

    return i;
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
        if( !strcasecmp( mgr->current->band->name, "US Cable" ) && mgr->us_cable_mode ) {
            snprintf( mgr->band_and_frequency, sizeof( mgr->band_and_frequency ),
                      "%s [%s]: %s", mgr->current->band->name,
                      mgr->us_cable_mode == NTSC_CABLE_MODE_HRC ? "HRC" : "IRC",
                      mgr->current->channel->name );
        } else {
            snprintf( mgr->band_and_frequency, sizeof( mgr->band_and_frequency ),
                      "%s: %s", mgr->current->band->name, mgr->current->channel->name );
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
    char tempname[ 32 ];

    if( !isFreePos( mgr, pos ) ) pos = getNextPos( mgr );

    if( !name ) {
        snprintf( tempname, sizeof( tempname ), "%d", pos );
        name = tempname;
    }

    if( !strcasecmp( bandname, "Custom" ) ) {
        int freq = (int) ((atof( channel ) * 1000.0) + 0.5);
        band_entry_t *newentry = malloc( sizeof( band_entry_t ) );
        if( newentry ) {
            char *entryname;
            if( asprintf( &entryname, "%.2fMHz", ((double) freq) / 1000.0 ) < 0 ) {
                entryname = "Error";
            }
            newentry->name = entryname;
            newentry->freq = freq;
            info = station_info_new( pos, name, &custom_band, newentry );
            if( info ) insert( mgr, info, 1 );
            return pos;
        }
    } else if( band ) {
        for( entry = band->channels; entry < &(band->channels[ band->count ]); entry++ ) {
            if( !strcasecmp( entry->name, channel ) ) {
                info = station_info_new( pos, name, band, entry );
                if( info ) insert( mgr, info, 1 );
                return pos;
            }
        }
    }
    return -1;
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

        info = station_info_new( pos, 0, band, &(band->channels[ i ]) );
        if( info ) insert( mgr, info, 0 );
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

const char *station_get_current_network_name( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->network_name;
    } else {
        return "";
    }
}

void station_set_current_network_name( station_mgr_t *mgr, const char *network_name )
{
    if( mgr->current ) {
        snprintf( mgr->current->network_name,
                  sizeof( mgr->current->network_name ), "%s", network_name );
    }
}

const char *station_get_current_xmltv_id( station_mgr_t *mgr )
{
    if( mgr->current && *mgr->current->xmltvid ) {
        return mgr->current->xmltvid;
    } else {
        return 0;
    }
}

void station_set_current_xmltv_id( station_mgr_t *mgr, const char *xmltvid )
{
    if( mgr->current ) {
        snprintf( mgr->current->xmltvid, sizeof( mgr->current->xmltvid ), "%s", xmltvid );
    }
}

const char *station_get_current_network_call_letters( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->network_call_letters;
    } else {
        return "";
    }
}

void station_set_current_network_call_letters( station_mgr_t *mgr, const char *call_letters )
{
    if( mgr->current ) {
        snprintf( mgr->current->network_call_letters,
                  sizeof( mgr->current->network_call_letters ), "%s", call_letters );
    }
}

const char *station_get_current_norm( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->norm;
    } else {
        return "";
    }
}

void station_set_current_norm( station_mgr_t *mgr, const char *norm )
{
    if( mgr->current ) {
        snprintf( mgr->current->norm, sizeof( mgr->current->norm ), "%s", norm );
    }
}

void station_set_current_brightness( station_mgr_t *mgr, int val )
{
    if( mgr->current ) {
        mgr->current->brightness = val;
    }
}

void station_set_current_contrast( station_mgr_t *mgr, int val )
{
    if( mgr->current ) {
        mgr->current->contrast = val;
    }
}

void station_set_current_colour( station_mgr_t *mgr, int val )
{
    if( mgr->current ) {
        mgr->current->colour = val;
    }
}

void station_set_current_hue( station_mgr_t *mgr, int val )
{
    if( mgr->current ) {
        mgr->current->hue = val;
    }
}

int station_get_current_brightness( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->brightness;
    } else {
        return -1;
    }
}

int station_get_current_contrast( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->contrast;
    } else {
        return -1;
    }
}

int station_get_current_colour( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->colour;
    } else {
        return -1;
    }
}

int station_get_current_hue( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->hue;
    } else {
        return -1;
    }
}

int station_get_current_finetune( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->finetune;
    } else {
        return 0;
    }
}

void station_set_current_finetune( station_mgr_t *mgr, int finetune )
{
    if( finetune >  50 ) finetune = 50;
    if( finetune < -49 ) finetune = -49;
    if( mgr->current ) {
        mgr->current->finetune = finetune;
    }
}

void station_activate_all_channels( station_mgr_t *mgr )
{
    station_info_t *rp = mgr->first;
    while( rp ) {
        rp->active = 1;
        rp = rp->next;
        if( rp == mgr->first ) break;
    }
}

int station_get_current_active( station_mgr_t *mgr )
{
    if( mgr->current ) {
        return mgr->current->active;
    }
    return 0;
}


int station_remove( station_mgr_t *mgr )
{ /* untested? */
    station_info_t *i = mgr->current;
    if( !mgr->current ) return 0;
    i->next->prev = i->prev;
    i->prev->next = i->next;
    
    if( i == mgr->first ) {
        mgr->first= i->next;
    }
    
    if( i == mgr->current ) {
        mgr->current = i->next;
    }

    if( i == i->next ) {
        mgr->current= NULL;
        mgr->first= NULL;
    }
    free( i );
    return 1;
}

station_info_t *ripout( station_mgr_t *mgr, int pos )
{
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
        res = insert( mgr, i, 1 );

    } else {
        station_info_t *old = ripout( mgr, pos );
        station_info_t *i = ripout( mgr, mgr->current->pos );
        
        old->pos= i->pos;
        i->pos = pos;
        res = insert( mgr, old, 1 ) && insert( mgr, i, 1 );
    }
    return res;
}

static xmlNodePtr find_station( xmlNodePtr node, const char *stationname )
{
    while( node ) {
        if( !xmlStrcasecmp( node->name, BAD_CAST "station" ) ) {
            xmlChar *name = xmlGetProp( node, BAD_CAST "name" );

            if( name && !xmlStrcasecmp( name, BAD_CAST stationname ) ) {
                xmlFree( name );
                return node;
            }
            if( name ) xmlFree( name );
        }

        node = node->next;
    }

    return 0;
}

int station_writeconfig( station_mgr_t *mgr )
{
    xmlDocPtr doc;
    xmlNodePtr top;
    xmlNodePtr list;
    char filename[ 255 ];
    station_info_t *rp = mgr->first;

    if( !rp ) return 0;

    strncpy( filename, getenv( "HOME" ), 235 );
    strncat( filename, "/.tvtime/stationlist.xml", 255 );

    doc = xmlParseFile( filename );
    if( !doc ) {
        if( file_is_openable_for_read( filename ) ) {
            fprintf( stderr, "station: Config file exists, but cannot be parsed.\n" );
            fprintf( stderr, "station: Stations will NOT be saved.\n" );
            return 0;
        } else {
            /* Config file doesn't exist, create a new one. */
            fprintf( stderr, "station: No station file found, creating a new one.\n" );
            doc = xmlNewDoc( BAD_CAST "1.0" );
            if( !doc ) {
                fprintf( stderr, "station: Could not create new config file.\n" );
                return 0;
            }
        }
    }

    top = xmlDocGetRootElement( doc );
    if( !top ) {
        /* Set the DTD */
        xmlDtdPtr dtd;
        dtd = xmlNewDtd( doc,
                         BAD_CAST "stationlist",
                         BAD_CAST "-//tvtime//DTD stationlist 1.0//EN",
                         BAD_CAST "http://tvtime.sourceforge.net/DTD/stationlist1.dtd" );
        doc->intSubset = dtd;
        if( !doc->children ) {
            xmlAddChild( (xmlNodePtr) doc, (xmlNodePtr) dtd );
        } else {
            xmlAddPrevSibling( doc->children, (xmlNodePtr) dtd );
        }

        /* Create the root node */
        top = xmlNewDocNode( doc, 0, BAD_CAST "stationlist", 0 );
        if( !top ) {
            fprintf( stderr, "station: Could not create toplevel element 'stationlist'.\n" );
            xmlFreeDoc( doc );
            return 0;
        } else {
            xmlDocSetRootElement( doc, top );
            xmlNewProp( top, BAD_CAST "xmlns", BAD_CAST "http://tvtime.sourceforge.net/DTD/" );
        }
    }

    list = find_list( top->xmlChildrenNode, mgr->norm, mgr->table );
    if( !list ) {
        list = xmlNewTextChild( top, 0, BAD_CAST "list", 0 );
        xmlNewProp( list, BAD_CAST "norm", BAD_CAST mgr->norm );
        xmlNewProp( list, BAD_CAST "frequencies", BAD_CAST mgr->table );
    }

    do {
        xmlNodePtr node = find_station( list->xmlChildrenNode, rp->name );
        char buf[ 256 ];

        snprintf( buf, sizeof( buf ), "%d", rp->pos );
        if( !node ) {
            node = xmlNewTextChild( list, 0, BAD_CAST "station", 0 );
        }
        xmlSetProp( node, BAD_CAST "name", BAD_CAST rp->name );
        xmlSetProp( node, BAD_CAST "active", BAD_CAST (rp->active ? "1" : "0") );

        snprintf( buf, sizeof( buf ), "%d", rp->pos );
        xmlSetProp( node, BAD_CAST "position", BAD_CAST buf );
        xmlSetProp( node, BAD_CAST "band", BAD_CAST rp->band->name );
        xmlSetProp( node, BAD_CAST "channel", BAD_CAST rp->channel->name );

        snprintf( buf, sizeof( buf ), "%d", rp->finetune );
        xmlSetProp( node, BAD_CAST "finetune", BAD_CAST buf );

        if( *(rp->network_name) ) {
            xmlSetProp( node, BAD_CAST "network", BAD_CAST rp->network_name );
        }
        if( *(rp->network_call_letters) ) {
            xmlSetProp( node, BAD_CAST "call", BAD_CAST rp->network_call_letters );
        }
        if( *(rp->norm) ) {
            xmlSetProp( node, BAD_CAST "norm", BAD_CAST rp->norm );
        }
        if( rp->brightness >= 0 ) {
            snprintf( buf, sizeof( buf ), "%d", rp->brightness );
            xmlSetProp( node, BAD_CAST "brightness", BAD_CAST buf );
        }
        if( rp->contrast >= 0 ) {
            snprintf( buf, sizeof( buf ), "%d", rp->contrast );
            xmlSetProp( node, BAD_CAST "contrast", BAD_CAST buf );
        }
        if( rp->colour >= 0 ) {
            snprintf( buf, sizeof( buf ), "%d", rp->colour );
            xmlSetProp( node, BAD_CAST "colour", BAD_CAST buf );
        }
        if( rp->hue >= 0 ) {
            snprintf( buf, sizeof( buf ), "%d", rp->hue );
            xmlSetProp( node, BAD_CAST "hue", BAD_CAST buf );
        }

        rp = rp->next;
    } while( rp != mgr->first );

    xmlKeepBlanksDefault( 0 );
    xmlSaveFormatFile( filename, doc, 1);
    xmlFreeDoc( doc );
    return 1;
}

// vim: set et
