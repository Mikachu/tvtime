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

typedef struct tvtime_osd_s tvtime_osd_t;

tvtime_osd_t *tvtime_osd_new( int width, int height, double frameaspect );
void tvtime_osd_delete( tvtime_osd_t *osd );

void tvtime_osd_show_channel_number( tvtime_osd_t *osd, const char *text );
void tvtime_osd_show_channel_info( tvtime_osd_t *osd, const char *text );
void tvtime_osd_show_channel_logo( tvtime_osd_t *osd );

void tvtime_osd_show_volume_bar( tvtime_osd_t *osd, int percentage );
void tvtime_osd_volume_muted( tvtime_osd_t *osd, int mutestate );

void tvtime_osd_show_data_bar( tvtime_osd_t *osd, const char *barname,
                               int percentage );
void tvtime_osd_show_message( tvtime_osd_t *osd, const char *message );
void tvtime_osd_composite_packed422( tvtime_osd_t *osd, unsigned char *output,
                                     int width, int height, int stride );
void tvtime_osd_advance_frame( tvtime_osd_t *osd );

#endif /* TVTIMEOSD_H_INCLUDED */
