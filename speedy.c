
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedy.h"

/* Function pointer definitions. */
void (*interpolate_packed422_scanline)( unsigned char *output,
                                        unsigned char *top,
                                        unsigned char *bot, int width );
void (*blit_colour_packed422_scanline)( unsigned char *output,
                                        int width, int y, int cb, int cr );
void (*blit_colour_packed4444_scanline)( unsigned char *output,
                                         int width, int alpha, int luma,
                                         int cb, int cr );
void (*blit_packed422_scanline)( unsigned char *dest, const unsigned char *src, int width );


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
    int colour = (cr << 24) | (cb << 16) | (luma << 8) | alpha;

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
    int colour = (cr << 24) | (cb << 16) | (luma << 8) | alpha;
    int i;

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    i = (width*4)/8;
    for(; i; --i ) {
        movq_r2m( mm2, *output );
        output += 8;
    }
    if( width & 1 ) {
        *((unsigned int *) output) = colour;
    }
    emms();
}


/* Some memcpy code from xine which originally came from mplayer. */

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

#define MMX_MMREG_SIZE 8
#define MIN_LEN 0x40  /* 64-byte blocks */

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
  if( len ) __memcpy(to, from, len);
  return retval;
}

void blit_packed422_scanline_mmxext_xine( unsigned char *dest, const unsigned char *src, int width )
{
    mmx2_memcpy( dest, src, width*2 );
}

void blit_packed422_scanline_i386_linux( unsigned char *dest, const unsigned char *src, int width )
{
    __memcpy( dest, src, width*2 );
}

void blit_packed422_scanline_c( unsigned char *dest, const unsigned char *src, int width )
{
    memcpy( dest, src, width*2 );
}

void composite_packed4444_alpha_to_packed422_scanline_c( unsigned char *output,
                                                         unsigned char *input,
                                                         unsigned char *foreground, int width, int alpha )
{
    int i;

    for( i = 0; i < width; i++ ) {
        if( foreground[ 0 ] ) {
            int a = ((foreground[ 0 ]*alpha)+0x80)>>8;
            int tmp1, tmp2;

            tmp1 = (foreground[ 1 ] - input[ 0 ]) * a;
            tmp2 = input[ 0 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *output = tmp2 & 0xff;

            if( ( i & 1 ) == 0 ) {
                tmp1 = (foreground[ 2 ] - input[ 1 ]) * a;
                tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                output[ 1 ] = tmp2 & 0xff;

                tmp1 = (foreground[ 3 ] - input[ 3 ]) * a;
                tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                output[ 3 ] = tmp2 & 0xff;
            }
        }
        foreground += 4;
        output += 2;
        input += 2;
    }
}

void composite_packed4444_alpha_to_packed422_scanline_mmxext( unsigned char *output,
                                                              unsigned char *input,
                                                              unsigned char *foreground, int width, int alpha )
{
    int i;

    // [ cr ][ cb ][ y1 ][ a1 ][ cr ][ cb ][ y2 ][ a2 ]
    // ->
    // [                ][ a1 ][                ][ a2 ]
    // [ a1 ][ a1 ][ a2 ][ a2 ]
    // [ cr ][ y1 ][ cb ][ y2 ]

    // [ cr ][ cb ][ y3 ][ a3 ][ cr ][ cb ][ y4 ][ a4 ]
    // ->
    // [                ][ a3 ][                ][ a4 ]
    // [ a3 ][ a3 ][ a4 ][ a4 ]
    // [ cr ][ y3 ][ cb ][ y4 ]

    // [ cr ][ y1 ][ cb ][ y2 ][ cr ][ y3 ][ cb ][ y4 ]
    // [ a1 ][ a1 ][ a1 ][ a2 ][

    // [ CR ][ Y1 ][ CB ][ Y2 ][ CR ][ Y3 ][ CB ][ Y4 ]

    for( i = 0; i < width; i++ ) {
        if( foreground[ 0 ] ) {
            int a = ((foreground[ 0 ]*alpha)+0x80)>>8;
            int tmp1, tmp2;

            tmp1 = (foreground[ 1 ] - input[ 0 ]) * a;
            tmp2 = input[ 0 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
            *output = tmp2 & 0xff;

            if( ( i & 1 ) == 0 ) {
                tmp1 = (foreground[ 2 ] - input[ 1 ]) * a;
                tmp2 = input[ 1 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                output[ 1 ] = tmp2 & 0xff;

                tmp1 = (foreground[ 3 ] - input[ 3 ]) * a;
                tmp2 = input[ 3 ] + ((tmp1 + (tmp1 >> 8) + 0x80) >> 8);
                output[ 3 ] = tmp2 & 0xff;
            }
        }
        foreground += 4;
        output += 2;
        input += 2;
    }
}

static uint32_t speedy_accel;

void setup_speedy_calls( void )
{
    speedy_accel = mm_accel();

    interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    blit_colour_packed422_scanline = blit_colour_packed422_scanline_c;
    blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_c;
    blit_packed422_scanline = blit_packed422_scanline_c;

    if( speedy_accel & MM_ACCEL_X86_MMXEXT ) {
        fprintf( stderr, "speedycode: Using MMXEXT optimized functions.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmxext;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmxext;
        blit_packed422_scanline = blit_packed422_scanline_mmxext_xine;
    } else if( speedy_accel & MM_ACCEL_X86_MMX ) {
        fprintf( stderr, "speedycode: Using MMX optimized functions.\n" );
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmx;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmx;
    } else {
        fprintf( stderr, "speedycode: No optimizations detected, "
                         "using C fallbacks.\n" );
    }
}

int speedy_get_accel( void )
{
    return speedy_accel;
}

