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

#ifndef VIDEOFILTER_H_INCLUDED
#define VIDEOFILTER_H_INCLUDED

typedef struct videofilter_s videofilter_t;

videofilter_t *videofilter_new( void );
void videofilter_delete( videofilter_t *vf );

void videofilter_set_bt8x8_correction( videofilter_t *vf, int correct );
void videofilter_set_full_extent_correction( videofilter_t *vf, int correct );
void videofilter_set_luma_power( videofilter_t *vf, double power );

int videofilter_active_on_scanline( videofilter_t *vf, int scanline );
void videofilter_packed422_scanline( videofilter_t *vf, uint8_t *data,
                                     int width, int xpos, int scanline );

#endif /* VIDEOFILTER_H_INCLUDED */
