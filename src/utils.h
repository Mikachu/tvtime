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

#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

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

#endif /* UTILS_H_INCLUDED */
