
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
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
void (*composite_packed4444_to_packed422_scanline)( unsigned char *output, unsigned char *input,
                                                    unsigned char *foreground, int width );
void (*composite_packed4444_alpha_to_packed422_scanline)( unsigned char *output,
                                                          unsigned char *input,
                                                          unsigned char *foreground,
                                                          int width, int alpha );
void (*composite_alphamask_to_packed4444_scanline)( unsigned char *output,
                                                unsigned char *input,
                                                unsigned char *mask, int width,
                                                int textluma, int textcb,
                                                int textcr );
void (*composite_alphamask_alpha_to_packed4444_scanline)( unsigned char *output,
                                                       unsigned char *input,
                                                       unsigned char *mask, int width,
                                                       int textluma, int textcb,
                                                       int textcr, int alpha );


static unsigned int speedy_time = 0;
static struct timeval cur_start_time;
static struct timeval cur_end_time;

#define SPEEDY_START() \
    gettimeofday( &cur_start_time, 0 )

#define SPEEDY_END() \
    gettimeofday( &cur_end_time, 0 ); \
    speedy_time += (   ( ( cur_end_time.tv_sec * 1000 * 1000 ) + cur_end_time.tv_usec ) \
                     - ( ( cur_start_time.tv_sec * 1000 * 1000 ) + cur_start_time.tv_usec ) )

#define PREFETCH_2048(x) \
    { int *pfetcha = (int *) x; \
        int pfetchtmp; \
        prefetchnta( pfetcha ); \
        prefetchnta( pfetcha + 64 ); \
        prefetchnta( pfetcha + 128 ); \
        prefetchnta( pfetcha + 192 ); \
        pfetcha += 256; \
        prefetchnta( pfetcha ); \
        prefetchnta( pfetcha + 64 ); \
        prefetchnta( pfetcha + 128 ); \
        prefetchnta( pfetcha + 192 ); }
/*
        pfetchtmp = pfetcha[ 0 ] + pfetcha[ 16 ] + pfetcha[ 32 ] + pfetcha[ 48 ] + \
            pfetcha[ 64 ] + pfetcha[ 80 ] + pfetcha[ 96 ] + pfetcha[ 112 ] + \
            pfetcha[ 128 ] + pfetcha[ 144 ] + pfetcha[ 160 ] + pfetcha[ 176 ] + \
            pfetcha[ 192 ] + pfetcha[ 208 ] + pfetcha[ 224 ] + pfetcha[ 240 ]; \
        pfetcha += 256; \
        pfetchtmp = pfetcha[ 0 ] + pfetcha[ 16 ] + pfetcha[ 32 ] + pfetcha[ 48 ] + \
            pfetcha[ 64 ] + pfetcha[ 80 ] + pfetcha[ 96 ] + pfetcha[ 112 ] + \
            pfetcha[ 128 ] + pfetcha[ 144 ] + pfetcha[ 160 ] + pfetcha[ 176 ] + \
            pfetcha[ 192 ] + pfetcha[ 208 ] + pfetcha[ 224 ] + pfetcha[ 240 ]; }
*/

static inline __attribute__ ((always_inline,const)) int multiply_alpha( int a, int r )
{
    int temp;
    temp = (r * a) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}


