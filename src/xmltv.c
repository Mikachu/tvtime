/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include "xmltv.h"

typedef struct program_s program_t;

struct xmltv_s
{
    xmlDocPtr doc;
    xmlNodePtr root;
    char curchannel[ 256 ];
    int refresh;
    xmlChar *curchan;
    xmlChar *display_chan;
    program_t *pro;
    program_t *next_pro;
    int is_tv_grab_na;
};

/**
 * This exists so that we can store program info during search without
 * modifying xmltv.
 */
struct program_s {
    xmlChar *title;
    xmlChar *subtitle;
    xmlChar *description;
    char times[ 256 ];
    time_t end_time;
};

/**
 * Support the Date::Manip timezone names.  This code will hopefully
 * go away when all XMLTV providers drop these names.  Using names
 * is a bad idea since there is no unified standard for them, and the
 * XMLTV DTD does not define a set of standard names to use.
 */
typedef struct tz_map_s {
    const char *name;
    int offset;
} tz_map_t;

tz_map_t date_manip_timezones[] = {
    { "IDLW",    -1200, }, { "NT",      -1100, }, { "HST",     -1000, },
    { "CAT",     -1000, }, { "AHST",    -1000, }, { "AKST",     -900, },
    { "YST",      -900, }, { "HDT",      -900, }, { "AKDT",     -800, },
    { "YDT",      -800, }, { "PST",      -800, }, { "PDT",      -700, },
    { "MST",      -700, }, { "MDT",      -600, }, { "CST",      -600, },
    { "CDT",      -500, }, { "EST",      -500, }, { "ACT",      -500, },
    { "SAT",      -400, }, { "BOT",      -400, }, { "EDT",      -400, },
    { "AST",      -400, }, { "AMT",      -400, }, { "ACST",     -400, },
    { "NFT",      -330, }, { "BRST",     -300, }, { "BRT",      -300, },
    { "AMST",     -300, }, { "ADT",      -300, }, { "ART",      -300, },
    { "NDT",      -230, }, { "AT",       -200, }, { "BRST",     -200, },
    { "FNT",      -200, }, { "WAT",      -100, }, { "FNST",     -100, },
    { "GMT",         0, }, { "UT",          0, }, { "UTC",         0, },
    { "WET",         0, }, { "CET",       100, }, { "FWT",       100, },
    { "MET",       100, }, { "MEZ",       100, }, { "MEWT",      100, },
    { "SWT",       100, }, { "BST",       100, }, { "GB",        100, },
    { "WEST",        0, }, { "CEST",      200, }, { "EET",       200, },
    { "FST",       200, }, { "MEST",      200, }, { "MESZ",      200, },
    { "METDST",    200, }, { "SAST",      200, }, { "SST",       200, },
    { "EEST",      300, }, { "BT",        300, }, { "MSK",       300, },
    { "EAT",       300, }, { "IT",        330, }, { "ZP4",       400, },
    { "MSD",       300, }, { "ZP5",       500, }, { "IST",       530, },
    { "ZP6",       600, }, { "NOVST",     600, }, { "NST",       630, },
    { "JAVT",      700, }, { "CCT",       800, }, { "AWST",      800, },
    { "WST",       800, }, { "PHT",       800, }, { "JST",       900, },
    { "ROK",       900, }, { "ACST",      930, }, { "CAST",      930, },
    { "AEST",     1000, }, { "EAST",     1000, }, { "GST",      1000, },
    { "ACDT",     1030, }, { "CADT",     1030, }, { "AEDT",     1100, },
    { "EADT",     1100, }, { "IDLE",     1200, }, { "NZST",     1200, },
    { "NZT",      1200, }, { "NZDT",     1300, } };

const int num_timezones = sizeof( date_manip_timezones ) / sizeof( tz_map_t );

/**
 * Timezone parsing code based loosely on the algorithm in
 * filldata.cpp of MythTV.
 */
static time_t parse_xmltv_timezone( const char *tzstr )
{
    if( strlen( tzstr ) == 5 && (tzstr[ 0 ] == '+' || tzstr[ 0 ] == '-') ) {
        char hour[ 3 ];
        int result;

        hour[ 0 ] = tzstr[ 1 ];
        hour[ 1 ] = tzstr[ 2 ];
        hour[ 2 ] = 0;

        result = (3600 * atoi( hour )) + (60 * atoi( tzstr + 3 ));

        return (tzstr[ 0 ] == '-') ? -result : result;
    } else {
        int i;

        for( i = 0; i < num_timezones; i++ ) {
            if( !strcasecmp( tzstr, date_manip_timezones[ i ].name ) ) {
                return 60 * ((date_manip_timezones[ i ].offset % 100) +
                             ((date_manip_timezones[ i ].offset / 100) * 60));
            }
        }
    }

    return 0;
}

