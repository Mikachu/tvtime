/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef INPUT_H_INCLUDED
#define INPUT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "tvtimeconf.h"
#include "console.h"
#include "commands.h"

typedef struct input_s input_t;

/* Input events. */
enum input_event
{
    I_NOOP,
    I_KEYDOWN,
    I_QUIT,
    I_BUTTONPRESS,
    I_MOUSEMOVE,
    I_REMOTE
};

input_t *input_new( config_t *cfg, commands_t *com, console_t *con, int verbose );
void input_delete( input_t *in );
void input_callback( input_t *in, int command, int arg );

#ifdef __cplusplus
};
#endif
#endif /* INPUT_H_INCLUDED */
