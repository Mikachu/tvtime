/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>,
 *                    Doug Bell <drbell@users.sourceforge.net>.
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

#ifndef MPEG2INPUT_H_INCLUDED
#define MPEG2INPUT_H_INCLUDED

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mpeg2input_s mpeg2input_t;

mpeg2input_t *mpeg2input_new( const char *filename, int track, int accel );
void mpeg2input_delete( mpeg2input_t *instance );
uint8_t *mpeg2input_get_curframe( mpeg2input_t *mpegin );
uint8_t *mpeg2input_get_lastframe( mpeg2input_t *mpegin );
uint8_t *mpeg2input_get_secondlastframe( mpeg2input_t *mpegin );
int mpeg2input_next_frame( mpeg2input_t *mpegin );
int mpeg2input_get_width( mpeg2input_t *mpegin );
int mpeg2input_get_height( mpeg2input_t *mpegin );

#ifdef __cplusplus
};
#endif
#endif /* MPEG2INPUT_H_INCLUDED */
