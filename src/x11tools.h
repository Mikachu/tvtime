/**
 * Copyright (C) 2000-2002. by A'rpi/ESP-team & others
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * Screen saver disabling routines from mplayer, 
 *   http://www.mplayerhq.hu/
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

#ifndef X11TOOLS_H_INCLUDED
#define X11TOOLS_H_INCLUDED

#include <X11/Xlib.h>

void saver_on( Display *mDisplay );
void saver_off( Display *mDisplay );

#endif /* X11TOOLS_H_INCLUDED */
