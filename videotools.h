

#ifndef VIDEOTOOLS_H_INCLUDED
#define VIDEOTOOLS_H_INCLUDED

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

/**
 * Builds a packed 4:2:2 frame from a field interpolating to frame size by
 * linear interpolation.
 */
void video_correction_planar422_field_to_packed422_frame( video_correction_t *vc,
                                                          unsigned char *output,
                                                          unsigned char *fieldluma,
                                                          unsigned char *fieldcb,
                                                          unsigned char *fieldcr,
                                                          int bottom_field,
                                                          int lstride, int cstride,
                                                          int width, int height );


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
void composite_textmask_packed422_scanline( unsigned char *output, unsigned char *bottom422,
                                            unsigned char *textmask, int width,
                                            int textluma, int textcb, int textcr, double textalpha );

/**
 * This filter actually does not meet the spec so I'm not
 * sure what to call it.  I got it from Poynton's site.
 */
void scanline_chroma422_to_chroma444_rec601( unsigned char *dest, unsigned char *src, int srcwidth );

#endif /* VIDEOTOOLS_H_INCLUDED */