static time_t parse_xmltv_date( const char *date )
{
    char syear[ 6 ];
    char smonth[ 3 ];
    char sday[ 3 ];
    char shour[ 3 ];
    char smin[ 3 ];
    char ssec[ 3 ];
    int len = strlen( date );
    struct tm tm_obj;
    time_t tz = 0;
    /*
     * For some reason, mktime() accepts broken-time arguments as localtime,
     * and there is no corresponding UTC function. *Sigh*.
     * For this reason we have to calculate the offset from GMT to adjust the
     * argument given to mktime().
     */
    time_t now = time( 0 );
    long gmtoff = localtime( &now )->tm_gmtoff;

    memset( syear, 0, sizeof( syear ) );
    memset( smonth, 0, sizeof( smonth ) );
    memset( sday, 0, sizeof( sday ) );
    memset( shour, 0, sizeof( shour ) );
    memset( smin, 0, sizeof( smin ) );

    /*
     * example *date = "20031022220000 +0200"
     * type:            YYYYMMDDhhmmss ZZzzz"
     * position:        0         1         2          
     *                  012345678901234567890
     */
    if( len >= 4 ) memcpy( syear, date, 4 );
    if( len >= 6 ) memcpy( smonth, date + 4, 2 );
    if( len >= 8 ) memcpy( sday, date + 6, 2 );
    if( len >= 10 ) memcpy( shour, date + 8, 2 );
    if( len >= 12 ) memcpy( smin, date + 10, 2 );
    if( len >= 14 ) memcpy( ssec, date + 12, 2 );
    if( len >= 15 ) tz = parse_xmltv_timezone( date + 15 ); else tz = 0;

    tm_obj.tm_sec = atoi( ssec ) - tz + gmtoff;
    tm_obj.tm_min = atoi( smin );
    tm_obj.tm_hour = atoi( shour );
    tm_obj.tm_mday = atoi( sday );
    tm_obj.tm_mon = atoi( smonth ) - 1;
    tm_obj.tm_year = atoi( syear ) - 1900;
    tm_obj.tm_isdst = -1;

    return mktime( &tm_obj );
}

/**
 * Necessary in order to avoid the assumption that the next node represents
 * the next show.
 */
static xmlNodePtr get_next_program( xmlNodePtr cur, const char *channelid,
                                    time_t time )
{
    time_t min_diff = 0;
    xmlNodePtr min_node = 0;

    cur = cur->xmlChildrenNode;
    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "programme" ) ) {
            xmlChar *channel = xmlGetProp( cur, BAD_CAST "channel" );
            if( channel && !xmlStrcasecmp( channel, BAD_CAST channelid ) ) {
                xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
                if( start ) {
                    time_t start_time;
                    time_t diff = 0;

                    start_time = parse_xmltv_date( (char *) start );
                    diff = start_time - time;
                    if ( diff > 0 ) { /* if diff == 0, it's the same show */
                        if ( min_diff == 0 || diff < min_diff ) {
                            min_diff = diff;
                            min_node = cur;
                        }
                    }
                    xmlFree( start );
                }
            }
            if( channel ) xmlFree( channel );
        }
        cur = cur->next;
    }

    return min_node;
}

static void reinit_program( program_t *pro, xmlNodePtr cur, time_t end_time )
{
    if( !pro ) {
        return;
    }

    if( cur ) {
        xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
        if( start ) {
            time_t start_time;
            struct tm start_tm;
            struct tm end_tm;
            start_time = parse_xmltv_date( (char *) start );
            localtime_r( &start_time, &start_tm );
            localtime_r( &end_time, &end_tm );
            /* FIXME: This needs i18n-approval. In French, for example, times
               are written as 18h30, and in Swedish, we write 18.30. Not to
               mention different leading-zero rules. */
            /* Printing the string here was a hack to begin with.  There
               should be functions to return the start/end time from the
               xmltv object so that formatting can be handled by the
               application.  -Billy */
            sprintf( pro->times, "%2d:%02d - %2d:%02d",
                     start_tm.tm_hour, start_tm.tm_min,
                     end_tm.tm_hour, end_tm.tm_min );
            xmlFree( start );
        }
        cur = cur->xmlChildrenNode;
        while( cur ) {
            if( !xmlStrcasecmp( cur->name, BAD_CAST "title" ) ) {
                pro->title = xmlNodeGetContent( cur );
            } else if( !xmlStrcasecmp( cur->name, BAD_CAST "sub-title" ) ) {
                pro->subtitle = xmlNodeGetContent( cur );
            } else if( !xmlStrcasecmp( cur->name, BAD_CAST "desc" ) ) {
                pro->description = xmlNodeGetContent( cur );
            }
            cur = cur->next;
        }
    } else {
         pro->title = pro->subtitle = pro->description = 0;
         *pro->times = 0;
    }
    pro->end_time = end_time;
}

