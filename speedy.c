
#include <stdio.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedy.h"

void interpolate_packed422_scanline_c( unsigned char *output,
                                       unsigned char *top,
                                       unsigned char *bot, int width )
{
    int i;

    for( i = width*2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }
}


void interpolate_packed422_scanline_mmxext( unsigned char *output,
                                            unsigned char *top,
                                            unsigned char *bot, int width )
{
    int i;

    for( i = width/4; i; --i ) {
        movq_m2r( *bot, mm2 );
        movq_m2r( *top, mm3 );
        pavgb_r2r( mm3, mm2 );
        movntq_r2m( mm2, *output );
        output += 8;
        top += 8;
        bot += 8;
    }

    /* Handle last few pixels. */
    if( width & 0x0f ) {
        for( i = (width & 0x0f)*2; i; --i ) {
            *output++ = ((*top++) + (*bot++)) >> 1;
        }
    }

    emms();
}

void blit_colour_packed422_scanline_c( unsigned char *output,
                                       int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;
    unsigned int *o = (unsigned int *) output;

    for( width /= 2; width; --width ) {
        *o++ = colour;
    }
}

void blit_colour_packed422_scanline_mmx( unsigned char *output,
                                         int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 4; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

void blit_colour_packed422_scanline_mmxext( unsigned char *output,
                                            int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 4; width; --width ) {
        movntq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

void blit_colour_packed4444_scanline_c( unsigned char *output, int width,
                                        int alpha, int luma, int cb, int cr )
{
    int j;

    for( j = 0; j < width; j++ ) {
        *output++ = alpha;
        *output++ = luma;
        *output++ = cb;
        *output++ = cr;
    }
}

void blit_colour_packed4444_scanline_mmx( unsigned char *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr )
{
    int colour = cr << 24 | cb << 16 | luma << 8 | alpha;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 2; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

void blit_colour_packed4444_scanline_mmxext( unsigned char *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr )
{
    int colour = cr << 24 | cb << 16 | luma << 8 | alpha;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 2; width; --width ) {
        movntq_r2m( mm2, *output );
        output += 8;
    }

    emms();
}

/* Original comments from mplayer (file: aclib.c)
 This part of code was taken by me from Linux-2.4.3 and slightly modified
for MMX, MMX2, SSE instruction set. I have done it since linux uses page aligned
blocks but mplayer uses weakly ordered data and original sources can not
speedup them. Only using PREFETCHNTA and MOVNTQ together have effect!

From IA-32 Intel Architecture Software Developer's Manual Volume 1,

Order Number 245470:
"10.4.6. Cacheability Control, Prefetch, and Memory Ordering Instructions"

Data referenced by a program can be temporal (data will be used again) or
non-temporal (data will be referenced once and not reused in the immediate
future). To make efficient use of the processor's caches, it is generally
desirable to cache temporal data and not cache non-temporal data. Overloading
the processor's caches with non-temporal data is sometimes referred to as
"polluting the caches".
The non-temporal data is written to memory with Write-Combining semantics.

The PREFETCHh instructions permits a program to load data into the processor
at a suggested cache level, so that it is closer to the processors load and
store unit when it is needed. If the data is already present in a level of
the cache hierarchy that is closer to the processor, the PREFETCHh instruction
will not result in any data movement.
But we should you PREFETCHNTA: Non-temporal data fetch data into location
close to the processor, minimizing cache pollution.

The MOVNTQ (store quadword using non-temporal hint) instruction stores
packed integer data from an MMX register to memory, using a non-temporal hint.
The MOVNTPS (store packed single-precision floating-point values using
non-temporal hint) instruction stores packed floating-point data from an
XMM register to memory, using a non-temporal hint.

The SFENCE (Store Fence) instruction controls write ordering by creating a
fence for memory store operations. This instruction guarantees that the results
of every store instruction that precedes the store fence in program order is
globally visible before any store instruction that follows the fence. The
SFENCE instruction provides an efficient way of ensuring ordering between
procedures that produce weakly-ordered data and procedures that consume that
data.

If you have questions please contact with me: Nick Kurshev: nickols_k@mail.ru.
*/

/*  mmx v.1 Note: Since we added alignment of destinition it speedups
    of memory copying on PentMMX, Celeron-1 and P2 upto 12% versus
    standard (non MMX-optimized) version.
    Note: on K6-2+ it speedups memory copying upto 25% and
          on K7 and P3 about 500% (5 times). 
*/

/* Additional notes on gcc assembly and processors: [MF]
prefetch is specific for AMD processors, the intel ones should be
prefetch0, prefetch1, prefetch2 which are not recognized by my gcc.
prefetchnta is supported both on athlon and pentium 3.

therefore i will take off prefetchnta instructions from the mmx1 version
to avoid problems on pentium mmx and k6-2.  

quote of the day:
"Using prefetches efficiently is more of an art than a science"
*/


/* for small memory blocks (<256 bytes) this version is faster */
#define small_memcpy(to,from,n)\
{\
register unsigned long int dummy;\
__asm__ __volatile__(\
  "rep; movsb"\
  :"=&D"(to), "=&S"(from), "=&c"(dummy)\
  :"0" (to), "1" (from),"2" (n)\
  : "memory");\
}

/* linux kernel __memcpy (from: /include/asm/string.h) */
static inline void * __memcpy(void * to, const void * from, size_t n)
{
int d0, d1, d2;

  if( n < 4 ) {
    small_memcpy(to,from,n);
  }
  else
    __asm__ __volatile__(
    "rep ; movsl\n\t"
    "testb $2,%b4\n\t"
    "je 1f\n\t"
    "movsw\n"
    "1:\ttestb $1,%b4\n\t"
    "je 2f\n\t"
    "movsb\n"
    "2:"
    : "=&c" (d0), "=&D" (d1), "=&S" (d2)
    :"0" (n/4), "q" (n),"1" ((long) to),"2" ((long) from)
    : "memory");
  
  return (to);
}

#define SSE_MMREG_SIZE 16
#define MMX_MMREG_SIZE 8

#define MMX1_MIN_LEN 0x800  /* 2K blocks */
#define MIN_LEN 0x40  /* 64-byte blocks */

/* SSE note: i tried to move 128 bytes a time instead of 64 but it
didn't make any measureable difference. i'm using 64 for the sake of
simplicity. [MF] */
static void * sse_memcpy(void * to, const void * from, size_t len)
{
  void *retval;
  size_t i;
  retval = to;
    
  /* PREFETCH has effect even for MOVSB instruction ;) */
  __asm__ __volatile__ (
    "   prefetchnta (%0)\n"
    "   prefetchnta 64(%0)\n"
    "   prefetchnta 128(%0)\n"
    "   prefetchnta 192(%0)\n"
    "   prefetchnta 256(%0)\n"
    : : "r" (from) );
    
  if(len >= MIN_LEN)
  {
    register unsigned long int delta;
    /* Align destinition to MMREG_SIZE -boundary */
    delta = ((unsigned long int)to)&(SSE_MMREG_SIZE-1);
    if(delta)
    {
      delta=SSE_MMREG_SIZE-delta;
      len -= delta;
      small_memcpy(to, from, delta);
    }
    i = len >> 6; /* len/64 */
    len&=63;
    if(((unsigned long)from) & 15)
      /* if SRC is misaligned */
      for(; i>0; i--)
      {
        __asm__ __volatile__ (
        "prefetchnta 320(%0)\n"
        "movups (%0), %%xmm0\n"
        "movups 16(%0), %%xmm1\n"
        "movups 32(%0), %%xmm2\n"
        "movups 48(%0), %%xmm3\n"
        "movntps %%xmm0, (%1)\n"
        "movntps %%xmm1, 16(%1)\n"
        "movntps %%xmm2, 32(%1)\n"
        "movntps %%xmm3, 48(%1)\n"
        :: "r" (from), "r" (to) : "memory");
        ((const unsigned char *)from)+=64;
        ((unsigned char *)to)+=64;
      }
    else 
      /*
         Only if SRC is aligned on 16-byte boundary.
         It allows to use movaps instead of movups, which required data
         to be aligned or a general-protection exception (#GP) is generated.
      */
      for(; i>0; i--)
      {
        __asm__ __volatile__ (
        "prefetchnta 320(%0)\n"
        "movaps (%0), %%xmm0\n"
        "movaps 16(%0), %%xmm1\n"
        "movaps 32(%0), %%xmm2\n"
        "movaps 48(%0), %%xmm3\n"
        "movntps %%xmm0, (%1)\n"
        "movntps %%xmm1, 16(%1)\n"
        "movntps %%xmm2, 32(%1)\n"
        "movntps %%xmm3, 48(%1)\n"
        :: "r" (from), "r" (to) : "memory");
        ((const unsigned char *)from)+=64;
        ((unsigned char *)to)+=64;
      }
    /* since movntq is weakly-ordered, a "sfence"
     * is needed to become ordered again. */
    __asm__ __volatile__ ("sfence":::"memory");
    /* enables to use FPU */
    __asm__ __volatile__ ("emms":::"memory");
  }
  /*
   *	Now do the tail of the block
   */
  if(len) __memcpy(to, from, len);
  return retval;
}

static void * mmx_memcpy(void * to, const void * from, size_t len)
{
  void *retval;
  size_t i;
  retval = to;

  if(len >= MMX1_MIN_LEN)
  {
    register unsigned long int delta;
    /* Align destinition to MMREG_SIZE -boundary */
    delta = ((unsigned long int)to)&(MMX_MMREG_SIZE-1);
    if(delta)
    {
      delta=MMX_MMREG_SIZE-delta;
      len -= delta;
      small_memcpy(to, from, delta);
    }
    i = len >> 6; /* len/64 */
    len&=63;
    for(; i>0; i--)
    {
      __asm__ __volatile__ (
      "movq (%0), %%mm0\n"
      "movq 8(%0), %%mm1\n"
      "movq 16(%0), %%mm2\n"
      "movq 24(%0), %%mm3\n"
      "movq 32(%0), %%mm4\n"
      "movq 40(%0), %%mm5\n"
      "movq 48(%0), %%mm6\n"
      "movq 56(%0), %%mm7\n"
      "movq %%mm0, (%1)\n"
      "movq %%mm1, 8(%1)\n"
      "movq %%mm2, 16(%1)\n"
      "movq %%mm3, 24(%1)\n"
      "movq %%mm4, 32(%1)\n"
      "movq %%mm5, 40(%1)\n"
      "movq %%mm6, 48(%1)\n"
      "movq %%mm7, 56(%1)\n"
      :: "r" (from), "r" (to) : "memory");
      ((const unsigned char *)from)+=64;
      ((unsigned char *)to)+=64;
    }
    __asm__ __volatile__ ("emms":::"memory");
  }
  /*
   *	Now do the tail of the block
   */
  if(len) __memcpy(to, from, len);
  return retval;
}

static void * mmx2_memcpy(void * to, const void * from, size_t len)
{
  void *retval;
  size_t i;
  retval = to;

  /* PREFETCH has effect even for MOVSB instruction ;) */
  __asm__ __volatile__ (
    "   prefetchnta (%0)\n"
    "   prefetchnta 64(%0)\n"
    "   prefetchnta 128(%0)\n"
    "   prefetchnta 192(%0)\n"
    "   prefetchnta 256(%0)\n"
    : : "r" (from) );

  if(len >= MIN_LEN)
  {
    register unsigned long int delta;
    /* Align destinition to MMREG_SIZE -boundary */
    delta = ((unsigned long int)to)&(MMX_MMREG_SIZE-1);
    if(delta)
    {
      delta=MMX_MMREG_SIZE-delta;
      len -= delta;
      small_memcpy(to, from, delta);
    }
    i = len >> 6; /* len/64 */
    len&=63;
    for(; i>0; i--)
    {
      __asm__ __volatile__ (
      "prefetchnta 320(%0)\n"
      "movq (%0), %%mm0\n"
      "movq 8(%0), %%mm1\n"
      "movq 16(%0), %%mm2\n"
      "movq 24(%0), %%mm3\n"
      "movq 32(%0), %%mm4\n"
      "movq 40(%0), %%mm5\n"
      "movq 48(%0), %%mm6\n"
      "movq 56(%0), %%mm7\n"
      "movntq %%mm0, (%1)\n"
      "movntq %%mm1, 8(%1)\n"
      "movntq %%mm2, 16(%1)\n"
      "movntq %%mm3, 24(%1)\n"
      "movntq %%mm4, 32(%1)\n"
      "movntq %%mm5, 40(%1)\n"
      "movntq %%mm6, 48(%1)\n"
      "movntq %%mm7, 56(%1)\n"
      :: "r" (from), "r" (to) : "memory");
      ((const unsigned char *)from)+=64;
      ((unsigned char *)to)+=64;
    }
     /* since movntq is weakly-ordered, a "sfence"
     * is needed to become ordered again. */
    __asm__ __volatile__ ("sfence":::"memory");
    __asm__ __volatile__ ("emms":::"memory");
  }
  /*
   *	Now do the tail of the block
   */
  if(len) __memcpy(to, from, len);
  return retval;
}

static void *linux_kernel_memcpy(void *to, const void *from, size_t len) {
  return __memcpy(to,from,len);
}

void blit_packed422_scanline_mmxext( unsigned char *dest, const unsigned char *src, int width )
{
    mmx2_memcpy( dest, src, width*2 );
}

void blit_packed422_scanline_c( unsigned char *dest, const unsigned char *src, int width )
{
    memcpy( dest, src, width*2 );
}


/**
 * Copyright (c) 2000 Steven Grimm.  All rights reserved.
 * mmx.h port copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This code is ported from DScaler: http://deinterlace.sf.net/
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

/**
 * Deinterlace the latest field, attempting to weave wherever it won't cause
 * visible artifacts.
 *
 * The data from the most recently captured field is always copied to the overlay
 * verbatim.  For the data from the previous field, the following algorithm is
 * applied to each pixel.
 *
 * We use the following notation for the top, middle, and bottom pixels
 * of concern:
 *
 * Field 1 | Field 2 | Field 3 | Field 4 |
 *         |   T0    |         |   T1    | scanline we copied in last iteration
 *   M0    |         |    M1   |         | intermediate scanline from alternate field
 *         |   B0    |         |   B1    | scanline we just copied
 *
 * We will weave M1 into the image if any of the following is true:
 *   - M1 is similar to either B1 or T1.  This indicates that no weave
 *     artifacts would be visible.  The SpatialTolerance setting controls
 *     how far apart the luminances can be before pixels are considered
 *     non-similar.
 *   - T1 and B1 and M1 are old.  In that case any weave artifact that
 *     appears isn't due to fast motion, since it was there in the previous
 *     frame too.  By "old" I mean similar to their counterparts in the
 *     previous frame; TemporalTolerance controls the maximum squared
 *     luminance difference above which a pixel is considered "new".
 *
 * Pixels are processed 4 at a time using MMX instructions.
 *
 * SQUARING NOTE:
 * We square luminance differences to amplify the effects of large
 * differences and to avoid dealing with negative differences.  Unfortunately,
 * we can't compare the square of difference directly against a threshold,
 * thanks to the lack of an MMX unsigned compare instruction.  The
 * problem is that if we had two pixels with luminance 0 and 255,
 * the difference squared would be 65025, which is a negative
 * 16-bit signed value and would thus compare less than a threshold.
 * We get around this by dividing all the luminance values by two before
 * squaring them; this results in an effective maximum luminance
 * difference of 127, whose square (16129) is safely comparable.
 */

const int64_t TwoFrameTemporalTolerance = 300;
const int64_t TwoFrameSpatialTolerance = 600;

void deinterlace_twoframe_packed422_scanline_mmxext( unsigned char *output,
                                                     unsigned char *t1,
                                                     unsigned char *m1,
                                                     unsigned char *b1,
                                                     unsigned char *t0,
                                                     unsigned char *m0,
                                                     unsigned char *b0,
                                                     int width )
{
    const int64_t YMask = 0x00ff00ff00ff00ff;
    const int64_t qwAllOnes = 0xffffffffffffffff;
    int64_t qwBobbedPixels;
    int64_t qwSpatialTolerance;
    int64_t qwTemporalTolerance;

    // divide by 4 because of squaring behavior.
    qwSpatialTolerance = TwoFrameSpatialTolerance / 4;
    qwSpatialTolerance += (qwSpatialTolerance << 48) + (qwSpatialTolerance << 32)
                                                     + (qwSpatialTolerance << 16);
    qwTemporalTolerance = TwoFrameTemporalTolerance / 4;
    qwTemporalTolerance += (qwTemporalTolerance << 48) + (qwTemporalTolerance << 32)
                                                       + (qwTemporalTolerance << 16);

    // TODO: Just bob any extra pixels.

    width /= 4;
    while( width-- ) {
        movq_m2r( *t1, mm0 );          // mm0 = T1
        movq_m2r( *b1, mm1 );          // mm1 = B1
        movq_m2r( *m1, mm2 );          // mm2 = M1

        // Average T1 and B1 so we can do interpolated bobbing if we bob onto T1.
        movq_r2r( mm1, mm7 );          // mm7 = B1
        pavgb_r2r( mm0, mm7 );         // mm7 = (T1+B1/2)

        movq_r2m( mm7, qwBobbedPixels );

        // Now that we've averaged them, we no longer care about the chroma
        // values of T1 and B1 (all our comparisons are luminance-only).
        pand_m2r( YMask, mm0 );         // mm0 = luminance(T1)
        pand_m2r( YMask, mm1 );         // mm1 = luminance(B1)

        // Find out whether M1 is new.  "New" means the square of the
        // luminance difference between M1 and M0 is less than the temporal
        // tolerance.
        //
        movq_r2r( mm2, mm7 );           // mm7 = M1
        movq_m2r( *m0, mm4 );           // mm4 = M0
        pand_m2r( YMask, mm7 );         // mm7 = luminance(M1)
        movq_r2r( mm7, mm6 );           // mm6 = luminance(M1)     used below
        pand_m2r( YMask, mm4 );         // mm4 = luminance(M0)
        psubsw_r2r( mm4, mm7 );         // mm7 = M1 - M0
        psraw_i2r( 1, mm7 );            // mm7 = M1 - M0 (see SQUARING NOTE above)
        pmullw_r2r( mm7, mm7 );         // mm7 = (M1 - M0) ^ 2
        pcmpgtw_m2r( qwTemporalTolerance, mm7 );
                                        // mm7 = 0xffff where (M1 - M0) ^ 2 > threshold, 0x0000 otherwise


        // Find out how different T1 and M1 are.
        movq_r2r( mm0, mm3 );           // mm3 = T1
        psubsw_r2r( mm6, mm3 );         // mm3 = T1 - M1
        psraw_i2r( 1, mm3 );            // mm3 = T1 - M1 (see SQUARING NOTE above)
        pmullw_r2r( mm3, mm3 );         // mm3 = (T1 - M1) ^ 2
        pcmpgtw_m2r( qwSpatialTolerance, mm3 );
                                        // mm3 = 0xffff where (T1 - M1) ^ 2 > threshold, 0x0000 otherwise


        // Find out how different B1 and M1 are.
        movq_r2r( mm1, mm4 );           // mm4 = B1
        psubsw_r2r( mm6, mm4 );         // mm4 = B1 - M1
        psraw_i2r( 1, mm4 );            // mm4 = B1 - M1 (see SQUARING NOTE above)
        pmullw_r2r( mm4, mm4 );         // mm4 = (B1 - M1) ^ 2
        pcmpgtw_m2r( qwSpatialTolerance, mm4 );
                                        // mm4 = 0xffff where (B1 - M1) ^ 2 > threshold, 0x0000 otherwise


        // We care about cases where M1 is different from both T1 and B1.
        pand_r2r( mm4, mm3 );           // mm3 = 0xffff where M1 is different from T1 and B1, 0x0000 otherwise

        // Find out whether T1 is new.
        movq_r2r( mm0, mm4 );           // mm4 = T1
        movq_m2r( *t0, mm5 );           // mm5 = T0
        pand_m2r( YMask, mm5 );         // mm5 = luminance(T0)
        psubsw_r2r( mm5, mm4 );         // mm4 = T1 - T0
        psraw_i2r( 1, mm4 );            // mm4 = T1 - T0 (see SQUARING NOTE above)
        pmullw_r2r( mm4, mm4 );         // mm4 = (T1 - T0) ^ 2 / 4
        pcmpgtw_m2r( qwTemporalTolerance, mm4 );
                                        // mm4 = 0xffff where (T1 - T0) ^ 2 > threshold, 0x0000 otherwise

        // Find out whether B1 is new.
        movq_r2r( mm1, mm5 );           // mm5 = B1
        movq_m2r( *b0, mm6 );           // mm6 = B0
        pand_m2r( YMask, mm6 );         // mm6 = luminance(B0)
        psubsw_r2r( mm6, mm5 );         // mm5 = B1 - B0
        psraw_i2r( 1, mm5 );            // mm5 = B1 - B0 (see SQUARING NOTE above)
        pmullw_r2r( mm5, mm5 );         // mm5 = (B1 - B0) ^ 2
        pcmpgtw_m2r( qwTemporalTolerance, mm5 );
                                        // mm5 = 0xffff where (B1 - B0) ^ 2 > threshold, 0x0000 otherwise

        // We care about cases where M1 is old and either T1 or B1 is old.
        por_r2r( mm5, mm4 );            // mm4 = 0xffff where T1 or B1 is new
        por_r2r( mm7, mm4 );            // mm4 = 0xffff where T1 or B1 or M1 is new
        movq_m2r( qwAllOnes, mm6 );     // mm6 = 0xffffffffffffffff
        pxor_r2r( mm6, mm4 );           // mm4 = 0xffff where T1 and B1 and M1 are old

        // Pick up the interpolated (T1+B1)/2 pixels.
        movq_m2r( qwBobbedPixels, mm1 );// mm1 = (T1 + B1) / 2

        // At this point:
        //  mm1 = (T1+B1)/2
        //  mm2 = M1
        //  mm3 = mask, 0xffff where M1 is different from both T1 and B1
        //  mm4 = mask, 0xffff where T1 and B1 and M1 are old
        //  mm6 = 0xffffffffffffffff
        //
        // Now figure out where we're going to weave and where we're going to bob.
        // We'll weave if all pixels are old or M1 isn't different from both its
        // neighbors.
        pxor_r2r( mm6, mm3 );           // mm3 = 0xffff where M1 is the same as either T1 or B1
        por_r2r( mm4, mm3 );            // mm3 = 0xffff where M1 and T1 and B1 are old or M1 = T1 or B1
        pand_r2r( mm3, mm2 );           // mm2 = woven data where T1 or B1 isn't new or they're different
        pandn_r2r( mm1, mm3 );          // mm3 = bobbed data where T1 or B1 is new and they're similar
        por_r2r( mm2, mm3 );            // mm3 = finished pixels

        // Put the pixels in place.
        movntq_r2m( mm3, *output );

        // Advance to the next set of pixels.
        output += 8;
        t0 += 8;
        m0 += 8;
        b0 += 8;
        t1 += 8;
        m1 += 8;
        b1 += 8;
    }
    emms();
}

const int64_t GreedyTwoFrameThreshold = 4;
const int64_t GreedyTwoFrameThreshold2 = 8;

void deinterlace_greedytwoframe_packed422_scanline_mmxext( unsigned char *output,
                                                           unsigned char *t1,
                                                           unsigned char *m1,
                                                           unsigned char *b1,
                                                           unsigned char *t0,
                                                           unsigned char *m0,
                                                           unsigned char *b0,
                                                           int width )
{
    const int64_t Mask = 0x7f7f7f7f7f7f7f7f;
    const int64_t DwordOne = 0x0000000100000001;    
    const int64_t DwordTwo = 0x0000000200000002;    
    int64_t qwGreedyTwoFrameThreshold;

    qwGreedyTwoFrameThreshold = GreedyTwoFrameThreshold;
    qwGreedyTwoFrameThreshold += (GreedyTwoFrameThreshold2 << 8);
    qwGreedyTwoFrameThreshold += (qwGreedyTwoFrameThreshold << 48) +
                                (qwGreedyTwoFrameThreshold << 32) + 
                                (qwGreedyTwoFrameThreshold << 16);

    width /= 4;
    while( width-- ) {
        movq_m2r( *t1, mm1 );
        movq_m2r( *m1, mm0 );
        movq_m2r( *b1, mm3 );
        movq_m2r( *m0, mm2 );

        // Average T1 and B1 so we can do interpolated bobbing if we bob onto T1.
        movq_r2r( mm3, mm7 );                 // mm7 = B1
        pavgb_r2r( mm1, mm7 );

        // calculate |M1-M0| put result in mm4 need to keep mm0 intact
        // if we have a good processor then make mm0 the average of M1 and M0
        // which should make weave look better when there is small amounts of
        // movement
        movq_r2r( mm0, mm4 );
        movq_r2r( mm2, mm5 );
        psubusb_r2r( mm2, mm4 );
        psubusb_r2r( mm0, mm5 );
        por_r2r( mm5, mm4 );
        psrlw_i2r( 1, mm4 );
        pavgb_r2r( mm2, mm0 );
        pand_r2r( mm6, mm4 );

        // if |M1-M0| > Threshold we want dword worth of twos
        pcmpgtb_m2r( qwGreedyTwoFrameThreshold, mm4 );
        pand_m2r( Mask, mm4 );          // get rid of any sign bit
        pcmpgtd_m2r( DwordOne, mm4 );   // do we want to bob
        pandn_m2r( DwordTwo, mm4 );

        movq_m2r( *t0, mm2 );           // mm2 = T0

        // calculate |T1-T0| put result in mm5
        movq_r2r( mm2, mm5 );
        psubusb_r2r( mm1, mm5 );
        psubusb_r2r( mm2, mm1 );
        por_r2r( mm1, mm5 );
        psrlw_i2r( 1, mm5 );
        pand_r2r( mm6, mm5 );

        // if |T1-T0| > Threshold we want dword worth of ones
        pcmpgtb_m2r( qwGreedyTwoFrameThreshold, mm5 );
        pand_r2r( mm6, mm5 );                // get rid of any sign bit
        pcmpgtd_m2r( DwordOne, mm5 );
        pandn_m2r( DwordOne, mm5 );
        paddd_r2r( mm5, mm4 );

        movq_m2r( *b0, mm2 ); // B0

        // calculate |B1-B0| put result in mm5
        movq_r2r( mm2, mm5 );
        psubusb_r2r( mm3, mm5 );
        psubusb_r2r( mm2, mm3 );
        por_r2r( mm3, mm5 );
        psrlw_i2r( 1, mm5 );
        pand_r2r( mm6, mm5 );

        // if |B1-B0| > Threshold we want dword worth of ones
        pcmpgtb_m2r( qwGreedyTwoFrameThreshold, mm5 );
        pand_r2r( mm6, mm5 );              // get rid of any sign bit
        pcmpgtd_m2r( DwordOne, mm5 );
        pandn_m2r( DwordOne, mm5 );
        paddd_r2r( mm5, mm4 );

        pcmpgtd_m2r( DwordTwo, mm4 );

        movq_r2r( mm4, mm5 );
         // mm4 now is 1 where we want to weave and 0 where we want to bob
        pand_r2r( mm0, mm4 );
        pandn_r2r( mm7, mm5 );
        por_r2r( mm5, mm4 );

        movntq_r2m( mm4, *output );

        // Advance to the next set of pixels.
        output += 8;
        t0 += 8;
        m0 += 8;
        b0 += 8;
        t1 += 8;
        m1 += 8;
        b1 += 8;
    }
    emms();
}

void deinterlace_twoframe_packed422_scanline_c( unsigned char *output,
                                                unsigned char *t1,
                                                unsigned char *m1,
                                                unsigned char *b1,
                                                unsigned char *t0,
                                                unsigned char *m0,
                                                unsigned char *b0,
                                                int width )
{
    interpolate_packed422_scanline( output, t1, b1, width );
}

void setup_speedy_calls( void )
{
    uint32_t accel = mm_accel();

    interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    blit_colour_packed422_scanline = blit_colour_packed422_scanline_c;
    blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_c;
    blit_packed422_scanline = blit_packed422_scanline_c;
    deinterlace_twoframe_packed422_scanline = deinterlace_twoframe_packed422_scanline_c;

    if( accel & MM_ACCEL_X86_MMXEXT ) {
        fprintf( stderr, "tvtime: Using MMXEXT optimized functions.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmxext;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmxext;
        blit_packed422_scanline = blit_packed422_scanline_mmxext;
        deinterlace_twoframe_packed422_scanline = deinterlace_twoframe_packed422_scanline_mmxext;
    } else if( accel & MM_ACCEL_X86_MMX ) {
        fprintf( stderr, "tvtime: Using MMX optimized functions.\n" );
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmx;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmx;
    } else {
        fprintf( stderr, "tvtime: No optimizations detected, "
                         "using C fallbacks.\n" );
    }
}

