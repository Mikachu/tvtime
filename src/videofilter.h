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

#ifndef VIDEOFILTER_H_INCLUDED
#define VIDEOFILTER_H_INCLUDED

typedef struct videofilter_s videofilter_t;

videofilter_t *videofilter_new( void );
void videofilter_delete( videofilter_t *vf );

void videofilter_set_bt8x8_correction( videofilter_t *vf, int correct );
void videofilter_set_full_extent_correction( videofilter_t *vf, int correct );
void videofilter_set_luma_power( videofilter_t *vf, double power );

int videofilter_active_on_scanline( videofilter_t *vf, int scanline );
void videofilter_packed422_scanline( videofilter_t *vf, unsigned char *data,
                                     int width, int xpos, int scanline );

#endif /* VIDEOFILTER_H_INCLUDED */
