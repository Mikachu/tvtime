/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef HAVE_FIFO_H
#define HAVE_FIFO_H

#include "tvtimeconf.h"

typedef struct fifo_s fifo_t;

fifo_t *fifo_new( config_t *ct, char *givenname );
char *fifo_next_line( fifo_t *fifo );
int fifo_next_command( fifo_t *fifo );
void fifo_delete( fifo_t *fifo );
char *fifo_get_filename( fifo_t *fifo );

#endif