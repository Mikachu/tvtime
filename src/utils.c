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

    cur = check_path( DATADIR, filename );
    if( cur ) return cur;

    cur = check_path( "../data", filename );
    if( cur ) return cur;

    cur = check_path( "./data", filename );
    if( cur ) return cur;

    return 0;
}

