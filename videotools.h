/**
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef VIDEOTOOLS_H_INCLUDED
#define VIDEOTOOLS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extent/scale correction code for luma and chroma.  This can
 * be used to do some tricks for increasing the brightness, and
 * fixing the broken bt878 luma extents.
 */
typedef struct video_correction_s video_correction_t;
video_correction_t *video_correction_new( void );
void video_correction_delete( video_correction_t *vc );
void video_correction_set_luma_power( video_correction_t *vc, double power );
void video_correction_correct_luma_scanline( video_correction_t *vc,
                                             unsigned char *output,
                                             unsigned char *luma, int width );
void video_correction_correct_packed422_scanline( video_correction_t *vc,
                                                  unsigned char *output,
                                                  unsigned char *input, int width );

/**
 * Builds a packed 4:2:2 frame from a field interpolating to frame size by
 * linear interpolation.  This version applies the video correction transform
 * as it interpolates.
 */
void video_correction_planar422_field_to_packed422_frame( video_correction_t *vc,
                                                          unsigned char *output,
                                                          unsigned char *fieldluma,
                                                          unsigned char *fieldcb,
                                                          unsigned char *fieldcr,
                                                          int bottom_field,
                                                          int lstride, int cstride,
                                                          int width, int height );


void video_correction_packed422_field_to_frame_top( video_correction_t *vc,
                                                    unsigned char *output,
                                                    int outstride,
                                                    unsigned char *field,
                                                    int fieldwidth,
                                                    int fieldheight,
                                                    int fieldstride );
void video_correction_packed422_field_to_frame_bot( video_correction_t *vc,
                                                    unsigned char *output,
                                                    int outstride,
                                                    unsigned char *field,
                                                    int fieldwidth,
                                                    int fieldheight,
                                                    int fieldstride );

/**
 * Builds a packed 4:2:2 frame from a field interpolating to frame size by
 * linear interpolation.
 */
void planar422_field_to_packed422_frame( unsigned char *output,
                                         unsigned char *fieldluma,
                                         unsigned char *fieldcb,
                                         unsigned char *fieldcr,
                                         int bottom_field,
                                         int lstride, int cstride,
                                         int width, int height );
void packed422_field_to_frame_top( unsigned char *output, int outstride,
                                   unsigned char *field, int fieldwidth,
                                   int fieldheight, int fieldstride );
void packed422_field_to_frame_bot( unsigned char *output, int outstride,
                                   unsigned char *field, int fieldwidth,
                                   int fieldheight, int fieldstride );

void blit_colour_4444_scanline( unsigned char *output, int width, int alpha, int luma, int cb, int cr );
void blit_colour_packed422_scanline( unsigned char *output, int width, int luma, int cb, int cr );
void blit_colour_4444( unsigned char *output, int width, int height, int stride, int alpha, int luma, int cb, int cr );
void blit_colour_packed422( unsigned char *output, int width, int height, int stride, int luma, int cb, int cr );

void create_packed422_from_planar422_scanline( unsigned char *output, unsigned char *luma,
                                               unsigned char *cb, unsigned char *cr, int width );
void interpolate_packed422_from_planar422_scanline( unsigned char *output,
                                                    unsigned char *topluma,
                                                    unsigned char *topcb,
                                                    unsigned char *topcr,
                                                    unsigned char *botluma,
                                                    unsigned char *botcb,
                                                    unsigned char *botcr,
                                                    int width );

/**
 * Builds a plane from a field interpolating to frame size by linear
 * interpolation.
 */
void luma_plane_field_to_frame( unsigned char *output, unsigned char *input,
                                int width, int height, int stride, int bottom_field );
void chroma_plane_field_to_frame( unsigned char *output, unsigned char *input,
                                  int width, int height, int stride, int bottom_field );

/**
 * Text masks are arrays of unsigned chars with values 0-4 for
 * transparent to full alpha.  This will probably change when I can do
 * a better renderer from ttfs.
 */
void composite_textmask_packed422_scanline( unsigned char *output, unsigned char *input,
                                            unsigned char *textmask, int width,
                                            int textluma, int textcb, int textcr, double textalpha );

void composite_alphamask_packed422_scanline( unsigned char *output, unsigned char *input,
                                             unsigned char *textmask, int width,
                                             int textluma, int textcb, int textcr, double textalpha );
void composite_alphamask_packed4444_scanline( unsigned char *output, unsigned char *input,
                                              unsigned char *mask, int width,
                                              int textluma, int textcb, int textcr );
void composite_packed4444_to_packed422_scanline( unsigned char *output, unsigned char *input,
                                                 unsigned char *foreground, int width );

void composite_alphamask_packed4444( unsigned char *output, int owidth, int oheight, int ostride,
                                     unsigned char *mask, int mwidth, int mheight, int mstride,
                                     int luma, int cb, int cr, int xpos, int ypos );
void composite_packed4444_to_packed422( unsigned char *output, int owidth, int oheight, int ostride,
                                        unsigned char *foreground, int fwidth, int fheight, int fstride,
                                        int xpos, int ypos );

/**
 * Alpha provided is from 0-256 not 0-255.
 */
void composite_packed4444_alpha_to_packed422_scanline( unsigned char *output, unsigned char *input,
                                                       unsigned char *foreground, int width, int alpha );
void composite_packed4444_alpha_to_packed422( unsigned char *output, int owidth, int oheight, int ostride,
                                              unsigned char *foreground, int fwidth, int fheight, int fstride,
                                              int xpos, int ypos, int alpha );

/**
 * This filter actually does not meet the spec so I'm not
 * sure what to call it.  I got it from Poynton's site.
 */
void chroma422_to_chroma444_rec601_scanline( unsigned char *dest, unsigned char *src, int srcwidth );

#ifdef __cplusplus
};
#endif
#endif /* VIDEOTOOLS_H_INCLUDED */
