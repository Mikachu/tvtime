/**
 * Copyright (C) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef TVTIMEOSD_H_INCLUDED
#define TVTIMEOSD_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

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

tvtime_osd_t *tvtime_osd_new( int width, int height, double pixel_aspect,
                              unsigned int channel_rgb, unsigned int other_rgb );
void tvtime_osd_delete( tvtime_osd_t *osd );
void tvtime_osd_set_pixel_aspect( tvtime_osd_t *osd, double pixel_aspect );

void tvtime_osd_show_info( tvtime_osd_t *osd );

void tvtime_osd_hold( tvtime_osd_t *osd, int hold );

void tvtime_osd_set_norm( tvtime_osd_t *osd, const char *norm );
void tvtime_osd_set_freq_table( tvtime_osd_t *osd, const char *freqtable );
void tvtime_osd_set_audio_mode( tvtime_osd_t *osd, const char *audiomode );
void tvtime_osd_set_input( tvtime_osd_t *osd, const char *text );
void tvtime_osd_set_channel_number( tvtime_osd_t *osd, const char *text );
void tvtime_osd_set_channel_name( tvtime_osd_t *osd, const char *text );
void tvtime_osd_set_hold_message( tvtime_osd_t *osd, const char *text );
void tvtime_osd_set_deinterlace_method( tvtime_osd_t *osd, const char *method );
void tvtime_osd_set_film_mode( tvtime_osd_t *osd, int mode );
void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate );
void tvtime_osd_signal_present( tvtime_osd_t *osd, int signal );
void tvtime_osd_set_framerate( tvtime_osd_t *osd, double framerate, int mode );

void tvtime_osd_set_timeformat( tvtime_osd_t *osd, const char *format );

void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage );

int tvtime_osd_data_bar_visible( tvtime_osd_t *osd );
void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage );
void tvtime_osd_show_message( tvtime_osd_t *osd, const char *message );

/**
 * This function must be called every frame to update the state of the OSD.
 */
void tvtime_osd_advance_frame( tvtime_osd_t *osd );

/**
 * Asks the OSD object to composite itself onto the given scanline.
 * xpos indicates the x position that the output pointer points at,
 * width is the maximum width we should composite to, and scanline
 * is which scanline of the output frame this request is for.
 */
void tvtime_osd_composite_packed422_scanline( tvtime_osd_t *osd,
                                              uint8_t *output,
                                              int width, int xpos,
                                              int scanline );

void tvtime_osd_set_network_call( tvtime_osd_t* osd, const char *str );
void tvtime_osd_set_network_name( tvtime_osd_t* osd, const char *str );
void tvtime_osd_set_show_name( tvtime_osd_t* osd, const char *str );
void tvtime_osd_set_show_rating( tvtime_osd_t* osd, const char *str );
void tvtime_osd_set_show_start( tvtime_osd_t* osd, const char *str );
void tvtime_osd_set_show_length( tvtime_osd_t* osd, const char *str );

#ifdef __cplusplus
};
#endif
#endif /* TVTIMEOSD_H_INCLUDED */
