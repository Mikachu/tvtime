/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef VGASYNC_H_INCLUDED
#define VGASYNC_H_INCLUDED

/**
 * Initialize the ugly hack.
 */
int vgasync_init( int verbose );

/**
 * Waits until a refresh has completed and we're back up at the top.
 */
void vgasync_spin_until_end_of_next_refresh( void );

/**
 * Waits until a refresh has completed and we're back up at the top.
 */
void vgasync_spin_until_out_of_refresh( void );

#endif /* VGASYNC_H_INCLUDED */
