/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef TVTIMEOSD_H_INCLUDED
#define TVTIMEOSD_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This object is the master on-screen-display controler.  It's basically
 * the high-level API to control the simple OSD items like the channel
 * number, volume bar, etc.
 *
 * The OSD currently lays out the screen as follows:
 *
 * +-----------                     ------+
 * |                                      |
 * |   Channel             Channel Info
 * |   Number              Channel Logo
 * |
 * |
 *
 * |
 * |  [ Muted - Volume Bar - Data Bar   ]
 * |                                      |
 * +----                            ------+
 */

typedef struct tvtime_osd_s tvtime_osd_t;

/**
 */
tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect );
void tvtime_osd_delete( tvtime_osd_t *osd );

void tvtime_osd_show_info( tvtime_osd_t *osd );
void tvtime_osd_set_norm( tvtime_osd_t *osd, const char *norm );
void tvtime_osd_set_input( tvtime_osd_t *osd, const char *text );
void tvtime_osd_set_channel_number( tvtime_osd_t *osd, const char *text );
void tvtime_osd_set_deinterlace_method( tvtime_osd_t *osd, const char *method );

void tvtime_osd_set_timeformat( tvtime_osd_t *osd, const char *format );


void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage );
void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate );

int tvtime_osd_data_bar_visible( tvtime_osd_t *osd );
void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage );
void tvtime_osd_show_message( tvtime_osd_t *osd, const char *message );

/**
 * This function must be called every frame to update the state of the OSD.
 */
void tvtime_osd_advance_frame( tvtime_osd_t *osd );

/**
 * Returns true if there is anything to composite on this scanline.
 */
int tvtime_osd_active_on_scanline( tvtime_osd_t *osd, int scanline );

/**
 * Asks the OSD object to composite itself onto the given scanline.
 * xpos indicates the x position that the output pointer points at,
 * width is the maximum width we should composite to, and scanline
 * is which scanline of the output frame this request is for.
 */
void tvtime_osd_composite_packed422_scanline( tvtime_osd_t *osd,
                                              unsigned char *output,
                                              int width, int xpos,
                                              int scanline );

#ifdef __cplusplus
};
#endif
#endif /* TVTIMEOSD_H_INCLUDED */
