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

struct xmltv_s
{
    xmlDocPtr doc;
    xmlNodePtr root;
    char curchannel[ 256 ];
    int refresh;
    xmlChar *curchan;
    program_t *pro;
    program_t *next_pro;
};

/* This exists so that we can store program info during search without modifying xmltv */
struct program_s {
    xmlChar *title;
    xmlChar *subtitle;
    xmlChar *description;
    char times[ 256 ];
    int end_year;
    int end_month;
    int end_day;
    int end_hour;
    int end_min;
};

/**
 * Timezone parsing code based loosely on the algorithm in
 * filldata.cpp of MythTV.
 */
static int parse_xmltv_timezone( const char *tzstring )
{
    /* We signal an error by setting it invalid (> 720min = 12hr). */
    int result = 721;

    if( strlen( tzstring ) == 5 && (tzstring[ 0 ] == '+' || tzstring[ 0 ] == '-') ) {
        char hour[ 3 ];
        int min;

        hour[ 0 ] = tzstring[ 1 ];
        hour[ 1 ] = tzstring[ 2 ];
        hour[ 2 ] = 0;

        result = atoi( hour );
        result *= 60;

        min = atoi( tzstring + 3 );
        result += min;

        if( tzstring[ 0 ] == '-' ) {
            result *= -1;
        }
    }

    return result;
}

static void parse_xmltv_date( int *year, int *month, int *day, int *hour, int *min, const char *date )
{
    char syear[ 6 ];
    char smonth[ 3 ];
    char sday[ 3 ];
    char shour[ 3 ];
    char smin[ 3 ];

    memset( syear, 0, sizeof( syear ) );
    memset( smonth, 0, sizeof( smonth ) );
    memset( sday, 0, sizeof( sday ) );
    memset( shour, 0, sizeof( shour ) );
    memset( smin, 0, sizeof( smin ) );

    memcpy( syear, date, 4 );
    memcpy( smonth, date + 4, 2 );
    memcpy( sday, date + 6, 2 );
    memcpy( shour, date + 8, 2 );
    memcpy( smin, date + 10, 2 );

    *year = atoi( syear );
    *month = atoi( smonth );
    *day = atoi( sday );
    *hour = atoi( shour );
    *min = atoi( smin );
}

static int date_compare( int year1, int month1, int day1, int hour1, int min1,
                         int year2, int month2, int day2, int hour2, int min2 )
{
    int first = min1 + (hour1 * 60) + (day1 * 60 * 24)
                + (month1 * 32 * 60 * 24) + (year1 * 12 * 32 * 60 * 24);
    int second = min2 + (hour2 * 60) + (day2 * 60 * 24)
                 + (month2 * 32 * 60 * 24) + (year2 * 12 * 32 * 60 * 24);
    return first - second;
}

