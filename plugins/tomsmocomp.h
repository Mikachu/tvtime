/**
 * Copyright (C) 2004 Billy Biggs <vektor@dumbterm.net>
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

#ifndef TOMSMOCOMP_H_INCLUDED
#define TOMSMOCOMP_H_INCLUDED

#include "dscalerapi.h"

#ifdef __cplusplus
extern "C" {
#endif

void tomsmocomp_init( void );
void tomsmocomp_filter_mmx( TDeinterlaceInfo* pInfo );
void tomsmocomp_filter_3dnow( TDeinterlaceInfo* pInfo );
void tomsmocomp_filter_sse( TDeinterlaceInfo* pInfo );

#ifdef __cplusplus
};
#endif

#endif /* TOMSMOCOMP_H_INCLUDED */
