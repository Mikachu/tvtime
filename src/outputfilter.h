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

#include <stdint.h>

#include "vbiscreen.h"

typedef struct outputfilter_s outputfilter_t;

outputfilter_t *outputfilter_new( int width, int height, double frameaspect, int verbose );
void outputfilter_delete( outputfilter_t *of );

vbiscreen_t *outputfilter_get_vbiscreen( outputfilter_t *of );

int outputfilter_active_on_scanline( outputfilter_t *of, int scanline );
void outputfilter_composite_packed422_scanline( outputfilter_t *of,
                                                uint8_t *output,
                                                int width, int xpos,
                                                int scanline );

#endif /* OUTPUTFILTER_H_INCLUDED */