void comb_factor_packed422_scanline( unsigned char *top, unsigned char *mid,
                                     unsigned char *bot, int width )
{
    const uint64_t qwYMask = 0x00ff00ff00ff00ff;
    const uint64_t qwOnes = 0x0001000100010001;
    uint64_t qwThreshold;

    SPEEDY_START();

    width /= 4;

    movq_m2r( qwThreshold, mm0 );
    movq_m2r( qwYMask, mm1 );
    movq_m2r( qwOnes, mm2 );
    pxor_r2r( mm7, mm7 );         /* mm7 = 0. */

    while( width-- ) {
        /* Load and keep just the luma. */
        movq_m2r( *top, mm3 );
        movq_m2r( *mid, mm4 );
        movq_m2r( *bot, mm5 );

        pand_r2r( mm1, mm3 );
        pand_r2r( mm1, mm4 );
        pand_r2r( mm1, mm5 );

        /* Work out mm6 = (top - mid) * (bot - mid) - ( (top - mid)^2 >> 7 ) */
        psrlw_i2r( 1, mm3 );
        psrlw_i2r( 1, mm4 );
        psrlw_i2r( 1, mm5 );

        /* mm6 = (top - mid) */
        movq_r2r( mm3, mm6 );
        psubw_r2r( mm4, mm6 );

        /* mm3 = (top - bot) */
        psubw_r2r( mm5, mm3 );

        /* mm5 = (bot - mid) */
        psubw_r2r( mm4, mm5 );

        /* mm6 = (top - mid) * (bot - mid) */
        pmullw_r2r( mm5, mm6 );

        /* mm3 = (top - bot)^2 >> 7 */
        pmullw_r2r( mm3, mm3 );   /* mm3 = (top - bot)^2 */
        psrlw_i2r( 7, mm3 );      /* mm3 = ((top - bot)^2 >> 7) */

        /* mm6 is what we want. */
        psubw_r2r( mm3, mm6 );

        /* FF's if greater than qwTheshold */
        pcmpgtw_r2r( mm0, mm6 );

        /* Add to count if we are greater than threshold */
        pand_r2r( mm2, mm6 );
        paddw_r2r( mm6, mm7 );

        top += 8;
        mid += 8;
        bot += 8;
    }
    emms();

    SPEEDY_END();
}



void interpolate_packed422_scanline_c( unsigned char *output,
                                       unsigned char *top,
                                       unsigned char *bot, int width )
{
    int i;

    SPEEDY_START();

    for( i = width*2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }

    SPEEDY_END();
}


