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

#ifndef XMLTV_H_INCLUDED
#define XMLTV_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xmltv_s xmltv_t;

/**
 * Creates a new XMLTV parser object.
 */
xmltv_t *xmltv_new( const char *filename );

/**
 * Cleans up and deletes an XMLTV parser object.
 */
void xmltv_delete( xmltv_t *xmltv );

/**
 * Sets the current channel to use for xmltv.
 */
void xmltv_set_channel( xmltv_t *xmltv, const char *channel );

/**
 * Refreshes the information given a new time.
 */
void xmltv_refresh( xmltv_t *xmltv, int year, int month, int day,
                    int hour, int min );

/**
 * Returns the current show title if one is set, 0 otherwise.
 */
const char *xmltv_get_title( xmltv_t *xmltv );

/**
 * Returns the current show sub-title if one is set, 0 otherwise.
 */
const char *xmltv_get_sub_title( xmltv_t *xmltv );

/**
 * Returns the current show description if one is set, 0 otherwise.
 */
const char *xmltv_get_description( xmltv_t *xmltv );

/**
 * Returns true if the show information must be updated (the current
 * program has ended.
 */
int xmltv_needs_refresh( xmltv_t *xmltv, int year, int month, int day,
                         int hour, int min );

/**
 * Looks up the id of a channel given its name.
 */
const char *xmltv_lookup_channel( xmltv_t *xmltv, const char *name );

#ifdef __cplusplus
};
#endif
#endif /* XMLTV_H_INCLUDED */
