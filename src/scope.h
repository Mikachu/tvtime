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

#ifndef SCOPE_H_INCLUDED
#define SCOPE_H_INCLUDED

typedef struct scope_s scope_t;

scope_t *scope_new( int width, int height );
void scope_delete( scope_t *scope );
void scope_set_scanline( scope_t *scope, uint8_t *scanline, int id );
int scope_active_on_scanline( scope_t *scope, int scanline );
void scope_composite_packed422_scanline( scope_t *scope, uint8_t *output,
                                         int width, int xpos, int scanline );


#endif /* SCOPE_H_INCLUDED */
