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
    xmlChar *title;
    xmlChar *subtitle;
    xmlChar *description;
    char times[ 256 ];
    int refresh;
    int end_year;
    int end_month;
    int end_day;
    int end_hour;
    int end_min;
    xmlChar *curchan;
};

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

static xmlNodePtr get_program( xmltv_t *xmltv, xmlDocPtr doc, xmlNodePtr cur, const char *channelid,
                               int year, int month, int day, int hour, int min )
{
    *xmltv->times = 0;

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
                    parse_xmltv_date( &start_year, &start_month, &start_day,
                                      &start_hour, &start_min, (char *) start );
                    if( date_compare( start_year, start_month, start_day, start_hour, start_min,
                                      year, month, day, hour, min ) <= 0 ) {
                        xmlChar *stop = xmlGetProp( cur, BAD_CAST "stop" );
                        if( stop ) {
                            parse_xmltv_date( &xmltv->end_year, &xmltv->end_month,
                                              &xmltv->end_day, &xmltv->end_hour,
                                              &xmltv->end_min, (char *) stop );
                            if( date_compare( xmltv->end_year, xmltv->end_month, xmltv->end_day,
                                              xmltv->end_hour, xmltv->end_min, year, month,
                                              day, hour, min ) > 0 ) {
                                sprintf( xmltv->times, "%2d:%02d - %2d:%02d",
                                         start_hour, start_min, xmltv->end_hour, xmltv->end_min  );
                                xmlFree( start );
                                xmlFree( channel );
                                xmlFree( stop );
                                return cur;
                            }
                            xmlFree( stop );
                        } else {
                            xmltv->end_year = start_year;
                            xmltv->end_month = start_month;
                            xmltv->end_day = start_day;
                            xmltv->end_hour = 23;
                            xmltv->end_min = 59;
                            if( date_compare( xmltv->end_year, xmltv->end_month, xmltv->end_day,
                                              xmltv->end_hour, xmltv->end_min, year, month,
                                              day, hour, min ) > 0 ) {
                                sprintf( xmltv->times, "%2d:%02d - %2d:%02d",
                                         start_hour, start_min, xmltv->end_hour, xmltv->end_min  );
                                xmlFree( start );
                                xmlFree( channel );
                                return cur;
                            }
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

static void get_program_info( xmltv_t *xmltv, xmlNodePtr program )
{
    program = program->xmlChildrenNode;
    while( program ) {
        if( !xmlStrcasecmp( program->name, BAD_CAST "title" ) ) {
            xmltv->title = xmlNodeGetContent( program );
        } else if( !xmlStrcasecmp( program->name, BAD_CAST "sub-title" ) ) {
            xmltv->subtitle = xmlNodeGetContent( program );
        } else if( !xmlStrcasecmp( program->name, BAD_CAST "desc" ) ) {
            xmltv->description = xmlNodeGetContent( program );
        }
        program = program->next;
    }
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

    xmltv->title = xmltv->subtitle = xmltv->description = xmltv->curchan = 0;
    *xmltv->times = 0;
    xmltv->refresh = 1;

    return xmltv;
}

void xmltv_delete( xmltv_t *xmltv )
{
    if( xmltv->title ) xmlFree( xmltv->title );
    if( xmltv->subtitle ) xmlFree( xmltv->subtitle );
    if( xmltv->description ) xmlFree( xmltv->description );
    if( xmltv->curchan ) xmlFree( xmltv->curchan );
    xmlFreeDoc( xmltv->doc );
    free( xmltv );
}

void xmltv_set_channel( xmltv_t *xmltv, const char *channel )
{
    snprintf( xmltv->curchannel, sizeof( xmltv->curchannel ), "%s", channel );
    xmltv->refresh = 1;
}

void xmltv_refresh( xmltv_t *xmltv, int year, int month, int day,
                    int hour, int min )
{
    xmlNodePtr program;

    if( xmltv->title ) xmlFree( xmltv->title );
    if( xmltv->subtitle ) xmlFree( xmltv->subtitle );
    if( xmltv->description ) xmlFree( xmltv->description );
    xmltv->title = xmltv->subtitle = xmltv->description = 0;

    program = get_program( xmltv, xmltv->doc, xmltv->root, xmltv->curchannel,
                           year, month, day, hour, min );
    if( program ) {
        get_program_info( xmltv, program );
    } else {
        xmltv->end_year = year;
        xmltv->end_month = month;
        xmltv->end_day = day;
        xmltv->end_hour = hour + 1;
        xmltv->end_min = min;
    }
    xmltv->refresh = 0;
}

const char *xmltv_get_title( xmltv_t *xmltv )
{
    return (char *) xmltv->title;
}

const char *xmltv_get_sub_title( xmltv_t *xmltv )
{
    return (char *) xmltv->subtitle;
}

const char *xmltv_get_description( xmltv_t *xmltv )
{
    return (char *) xmltv->description;
}

const char *xmltv_get_times( xmltv_t *xmltv )
{
    return xmltv->times;
}

int xmltv_needs_refresh( xmltv_t *xmltv, int year, int month, int day,
                         int hour, int min )
{
    return (xmltv->refresh || (date_compare( year, month, day, hour, min,
                                             xmltv->end_year, xmltv->end_month, xmltv->end_day,
                                             xmltv->end_hour, xmltv->end_min ) >= 0));
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