program_t *program_new()
{
    program_t *pro = malloc( sizeof( program_t ) );
    reinit_program( pro, 0, 0 );
    return pro;
}

static xmlNodePtr get_program( xmlNodePtr root, program_t *pro,
                               const char *channelid, time_t time )
{
    xmlNodePtr cur = root->xmlChildrenNode;
    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "programme" ) ) {
            xmlChar *channel = xmlGetProp( cur, BAD_CAST "channel" );
            if( channel && !xmlStrcasecmp( channel, BAD_CAST channelid ) ) {
                xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
                if( start ) {
                    time_t start_time;
                    time_t end_time = 0;
                    start_time = parse_xmltv_date( (char *) start );

                    if( start_time <= time ) {
                        xmlChar *stop = xmlGetProp( cur, BAD_CAST "stop" );
                        if( stop ) {
                            end_time = parse_xmltv_date( (char *) stop );
                            xmlFree( stop );
                        } else {
                            xmlNodePtr next_program = get_next_program( root,
                                                        channelid, start_time );
                            xmlChar *next_start = 0;
                            if ( next_program ) {
                                next_start = xmlGetProp( next_program,
                                                         BAD_CAST "start" );
                                if ( next_start ) {
                                    end_time = parse_xmltv_date( (char *) next_start );
                                }
                            }
                            if ( !next_program || !next_start ) {
                                /* Set to 23:59 the same day. */
                                end_time =
                                    start_time - (start_time % 86400) + 86340;
                            }
                            if ( next_start ) xmlFree( next_start );
                        }
                        if( end_time > time ) {
                            reinit_program( pro, cur, end_time );
                            xmlFree( start );
                            xmlFree( channel );
                            return cur;
                        }
                    }
                    xmlFree( start );
                }
            }
            if( channel ) xmlFree( channel );
        }
        cur = cur->next;
    }
    return 0;
}

static int xmltv_is_tv_grab_na( xmltv_t *xmltv )
{
    xmlNodePtr cur = xmltv->root->xmlChildrenNode;

    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "channel" ) ) {
            xmlChar *id = xmlGetProp( cur, BAD_CAST "id" );
            int is_na = 0;

            if( strstr( (char *) id, ".zap2it.com" ) ) {
                is_na = 1;
            }
            xmlFree( id );
            return is_na;
        }
        cur = cur->next;
    }

    return 0;
}

xmltv_t *xmltv_new( const char *filename )
{
    xmltv_t *xmltv = malloc( sizeof( xmltv_t ) );

    if( !xmltv ) {
        return 0;
    }

    xmltv->doc = xmlParseFile( filename );
    if( !xmltv->doc ) {
        fprintf( stderr, "xmltv: Can't open file %s.\n", filename );
        free( xmltv );
        return 0;
    }

    xmltv->root = xmlDocGetRootElement( xmltv->doc );
    if( !xmltv->root ) {
        fprintf( stderr, "xmltv: Empty XML document, "
                         "%s is not a valid xmltv file.\n",
                filename );
        xmlFreeDoc( xmltv->doc );
        free( xmltv );
        return 0;
    }

    if( xmlStrcmp( xmltv->root->name, BAD_CAST "tv" ) ) {
        fprintf( stderr, "xmltv: Root node is not <tv>, "
                         "%s is not a valid xmltv file.\n",
                 filename );
        xmlFreeDoc( xmltv->doc );
        free( xmltv );
        return 0;
    }

    xmltv->pro = program_new();
    xmltv->next_pro = program_new();
    xmltv->curchan = 0;
    xmltv->display_chan = 0;
    xmltv->refresh = 1;
    xmltv->is_tv_grab_na = xmltv_is_tv_grab_na( xmltv );

    return xmltv;
}

void program_delete( program_t *pro ) {
    if( pro->title ) xmlFree( pro->title );
    if( pro->subtitle ) xmlFree( pro->subtitle );
    if( pro->description ) xmlFree( pro->description );
    reinit_program( pro, 0, 0 );
    free ( pro );
}

void xmltv_delete( xmltv_t *xmltv )
{
    program_delete( xmltv->pro );
    program_delete( xmltv->next_pro );
    if( xmltv->curchan ) xmlFree( xmltv->curchan );
    if( xmltv->display_chan ) xmlFree( xmltv->display_chan );
    xmlFreeDoc( xmltv->doc );
    free( xmltv );
}

