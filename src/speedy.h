/**
 * Copyright (c) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef SPEEDY_H_INCLUDED
#define SPEEDY_H_INCLUDED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Speedy is a collection of optimized functions plus their C fallbacks.
 * This includes a simple system to select which functions to use
 * at runtime.
 *
 * The optimizations are done with the help of the mmx.h system, from
 * libmpeg2 by Michel Lespinasse and Aaron Holtzman.
 *
 * The library is organized in two parts.  First, we define specific
 * implementations directly, organized by what optimizations they
 * require, and then we define function pointers which are initialized
 * to point at the 'fastest' version of each function.  Functions may
 * depend on this best implementation.
 */


/**
 * Interpolates a packed 4:2:2 scanline using linear interpolation.
 */
void interpolate_packed422_scanline_c( uint8_t *output, uint8_t *top,
                                       uint8_t *bot, int width );
void interpolate_packed422_scanline_mmx( uint8_t *output, uint8_t *top,
                                         uint8_t *bot, int width );
void interpolate_packed422_scanline_mmxext( uint8_t *output, uint8_t *top,
                                            uint8_t *bot, int width );

/**
 * Blits a colour to a packed 4:2:2 scanline.
 */
void blit_colour_packed422_scanline_c( uint8_t *output, int width,
                                       int y, int cb, int cr );
void blit_colour_packed422_scanline_mmx( uint8_t *output, int width,
                                         int y, int cb, int cr );
void blit_colour_packed422_scanline_mmxext( uint8_t *output, int width,
                                            int y, int cb, int cr );

/**
 * Blits a colour to a packed 4:4:4:4 scanline.  I use luma/cb/cr instead of
 * RGB but this will of course work for either.
 */
void blit_colour_packed4444_scanline_c( uint8_t *output, int width,
                                        int alpha, int luma, int cb, int cr );
void blit_colour_packed4444_scanline_mmx( uint8_t *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr );
void blit_colour_packed4444_scanline_mmxext( uint8_t *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr );

/**
 * Fast memcpy function, used by all of the blit functions.  Won't blit
 * anything if dest == src.
 */
void speedy_memcpy_c( void *dest, const void *src, size_t n );
void speedy_memcpy_mmx( void *d, const void *s, size_t n );
void speedy_memcpy_mmxext( void *d, const void *s, size_t n );

/**
 * Blit from and to packed 4:2:2 scanline.
 */
void blit_packed422_scanline_c( uint8_t *dest, const uint8_t *src, int width );
void blit_packed422_scanline_mmx( uint8_t *dest, const uint8_t *src, int width );
void blit_packed422_scanline_mmxext( uint8_t *dest, const uint8_t *src, int width );

/**
 * Blend between two packed 4:2:2 scanline.  Pos is the fade value in
 * the range 0-256.  A value of 0 gives 100% src1, and a value of 256
 * gives 100% src2.  Anything in between gives the appropriate faded
 * version.
 */
void blend_packed422_scanline_c( uint8_t *output, uint8_t *src1,
                                 uint8_t *src2, int width, int pos );
void blend_packed422_scanline_mmxext( uint8_t *output, uint8_t *src1,
                                      uint8_t *src2, int width, int pos );

/**
 * Composites a packed 4:4:4:4 scanline onto a packed 4:2:2 scanline.
 * Chroma is downsampled by dropping samples (nearest neighbour).  The
 * alpha value provided is in the range 0-256 and is first applied to
 * the input (for fadeouts).
 */
void composite_packed4444_alpha_to_packed422_scanline_c( uint8_t *output,
                                                         uint8_t *input,
                                                         uint8_t *foreground,
                                                         int width, int alpha );
void composite_packed4444_alpha_to_packed422_scanline_mmxext( uint8_t *output,
                                                              uint8_t *input,
                                                              uint8_t *foreground,
                                                              int width, int alpha );

/**
 * Composites a packed 4:4:4:4 scanline onto a packed 4:2:2 scanline.
 * Chroma is downsampled by dropping samples (nearest neighbour).
 */
void composite_packed4444_to_packed422_scanline_c( uint8_t *output, uint8_t *input,
                                                   uint8_t *foreground, int width );
void composite_packed4444_to_packed422_scanline_mmxext( uint8_t *output, uint8_t *input,
                                                        uint8_t *foreground, int width );

/**
 * Takes an alphamask and the given colour (in Y'CbCr) and composites it
 * onto a packed 4:4:4:4 scanline.
 */
void composite_alphamask_to_packed4444_scanline_c( uint8_t *output, uint8_t *input,
                                                   uint8_t *mask, int width,
                                                   int textluma, int textcb,
                                                   int textcr );
void composite_alphamask_to_packed4444_scanline_mmxext( uint8_t *output, uint8_t *input,
                                                        uint8_t *mask, int width,
                                                        int textluma, int textcb,
                                                        int textcr );

