/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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
#include <errno.h>
#include <string.h>
#include "parser.h"


struct nv_pair {
    char *name;
    char *value;
};

void parser_delete( parser_file_t *pf )
{
    int i;

    if( pf->file_contents ) free( pf->file_contents );
    for( i = 0; pf->nv_pairs[i].name && i < pf->num_pairs; i++ ) {
        free( pf->nv_pairs[i].name );
        free( pf->nv_pairs[i].value );
    }
    free( pf->nv_pairs );
}

int parser_openfile( parser_file_t *pf, const char *filename )
{
    if( !filename ) return 0;

    if( pf->fh ) {
        fclose( pf->fh );
        pf->fh = NULL;
    }

    pf->fh = fopen( filename, "r" );
    if( !(pf->fh) ) {
        return 0;
    }

    return 1;
}

const char *parser_get( parser_file_t *pf, const char *name, int k )
{
    int i;

    if( !pf ) {
        fprintf( stderr, "parser: No parser_file_t* given.\n" );
        return NULL;
    }

    for( i=0; i < pf->num_pairs && pf->nv_pairs[i].name; i++ ) {
        if( !strcasecmp( name, pf->nv_pairs[i].name ) ) {
            if ( --k <= 0 ) {
                    return pf->nv_pairs[i].value;
            }
        }
    }

    return NULL;
}

/*
void parser_set( parser_file_t *pf, const char *name, int k, 
                 const char * value )
{
    parser_fetch( pf, name, k, (void**)value, 1 );
}


const char *parser_get( parser_file_t *pf, const char *name, int k )
{
    char *value;

    parser_fetch( pf, name, k, (void**)&value, 0 );

    return value;
}
*/

int parser_readfile( parser_file_t *pf )
{
    long length=0;
    size_t num_read = 0;
    char *file;

    if( !(pf->fh) ) {
        fprintf( stderr, "parser: File not open for reading.\n" );
        return 0;
    }

    fseek( pf->fh, 0L, SEEK_END );
    length = ftell( pf->fh );
    fseek( pf->fh, 0L, SEEK_SET );
    file = (char *)malloc(length+1);
    if( !file ) {
        fprintf( stderr, "parser: Insufficient memory to read file.\n" );
        return 0;
    }
    num_read = fread( (void*)file, sizeof(char), length+1, pf->fh );
    if( num_read == 0 ) {
        if( ferror( pf->fh ) > 0 ) {
            fprintf( stderr, "parser: Could not read file.\n" );
            return 0;
        }   
    }

    file[length+1] = '\0';
    pf->file_contents = file;
    pf->file_length = length+1;

    return 1;
}

int parser_merge_file( parser_file_t* pf, const char *filename )
{
    char *ptr, *name, *value, *start, *tmp;
    int pairs=0, saweq=0;
    
    if( !parser_openfile( pf, filename) ) {
        return 0;
    }

    if( !parser_readfile( pf ) ) {
        fprintf( stderr, "parser: Could not read %s.\n", 
                 filename ? filename : "(null)" );
        return 0;
    }
    
    pf->filename = strdup( filename );
    if( !pf->filename ) {
        fprintf( stderr, "parser: Insufficient memory to strdup filename.\n" );
        return 0;
    }

    ptr = pf->file_contents;
    while( *ptr ) {
        if( *ptr == '\n' || *ptr == '\r' ) *ptr = '\0';
        ptr++;
    }
    ptr = pf->file_contents;
    for( ; ptr < pf->file_contents + pf->file_length; ) {
        switch( *ptr ) {
        case ' ':
        case '\t':
        case '\0':
            ptr++;
            break;

        case '#':
            while( *ptr ) ptr++;
            ptr++;
            break;

        default:
            start = ptr;
            while( *ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '=') ptr++;
            if( !*ptr ) {
                fprintf( stderr, "parser: parse error.\n" );
                free( pf->file_contents );
                pf->file_contents = NULL;
                return 0;
            }
            saweq=0;
            if( *ptr && *ptr == '=') saweq = 1;
            *ptr = '\0';

            name = strdup( start );
            if( !name ) {
                fprintf( stderr, "parser: Error doing strdup.\n" );
                free( pf->file_contents );
                pf->file_contents = NULL;
                return 0;
            }
            ptr++;

            /* skip spaces */
            while( !saweq && *ptr && (*ptr == ' ' || *ptr == '\t') ) ptr++;
            if( saweq || (*ptr && *ptr == '=') ) { 
                if( !saweq ) ptr++;
                /* skip spaces after equal sign */
                while( *ptr && (*ptr == ' ' || *ptr == '\t') ) ptr++;
            } else {
                fprintf( stderr, "parser: missing equal sign in config.\n" );
                free( pf->file_contents );
                pf->file_contents = NULL;
                return 0;                
            }
            value = strdup( ptr );
            /* remove spaces from end of value */
            tmp = value + strlen( value ) - 1;
            while( *tmp == ' ' || *tmp == '\t' ) {
                *tmp = '\0';
                tmp--;
            }

            if( !value ) {
                fprintf( stderr, "parser: Error doing strdup.\n" );
                free( pf->file_contents );
                pf->file_contents = NULL;
                return 0;
            }

            while( *ptr ) ptr++;
            ptr++; /* skip over the 0 */

            /* keep name/value */
            if( pairs >= pf->num_pairs ) {
                struct nv_pair *tmp;
                tmp = (struct nv_pair*)realloc( (void*)pf->nv_pairs, 
                                                (pf->num_pairs+25)*sizeof(struct nv_pair) );
                if( !tmp ) {
                    fprintf( stderr, "parser: Error reallocing.\n" );
                    free( pf->file_contents );
                    pf->file_contents = NULL;
                    return 0;
                }
                pf->nv_pairs = tmp;
                memset( (void*)tmp+(pf->num_pairs*sizeof(struct nv_pair)),
                        0, sizeof(struct nv_pair)*25);
                pf->num_pairs += 25;
            }

            pf->nv_pairs[pairs].name = name;
            pf->nv_pairs[pairs].value = value;
            pairs++;
            break;
        }
    }

    free( pf->file_contents );
    fclose( pf->fh );
    pf->fh = NULL;
    pf->file_length = 0;
    pf->file_contents = NULL;

    return 1;
}

int parser_new( parser_file_t *pf, const char *filename )
{
    if( !pf ) return 0;

    pf->fh = NULL;
    pf->file_length = 0;
    pf->file_contents = NULL;

    /* default to 25 nv pairs -- will realloc later if necessary */
    pf->nv_pairs = (struct nv_pair*)malloc(sizeof(struct nv_pair)*25);
    memset( pf->nv_pairs, 0, sizeof(struct nv_pair)*25 );
    pf->num_pairs = 25;

    return parser_merge_file( pf, filename );
}

int parser_dump( parser_file_t *pf )
{
    int i;

    if( !pf ) return 0;

    for( i=0; i < pf->num_pairs; i++ ) {
        if( !pf->nv_pairs[i].name ) break;
        fprintf( stderr, "parser: %s = %s\n", pf->nv_pairs[i].name, pf->nv_pairs[i].value );
    }
    return 1;
}

