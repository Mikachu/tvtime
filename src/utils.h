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
 */

#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include "tvtimeconf.h"
#include <stdint.h>

/**
 * Useful functions that don't belong anywhere else.
 */

/**
 * Returns true if the file is openable for read.
 */
int file_is_openable_for_read( const char *filename );

/**
 * Returns a string for the location of the given file, checking
 * paths that may be used before make install.  Returns 0 if the
 * file is not found, otherwise returns a new string that must be
 * freed using free().
 */
char *get_tvtime_file( const char *filename );

/**
 * Returns the standard path list for tvtime.
 */
const char *get_tvtime_paths( void );

/**
 * Returns a FIFO directory name.
 */
const char *get_tvtime_fifodir( config_t *ct );

/**
 * Returns a FIFO file name.
 */
const char *get_tvtime_fifo( config_t *ct );

/**
 * Expands a pathname using wordexp.  This expands ~/foo
 * and ~username/foo to full paths.
 */
char *expand_user_path( const char *path );

/**
 * Returns a UCS4 (is this right?) character from a UTF-8 buffer.
 */
uint32_t utf8_to_unicode( const char *utf, int *len );

#endif /* UTILS_H_INCLUDED */