void interpolate_packed422_scanline_mmxext( unsigned char *output,
                                            unsigned char *top,
                                            unsigned char *bot, int width )
{
    int i;

    SPEEDY_START();

    for( i = width/4; i; --i ) {
        movq_m2r( *bot, mm2 );
        movq_m2r( *top, mm3 );
        pavgb_r2r( mm3, mm2 );
        movq_r2m( mm2, *output );
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

    SPEEDY_END();
}

void blit_colour_packed422_scanline_c( unsigned char *output,
                                       int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;
    unsigned int *o = (unsigned int *) output;

    SPEEDY_START();

    for( width /= 2; width; --width ) {
        *o++ = colour;
    }

    SPEEDY_END();
}

void blit_colour_packed422_scanline_mmx( unsigned char *output,
                                         int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;

    SPEEDY_START();

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 4; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();

    SPEEDY_END();
}

void blit_colour_packed422_scanline_mmxext( unsigned char *output,
                                            int width, int y, int cb, int cr )
{
    int colour = cr << 24 | y << 16 | cb << 8 | y;

    SPEEDY_START();

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 4; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();

    SPEEDY_END();
}

void blit_colour_packed4444_scanline_c( unsigned char *output, int width,
                                        int alpha, int luma, int cb, int cr )
{
    int j;

    SPEEDY_START();

    for( j = 0; j < width; j++ ) {
        *output++ = alpha;
        *output++ = luma;
        *output++ = cb;
        *output++ = cr;
    }

    SPEEDY_END();
}

void blit_colour_packed4444_scanline_mmx( unsigned char *output, int width,
                                          int alpha, int luma,
                                          int cb, int cr )
{
    int colour = (cr << 24) | (cb << 16) | (luma << 8) | alpha;

    SPEEDY_START();

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( width /= 2; width; --width ) {
        movq_r2m( mm2, *output );
        output += 8;
    }

    emms();

    SPEEDY_END();
}

void blit_colour_packed4444_scanline_mmxext( unsigned char *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr )
{
    int colour = (cr << 24) | (cb << 16) | (luma << 8) | alpha;
    int i;

    SPEEDY_START();

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

    SPEEDY_END();
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
/*
      "movntq %%mm0, (%1)\n"
      "movntq %%mm1, 8(%1)\n"
      "movntq %%mm2, 16(%1)\n"
      "movntq %%mm3, 24(%1)\n"
      "movntq %%mm4, 32(%1)\n"
      "movntq %%mm5, 40(%1)\n"
      "movntq %%mm6, 48(%1)\n"
      "movntq %%mm7, 56(%1)\n"
*/
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
     /* since movntq is weakly-ordered, a "sfence"
     * is needed to become ordered again. */
    //__asm__ __volatile__ ("sfence":::"memory");
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
    SPEEDY_START();
    if( dest != src ) mmx2_memcpy( dest, src, width*2 );
    SPEEDY_END();
}

void blit_packed422_scanline_i386_linux( unsigned char *dest, const unsigned char *src, int width )
{
    SPEEDY_START();
    if( dest != src ) __memcpy( dest, src, width*2 );
    SPEEDY_END();
}

void blit_packed422_scanline_c( unsigned char *dest, const unsigned char *src, int width )
{
    SPEEDY_START();
    if( dest != src ) memcpy( dest, src, width*2 );
    SPEEDY_END();
}

void composite_packed4444_alpha_to_packed422_scanline_c( unsigned char *output,
                                                         unsigned char *input,
                                                         unsigned char *foreground, int width, int alpha )
{
    int i;

    SPEEDY_START();
    for( i = 0; i < width; i++ ) {
        int af = foreground[ 0 ];

        if( af ) {
            int a = ((af * alpha) + 0x80) >> 8;


            if( a == 0xff ) {
                output[ 0 ] = foreground[ 1 ];

                if( ( i & 1 ) == 0 ) {
                    output[ 1 ] = foreground[ 2 ];
                    output[ 3 ] = foreground[ 3 ];
                }
            } else if( a ) {
                /**
                 * (1 - alpha)*B + alpha*F
                 * (1 - af*a)*B + af*a*F
                 *  B - af*a*B + af*a*F
                 *  B + a*(af*F - af*B)
                 */

                output[ 0 ] = input[ 0 ]
                            + ((alpha*( foreground[ 1 ]
                                        - multiply_alpha( foreground[ 0 ], input[ 0 ] ) ) + 0x80) >> 8);

                if( ( i & 1 ) == 0 ) {

                    /**
                     * At first I thought I was doing this incorrectly, but
                     * the following math has convinced me otherwise.
                     *
                     * C_r = (1 - alpha)*B + alpha*F
                     * C_r = B - af*a*B + af*a*F
                     *
                     * C_r = 128 + ((1 - af*a)*(B - 128) + a*af*(F - 128))
                     * C_r = 128 + (B - af*a*B - 128 + af*a*128 + a*af*F - a*af*128)
                     * C_r = B - af*a*B + a*af*F
                     */

                    output[ 1 ] = input[ 1 ] + ((alpha*( foreground[ 2 ]
                                            - multiply_alpha( foreground[ 0 ], input[ 1 ] ) ) + 0x80) >> 8);
                    output[ 3 ] = input[ 3 ] + ((alpha*( foreground[ 3 ]
                                            - multiply_alpha( foreground[ 0 ], input[ 3 ] ) ) + 0x80) >> 8);
                }
            }
        }
        foreground += 4;
        output += 2;
        input += 2;
    }
    SPEEDY_END();
}

void composite_packed4444_alpha_to_packed422_scanline_mmxext( unsigned char *output,
                                                              unsigned char *input,
                                                              unsigned char *foreground,
                                                              int width, int alpha )
{
    const uint64_t alpha2 = 0x0000FFFF00000000;
    const uint64_t alpha1 = 0xFFFF0000FFFFFFFF;
    const uint64_t round  = 0x0080008000800080;
    int i;

    if( !alpha ) {
        blit_packed422_scanline( output, input, width );
        return;
    }

    if( alpha == 256 ) {
        composite_packed4444_to_packed422_scanline( output, input, foreground, width );
        return;
    }

    SPEEDY_START();
    PREFETCH_2048( input );
    PREFETCH_2048( foreground );

    movq_m2r( alpha, mm2 );
    pshufw_r2r( mm2, mm2, 0 );
    pxor_r2r( mm7, mm7 );

    for( i = width/2; i; i-- ) {
        int fg1 = *((unsigned int *) foreground);
        int fg2 = *(((unsigned int *) foreground)+1);

        if( fg1 || fg2 ) {
            // mm1 = [ cr ][ y ][ cb ][ y ]
            movd_m2r( *input, mm1 );
            punpcklbw_r2r( mm7, mm1 );

            movq_m2r( *foreground, mm3 );
            movq_r2r( mm3, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpckhbw_r2r( mm7, mm4 );
            // mm3 and mm4 will be the appropriate colours, mm5 and mm6 for alpha.

            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0 a ][ 0 a ][ 0 a ][ 0 a ]
            pshufw_r2r( mm3, mm5, 0 );
            pshufw_r2r( mm4, mm6, 0 );
            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 3 cr ][ 0 a ][ 2 cb ][ 1 y ]  == 11001000 == 201
            pshufw_r2r( mm3, mm3, 201 );
            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0 a ][ 1 y ][ 0 a ][ 0 a ]  == 00010000 == 16
            pshufw_r2r( mm4, mm4, 16 );

            pand_m2r( alpha1, mm3 );
            pand_m2r( alpha2, mm4 );
            pand_m2r( alpha1, mm5 );
            pand_m2r( alpha2, mm6 );
            por_r2r( mm4, mm3 );
            por_r2r( mm6, mm5 );

            // now, mm5 is af and mm1 is B.  Need to multiply them.
            pmullw_r2r( mm1, mm5 );

            // Multiply by appalpha.
            pmullw_r2r( mm2, mm3 );
            paddw_m2r( round, mm3 );
            psrlw_i2r( 8, mm3 );
            // Result is now B + F.
            paddw_r2r( mm3, mm1 );

            // Round up appropriately.
            paddw_m2r( round, mm5 );

            // mm6 contains our i>>8;
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );

            // Add mm6 back into mm5.  Now our result is in the high bytes.
            paddw_r2r( mm6, mm5 );

            // Shift down.
            psrlw_i2r( 8, mm5 );

            // Multiply by appalpha.
            pmullw_r2r( mm2, mm5 );
            paddw_m2r( round, mm5 );
            psrlw_i2r( 8, mm5 );

            psubusw_r2r( mm5, mm1 );

            // mm1 = [ B + F - af*B ]
            packuswb_r2r( mm1, mm1 );
            movd_r2m( mm1, *output );
        }

        foreground += 8;
        output += 4;
        input += 4;
    }
    emms();

    SPEEDY_END();
}

