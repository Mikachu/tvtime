/**
 * Weave frames, top-field-first.
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
#include "copyfunctions.h"
#include "deinterlace.h"

static void deinterlace_scanline_weave( uint8_t *output,
                                        deinterlace_scanline_data_t *data,
                                        int width )
{
    blit_packed422_scanline( output, data->m1, width );
}

static void copy_scanline( uint8_t *output,
                           deinterlace_scanline_data_t *data,
                           int width )
{
    if( data->bottom_field ) {
        blit_packed422_scanline( output, data->m0, width );
    } else {
        blit_packed422_scanline( output, data->m2, width );
    }
}


static deinterlace_method_t weavemethod =
{
    "Progressive: Top Field First",
    "ProgressiveTFF",
    3,
    0,
    0,
    1,
    deinterlace_scanline_weave,
    copy_scanline,
    0,
    0,
    { "Constructs frames from pairs of fields.  Use",
      "this if you are watching a film broadcast or",
      "DVD in a PAL area, or if you are using a",
      "video game console system which sends a",
      "progressive signal.",
      "",
      "Depending on the content, it may be top- or",
      "bottom-field first.  Until we have full",
      "detection in tvtime, this must be determined",
      "experimentally." }
};

deinterlace_method_t *weavetff_get_method( void )
{
    return &weavemethod;
}


