/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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
 *
 * Contains UTF-8 to unicode conversion from encoding.c in libxml2.

Except where otherwise noted in the source code (trio files, hash.c and list.c)
covered by a similar licence but with different Copyright notices:

 Copyright (C) 1998-2002 Daniel Veillard.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.

 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include <dirent.h>
#include <pwd.h>
#include "tvtimeconf.h"
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
#ifdef FONTDIR
    return FONTDIR ":" DATADIR ":../data:./data";
#else
    return DATADIR ":../data:./data";
#endif
}

const char *get_tvtime_fifodir( config_t *ct )
{
    static char *fifodir = NULL;

    if( !fifodir ) {
        /* We have nothing in our cache. */
        struct passwd *pwuid = NULL;
        char *user = NULL;
        DIR *dirhandle = NULL;

        /* Check for the user's existance. */
        pwuid = getpwuid( config_get_uid( ct ) );
        if( pwuid ) {
            if( asprintf( &user,
                          "%s",
                          pwuid->pw_name) < 0 ) {
                fprintf( stderr, "utils: Out of memory.\n" );
                return NULL;
            }
        } else {
            if( asprintf( &user,
                          "%u",
                          config_get_uid( ct ) ) < 0 ) {
                fprintf( stderr, "utils: Out of memory.\n" );
                return NULL;
            }
        }

        /* First try the FIFODIR. */
        if( asprintf( &fifodir, 
                      "%s/TV-%s",
                      FIFODIR,
                      user ) < 0 ) {
            free( user );
            fprintf( stderr, "utils: Out of memory.\n" );
            return NULL;
        }
        free( user );

        /* We will try to ensure that the FIFO directory exists,
         * and create the user's subdirectory while we're at it. */
        errno = 0;
        if( mkdir( fifodir, S_IRWXU ) < 0 ) {
            if( errno == EEXIST ) {
                dirhandle = opendir( fifodir );
                if( dirhandle ) {
                    struct stat dirstat;
                    closedir( dirhandle );
                    if( !stat( fifodir, &dirstat ) ) {
                        if( dirstat.st_uid == config_get_uid( ct ) ) {
                            return fifodir;
                        }
                    }
                }
            }
        }

        /* We will now resort to using the home directory. */
        free( fifodir );
        user = getenv( "HOME" );
        if( !user ) {
            fprintf( stderr, "utils: You're HOMEless, go away!\n" );
            return NULL;
        }
        if( asprintf( &fifodir,
                      "%s/.tvtime",
                      user ) < 0 ) {
            fprintf( stderr, "utils: Out of memory.\n" );
            return NULL;
        }
        free( user );

        /* We will try to ensure that the FIFO directory exists. */
        errno = 0;
        if( mkdir( fifodir, S_IRWXU ) < 0 ) {
            if( errno == EEXIST ) {
                dirhandle = opendir( fifodir );
                if( dirhandle ) {
                    struct stat dirstat;
                    closedir( dirhandle );
                    if( !stat( fifodir, &dirstat ) ) {
                        if( dirstat.st_uid == config_get_uid( ct ) ) {
                            return fifodir;
                        }
                    }
                }
            }
        }

        /* It appears we failed.  Bail out. */
        free( fifodir );
        return NULL;
    }

    /* Return the value in the cache. */
    return fifodir;
}

const char *get_tvtime_fifo( config_t *ct )
{
    static char *fifo = NULL;

    if( !fifo ) {
        /* We have nothing in our cache. */
        const char *fifodir = NULL;
        char *hostname = NULL;
        size_t hostenv_size = 256; 
        char *hostenv = NULL;
	char *hostenv_realloc = NULL;

        /* Get the FIFO directory. */
        fifodir = get_tvtime_fifodir( ct );
        if( !fifodir ) {
            fprintf( stderr, "utils: No FIFO directory found.!\n" );
            return NULL;
        }

        /* Try to get a hostname. */
        hostenv = (char *) malloc( hostenv_size );
        while( gethostname( hostenv, hostenv_size ) < 0 ) {
            hostenv_size *= 2;
            hostenv_realloc = realloc( hostenv, hostenv_size );
            if( hostenv_realloc == NULL ) {
                /* Use a partial hostname. */
                hostenv_size /= 2;
                break;
            } else {
                hostenv = hostenv_realloc;
            }
        }

        /* Use this hostname as a suffix to the FIFO */
        if( hostenv ) {
            if( asprintf( &hostname, "-%s", hostenv ) < 0 ) {
                fprintf( stderr, "utils: Out of memory.\n" );
                /* Try to get away with no hostname. */
                if( asprintf( &hostname, "%s", "" ) < 0 ) {
                    fprintf( stderr, "utils: Really out of memory.\n" );
                    return NULL;
                }
            }
        } else {
            if( asprintf( &hostname, "%s", "" ) < 0 ) {
                fprintf( stderr, "utils: Really out of memory.\n" );
                return NULL;
            }
        }

        if( asprintf( &fifo,
                      "%s/tvtimefifo%s",
                      fifodir,
                      hostname ) < 0 ) {
            fprintf( stderr, "utils: Out of memory.\n" );
            if( asprintf( &fifo, "%s", "" ) < 0 ) {
                fprintf( stderr, "utils: Really out of memory.\n" );
                return NULL;
            }
        }
    }

    /* Return the value in the cache. */
    return fifo;
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

/**
 * xmlGetUTF8Char:
 *  utf:  a sequence of UTF-8 encoded bytes
 *  len:  a pointer to @bytes len
 *
 * Read one UTF8 Char from @utf
 * Returns the char value or -1 in case of error and update
 * len with the number of bytes used.
 */
uint32_t utf8_to_unicode( const char *utf, int *len )
{
    unsigned int c;
    
    if (utf == NULL)
   goto error; 
    if (len == NULL)
   goto error; 
    if (*len < 1)
   goto error; 
   
    c = utf[0];
    if (c & 0x80) {
   if (*len < 2)
       goto error;
   if ((utf[1] & 0xc0) != 0x80)
       goto error;
   if ((c & 0xe0) == 0xe0) {
       if (*len < 3) 
      goto error; 
       if ((utf[2] & 0xc0) != 0x80)
      goto error;
       if ((c & 0xf0) == 0xf0) {
      if (*len < 4)
          goto error;
      if ((c & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80)
          goto error;
      *len = 4;
      /* 4-byte code */
      c = (utf[0] & 0x7) << 18;
      c |= (utf[1] & 0x3f) << 12;
      c |= (utf[2] & 0x3f) << 6;
      c |= utf[3] & 0x3f;
       } else {
         /* 3-byte code */
      *len = 3;
      c = (utf[0] & 0xf) << 12;
      c |= (utf[1] & 0x3f) << 6;
      c |= utf[2] & 0x3f;
       }
   } else {
     /* 2-byte code */
       *len = 2;
       c = (utf[0] & 0x1f) << 6;
       c |= utf[1] & 0x3f;
   }
    } else {
   /* 1-byte code */
   *len = 1;
    }
    return c;

error:
    *len = 0;
    return 0;
}

