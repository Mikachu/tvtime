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

#ifndef COMMANDS_H_INCLUDED
#define COMMANDS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct commands_s commands_t;

#include "input.h"
#include "tvtimeosd.h"
#include "videoinput.h"
#include "tvtimeconf.h"
#include "videocorrection.h"
#include "menu.h"
#include "console.h"
#include "vbidata.h"


commands_t *commands_new( config_t *cfg, videoinput_t *vidin, 
                          tvtime_osd_t *osd, video_correction_t *vc );
void commands_delete( commands_t *in );
void commands_handle( commands_t *in, int command, int arg );

int commands_quit( commands_t *in );
int commands_take_screenshot( commands_t *in );
int commands_print_debug( commands_t *in );
int commands_show_bars( commands_t *in );
int commands_toggle_fullscreen( commands_t *in );
int commands_toggle_aspect( commands_t *in );
int commands_toggle_deinterlacing_mode( commands_t *in );
int commands_toggle_menu( commands_t *in );
int commands_half_framerate( commands_t *in );
void commands_set_console( commands_t *in, console_t *con );
void commands_set_vbidata( commands_t *in, vbidata_t *con );

int commands_console_on( commands_t *in );
int commands_menu_on( commands_t *in );

void commands_set_menu( commands_t *in, menu_t *m );

void commands_next_frame( commands_t *in );

#ifdef __cplusplus
};
#endif
#endif /* COMMANDS_H_INCLUDED */
