/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This file is subject to the terms of the GNU General Public License as
 * published by the Free Software Foundation.  A copy of this license is
 * included with this software distribution in the file COPYING.  If you
 * do not have a copy, you may obtain a copy by writing to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#ifndef OUTPUTFILTER_H_INCLUDED
#define OUTPUTFILTER_H_INCLUDED

#include "vbiscreen.h"

typedef struct outputfilter_s outputfilter_t;

outputfilter_t *outputfilter_new( int width, int height, double frameaspect, int verbose );
void outputfilter_delete( outputfilter_t *of );

vbiscreen_t *outputfilter_get_vbiscreen( outputfilter_t *of );

int outputfilter_active_on_scanline( outputfilter_t *of, int scanline );
void outputfilter_composite_packed422_scanline( outputfilter_t *of,
                                                unsigned char *output,
                                                int width, int xpos,
                                                int scanline );

#endif /* OUTPUTFILTER_H_INCLUDED */