/* Necessary in order to avoid the assumption that the next node represents the next show */
static xmlNodePtr get_next_program( xmlNodePtr cur, const char *channelid,
                                    int year, int month, int day, int hour, int min )
{
    int min_diff = 0;
    xmlNodePtr min_node = 0;

    cur = cur->xmlChildrenNode;
    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "programme" ) ) {
            xmlChar *channel = xmlGetProp( cur, BAD_CAST "channel" );
            if( channel && !xmlStrcasecmp( channel, BAD_CAST channelid ) ) {
                xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
                if( start ) {
                    int start_year;
                    int start_month;
                    int start_day;
                    int start_hour;
                    int start_min;
                    int diff = 0;

                    parse_xmltv_date( &start_year, &start_month, &start_day,
                                      &start_hour, &start_min, (char *) start );
                    diff = date_compare( start_year, start_month, start_day, start_hour, start_min,
                                         year, month, day, hour, min );
                    if ( diff > 0 ) { // if diff == 0, it's the same show
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

static void reinit_program( program_t *pro, xmlNodePtr cur, int end_year, int end_month,
                            int end_day, int end_hour, int end_min )
{
    if ( !pro ) {
        return;
    }

    if ( cur ) {
        xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
        if( start ) {
            int start_year;
            int start_month;
            int start_day;
            int start_hour;
            int start_min;
            parse_xmltv_date( &start_year, &start_month, &start_day,
                              &start_hour, &start_min, (char *) start );
            sprintf( pro->times, "%2d:%02d - %2d:%02d",
                     start_hour, start_min, end_hour, end_min  );
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
    pro->end_year = end_year;
    pro->end_month = end_month;
    pro->end_day = end_day;
    pro->end_hour = end_hour;
    pro->end_min = end_min;
}

program_t *program_new()
{
    program_t *pro = malloc( sizeof( program_t ) );
    reinit_program( pro, 0, 0, 0, 0, 0, 0 );
    return pro;
}

static xmlNodePtr get_program( xmlNodePtr root, program_t *pro, const char *channelid,
                               int year, int month, int day, int hour, int min )
{
    xmlNodePtr cur = root->xmlChildrenNode;
    while( cur ) {
        if( !xmlStrcasecmp( cur->name, BAD_CAST "programme" ) ) {
            xmlChar *channel = xmlGetProp( cur, BAD_CAST "channel" );
            if( channel && !xmlStrcasecmp( channel, BAD_CAST channelid ) ) {
                xmlChar *start = xmlGetProp( cur, BAD_CAST "start" );
                if( start ) {
                    int start_year;
                    int start_month;
                    int start_day;
                    int start_hour;
                    int start_min;
                    int end_year;
                    int end_month;
                    int end_day;
                    int end_hour;
                    int end_min;
                    parse_xmltv_date( &start_year, &start_month, &start_day,
                                      &start_hour, &start_min, (char *) start );
                    if( date_compare( start_year, start_month, start_day, start_hour, start_min,
                                      year, month, day, hour, min ) <= 0 ) {
                        xmlChar *stop = xmlGetProp( cur, BAD_CAST "stop" );
                        if( stop ) {
                            parse_xmltv_date( &end_year, &end_month, &end_day, &end_hour,
                                              &end_min, (char *) stop );
                            xmlFree( stop );
                        } else {
                            xmlNodePtr next_program = get_next_program(root, channelid,
                                                                       start_year, start_month, start_day,
                                                                       start_hour, start_min);
                            xmlChar *next_start = 0;
                            if ( next_program ) {
                                next_start = xmlGetProp( next_program, BAD_CAST "start" );
                                if ( next_start ) {
                                    parse_xmltv_date( &end_year, &end_month, &end_day,
                                                      &end_hour, &end_min, (char *) next_start );
                                }
                            }
                            if ( !next_program || !next_start ) {
                                end_year = start_year;
                                end_month = start_month;
                                end_day = start_day;
                                end_hour = 23;
                                end_min = 59;
                            }
                            if ( next_start ) xmlFree( next_start );
                        }

                        if( date_compare( end_year, end_month, end_day,
                                          end_hour, end_min, year, month,
                                          day, hour, min ) > 0 ) {
                            reinit_program( pro, cur, end_year, end_month, end_day, end_hour, end_min );
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
        fprintf( stderr, "xmltv: Empty XML document, %s is not a valid xmltv file.\n",
                filename );
        xmlFreeDoc( xmltv->doc );
        free( xmltv );
        return 0;
    }

    if( xmlStrcmp( xmltv->root->name, BAD_CAST "tv" ) ) {
        fprintf( stderr, "xmltv: Root node is not <tv>, %s is not a valid xmltv file.\n",
                 filename );
        xmlFreeDoc( xmltv->doc );
        free( xmltv );
        return 0;
    }

    xmltv->pro = program_new();
    xmltv->next_pro = program_new();
    xmltv->curchan = 0;
    xmltv->refresh = 1;

    return xmltv;
}

void program_delete( program_t *pro ) {
    if( pro->title ) xmlFree( pro->title );
    if( pro->subtitle ) xmlFree( pro->subtitle );
    if( pro->description ) xmlFree( pro->description );
    reinit_program( pro, 0, 0, 0, 0, 0, 0 );
    free ( pro );
}

void xmltv_delete( xmltv_t *xmltv )
{
    program_delete( xmltv->pro );
    program_delete( xmltv->next_pro );
    if( xmltv->curchan ) xmlFree( xmltv->curchan );
    xmlFreeDoc( xmltv->doc );
    free( xmltv );
}

void xmltv_set_channel( xmltv_t *xmltv, const char *channel )
{
    if( channel ) {
        snprintf( xmltv->curchannel, sizeof( xmltv->curchannel ), "%s", channel );
    } else {
        *xmltv->curchannel = '\0';
    }
    xmltv->refresh = 1;
}

void xmltv_refresh( xmltv_t *xmltv, int year, int month, int day,
                    int hour, int min )
{
    xmlNodePtr program_node = 0;

    if( xmltv->pro->title ) xmlFree( xmltv->pro->title );
    if( xmltv->pro->subtitle ) xmlFree( xmltv->pro->subtitle );
    if( xmltv->pro->description ) xmlFree( xmltv->pro->description );
    reinit_program( xmltv->pro, 0, 0, 0, 0, 0, 0 );

    if( xmltv->next_pro->title ) xmlFree( xmltv->next_pro->title );
    if( xmltv->next_pro->subtitle ) xmlFree( xmltv->next_pro->subtitle );
    if( xmltv->next_pro->description ) xmlFree( xmltv->next_pro->description );
    reinit_program( xmltv->next_pro, 0, 0, 0, 0, 0, 0 );

    if( *xmltv->curchannel ) {
        program_node = get_program( xmltv->root, xmltv->pro, xmltv->curchannel,
                                    year, month, day, hour, min );

        if( program_node ) {
            get_program( xmltv->root, xmltv->next_pro, xmltv->curchannel,
                         xmltv->pro->end_year, xmltv->pro->end_month, xmltv->pro->end_day,
                         xmltv->pro->end_hour, xmltv->pro->end_min );
        } else {
            xmltv->pro->end_year = year;
            xmltv->pro->end_month = month;
            xmltv->pro->end_day = day;
            xmltv->pro->end_hour = hour + 1;
            xmltv->pro->end_min = min;
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

int xmltv_needs_refresh( xmltv_t *xmltv, int year, int month, int day,
                         int hour, int min )
{
    return (xmltv->refresh || (date_compare( year, month, day, hour, min,
                                             xmltv->pro->end_year, xmltv->pro->end_month, xmltv->pro->end_day,
                                             xmltv->pro->end_hour, xmltv->pro->end_min ) >= 0));
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