void xmltv_set_channel( xmltv_t *xmltv, const char *channel )
{
    if( channel ) {
        snprintf( xmltv->curchannel, sizeof( xmltv->curchannel ),
                  "%s", channel );
    } else {
        *xmltv->curchannel = '\0';
    }
    xmltv->refresh = 1;
}

void xmltv_refresh( xmltv_t *xmltv )
{
    xmlNodePtr program_node = 0;
    time_t curtime = time( 0 );

    if( xmltv->pro->title ) xmlFree( xmltv->pro->title );
    if( xmltv->pro->subtitle ) xmlFree( xmltv->pro->subtitle );
    if( xmltv->pro->description ) xmlFree( xmltv->pro->description );
    reinit_program( xmltv->pro, 0, 0 );

    if( xmltv->next_pro->title ) xmlFree( xmltv->next_pro->title );
    if( xmltv->next_pro->subtitle ) xmlFree( xmltv->next_pro->subtitle );
    if( xmltv->next_pro->description ) xmlFree( xmltv->next_pro->description );
    reinit_program( xmltv->next_pro, 0, 0 );

    if( *xmltv->curchannel ) {
        program_node = get_program( xmltv->root, xmltv->pro, xmltv->curchannel,
                                    curtime );

        if( program_node ) {
            get_program( xmltv->root, xmltv->next_pro, xmltv->curchannel,
                         xmltv->pro->end_time );
        } else {
            xmltv->pro->end_time = curtime + 3600;
        }
    }
    xmltv->refresh = 0;
}

const char *xmltv_get_title( xmltv_t *xmltv )
{
    return (char *) xmltv->pro->title;
}

const char *xmltv_get_sub_title( xmltv_t *xmltv )
{
    return (char *) xmltv->pro->subtitle;
}

const char *xmltv_get_description( xmltv_t *xmltv )
{
    return (char *) xmltv->pro->description;
}

const char *xmltv_get_times( xmltv_t *xmltv )
{
    return xmltv->pro->times;
}

const char *xmltv_get_next_title( xmltv_t *xmltv )
{
    return (char *) xmltv->next_pro->title;
}

const char *xmltv_get_channel( xmltv_t *xmltv )
{
    return (char *) xmltv->curchannel;
}

int xmltv_needs_refresh( xmltv_t *xmltv )
{
    time_t curtime = time( 0 );

    return xmltv->refresh || (curtime >= xmltv->pro->end_time);
}

const char *xmltv_lookup_channel( xmltv_t *xmltv, const char *name )
{
    xmlNodePtr cur = xmltv->root->xmlChildrenNode;

    if( xmltv->curchan ) xmlFree( xmltv->curchan );
    xmltv->curchan = 0;

    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "channel" ) ) {
            xmlNodePtr sub = cur->xmlChildrenNode;
            while( sub ) {
                if( !xmlStrcasecmp( sub->name, BAD_CAST "display-name" ) ) {
                    xmlChar *curname = xmlNodeGetContent( sub );
                    if( curname ) {
                        if( !xmlStrcasecmp( curname, BAD_CAST name ) ) {
                            xmltv->curchan = xmlGetProp( cur, BAD_CAST "id" );
                            xmlFree( curname );
                            return (char *) xmltv->curchan;
                        }
                        xmlFree( curname );
                    }
                }
                sub = sub->next;
            }
        }
        cur = cur->next;
    }

    return 0;
}

static const char *tv_grab_na_skip( const char *name )
{
    const char *ret = name;
    if( *ret == 'C' ) {
        while( *ret && *ret != ' ' ) ret++;
        if( *ret ) ret++;
    }

    while( *ret && *ret != ' ' ) ret++;
    if( *ret ) ret++;

    return ret;
}

const char *xmltv_lookup_channel_name( xmltv_t *xmltv, const char *id )
{
    xmlNodePtr cur = xmltv->root->xmlChildrenNode;

    if( xmltv->display_chan ) xmlFree( xmltv->display_chan );
    xmltv->display_chan = 0;

    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "channel" ) ) {
            xmlChar *curid = xmlGetProp( cur, BAD_CAST "id" );
            if ( curid ) {
                if ( !xmlStrcasecmp( curid, BAD_CAST id ) ) {
                    xmlNodePtr sub = cur->xmlChildrenNode;
                    while( sub && xmlStrcasecmp( sub->name, BAD_CAST "display-name" ) ) {
                        sub = sub->next;
                    }
                    if ( sub ) {
                        xmltv->display_chan = xmlNodeGetContent( sub );
                        xmlFree( curid );
                        if( xmltv->is_tv_grab_na ) {
                            return tv_grab_na_skip( (char *) xmltv->display_chan );
                        } else {
                            return (char *) xmltv->display_chan;
                        }
                    }
                }
                xmlFree( curid );
            }
        }
        cur = cur->next;
    }

    return 0;
}

