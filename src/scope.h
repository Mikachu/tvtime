/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef SCOPE_H_INCLUDED
#define SCOPE_H_INCLUDED

typedef struct scope_s scope_t;

scope_t *scope_new( int width, int height );
void scope_delete( scope_t *scope );
void scope_set_scanline( scope_t *scope, unsigned char *scanline, int id );
int scope_active_on_scanline( scope_t *scope, int scanline );
void scope_composite_packed422_scanline( scope_t *scope,
                                         unsigned char *output,
                                         int width, int xpos, int scanline );


#endif /* SCOPE_H_INCLUDED */
