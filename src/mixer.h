/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 *
 * Mixer routines stolen from mplayer, http://mplayer.sourceforge.net.
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

#ifndef MIXER_H_INCLUDED
#define MIXER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

int mixer_get_volume( void );
int mixer_set_volume( int percentdiff );
void mixer_mute( int mute );
int mixer_conditional_mute( void );
void mixer_toggle_mute( void );
int mixer_ismute( void );

#ifdef __cplusplus
};
#endif
#endif /* MIXER_H_INCLUDED */
