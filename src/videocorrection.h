/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef VIDEOCORRECTION_H_INCLUDED
#define VIDEOCORRECTION_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extent/scale correction code for luma and chroma.  This can
 * be used to do some tricks for increasing the brightness, and
 * fixing the broken bt878 luma extents.
 */

typedef struct video_correction_s video_correction_t;

video_correction_t *video_correction_new( int bt8x8_correction, int full_extent_correction );
void video_correction_delete( video_correction_t *vc );
void video_correction_set_luma_power( video_correction_t *vc, double power );
void video_correction_correct_luma_scanline( video_correction_t *vc,
                                             unsigned char *output,
                                             unsigned char *luma, int width );
void video_correction_correct_packed422_scanline( video_correction_t *vc,
                                                  unsigned char *output,
                                                  unsigned char *input,
                                                  int width );

#ifdef __cplusplus
};
#endif
#endif /* VIDEOCORRECTION_H_INCLUDED */
