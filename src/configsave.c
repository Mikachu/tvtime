/**
 * Copyright (c) 2003 Aleander Belov <asbel@mail.ru>.
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
 
/* For saving configuration in file use function 
   int configsave(const char *INIT_name, const char *INIT_val, const int INIT_num).
   
   INIT_name - Parameter name
   INIT_val - Parameter value
   INIT_num - number of parameter (f.e. for key_quit)
   
   look at the end of file for example and change 
   #define CFGFILE "/home/asbel/.tvtime/tvtimerc".
   for compile use gcc configsave.c && ./a.out
*/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libxml/parser.h>
#include "configsave.h"

struct configsave_s
{
    char *configFile;
    xmlDocPtr Doc;
};

static xmlNodePtr find_option( xmlNodePtr node, const char *optname )
{
    while( node ) {
        if( !xmlStrcasecmp( node->name, BAD_CAST "option" ) ) {
            xmlChar *name = xmlGetProp( node, BAD_CAST "name" );

            if( name && !xmlStrcasecmp( name, BAD_CAST optname ) ) {
                xmlFree( name );
                return node;
            }
            if( name ) xmlFree( name );
        }

        node = node->next;
    }

    return 0;
}

/**
 * This should be moved elsewhere.
 */
static int file_is_openable_for_read( const char *filename )
{
    int fd;
    fd = open( filename, O_RDONLY );
    if( fd < 0 ) {
        return 0;
    } else {
        close( fd );
        return 1;
    }
}


/* Attempt to parse the file for key elements and create them if they don't exist */
configsave_t *configsave_open( const char *filename )
{
    configsave_t *cs = (configsave_t *) malloc( sizeof( configsave_t ) );
    xmlNodePtr top;

    if( !cs ) {
        return 0;
    }

    cs->configFile = strdup( filename );
    if( !cs->configFile ) {
        free( cs );
        return 0;
    }

    cs->Doc = xmlParseFile( cs->configFile );
    if( !cs->Doc ) {
        if( file_is_openable_for_read( cs->configFile ) ) {
            fprintf( stderr, "configsave: Config file exists, but cannot be parsed.\n" );
            fprintf( stderr, "configsave: Settings will NOT be saved.\n" );
            free( cs->configFile );
            free( cs );
            return 0;
        } else {
            /* Config file doesn't exist, create a new one. */
            fprintf( stderr, "configsave: No config file found, creating a new one.\n" );
            cs->Doc = xmlNewDoc( BAD_CAST "1.0" );
            if( !cs->Doc ) {
                fprintf( stderr, "configsave: Could not create new config file.\n" );
                free( cs->configFile );
                free( cs );
                return 0;
            }
        }
    }

    top = xmlDocGetRootElement( cs->Doc );
    if( !top ) {
        top = xmlNewDocNode( cs->Doc, 0, BAD_CAST "tvtime", 0 );
        if( !top ) {
            fprintf( stderr, "configsave: Could not create toplevel element 'tvtime'.\n" );
            xmlFreeDoc( cs->Doc );
            free( cs->configFile );
            free( cs );
            return 0;
        } else {
            xmlDocSetRootElement( cs->Doc, top );
        }
    }

    if( xmlStrcasecmp( top->name, BAD_CAST "tvtime" ) ) {
        fprintf( stderr, "configsave: Root node in file %s should be 'tvtime'.\n", cs->configFile );
        xmlFreeDoc( cs->Doc );
        free( cs->configFile );
        free( cs );
        return 0;
    }

    xmlKeepBlanksDefault( 0 );
    xmlSaveFormatFile( cs->configFile, cs->Doc, 1 );
    return cs;
}

void configsave_close( configsave_t *cs )
{
    xmlFreeDoc( cs->Doc );
    free( cs->configFile );
    free( cs );
}

int configsave( configsave_t *cs, const char *INIT_name, const char *INIT_val, const int INIT_num )
{
    xmlNodePtr top, node;
    xmlAttrPtr attr;

    top = xmlDocGetRootElement( cs->Doc );
    if( !top ) {
        fprintf( stderr, "configsave: Error, can't get document root.\n" );
        return 0;
    }

    node = find_option( top->xmlChildrenNode, INIT_name );
    if( !node ) {
        node = xmlNewTextChild( top, 0, BAD_CAST "option", 0 );
        attr = xmlNewProp( node, BAD_CAST "name", BAD_CAST INIT_name );
        attr = xmlNewProp( node, BAD_CAST "value", BAD_CAST INIT_val );
    } else {
        xmlSetProp( node, BAD_CAST "value", BAD_CAST INIT_val );
    }

    xmlKeepBlanksDefault( 0 );
    xmlSaveFormatFile( cs->configFile, cs->Doc, 1 );
    return 1;
}

