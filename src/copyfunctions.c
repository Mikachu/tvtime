/**
 * Copyright (C) 2005 Billy Biggs <vektor@dumbterm.net>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include "copyfunctions.h"

/* Function pointer definitions. */
void (*interpolate_packed422_scanline)( uint8_t *output, uint8_t *top,
                                        uint8_t *bot, int width );
void (*blit_packed422_scanline)( uint8_t *dest, const uint8_t *src, int width );
void (*fast_memcpy)( void *output, const void *input, size_t size );


static void interpolate_packed422_scanline_c( uint8_t *output, uint8_t *top,
                                              uint8_t *bot, int width )
{
    int i;

    for( i = width*2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }
}

#ifdef ARCH_X86
static void interpolate_packed422_scanline_mmx( uint8_t *output, uint8_t *top,
                                                uint8_t *bot, int width )
{
    const mmx_t shiftmask = { 0xfefffefffefffeffULL };  /* To avoid shifting chroma to luma. */
    int i;

    for( i = width/16; i; --i ) {
        movq_m2r( *bot, mm0 );
        movq_m2r( *top, mm1 );
        movq_m2r( *(bot + 8), mm2 );
        movq_m2r( *(top + 8), mm3 );
        movq_m2r( *(bot + 16), mm4 );
        movq_m2r( *(top + 16), mm5 );
        movq_m2r( *(bot + 24), mm6 );
        movq_m2r( *(top + 24), mm7 );
        pand_m2r( shiftmask, mm0 );
        pand_m2r( shiftmask, mm1 );
        pand_m2r( shiftmask, mm2 );
        pand_m2r( shiftmask, mm3 );
        pand_m2r( shiftmask, mm4 );
        pand_m2r( shiftmask, mm5 );
        pand_m2r( shiftmask, mm6 );
        pand_m2r( shiftmask, mm7 );
        psrlw_i2r( 1, mm0 );
        psrlw_i2r( 1, mm1 );
        psrlw_i2r( 1, mm2 );
        psrlw_i2r( 1, mm3 );
        psrlw_i2r( 1, mm4 );
        psrlw_i2r( 1, mm5 );
        psrlw_i2r( 1, mm6 );
        psrlw_i2r( 1, mm7 );
        paddb_r2r( mm1, mm0 );
        paddb_r2r( mm3, mm2 );
        paddb_r2r( mm5, mm4 );
        paddb_r2r( mm7, mm6 );
        movq_r2m( mm0, *output );
        movq_r2m( mm2, *(output + 8) );
        movq_r2m( mm4, *(output + 16) );
        movq_r2m( mm6, *(output + 24) );
        output += 32;
        top += 32;
        bot += 32;
    }
    width = (width & 0xf);

    for( i = width/4; i; --i ) {
        movq_m2r( *bot, mm0 );
        movq_m2r( *top, mm1 );
        pand_m2r( shiftmask, mm0 );
        pand_m2r( shiftmask, mm1 );
        psrlw_i2r( 1, mm0 );
        psrlw_i2r( 1, mm1 );
        paddb_r2r( mm1, mm0 );
        movq_r2m( mm0, *output );
        output += 8;
        top += 8;
        bot += 8;
    }
    width = width & 0x7;

    /* Handle last few pixels. */
    for( i = width * 2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }

    emms();
}
#endif

#ifdef ARCH_X86
static void interpolate_packed422_scanline_mmxext( uint8_t *output, uint8_t *top,
                                                   uint8_t *bot, int width )
{
    int i;

    for( i = width/16; i; --i ) {
        movq_m2r( *bot, mm0 );
        movq_m2r( *top, mm1 );
        movq_m2r( *(bot + 8), mm2 );
        movq_m2r( *(top + 8), mm3 );
        movq_m2r( *(bot + 16), mm4 );
        movq_m2r( *(top + 16), mm5 );
        movq_m2r( *(bot + 24), mm6 );
        movq_m2r( *(top + 24), mm7 );
        pavgb_r2r( mm1, mm0 );
        pavgb_r2r( mm3, mm2 );
        pavgb_r2r( mm5, mm4 );
        pavgb_r2r( mm7, mm6 );
        movntq_r2m( mm0, *output );
        movntq_r2m( mm2, *(output + 8) );
        movntq_r2m( mm4, *(output + 16) );
        movntq_r2m( mm6, *(output + 24) );
        output += 32;
        top += 32;
        bot += 32;
    }
    width = (width & 0xf);

    for( i = width/4; i; --i ) {
        movq_m2r( *bot, mm0 );
        movq_m2r( *top, mm1 );
        pavgb_r2r( mm1, mm0 );
        movntq_r2m( mm0, *output );
        output += 8;
        top += 8;
        bot += 8;
    }
    width = width & 0x7;

    /* Handle last few pixels. */
    for( i = width * 2; i; --i ) {
        *output++ = ((*top++) + (*bot++)) >> 1;
    }

    sfence();
    emms();
}
#endif