/**
 * Takes an alphamask and the given colour (in Y'CbCr) and composites it
 * onto a packed 4:4:4:4 scanline.  The alpha value provided is in the
 * range 0-256 and is first applied to the input (for fadeouts).
 */
void composite_alphamask_alpha_to_packed4444_scanline_c( uint8_t *output, uint8_t *input,
                                                         uint8_t *mask, int width,
                                                         int textluma, int textcb,
                                                         int textcr, int alpha );

/**
 * Premultiplies the colour by the alpha channel in a packed 4:4:4:4
 * scanline.
 */
void premultiply_packed4444_scanline_c( uint8_t *output, uint8_t *input, int width );
void premultiply_packed4444_scanline_mmxext( uint8_t *output, uint8_t *input, int width );

/**
 * Calculates the 'comb factor' for a set of three scanlines.  This is a
 * metric where higher values indicate a more likely chance that the two
 * fields are at separate points in time.
 */
unsigned int comb_factor_packed422_scanline_mmx( uint8_t *top, uint8_t *mid,
                                                 uint8_t *bot, int width );

/**
 * Calculates the 'difference factor' for two scanlines.  This is a
 * metric where higher values indicate that the two scanlines are more
 * different.
 */
unsigned int diff_factor_packed422_scanline_c( uint8_t *cur, uint8_t *old, int width );
unsigned int diff_factor_packed422_scanline_mmx( uint8_t *cur, uint8_t *old, int width );

/**
 * In-place [1 2 1] filter.
 */
void filter_luma_121_packed422_inplace_scanline_c( uint8_t *data, int width );

/**
 * In-place [1 4 6 4 1] filter.
 */
void filter_luma_14641_packed422_inplace_scanline_c( uint8_t *data, int width );

/**
 * Sets the chroma of the scanline to neutral (128) in-place.
 */
void kill_chroma_packed422_inplace_scanline_c( uint8_t *data, int width );
void kill_chroma_packed422_inplace_scanline_mmx( uint8_t *data, int width );

/**
 * Mirrors the scanline in-place.
 */
void mirror_packed422_inplace_scanline_c( uint8_t *data, int width );

/**
 * Mirrors the first half of the scanline onto the second half in-place.
 */
void halfmirror_packed422_inplace_scanline_c( uint8_t *data, int width );

/**
 * Takes an alpha mask and subpixelly blits it using linear
 * interpolation.
 */
void a8_subpix_blit_scanline_c( uint8_t *output, uint8_t *input,
                                int lasta, int startpos, int width );

/**
 * Vertical subpixel blit for packed 4:2:2 scanlines using linear
 * interpolation.
 */
void subpix_blit_vertical_packed422_scanline_c( uint8_t *output, uint8_t *top,
                                                uint8_t *bot, int subpixpos, int width );

/**
 * 1/4 vertical subpixel blit for packed 4:2:2 scanlines using linear
 * interpolation.
 */
void quarter_blit_vertical_packed422_scanline_c( uint8_t *output, uint8_t *one,
                                                 uint8_t *three, int width );
void quarter_blit_vertical_packed422_scanline_mmxext( uint8_t *output, uint8_t *one,
                                                      uint8_t *three, int width );

/**
 * Convert a packed 4:4:4 surface to a packed 4:2:2 surface using
 * nearest neighbour chroma downsampling.
 */
void packed444_to_packed422_scanline_c( uint8_t *output, uint8_t *input, int width );

/**
 * Converts packed 4:2:2 to packed 4:4:4 scanlines using nearest
 * neighbour chroma upsampling.
 */
void packed422_to_packed444_scanline_c( uint8_t *output, uint8_t *input, int width );

/**
 * This filter actually does not meet the spec so calling it rec601
 * is a bit of a lie.  I got the filter from Poynton's site.  This
 * converts a scanline from packed 4:2:2 to packed 4:4:4.
 */
void packed422_to_packed444_rec601_scanline( uint8_t *dest, uint8_t *src, int width );

/**
 * Conversions between Y'CbCr and R'G'B'.
 */
void packed444_to_rgb24_rec601_reference_scanline( uint8_t *output, uint8_t *input, int width );
void packed444_to_rgb24_rec601_scanline( uint8_t *output, uint8_t *input, int width );
void rgb24_to_packed444_rec601_scanline( uint8_t *output, uint8_t *input, int width );
void rgba32_to_packed4444_rec601_scanline( uint8_t *output, uint8_t *input, int width );

/**
 * I don't think these functions are correctly named or implemented, but
 * they do belong here in speedy.
 */
void packed444_to_nonpremultiplied_packed4444_scanline( uint8_t *output, 
                                                        uint8_t *input,
                                                        int width, int alpha );
int aspect_adjust_packed4444_scanline( uint8_t *output,
                                       uint8_t *input, 
                                       int width,
                                       double aspectratio );

