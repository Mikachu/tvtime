/**
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef SDLOUTPUT_H_INCLUDED
#define SDLOUTPUT_H_INCLUDED

#include "input.h"
#include "outputapi.h"

#ifdef __cplusplus
extern "C" {
#endif

output_api_t *get_sdl_output( void );

#ifdef __cplusplus
};
#endif
#endif /* SDLOUTPUT_H_INCLUDED */