/* linux kernel __memcpy (from: /include/asm/string.h) */
#ifdef ARCH_X86
static inline __attribute__ ((always_inline,const)) void small_memcpy( void *to, const void *from, size_t n )
{
    int d0, d1, d2;
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
}
#endif

static void fast_memcpy_c( void *dest, const void *src, size_t n )
{
    if( dest != src ) {
        memcpy( dest, src, n );
    }
}

#ifdef ARCH_X86
static void fast_memcpy_mmx( void *d, const void *s, size_t n )
{
    const uint8_t *src = s;
    uint8_t *dest = d;

    if( dest != src ) {
        while( n > 64 ) {
            movq_m2r( src[ 0 ], mm0 );
            movq_m2r( src[ 8 ], mm1 );
            movq_m2r( src[ 16 ], mm2 );
            movq_m2r( src[ 24 ], mm3 );
            movq_m2r( src[ 32 ], mm4 );
            movq_m2r( src[ 40 ], mm5 );
            movq_m2r( src[ 48 ], mm6 );
            movq_m2r( src[ 56 ], mm7 );
            movq_r2m( mm0, dest[ 0 ] );
            movq_r2m( mm1, dest[ 8 ] );
            movq_r2m( mm2, dest[ 16 ] );
            movq_r2m( mm3, dest[ 24 ] );
            movq_r2m( mm4, dest[ 32 ] );
            movq_r2m( mm5, dest[ 40 ] );
            movq_r2m( mm6, dest[ 48 ] );
            movq_r2m( mm7, dest[ 56 ] );
            dest += 64;
            src += 64;
            n -= 64;
        }

        while( n > 8 ) {
            movq_m2r( src[ 0 ], mm0 );
            movq_r2m( mm0, dest[ 0 ] );
            dest += 8;
            src += 8;
            n -= 8;
        }

        if( n ) small_memcpy( dest, src, n );

        emms();
    }
}
#endif

#ifdef ARCH_X86
static void fast_memcpy_mmxext( void *d, const void *s, size_t n )
{
    const uint8_t *src = s;
    uint8_t *dest = d;

    if( dest != src ) {
        while( n > 64 ) {
            movq_m2r( src[ 0 ], mm0 );
            movq_m2r( src[ 8 ], mm1 );
            movq_m2r( src[ 16 ], mm2 );
            movq_m2r( src[ 24 ], mm3 );
            movq_m2r( src[ 32 ], mm4 );
            movq_m2r( src[ 40 ], mm5 );
            movq_m2r( src[ 48 ], mm6 );
            movq_m2r( src[ 56 ], mm7 );
            movntq_r2m( mm0, dest[ 0 ] );
            movntq_r2m( mm1, dest[ 8 ] );
            movntq_r2m( mm2, dest[ 16 ] );
            movntq_r2m( mm3, dest[ 24 ] );
            movntq_r2m( mm4, dest[ 32 ] );
            movntq_r2m( mm5, dest[ 40 ] );
            movntq_r2m( mm6, dest[ 48 ] );
            movntq_r2m( mm7, dest[ 56 ] );
            dest += 64;
            src += 64;
            n -= 64;
        }

        while( n > 8 ) {
            movq_m2r( src[ 0 ], mm0 );
            movntq_r2m( mm0, dest[ 0 ] );
            dest += 8;
            src += 8;
            n -= 8;
        }

        if( n ) small_memcpy( dest, src, n );

        sfence();
        emms();
    }
}
#endif

static void blit_packed422_scanline_c( uint8_t *dest, const uint8_t *src, int width )
{
    fast_memcpy_c( dest, src, width*2 );
}

#ifdef ARCH_X86
static void blit_packed422_scanline_mmx( uint8_t *dest, const uint8_t *src, int width )
{
    fast_memcpy_mmx( dest, src, width*2 );
}
#endif

#ifdef ARCH_X86
static void blit_packed422_scanline_mmxext( uint8_t *dest, const uint8_t *src, int width )
{
    fast_memcpy_mmxext( dest, src, width*2 );
}
#endif


static uint32_t copy_accel;

void setup_copyfunctions( uint32_t accel )
{
    copy_accel = accel;

    interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    blit_packed422_scanline = blit_packed422_scanline_c;
    fast_memcpy = fast_memcpy_c;

#ifdef ARCH_X86
    if( speedy_accel & MM_ACCEL_X86_MMXEXT ) {
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
        blit_packed422_scanline = blit_packed422_scanline_mmxext;
        fast_memcpy = fast_memcpy_mmxext;
    } else if( speedy_accel & MM_ACCEL_X86_MMX ) {
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmx;
        blit_packed422_scanline = blit_packed422_scanline_mmx;
        fast_memcpy = fast_memcpy_mmx;
    }
#endif
}

uint32_t copyfunctions_get_accel( void )
{
    return copy_accel;
}

