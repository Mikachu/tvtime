/**
 * Copyright (C) 2002, 2003 Doug Bell <drbell@users.sourceforge.net>
 *
 * Some mixer routines from mplayer, http://mplayer.sourceforge.net.
 * Copyright (C) 2000-2002. by A'rpi/ESP-team & others
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

/**
 * Sets the mixer device and channel.  The device name is of the form
 * devicename:channelname.  The default is /dev/mixer:line.
 */
void mixer_set_device( const char *devname );

/**
 * Returns the current volume setting.
 */
int mixer_get_volume( void );

/**
 * Tunes the relative volume.
 */
int mixer_set_volume( int percentdiff );

/**
 * Sets the mute state.
 */
void mixer_mute( int mute );

/**
 * Returns true if the mixer is muted.
 */
int mixer_ismute( void );

/**
 * Closes the mixer device if it is open.
 */
void mixer_close_device( void );

#ifdef __cplusplus
};
#endif
#endif /* MIXER_H_INCLUDED */
