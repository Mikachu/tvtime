/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef VIDMODE_H_INCLUDED
#define VIDMODE_H_INCLUDED

typedef struct vidmode_s vidmode_t;

vidmode_t *vidmode_new( void );
void vidmode_delete( vidmode_t *vm );

void vidmode_print_modeline( vidmode_t *vm );

int vidmode_get_current_width( vidmode_t *vm );
int vidmode_get_current_height( vidmode_t *vm );
double vidmode_get_current_refresh_rate( vidmode_t *vm );

#endif /* VIDMODE_H_INCLUDED */
