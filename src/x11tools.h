/**
 * Copyright (C) 2000-2002. by A'rpi/ESP-team & others
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * Screen saver disabling routines from mplayer, 
 *   http://www.mplayerhq.hu/
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

#ifndef X11TOOLS_H_INCLUDED
#define X11TOOLS_H_INCLUDED

#include <X11/Xlib.h>

void saver_on( Display *mDisplay );
void saver_off( Display *mDisplay );

#endif /* X11TOOLS_H_INCLUDED */
