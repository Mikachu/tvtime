/**
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef XCOMMON_H_INCLUDED
#define XCOMMON_H_INCLUDED

#include <X11/Xlib.h>
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int x;
    int y;
    unsigned int width;
    unsigned int height;
} area_t;

typedef struct _mwmhints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    int input_mode;
    unsigned long status;
} MWMHints;

/* Motif window hints */
#define MWM_HINTS_FUNCTIONS     (1L << 0)
#define MWM_HINTS_DECORATIONS   (1L << 1)
#define MWM_HINTS_INPUT_MODE    (1L << 2)
#define MWM_HINTS_STATUS        (1L << 3)
/* bit definitions for MwmHints.functions */
#define MWM_FUNC_ALL            (1L << 0)
#define MWM_FUNC_RESIZE         (1L << 1)
#define MWM_FUNC_MOVE           (1L << 2)
#define MWM_FUNC_MINIMIZE       (1L << 3)
#define MWM_FUNC_MAXIMIZE       (1L << 4)
#define MWM_FUNC_CLOSE          (1L << 5)
/* bit definitions for MwmHints.decorations */
#define MWM_DECOR_ALL           (1L << 0)
#define MWM_DECOR_BORDER        (1L << 1)
#define MWM_DECOR_RESIZEH       (1L << 2)
#define MWM_DECOR_TITLE         (1L << 3)
#define MWM_DECOR_MENU          (1L << 4)
#define MWM_DECOR_MINIMIZE      (1L << 5)
#define MWM_DECOR_MAXIMIZE      (1L << 6)
/* bit definitions for MwmHints.inputMode */
#define MWM_INPUT_MODELESS                  0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL 1
#define MWM_INPUT_SYSTEM_MODAL              2
#define MWM_INPUT_FULL_APPLICATION_MODAL    3
#define PROP_MWM_HINTS_ELEMENTS             5

int xcommon_open_display( const char *user_geometry, int aspect, int verbose );
void xcommon_close_display( void );

Display *xcommon_get_display( void );
Window xcommon_get_output_window( void );
GC xcommon_get_gc( void );
int xcommon_is_fullscreen( void );
int xcommon_is_alwaysontop( void );
int xcommon_get_visible_width( void );
int xcommon_get_visible_height( void );

void xcommon_ping_screensaver( void );
area_t xcommon_get_video_area( void );
area_t xcommon_get_window_area( void );
void xcommon_set_video_scale( area_t scalearea );
void xcommon_clear_screen( void );
int xcommon_toggle_fullscreen( int fullscreen_width, int fullscreen_height );
int xcommon_toggle_root( int fullscreen_width, int fullscreen_height );
int xcommon_toggle_aspect( void );
int xcommon_toggle_alwaysontop( void );
void xcommon_poll_events( input_t *in );
void xcommon_set_window_caption( const char *caption );
void xcommon_set_window_position( int x, int y );
void xcommon_set_window_height( int window_height );
int xcommon_is_exposed( void );
void xcommon_set_colourkey( int colourkey );
void xcommon_frame_drawn( void );
void xcommon_set_fullscreen_position( int pos );
void xcommon_set_matte( int ystart, int height );
void xcommon_set_square_pixel_mode( int squarepixel );
int xcommon_is_fullscreen_supported( void );
int xcommon_is_alwaysontop_supported( void );
void xcommon_update_xawtv_station( int frequency, int channel_id,
                                   const char *channel_name );
void xcommon_update_server_time( unsigned long timestamp );

#ifdef __cplusplus
};
#endif
#endif /* XCOMMON_H_INCLUDED */
