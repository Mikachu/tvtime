/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef MENU_H_INCLUDED
#define MENU_H_INCLUDED

#include "input.h"
#include "tvtimeconf.h"

typedef struct menu_s menu_t;

menu_t *menu_new( input_t *in, config_t *cfg, int width, 
                  int height, double aspect );

void menu_init( menu_t *m );

int menu_callback( menu_t *m, InputEvent command, int arg );


#endif