void composite_packed4444_to_packed422_scanline_c( unsigned char *output,
                                                   unsigned char *input,
                                                   unsigned char *foreground, int width )
{
    int i;
    SPEEDY_START();
    for( i = 0; i < width; i++ ) {
        int a = foreground[ 0 ];

        if( a == 0xff ) {
            output[ 0 ] = foreground[ 1 ];

            if( ( i & 1 ) == 0 ) {
                output[ 1 ] = foreground[ 2 ];
                output[ 3 ] = foreground[ 3 ];
            }
        } else if( a ) {
            /**
             * (1 - alpha)*B + alpha*F
             *  B + af*F - af*B
             */

            output[ 0 ] = input[ 0 ] + foreground[ 1 ] - multiply_alpha( foreground[ 0 ], input[ 0 ] );

            if( ( i & 1 ) == 0 ) {

                /**
                 * C_r = (1 - af)*B + af*F
                 * C_r = B - af*B + af*F
                 */

                output[ 1 ] = input[ 1 ] + foreground[ 2 ] - multiply_alpha( foreground[ 0 ], input[ 1 ] );
                output[ 3 ] = input[ 3 ] + foreground[ 3 ] - multiply_alpha( foreground[ 0 ], input[ 3 ] );
            }
        }
        foreground += 4;
        output += 2;
        input += 2;
    }
    SPEEDY_END();
}


