/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This file is subject to the terms of the GNU General Public License as
 * published by the Free Software Foundation.  A copy of this license is
 * included with this software distribution in the file COPYING.  If you
 * do not have a copy, you may obtain a copy by writing to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#ifndef RVRREADER_H_INCLUDED
#define RVRREADER_H_INCLUDED

#include <ree.h>

typedef struct rvrreader_s rvrreader_t;

rvrreader_t *rvrreader_new( const char *filename );
void rvrreader_delete( rvrreader_t *reader );

ree_file_header_t *rvrreader_get_fileheader( rvrreader_t *reader );
int rvrreader_get_width( rvrreader_t *reader );
int rvrreader_get_height( rvrreader_t *reader );

int rvrreader_next_frame( rvrreader_t *reader );

unsigned char *rvrreader_get_curframe( rvrreader_t *reader );
unsigned char *rvrreader_get_lastframe( rvrreader_t *reader );
unsigned char *rvrreader_get_secondlastframe( rvrreader_t *reader );

#endif /* RVRREADER_H_INCLUDED */
