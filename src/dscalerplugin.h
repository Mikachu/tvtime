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

#ifndef DSCALERPLUGIN_H_INCLUDED
#define DSCALERPLUGIN_H_INCLUDED

#include "DS_Deinterlace.h"

void __cdecl dscaler_memcpy( void *output, void *input, size_t size );

FILTER_METHOD *load_dscaler_filter( char *filename );
void unload_dscaler_filter( FILTER_METHOD *method );

DEINTERLACE_METHOD *load_dscaler_deinterlacer( char *filename );
void unload_dscaler_deinterlacer( DEINTERLACE_METHOD *method );

#endif /* DSCALERPLUGIN_H_INCLUDED */