void composite_packed4444_to_packed422_scanline_mmxext( unsigned char *output,
                                                        unsigned char *input,
                                                        unsigned char *foreground, int width )
{
    const uint64_t alpha2 = 0x0000FFFF00000000;
    const uint64_t alpha1 = 0xFFFF0000FFFFFFFF;
    const uint64_t round  = 0x0080008000800080;
    int i;

    SPEEDY_START();
    PREFETCH_2048( input );
    PREFETCH_2048( foreground );

    pxor_r2r( mm7, mm7 );
    for( i = width/2; i; i-- ) {
        int fg1 = *((unsigned int *) foreground);
        int fg2 = *(((unsigned int *) foreground)+1);

        if( (fg1 & 0xff) == 0xff && (fg2 & 0xff) == 0xff ) {
            movq_m2r( *foreground, mm3 );
            movq_r2r( mm3, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpckhbw_r2r( mm7, mm4 );
            // mm3 and mm4 will be the appropriate colours, mm5 and mm6 for alpha.
            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 3 cr ][ 0 a ][ 2 cb ][ 1 y ]  == 11001000 == 201
            pshufw_r2r( mm3, mm3, 201 );
            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0  a ][ 1 y ][ 0  a ][ 0 a ]  == 00010000 == 16
            pshufw_r2r( mm4, mm4, 16 );
            pand_m2r( alpha1, mm3 );
            pand_m2r( alpha2, mm4 );
            por_r2r( mm4, mm3 );
            // mm1 = [ B + F - af*B ]
            packuswb_r2r( mm3, mm3 );
            movd_r2m( mm3, *output );
        } else if( fg1 || fg2 ) {
            // mm1 = [ cr ][ y ][ cb ][ y ]
            movd_m2r( *input, mm1 );
            punpcklbw_r2r( mm7, mm1 );

            movq_m2r( *foreground, mm3 );
            movq_r2r( mm3, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpckhbw_r2r( mm7, mm4 );
            // mm3 and mm4 will be the appropriate colours, mm5 and mm6 for alpha.

            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0 a ][ 0 a ][ 0 a ][ 0 a ]
            pshufw_r2r( mm3, mm5, 0 );
            pshufw_r2r( mm4, mm6, 0 );
            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 3 cr ][ 0 a ][ 2 cb ][ 1 y ]  == 11001000 == 201
            pshufw_r2r( mm3, mm3, 201 );
            // [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0  a ][ 1 y ][ 0  a ][ 0 a ]  == 00010000 == 16
            pshufw_r2r( mm4, mm4, 16 );

            pand_m2r( alpha1, mm3 );
            pand_m2r( alpha2, mm4 );
            pand_m2r( alpha1, mm5 );
            pand_m2r( alpha2, mm6 );
            por_r2r( mm4, mm3 );
            por_r2r( mm6, mm5 );

            // now, mm5 is af and mm1 is B.  Need to multiply them.
            pmullw_r2r( mm1, mm5 );

            // Result is now B + F.
            paddw_r2r( mm3, mm1 );

            // Round up appropriately.
            paddw_m2r( round, mm5 );

            // mm6 contains our i>>8;
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );

            // Add mm6 back into mm5.  Now our result is in the high bytes.
            paddw_r2r( mm6, mm5 );

            // Shift down.
            psrlw_i2r( 8, mm5 );

            psubusw_r2r( mm5, mm1 );

            // mm1 = [ B + F - af*B ]
            packuswb_r2r( mm1, mm1 );
            movd_r2m( mm1, *output );
        }

        foreground += 8;
        output += 4;
        input += 4;
    }
    emms();

    SPEEDY_END();
}

