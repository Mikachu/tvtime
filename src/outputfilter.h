/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef OUTPUTFILTER_H_INCLUDED
#define OUTPUTFILTER_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
#include "tvtimeosd.h"
#include "vbiscreen.h"
#include "vtscreen.h"
#include "console.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct outputfilter_s outputfilter_t;

outputfilter_t *outputfilter_new( void );
void outputfilter_delete( outputfilter_t *of );

void outputfilter_set_vbiscreen( outputfilter_t *of, vbiscreen_t *vbiscreen );
void outputfilter_set_vtscreen( outputfilter_t *of, vtscreen_t *vts );
void outputfilter_set_osd( outputfilter_t *of, tvtime_osd_t *osd );
void outputfilter_set_console( outputfilter_t *of, console_t *console );
void outputfilter_set_pixel_aspect( outputfilter_t *of, double pixelaspect );

int outputfilter_active_on_scanline( outputfilter_t *of, int scanline );
void outputfilter_composite_packed422_scanline( outputfilter_t *of,
                                                uint8_t *output,
                                                int width, int xpos,
                                                int scanline );

#ifdef __cplusplus
};
#endif
#endif /* OUTPUTFILTER_H_INCLUDED */
