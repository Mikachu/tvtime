/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

#ifndef HAVE_FIFO_H
#define HAVE_FIFO_H

typedef struct fifo_s fifo_t;

fifo_t *fifo_new( const char *filename );
void fifo_delete( fifo_t *fifo );

const char *fifo_get_filename( fifo_t *fifo );
int fifo_get_next_command( fifo_t *fifo );
const char *fifo_get_arguments( fifo_t *fifo );

#endif /* HAVE_FIFO_H */