void composite_alphamask_to_packed4444_scanline_c( unsigned char *output,
                                                   unsigned char *input,
                                                   unsigned char *mask,
                                                   int width,
                                                   int textluma, int textcb,
                                                   int textcr )
{
    unsigned int opaque = (textcr << 24) | (textcb << 16) | (textluma << 8) | 0xff;
    int i;

    SPEEDY_START();

    for( i = 0; i < width; i++ ) {
        int a = *mask;

        if( a == 0xff ) {
            *((unsigned int *) output) = opaque;
        } else if( (input[ 0 ] == 0x00) ) {
            *((unsigned int *) output) = (multiply_alpha( a, textcr ) << 24)
                                       | (multiply_alpha( a, textcb ) << 16)
                                       | (multiply_alpha( a, textluma ) << 8) | a;
        } else if( a ) {
            *((unsigned int *) output) = ((input[ 3 ] + multiply_alpha( a, textcr - input[ 3 ] )) << 24)
                                      | ((input[ 2 ] + multiply_alpha( a, textcb - input[ 2 ] )) << 16)
                                      | ((input[ 1 ] + multiply_alpha( a, textluma - input[ 1 ] )) << 8)
                                      | (a + multiply_alpha( 0xff - a, input[ 0 ] ));
        }
        mask++;
        output += 4;
        input += 4;
    }
    SPEEDY_END();
}

void composite_alphamask_to_packed4444_scanline_mmxext( unsigned char *output,
                                                   unsigned char *input,
                                                   unsigned char *mask,
                                                   int width,
                                                   int textluma, int textcb,
                                                   int textcr )
{
    unsigned int opaque = (textcr << 24) | (textcb << 16) | (textluma << 8) | 0xff;
    const uint64_t round  = 0x0080008000800080;
    int i;

    SPEEDY_START();

    movd_m2r( textluma, mm1 );
    movd_m2r( textcb, mm2 );
    movd_m2r( textcr, mm3 );

    // mm1 = [  0 ][  0 ][ y ][ 0 ] == 11110011 == 243
    pshufw_r2r( mm1, mm1, 243 );
    // mm2 = [  0 ][ cb ][ 0 ][ 0 ] == 11001111 == 207
    pshufw_r2r( mm2, mm2, 207 );
    // mm3 = [ cr ][  0 ][ 0 ][ 0 ] == 00111111 == 63
    pshufw_r2r( mm3, mm3, 63 );

    // mm1 = [ cr ][ cb ][ y ][ 0 ]
    por_r2r( mm2, mm1 );
    por_r2r( mm3, mm1 );

    for( i = 0; i < width; i++ ) {
        int a = *mask;

        if( a == 0xff ) {
            *((unsigned int *) output) = opaque;
        } else if( (input[ 0 ] == 0x00) ) {
            movd_m2r( a, mm0 );
            movq_r2r( mm0, mm7 );
            pshufw_r2r( mm0, mm0, 0 );
            movq_r2r( mm1, mm5 );
            pmullw_r2r( mm0, mm5 );
            paddw_m2r( round, mm5 );
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );
            paddw_r2r( mm6, mm5 );
            psrlw_i2r( 8, mm5 );
            por_r2r( mm7, mm5 );
            packuswb_r2r( mm5, mm5 );
            movd_r2m( mm5, *output );
/*
            *((unsigned int *) output) = (multiply_alpha( a, textcr ) << 24)
                                       | (multiply_alpha( a, textcb ) << 16)
                                       | (multiply_alpha( a, textluma ) << 8) | a;
*/
        } else if( a ) {
/*
            movd_m2r( a, mm0 );
            movq_r2r( mm0, mm7 );
            pshufw_r2r( mm0, mm0, 0 );
            movq_r2r( mm1, mm5 );

            pmullw_r2r( mm0, mm5 );
            paddw_m2r( round, mm5 );
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );
            paddw_r2r( mm6, mm5 );
            psrlw_i2r( 8, mm5 );
            por_r2r( mm7, mm5 );
            packuswb_r2r( mm5, mm5 );
            movd_r2m( mm5, *output );
*/
            *((unsigned int *) output) = ((input[ 3 ] + multiply_alpha( a, textcr - input[ 3 ] )) << 24)
                                      | ((input[ 2 ] + multiply_alpha( a, textcb - input[ 2 ] )) << 16)
                                      | ((input[ 1 ] + multiply_alpha( a, textluma - input[ 1 ] )) << 8)
                                      | (a + multiply_alpha( 0xff - a, input[ 0 ] ));
        }
        mask++;
        output += 4;
        input += 4;
    }
    emms();
    SPEEDY_END();
}

