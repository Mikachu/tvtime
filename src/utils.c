/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>
#include "utils.h"

int file_is_openable_for_read( const char *filename )
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

static char *check_path( const char *path, const char *filename )
{
    char *cur;

    if( asprintf( &cur, "%s/%s", path, filename ) < 0 ) {
        /* Memory not available, so we're not getting anywhere. */
        return 0;
    } else if( !file_is_openable_for_read( cur ) ) {
        free( cur );
        return 0;
    }

    return cur;
}

const char *get_tvtime_paths( void )
{
    return DATADIR ":../data:./data";
}

char *get_tvtime_file( const char *filename )
{
    char *cur;

#ifdef FONTDIR
    /* If FONTDIR is defined, we'll look for files there first.
     * This is designed for distributions that already have a standard
     * FreeFont package installed. */
    cur = check_path( FONTDIR, filename );
    if( cur ) return cur;
#endif

    cur = check_path( DATADIR, filename );
    if( cur ) return cur;

    cur = check_path( "../data", filename );
    if( cur ) return cur;

    cur = check_path( "./data", filename );
    if( cur ) return cur;

    return 0;
}

char *expand_user_path( const char *path )
{
    wordexp_t result;
    char *ret = 0;
    int i;

    /* Expand the string.  */
    switch( wordexp( path, &result, 0 ) ) {
        case 0:  /* Successful.  */
            break;
        case WRDE_NOSPACE:
            /**
             * If the error was `WRDE_NOSPACE',
             * then perhaps part of the result was allocated.
             */
            wordfree( &result );
        default: /* Some other error.  */
            return 0;
    }

    if( asprintf( &ret, "%s", result.we_wordv[ 0 ] ) < 0 ) {
        ret = 0;
    }

    wordfree( &result );
    return ret;
}

