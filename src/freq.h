/**
 * Copyright (c) 2003 Achim Schneider <batchall@mordor.ch>
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

#ifndef FREQ_H_INCLUDED
#define FREQ_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define US_CABLE_NOMINAL 	0
#define US_CABLE_HRC		1
#define US_CABLE_IRC		2

int freq_byName( const char **band, const char **channel, int us_cable );

typedef int (*freq_callback_t)( const char *band, const char *channel, unsigned int freq );
int freq_for_band( const char *band, int us_cable, freq_callback_t f );

// tab-seperated
//char *freq_getBands();

#ifdef __cplusplus
};
#endif
#endif /* FREQ_H_INCLUDED */