void composite_alphamask_alpha_to_packed4444_scanline_c( unsigned char *output,
                                                       unsigned char *input,
                                                       unsigned char *mask, int width,
                                                       int textluma, int textcb,
                                                       int textcr, int alpha )
{
    unsigned int opaque = (textcr << 24) | (textcb << 16) | (textluma << 8) | 0xff;
    int i;

    SPEEDY_START();

    for( i = 0; i < width; i++ ) {
        int af = *mask;

        if( af ) {
           int a = ((af * alpha) + 0x80) >> 8;

           if( a == 0xff ) {
               *((unsigned int *) output) = opaque;
           } else if( (input[ 0 ] == 0x00) ) {
               *((unsigned int *) output) = (multiply_alpha( a, textcr ) << 24)
                                          | (multiply_alpha( a, textcb ) << 16)
                                          | (multiply_alpha( a, textluma ) << 8) | a;
           } else if( a ) {
               *((unsigned int *) output) = ((input[ 3 ] + multiply_alpha( a, textcr - input[ 3 ] )) << 24)
                                         | ((input[ 2 ] + multiply_alpha( a, textcb - input[ 2 ] )) << 16)
                                         | ((input[ 1 ] + multiply_alpha( a, textluma - input[ 1 ] )) << 8)
                                         | (a + multiply_alpha( 0xff - a, input[ 0 ] ));
           }
        }
        mask++;
        output += 4;
        input += 4;
    }

    SPEEDY_END();
}



static uint32_t speedy_accel;

void setup_speedy_calls( void )
{
    speedy_accel = mm_accel();

    interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    blit_colour_packed422_scanline = blit_colour_packed422_scanline_c;
    blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_c;
    blit_packed422_scanline = blit_packed422_scanline_c;
    composite_packed4444_to_packed422_scanline = composite_packed4444_to_packed422_scanline_c;
    composite_packed4444_alpha_to_packed422_scanline = composite_packed4444_alpha_to_packed422_scanline_c;
    composite_alphamask_to_packed4444_scanline = composite_alphamask_to_packed4444_scanline_c;
    composite_alphamask_alpha_to_packed4444_scanline = composite_alphamask_alpha_to_packed4444_scanline_c;

    if( speedy_accel & MM_ACCEL_X86_MMXEXT ) {
        fprintf( stderr, "speedycode: Using MMXEXT optimized functions.\n" );
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmxext;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmxext;
        blit_packed422_scanline = blit_packed422_scanline_mmxext_xine;
        composite_packed4444_to_packed422_scanline = composite_packed4444_to_packed422_scanline_mmxext;
        composite_packed4444_alpha_to_packed422_scanline = composite_packed4444_alpha_to_packed422_scanline_mmxext;
        composite_alphamask_to_packed4444_scanline = composite_alphamask_to_packed4444_scanline_mmxext;
    } else if( speedy_accel & MM_ACCEL_X86_MMX ) {
        fprintf( stderr, "speedycode: Using MMX optimized functions.\n" );
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmx;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmx;
    } else {
        fprintf( stderr, "speedycode: No MMX or MMXEXT support detected, "
                         "using C fallbacks.\n" );
    }
}

int speedy_get_accel( void )
{
    return speedy_accel;
}

unsigned int speedy_get_usecs( void )
{
    return speedy_time;
}

void speedy_reset_timer( void )
{
    speedy_time = 0;
}

