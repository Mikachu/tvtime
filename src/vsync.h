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

#ifndef VSYNC_H_INCLUDED
#define VSYNC_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This file is the beginning of my abstraction layer for vsync
 * interrupt handling for applications.
 *
 * If you intend to use this code in your application, please contact me
 * so we can finish up the parts which are lacking.
 *
 *  - Billy Biggs <vektor@dumbterm.net>
 */

int vsync_init( void );
void vsync_shutdown( void );
void vsync_wait_for_retrace( void );

#ifdef __cplusplus
};
#endif
#endif /* VSYNC_H_INCLUDED */
