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

const band_t *getBand( const char *band )
{
    const band_t *rp;
    
    for( rp = bands; rp < &bands[ numbands ]; ++rp ) {
        if( !strcasecmp( rp->name, band ) ) {
            return rp;
        }
    }

    return 0;
}

int freq_byName( const char **band, const char **channel, int us_cable )
{
    const band_t *b;
    const band_entry_t *rp;

    b = getBand( *band ); 
    if( !b ) return 0;

    for( rp = b->channels; rp < &(b->channels[ b->count ]); ++rp ) {
        if( !strcasecmp( rp->name, *channel ) ) {

            *band = b->name;
            *channel = rp->name;

            if( us_cable != US_CABLE_NOMINAL && !strcmp( b->name, "US Cable" ) ) {
                return (us_cable == US_CABLE_HRC) ? NTSC_CABLE_HRC( rp->freq )
                                                  : NTSC_CABLE_IRC( rp->freq );
            }

            return rp->freq;
        }
    }

    return 1;
}


int freq_byPos( const char *band, int channel )
{
    const band_t *b;

    b = getBand( band );
    if( !b ) return 0;

    if( b->count >= channel || channel < 0 ) {
        return 0;
    }

    return b->channels[ channel ].freq;
}


int freq_for_band( const char *band, int us_cable, freq_callback_t f )
{
    const band_t *b;
    const band_entry_t *rp;

    b = getBand( band );
    if( !b ) return 0;

    for( rp = b->channels; rp < &b->channels[ b->count ]; ++rp ) {
        int freq = rp->freq;

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

