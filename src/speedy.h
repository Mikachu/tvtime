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
 */


/**
 * Interpolates a packed 4:2:2 scanline using linear interpolation.
 */
void interpolate_packed422_scanline_c( uint8_t *output, uint8_t *top,
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
void blit_colour_packed4444_scanline_mmxext( uint8_t *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr );
void blit_colour_packed4444_scanline_mmx( uint8_t *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr );
void blit_colour_packed4444_scanline_c( uint8_t *output, int width,
                                        int alpha, int luma, int cb, int cr );

/**
 * Scanline blitter for packed 4:2:2 scanlines.  This implementation uses
 * the fast memcpy code from xine which got it from mplayer.
 */
void blit_packed422_scanline_mmxext_billy( uint8_t *dest, const uint8_t *src, int width );
void blit_packed422_scanline_i386_linux( uint8_t *dest, const uint8_t *src, int width );
void blit_packed422_scanline_c( uint8_t *dest, const uint8_t *src, int width );

/* Alpha provided is from 0-256 not 0-255. */
void blend_packed422_scanline_c( uint8_t *output, uint8_t *src1,
                                 uint8_t *src2, int width, int pos );
void blend_packed422_scanline_mmxext( uint8_t *output, uint8_t *src1,
                                      uint8_t *src2, int width, int pos );
void composite_packed4444_alpha_to_packed422_scanline_c( uint8_t *output,
                                                         uint8_t *input,
                                                         uint8_t *foreground,
                                                         int width, int alpha );
void composite_packed4444_alpha_to_packed422_scanline_mmxext( uint8_t *output,
                                                              uint8_t *input,
                                                              uint8_t *foreground,
                                                              int width, int alpha );
void composite_packed4444_to_packed422_scanline_c( uint8_t *output, uint8_t *input,
                                                   uint8_t *foreground, int width );
void composite_packed4444_to_packed422_scanline_mmxext( uint8_t *output, uint8_t *input,
                                                        uint8_t *foreground, int width );
void composite_alphamask_to_packed4444_scanline_c( uint8_t *output, uint8_t *input,
                                                   uint8_t *mask, int width,
                                                   int textluma, int textcb,
                                                   int textcr );
void composite_alphamask_alpha_to_packed4444_scanline_c( uint8_t *output, uint8_t *input,
                                                         uint8_t *mask, int width,
                                                         int textluma, int textcb,
                                                         int textcr, int alpha );
void composite_alphamask_to_packed4444_scanline_mmxext( uint8_t *output, uint8_t *input,
                                                        uint8_t *mask, int width,
                                                        int textluma, int textcb,
                                                        int textcr );
void premultiply_packed4444_scanline_c( uint8_t *output, uint8_t *input, int width );
void premultiply_packed4444_scanline_mmxext( uint8_t *output, uint8_t *input, int width );

unsigned int comb_factor_packed422_scanline_mmx( uint8_t *top, uint8_t *mid,
                                                 uint8_t *bot, int width );
unsigned int diff_factor_packed422_scanline_c( uint8_t *cur, uint8_t *old, int width );
unsigned int diff_factor_packed422_scanline_mmx( uint8_t *cur, uint8_t *old, int width );

void filter_luma_121_packed422_inplace_scanline_c( uint8_t *data, int width );
void filter_luma_14641_packed422_inplace_scanline_c( uint8_t *data, int width );
void kill_chroma_packed422_inplace_scanline_mmx( uint8_t *data, int width );
void kill_chroma_packed422_inplace_scanline_c( uint8_t *data, int width );
void mirror_packed422_inplace_scanline_c( uint8_t *data, int width );
void halfmirror_packed422_inplace_scanline_c( uint8_t *data, int width );
void a8_subpix_blit_scanline_c( uint8_t *output, uint8_t *input,
                                int lasta, int startpos, int width );

void cheap_packed444_to_packed422_scanline( uint8_t *output, uint8_t *input, int width );
void cheap_packed422_to_packed444_scanline( uint8_t *output, uint8_t *input, int width );
void subpix_blit_vertical_packed422_scanline_c( uint8_t *output, uint8_t *top,
                                                uint8_t *bot, int subpixpos, int width );
void quarter_blit_vertical_packed422_scanline_c( uint8_t *output, uint8_t *one,
                                                 uint8_t *three, int width );
void quarter_blit_vertical_packed422_scanline_mmxext( uint8_t *output, uint8_t *one,
                                                      uint8_t *three, int width );

/**
 * This filter actually does not meet the spec so calling it rec601
 * is a bit of a lie.  I got the filter from Poynton's site.
 */
void packed422_to_packed444_rec601_scanline( uint8_t *dest, uint8_t *src, int width );

/* Struct for pulldown detection metrics. */
typedef struct pulldown_metrics_s {
    /* difference: total, even lines, odd lines */
    int d, e, o;
    /* noise: temporal, spacial (current), spacial (past) */
    int t, s, p;
} pulldown_metrics_t;

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
extern void (*speedy_memcpy)( void *output, void *input, size_t size );
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
