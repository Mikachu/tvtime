/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 *
 * Mixer routines stolen from mplayer, http://mplayer.sourceforge.net.
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


#ifndef INPUT_H_INCLUDED
#define INPUT_H_INCLUDED

typedef struct input_s input_t;

typedef enum InputEvent_e {

    I_NOOP                  = 0,
    I_KEYDOWN               = (1<<0)

} InputEvent;



input_t *input_new( config_t *cfg, osd_t *osd, videoinput_t *vidin );
void    input_delete( input_t *in );
void    input_callback( input_t *in, InputEvent command, int arg );

#endif
