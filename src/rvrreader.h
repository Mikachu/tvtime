/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef RVRREADER_H_INCLUDED
#define RVRREADER_H_INCLUDED

#include <ree.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rvrreader_s rvrreader_t;

rvrreader_t *rvrreader_new( const char *filename );
void rvrreader_delete( rvrreader_t *reader );

ree_file_header_t *rvrreader_get_fileheader( rvrreader_t *reader );
int rvrreader_get_width( rvrreader_t *reader );
int rvrreader_get_height( rvrreader_t *reader );

int rvrreader_next_frame( rvrreader_t *reader );

uint8_t *rvrreader_get_curframe( rvrreader_t *reader );
uint8_t *rvrreader_get_lastframe( rvrreader_t *reader );
uint8_t *rvrreader_get_secondlastframe( rvrreader_t *reader );

#ifdef __cplusplus
};
#endif
#endif /* RVRREADER_H_INCLUDED */
