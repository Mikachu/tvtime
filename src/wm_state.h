/* Ogle - A video player
 * Copyright (C) 2001, 2002 Bj�rn Englund, H�kan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef WM_STATE_H
#define WM_STATE_H

#include <X11/Xlib.h>

typedef enum {
  WINDOW_STATE_NORMAL,
  WINDOW_STATE_FULLSCREEN
} WindowState_t;


int ChangeWindowState(Display *dpy, Window win, WindowState_t state);

#endif