/**
 * Sub-pixel data bar renderer.  There are 128 bars.  Is this even useful?
 */
void composite_bars_packed4444_scanline( uint8_t *output,
                                         uint8_t *background, int width,
                                         int a, int luma, int cb, int cr,
                                         int percentage );

/**
 * Struct for pulldown detection metrics.
 */
typedef struct pulldown_metrics_s {
    /* difference: total, even lines, odd lines */
    int d, e, o;
    /* noise: temporal, spacial (current), spacial (past) */
    int t, s, p;
} pulldown_metrics_t;

/**
 * Calculates the block difference metrics for dalias' pulldown
 * detection algorithm.
 */
void diff_packed422_block8x8_c( pulldown_metrics_t *m, uint8_t *old,
                                uint8_t *new, int os, int ns );
void diff_packed422_block8x8_mmx( pulldown_metrics_t *m, uint8_t *old,
                                  uint8_t *new, int os, int ns );

/**
 * Here are the function pointers which will be initialized to point at the
 * fastest available version of the above after a call to setup_speedy_calls().
 */
extern void (*interpolate_packed422_scanline)( uint8_t *output, uint8_t *top,
                                               uint8_t *bot, int width );
extern void (*blit_colour_packed422_scanline)( uint8_t *output,
                                               int width, int y, int cb, int cr );
extern void (*blit_colour_packed4444_scanline)( uint8_t *output,
                                                int width, int alpha, int luma,
                                                int cb, int cr );
extern void (*blit_packed422_scanline)( uint8_t *dest, const uint8_t *src, int width );
extern void (*composite_packed4444_to_packed422_scanline)( uint8_t *output,
                                                           uint8_t *input,
                                                           uint8_t *foreground,
                                                           int width );
extern void (*composite_packed4444_alpha_to_packed422_scanline)( uint8_t *output,
                                                                 uint8_t *input,
                                                                 uint8_t *foreground,
                                                                 int width, int alpha );
extern void (*composite_alphamask_to_packed4444_scanline)( uint8_t *output,
                                                           uint8_t *input,
                                                           uint8_t *mask, int width,
                                                           int textluma, int textcb,
                                                           int textcr );
extern void (*composite_alphamask_alpha_to_packed4444_scanline)( uint8_t *output,
                                                                 uint8_t *input,
                                                                 uint8_t *mask, int width,
                                                                 int textluma, int textcb,
                                                                 int textcr, int alpha );
extern void (*premultiply_packed4444_scanline)( uint8_t *output, uint8_t *input, int width );
extern void (*blend_packed422_scanline)( uint8_t *output, uint8_t *src1,
                                         uint8_t *src2, int width, int pos );
extern void (*filter_luma_121_packed422_inplace_scanline)( uint8_t *data, int width );
extern void (*filter_luma_14641_packed422_inplace_scanline)( uint8_t *data, int width );
extern unsigned int (*diff_factor_packed422_scanline)( uint8_t *cur, uint8_t *old, int width );
extern unsigned int (*comb_factor_packed422_scanline)( uint8_t *top, uint8_t *mid,
                                                       uint8_t *bot, int width );
extern void (*kill_chroma_packed422_inplace_scanline)( uint8_t *data, int width );
extern void (*mirror_packed422_inplace_scanline)( uint8_t *data, int width );
extern void (*halfmirror_packed422_inplace_scanline)( uint8_t *data, int width );
extern void (*speedy_memcpy)( void *output, const void *input, size_t size );
extern void (*diff_packed422_block8x8)( pulldown_metrics_t *m, uint8_t *old,
                                        uint8_t *new, int os, int ns );
extern void (*a8_subpix_blit_scanline)( uint8_t *output, uint8_t *input,
                                        int lasta, int startpos, int width );
extern void (*quarter_blit_vertical_packed422_scanline)( uint8_t *output, uint8_t *one,
                                                         uint8_t *three, int width );
extern void (*subpix_blit_vertical_packed422_scanline)( uint8_t *output, uint8_t *top,
                                                        uint8_t *bot, int subpixpos, int width );

/**
 * Sets up the function pointers to point at the fastest function available.
 */
void setup_speedy_calls( int verbose );

/**
 * Returns a bitfield of what accellerations are available.  See mm_accel.h.
 */
int speedy_get_accel( void );

/**
 * Returns the time in clock ticks.
 */
unsigned int speedy_get_cycles( void );

/**
 * Resets the speedy timer.
 */
void speedy_reset_timer( void );

/**
 * Returns a measurement of the CPU speed.
 */
double speedy_measure_cpu_mhz( void );

/**
 * Prints info about the CPU.
 */
void speedy_print_cpu_info( void );

#ifdef __cplusplus
};
#endif
#endif /* SPEEDY_H_INCLUDED */
