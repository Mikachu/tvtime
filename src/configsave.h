/**
 * Copyright (c) 2003 Alexander Belov <asbel@mail.ru>
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

#ifndef CONFIGSAVE_H_INCLUDED
#define CONFIGSAVE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct configsave_s configsave_t;

configsave_t *configsave_open( const char *filename );
void configsave_close( configsave_t *cs );
int configsave( configsave_t *cs, const char *INIT_name, const char *INIT_val, const int INIT_num );

#ifdef __cplusplus
};
#endif
#endif /* CONFIGSAVE_H_INCLUDED */
