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


#ifndef INPUT_H_INCLUDED
#define INPUT_H_INCLUDED

#include "tvtimeosd.h"
#include "videoinput.h"
#include "videotools.h"
#include "tvtimeconf.h"

typedef struct input_s input_t;

typedef enum InputEvent_e {

    I_NOOP                      = 0,
    I_KEYDOWN                   = (1<<0),
    I_QUIT                      = (1<<1)

} InputEvent;

/* Modifiers */
#define I_SHIFT                 (1<<16)
#define I_META                  (1<<17)
#define I_CTRL                  (1<<18)

/* Arrows + Home/End pad */
#define I_UP                    273
#define I_DOWN                  274
#define I_RIGHT                 275
#define I_LEFT                  276
#define I_INSERT                277
#define I_HOME                  278
#define I_END                   279
#define I_PGUP                  280
#define I_PGDN                  281

/* Function keys */
#define I_F1                    282
#define I_F2                    283
#define I_F3			        284
#define I_F4			        285
#define I_F5                    286
#define I_F6                    287
#define I_F7                    288
#define I_F8                    289
#define I_F9                    290
#define I_F10                   291
#define I_F11                   292
#define I_F12                   293
#define I_F13                   294
#define I_F14                   295
#define I_F15                   296

/* Misc. */
#define I_ESCAPE                27
#define I_ENTER                 13
#define I_PRINT                 316
#define I_MENU                  319

input_t *input_new( config_t *cfg, videoinput_t *vidin, 
                    tvtime_osd_t *osd, video_correction_t *vc );
void input_delete( input_t *in );
void input_callback( input_t *in, InputEvent command, int arg );
void input_menu_callback( input_t *in, InputEvent command, int arg );

int input_quit( input_t *in );
int input_take_screenshot( input_t *in );
int input_videohold( input_t *in );
int input_print_debug( input_t *in );
int input_show_bars( input_t *in );
int input_show_test( input_t *in );
int input_toggle_fullscreen( input_t *in );
int input_toggle_aspect( input_t *in );
int input_toggle_deinterlacing_mode( input_t *in );

void input_next_frame( input_t *in );
void input_menu_init( input_t *in );

#endif /* INPUT_H_INCLUDED */
