

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
void video_correction_set_active( video_correction_t *vc, int active );
void video_correction_correct_luma_plane( video_correction_t *vc, unsigned char *luma, int width, int height, int stride );
void video_correction_correct_luma_scanline( video_correction_t *vc, unsigned char *output, unsigned char *luma, int width );
void video_correction_correct_cb_plane( video_correction_t *vc, unsigned char *cb, int width, int height, int stride );
void video_correction_correct_cr_plane( video_correction_t *vc, unsigned char *cr, int width, int height, int stride );


void build_plane( unsigned char *output, unsigned char *field,
                  int fieldstride, int width, int height,
                  int bottom_field, int chroma );
void build_packed_422_frame( unsigned char *output, unsigned char *fieldluma,
                             unsigned char *fieldcb, unsigned char *fieldcr,
                             int bottom_field, int lstride, int cstride,
                             int width, int height, video_correction_t *vc );

/**
 * This filter actually does not meet the spec so I'm not
 * sure what to call it.  I got it from Poynton's site.
 */
void scanline_chroma422_to_chroma444_rec601( unsigned char *dest, unsigned char *src, int srcwidth );

#endif /* VIDEOTOOLS_H_INCLUDED */
