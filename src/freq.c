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

#include <string.h>
#include "freq.h"
#include "bands.h"



band_t *getBand( char *band ) {
	band_t *rp;
	
	for ( rp= (band_t *)bands; rp < &bands[numbands]; ++rp )
		if ( !strcasecmp( rp->name, band ) )
			return rp;

	return NULL;
}

int freq_byName( char **band, char **channel, int us_cable) {
	band_t *b;
	band_entry_t *rp;
	
	if ( NULL == ( b= getBand(*band) ) )
		return 0;
	for ( rp= (band_entry_t *)b->channels; rp < &(b->channels[b->count]); ++rp ) {
		if ( !strcasecmp( rp->name, *channel ) ) {
			if ( us_cable != US_CABLE_NOMINAL && !strcmp( b->name, "US Cable") ) {
				return  us_cable == US_CABLE_HRC
					? NTSC_CABLE_HRC(rp->freq)
					: NTSC_CABLE_IRC(rp->freq);
			}
			*band= (char *)b->name;
			*channel= (char *)rp->name;
			return rp->freq;
		}
	}

	return 1;
}


int freq_byPos( char *band, int channel ) {
	band_t *b;
	
	if ( NULL == ( b= getBand(band) ) )
		return 0;
	
	if ( b->count >= channel || channel < 0 )
		return 0;

	return b->channels[channel].freq;
}


int freq_for_band(char *band, int us_cable, freq_callback_t f) {
	band_t *b;
	band_entry_t *rp;
	if ( NULL == ( b= getBand(band) ) ) 
		return 0;

	for ( rp= (band_entry_t *)b->channels; rp < &b->channels[b->count]; ++rp ) {
		int freq= rp->freq;
		if ( us_cable != US_CABLE_NOMINAL && !strcmp( b->name, "US Cable") ) {
			freq=  us_cable == US_CABLE_HRC
				? NTSC_CABLE_HRC(rp->freq)
				: NTSC_CABLE_IRC(rp->freq);
		}
		
		(*f) ((char *)b->name, (char *)rp->name, freq);
	}

	return 1;
}
//char *freq_getBands() {
//	return band_names;
//}




