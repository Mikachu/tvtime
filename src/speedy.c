/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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

/*
 * Uses code from:
 *
 *  linux/arch/i386/kernel/setup.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *
 * Found in linux 2.4.20.
 *
 * Also helped from code in 'cpuinfo.c' found in mplayer.
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <inttypes.h>
#include <unistd.h>
#include <ctype.h>
#include "config.h"
#include "attributes.h"
#include "mmx.h"
#include "mm_accel.h"
#include "speedtools.h"
#include "speedy.h"

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

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
void (*premultiply_packed4444_scanline)( unsigned char *output, unsigned char *input, int width );
void (*blend_packed422_scanline)( unsigned char *output, unsigned char *src1,
                                  unsigned char *src2, int width, int pos );
void (*filter_luma_121_packed422_inplace_scanline)( unsigned char *data, int width );
void (*filter_luma_14641_packed422_inplace_scanline)( unsigned char *data, int width );
unsigned int (*diff_factor_packed422_scanline)( unsigned char *cur, unsigned char *old, int width );
unsigned int (*comb_factor_packed422_scanline)( unsigned char *top, unsigned char *mid,
                                                unsigned char *bot, int width );
void (*kill_chroma_packed422_inplace_scanline)( unsigned char *data, int width );
void (*mirror_packed422_inplace_scanline)( unsigned char *data, int width );
void (*halfmirror_packed422_inplace_scanline)( unsigned char *data, int width );
void (*speedy_memcpy)( void *output, void *input, size_t size );
void (*diff_packed422_block8x8)( pulldown_metrics_t *m, unsigned char *old,
                                 unsigned char *new, int os, int ns );
void (*a8_subpix_blit_scanline)( unsigned char *output, unsigned char *input,
                                 int lasta, int startpos, int width );


static uint64_t speedy_time = 0;
static uint64_t cur_start_time, cur_end_time;

#define SPEEDY_START() \
    rdtscll( cur_start_time );

#define SPEEDY_END() \
    rdtscll( cur_end_time ); \
    speedy_time += cur_end_time - cur_start_time;

static inline __attribute__ ((always_inline,const)) int multiply_alpha( int a, int r )
{
    int temp;
    temp = (r * a) + 0x80;
    return ((temp + (temp >> 8)) >> 8);
}

static inline __attribute__ ((always_inline,const)) unsigned char clip255( int x )
{
    if( x > 255 ) {
        return 255;
    } else if( x < 0 ) {
        return 0;
    } else {
        return x;
    }
}

unsigned long CombJaggieThreshold = 73;

unsigned int comb_factor_packed422_scanline_mmx( unsigned char *top, unsigned char *mid,
                                                 unsigned char *bot, int width )
{
    const mmx_t qwYMask = { 0x00ff00ff00ff00ffULL };
    const mmx_t qwOnes = { 0x0001000100010001ULL };
    mmx_t qwThreshold;
    unsigned int temp1, temp2;

    SPEEDY_START();

    width /= 4;

    qwThreshold.uw[ 0 ] = CombJaggieThreshold;
    qwThreshold.uw[ 1 ] = CombJaggieThreshold;
    qwThreshold.uw[ 2 ] = CombJaggieThreshold;
    qwThreshold.uw[ 3 ] = CombJaggieThreshold;

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

    movd_r2m( mm7, temp1 );
    psrlq_i2r( 32, mm7 );
    movd_r2m( mm7, temp2 );
    temp1 += temp2;
    temp2 = temp1;
    temp1 >>= 16;
    temp1 += temp2 & 0xffff;

    emms();

    SPEEDY_END();

    return temp1;
}

static unsigned long BitShift = 6;

unsigned int diff_factor_packed422_scanline_c( unsigned char *cur, unsigned char *old, int width )
{
    unsigned int ret = 0;

    SPEEDY_START();

    width /= 4;

    while( width-- ) {
        unsigned int tmp1 = (cur[ 0 ] + cur[ 2 ] + cur[ 4 ] + cur[ 6 ])>>2;
        unsigned int tmp2 = (old[ 0 ] + old[ 2 ] + old[ 4 ] + old[ 6 ])>>2;
        tmp1  = (tmp1 - tmp2);
        tmp1 *= tmp1;
        tmp1 >>= BitShift;
        ret += tmp1;
        cur += 8;
        old += 8;
    }
    SPEEDY_END();

    return ret;
}

unsigned int diff_factor_packed422_scanline_test_c( unsigned char *cur, unsigned char *old, int width )
{
    unsigned int ret = 0;

    SPEEDY_START();

    width /= 16;

    while( width-- ) {
        unsigned int tmp1 = (cur[ 0 ] + cur[ 2 ] + cur[ 4 ] + cur[ 6 ])>>2;
        unsigned int tmp2 = (old[ 0 ] + old[ 2 ] + old[ 4 ] + old[ 6 ])>>2;
        tmp1  = (tmp1 - tmp2);
        tmp1 *= tmp1;
        tmp1 >>= BitShift;
        ret += tmp1;
        cur += (8*4);
        old += (8*4);
    }
    SPEEDY_END();

    return ret;
}


unsigned int diff_factor_packed422_scanline_mmx( unsigned char *cur, unsigned char *old, int width )
{
    const mmx_t qwYMask = { 0x00ff00ff00ff00ffULL };
    unsigned int temp1, temp2;

    SPEEDY_START();

    width /= 4;

    movq_m2r( qwYMask, mm1 );
    movd_m2r( BitShift, mm7 );
    pxor_r2r( mm0, mm0 );

    while( width-- ) {
        movq_m2r( *cur, mm4 );
        movq_m2r( *old, mm5 );

        pand_r2r( mm1, mm4 );
        pand_r2r( mm1, mm5 );

        psubw_r2r( mm5, mm4 );   /* mm4 = Y1 - Y2            */
        pmaddwd_r2r( mm4, mm4 ); /* mm4 = (Y1 - Y2)^2        */
        psrld_r2r( mm7, mm4 );   /* divide mm4 by 2^BitShift */
        paddd_r2r( mm4, mm0 );   /* keep total in mm0        */

        cur += 8;
        old += 8;
    }

    movd_r2m( mm0, temp1 );
    psrlq_i2r( 32, mm0 );
    movd_r2m( mm0, temp2 );
    temp1 += temp2;

    emms();

    SPEEDY_END();

    return temp1;
}

#define ABS(a) (((a) < 0)?-(a):(a))

void diff_packed422_block8x8_mmx( pulldown_metrics_t *m, unsigned char *old,
                                  unsigned char *new, int os, int ns )
{
    const mmx_t ymask = { 0x00ff00ff00ff00ffULL };
    short out[ 24 ]; /* Output buffer for the partial metrics from the mmx code. */
    unsigned char *outdata = (unsigned char *) out;
    unsigned char *oldp, *newp;
    int i;

    SPEEDY_START();

    pxor_r2r( mm4, mm4 );  // 4 even difference sums.
    pxor_r2r( mm5, mm5 );  // 4 odd difference sums.
    pxor_r2r( mm7, mm7 );  // zeros

    oldp = old; newp = new;
    for( i = 4; i; --i ) {
        // Even difference.
        movq_m2r( oldp[0], mm0 );
        movq_m2r( oldp[8], mm2 );
        pand_m2r( ymask, mm0 );
        pand_m2r( ymask, mm2 );
        oldp += os;

        movq_m2r( newp[0], mm1 );
        movq_m2r( newp[8], mm3 );
        pand_m2r( ymask, mm1 );
        pand_m2r( ymask, mm3 );
        newp += ns;

        movq_r2r( mm0, mm6 );
        psubusb_r2r( mm1, mm0 );
        psubusb_r2r( mm6, mm1 );
        movq_r2r( mm2, mm6 );
        psubusb_r2r( mm3, mm2 );
        psubusb_r2r( mm6, mm3 );

        paddw_r2r( mm0, mm4 );
        paddw_r2r( mm1, mm4 );
        paddw_r2r( mm2, mm4 );
        paddw_r2r( mm3, mm4 );

        // Odd difference.
        movq_m2r( oldp[0], mm0 );
        movq_m2r( oldp[8], mm2 );
        pand_m2r( ymask, mm0 );
        pand_m2r( ymask, mm2 );
        oldp += os;

        movq_m2r( newp[0], mm1 );
        movq_m2r( newp[8], mm3 );
        pand_m2r( ymask, mm1 );
        pand_m2r( ymask, mm3 );
        newp += ns;

        movq_r2r( mm0, mm6 );
        psubusb_r2r( mm1, mm0 );
        psubusb_r2r( mm6, mm1 );
        movq_r2r( mm2, mm6 );
        psubusb_r2r( mm3, mm2 );
        psubusb_r2r( mm6, mm3 );

        paddw_r2r( mm0, mm5 );
        paddw_r2r( mm1, mm5 );
        paddw_r2r( mm2, mm5 );
        paddw_r2r( mm3, mm5 );
    }
    movq_r2m( mm4, outdata[0] );
    movq_r2m( mm5, outdata[8] );

    m->e = out[0] + out[1] + out[2] + out[3];
    m->o = out[4] + out[5] + out[6] + out[7];
    m->d = m->e + m->o;

    pxor_r2r( mm4, mm4 );  // Past spacial noise.
    pxor_r2r( mm5, mm5 );  // Temporal noise.
    pxor_r2r( mm6, mm6 );  // Current spacial noise.

    // First loop to measure first four columns
    oldp = old; newp = new;
    for( i = 4; i; --i ) {
        movq_m2r( oldp[0], mm0 );
        movq_m2r( oldp[os], mm1 );
        pand_m2r( ymask, mm0 );
        pand_m2r( ymask, mm1 );
        oldp += (os*2);

        movq_m2r( newp[0], mm2 );
        movq_m2r( newp[ns], mm3 );
        pand_m2r( ymask, mm2 );
        pand_m2r( ymask, mm3 );
        newp += (ns*2);

        paddw_r2r( mm1, mm4 );
        paddw_r2r( mm1, mm5 );
        paddw_r2r( mm3, mm6 );
        psubw_r2r( mm0, mm4 );
        psubw_r2r( mm2, mm5 );
        psubw_r2r( mm2, mm6 );
    }
    movq_r2m( mm4, outdata[0] );
    movq_r2m( mm5, outdata[16] );
    movq_r2m( mm6, outdata[32] );

    pxor_r2r( mm4, mm4 );
    pxor_r2r( mm5, mm5 );
    pxor_r2r( mm6, mm6 );

    // Second loop for the last four columns
    oldp = old; newp = new;
    for( i = 4; i; --i ) {
        movq_m2r( oldp[8], mm0 );
        movq_m2r( oldp[os+8], mm1 );
        pand_m2r( ymask, mm0 );
        pand_m2r( ymask, mm1 );
        oldp += (os*2);

        movq_m2r( newp[8], mm2 );
        movq_m2r( newp[ns+8], mm3 );
        pand_m2r( ymask, mm2 );
        pand_m2r( ymask, mm3 );
        newp += (ns*2);

        paddw_r2r( mm1, mm4 );
        paddw_r2r( mm1, mm5 );
        paddw_r2r( mm3, mm6 );
        psubw_r2r( mm0, mm4 );
        psubw_r2r( mm2, mm5 );
        psubw_r2r( mm2, mm6 );
    }
    movq_r2m( mm4, outdata[8] );
    movq_r2m( mm5, outdata[24] );
    movq_r2m( mm6, outdata[40] );

    m->p = m->t = m->s = 0;
    for (i=0; i<8; i++) {
        // FIXME: move abs() into the mmx code!
        m->p += ABS(out[i]);
        m->t += ABS(out[8+i]);
        m->s += ABS(out[16+i]);
    }

    emms();

    SPEEDY_END();
}

void diff_packed422_block8x8_c( pulldown_metrics_t *m, unsigned char *old,
                                unsigned char *new, int os, int ns )
{
    int x, y, e=0, o=0, s=0, p=0, t=0;
    unsigned char *oldp, *newp;

    SPEEDY_START();
    m->s = m->p = m->t = 0;
    for (x = 8; x; x--) {
        oldp = old; old += 2;
        newp = new; new += 2;
        s = p = t = 0;
        for (y = 4; y; y--) {
            e += ABS(newp[0] - oldp[0]);
            o += ABS(newp[ns] - oldp[os]);
            s += newp[ns]-newp[0];
            p += oldp[os]-oldp[0];
            t += oldp[os]-newp[0];
            oldp += os<<1;
            newp += ns<<1;
        }
        m->s += ABS(s);
        m->p += ABS(p);
        m->t += ABS(t);
    }
    m->e = e;
    m->o = o;
    m->d = e+o;
    SPEEDY_END();
}



void cheap_packed444_to_packed422_scanline( unsigned char *output,
                                            unsigned char *input, int width )
{
    SPEEDY_START();
    width /= 2;
    while( width-- ) {
        output[ 0 ] = input[ 0 ];
        output[ 1 ] = input[ 1 ];
        output[ 2 ] = input[ 3 ];
        output[ 3 ] = input[ 2 ];
        output += 4;
        input += 6;
    }
    SPEEDY_END();
}

void cheap_packed422_to_packed444_scanline( unsigned char *output,
                                            unsigned char *input, int width )
{
    SPEEDY_START();
    width /= 2;
    while( width-- ) {
        output[ 0 ] = input[ 0 ];
        output[ 1 ] = input[ 1 ];
        output[ 2 ] = input[ 3 ];
        output[ 3 ] = input[ 2 ];
        output[ 4 ] = input[ 1 ];
        output[ 5 ] = input[ 3 ];
        output += 6;
        input += 4;
    }
    SPEEDY_END();
}

/**
 * For the middle pixels, the filter kernel is:
 *
 * [-1 3 -6 12 -24 80 80 -24 12 -6 3 -1]
 */
void packed422_to_packed444_rec601_scanline( unsigned char *dest, unsigned char *src, int width )
{
    int i;

    SPEEDY_START();
    /* Process two input pixels at a time.  Input is [Y'][Cb][Y'][Cr]. */
    for( i = 0; i < width / 2; i++ ) {
        dest[ (i*6) + 0 ] = src[ (i*4) + 0 ];
        dest[ (i*6) + 1 ] = src[ (i*4) + 1 ];
        dest[ (i*6) + 2 ] = src[ (i*4) + 3 ];

        dest[ (i*6) + 3 ] = src[ (i*4) + 2 ];
        if( i > (5*2) && i < ((width/2) - (6*2)) ) {
            dest[ (i*6) + 4 ] = clip255( ((  (80*(src[ (i*4) + 1 ] + src[ (i*4) + 5 ]))
                                           - (24*(src[ (i*4) - 3 ] + src[ (i*4) + 9 ]))
                                           + (12*(src[ (i*4) - 7 ] + src[ (i*4) + 13]))
                                           - ( 6*(src[ (i*4) - 11] + src[ (i*4) + 17]))
                                           + ( 3*(src[ (i*4) - 15] + src[ (i*4) + 21]))
                                           - (   (src[ (i*4) - 19] + src[ (i*4) + 25]))) + 64) >> 7 );
            dest[ (i*6) + 5 ] = clip255( ((  (80*(src[ (i*4) + 3 ] + src[ (i*4) + 7 ]))
                                           - (24*(src[ (i*4) - 1 ] + src[ (i*4) + 11]))
                                           + (12*(src[ (i*4) - 5 ] + src[ (i*4) + 15]))
                                           - ( 6*(src[ (i*4) - 9 ] + src[ (i*4) + 19]))
                                           + ( 3*(src[ (i*4) - 13] + src[ (i*4) + 23]))
                                           - (   (src[ (i*4) - 17] + src[ (i*4) + 27]))) + 64) >> 7 );
        } else if( i < ((width/2) - 1) ) {
            dest[ (i*6) + 4 ] = (src[ (i*4) + 1 ] + src[ (i*4) + 5 ] + 1) >> 1;
            dest[ (i*6) + 5 ] = (src[ (i*4) + 3 ] + src[ (i*4) + 7 ] + 1) >> 1;
        } else {
            dest[ (i*6) + 4 ] = src[ (i*4) + 1 ];
            dest[ (i*6) + 5 ] = src[ (i*4) + 3 ];
        }
    }
    SPEEDY_END();
}

void kill_chroma_packed422_inplace_scanline_mmx( unsigned char *data, int width )
{
    const mmx_t ymask = { 0x00ff00ff00ff00ffULL };
    const mmx_t nullchroma = { 0x8000800080008000ULL };

    SPEEDY_START();

    movq_m2r( ymask, mm7 );
    movq_m2r( nullchroma, mm6 );
    for(; width > 4; width -= 4 ) {
        movq_m2r( *data, mm0 );
        pand_r2r( mm7, mm0 );
        paddb_r2r( mm6, mm0 );
        movq_r2m( mm0, *data );
        data += 8;
    }
    emms();

    while( width-- ) {
        data[ 1 ] = 128;
        data += 2;
    }
    SPEEDY_END();
}

void kill_chroma_packed422_inplace_scanline_c( unsigned char *data, int width )
{
    SPEEDY_START();
    while( width-- ) {
        data[ 1 ] = 128;
        data += 2;
    }
    SPEEDY_END();
}

/*
// this duplicates alternate lines in alternate frames to highlight or mute
// the effects of chroma crawl. it is not a solution or proper filter. it's
// only for testing.
void testing_packed422_inplace_scanline_c( unsigned char *data, int width, int scanline )
{
    volatile static int topbottom = 0;
    static unsigned char scanbuffer[2048];

    SPEEDY_START();
    if( scanline <= 1 ) {
        topbottom = scanline;
        memcpy(scanbuffer, data, width*2);
    }
    if ( scanline < 10 ) {
        printf("scanline: %d %d\n", scanline, topbottom);
    }
    if ( ((scanline-topbottom)/2)%2 && scanline > 1 ) {
        memcpy(data, scanbuffer, width*2);
    } else {
        memcpy(scanbuffer, data, width*2);
    }
    SPEEDY_END();
}
*/

void mirror_packed422_inplace_scanline_c( unsigned char *data, int width )
{
    int x, tmp1, tmp2;
    int width2 = width*2;

    SPEEDY_START();
    for( x = 0; x < width; x += 2 ) {
        tmp1 = data[ x   ];
        tmp2 = data[ x+1 ];
        data[ x   ] = data[ width2 - x     ];
        data[ x+1 ] = data[ width2 - x + 1 ];
        data[ width2 - x     ] = tmp1;
        data[ width2 - x + 1 ] = tmp2;
    }
    SPEEDY_END();
}

void halfmirror_packed422_inplace_scanline_c( unsigned char *data, int width )
{
    int x;

    SPEEDY_START();
    for( x = 0; x < width; x += 2 ) {
        data[ width + x     ] = data[ width - x     ];
        data[ width + x + 1 ] = data[ width - x + 1 ];
    }
    SPEEDY_END();
}

void filter_luma_121_packed422_inplace_scanline_c( unsigned char *data, int width )
{
    int r1 = 0;
    int r2 = 0;

    SPEEDY_START();
    data += 2;
    width -= 1;
    while( width-- ) {
        int s1, s2;
        s1 = *data + r1; r1 = *data;
        s2 = s1    + r2; r2 = s1;
        *(data - 2) = s2 >> 2;
        data += 2;
    }
    SPEEDY_END();
}

void filter_luma_14641_packed422_inplace_scanline_c( unsigned char *data, int width )
{
    int r1 = 0;
    int r2 = 0;
    int r3 = 0;
    int r4 = 0;

    SPEEDY_START();
    width -= 4;
    data += 4;
    while( width-- ) {
        int s1, s2, s3, s4;
        s1 = *data + r1; r1 = *data;
        s2 = s1    + r2; r2 = s1;
        s3 = s2    + r3; r3 = s2;
        s4 = s3    + r4; r4 = s3;
        *(data - 4) = s4 >> 4;
        data += 2;
    }
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

    emms();

    SPEEDY_END();
}

void blit_colour_packed422_scanline_c( unsigned char *output,
                                       int width, int y, int cb, int cr )
{
    unsigned int colour = cr << 24 | y << 16 | cb << 8 | y;
    unsigned int *o = (unsigned int *) output;

    SPEEDY_START();

    for( width /= 2; width; --width ) {
        *o++ = colour;
    }

    SPEEDY_END();
}

void blit_colour_packed422_scanline_mmxext( unsigned char *output,
                                            int width, int y, int cb, int cr )
{
    unsigned int colour = cr << 24 | y << 16 | cb << 8 | y;
    int i;

    SPEEDY_START();

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( i = width / 16; i; --i ) {
        movntq_r2m( mm2, *output );
        movntq_r2m( mm2, *(output + 8) );
        movntq_r2m( mm2, *(output + 16) );
        movntq_r2m( mm2, *(output + 24) );
        output += 32;
    }
    width = (width & 0xf);

    for( i = width / 4; i; --i ) {
        movntq_r2m( mm2, *output );
        output += 8;
    }
    width = (width & 0x7);

    for( i = width / 2; i; --i ) {
        *((unsigned int *) output) = colour;
        output += 4;
    }

    if( width & 1 ) {
        *output = y;
        *(output + 1) = cb;
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

void blit_colour_packed4444_scanline_mmxext( unsigned char *output, int width,
                                             int alpha, int luma,
                                             int cb, int cr )
{
    unsigned int colour = (cr << 24) | (cb << 16) | (luma << 8) | alpha;
    int i;

    SPEEDY_START();

    movd_m2r( colour, mm1 );
    movd_m2r( colour, mm2 );
    psllq_i2r( 32, mm1 );
    por_r2r( mm1, mm2 );

    for( i = width / 8; i; --i ) {
        movntq_r2m( mm2, *output );
        movntq_r2m( mm2, *(output + 8) );
        movntq_r2m( mm2, *(output + 16) );
        movntq_r2m( mm2, *(output + 24) );
        output += 32;
    }
    width = (width & 0x7);

    for( i = width / 2; i; --i ) {
        movntq_r2m( mm2, *output );
        output += 8;
    }
    width = (width & 0x1);

    if( width ) {
        *((unsigned int *) output) = colour;
        output += 4;
    }

    emms();

    SPEEDY_END();
}


/**
 * Some memcpy code inspired by the xine code which originally came
 * from mplayer.
 */

/* linux kernel __memcpy (from: /include/asm/string.h) */
static inline void * __memcpy(void * to, const void * from, size_t n)
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
return (to);
}

static void temp_memcpy( void *to, void *from, size_t n )
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

/**
 * Scanlines are assumed to be approximately 2048 bytes.
 */
void blit_packed422_scanline_mmxext_billy( unsigned char *dest, const unsigned char *src, int width )
{
    SPEEDY_START();

    if( dest != src ) {
        int i;

        READ_PREFETCH_2048( src );

        for( i = width/32; i; i-- ) {
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
        }
        i = (width * 2) & 63;
        if( i ) __memcpy( dest, src, i );
        sfence();
        emms();
    }

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
    const mmx_t alpha2 = { 0x0000FFFF00000000ULL };
    const mmx_t alpha1 = { 0xFFFF0000FFFFFFFFULL };
    const mmx_t round  = { 0x0080008000800080ULL };
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
    READ_PREFETCH_2048( input );
    READ_PREFETCH_2048( foreground );

    movq_m2r( alpha, mm2 );
    pshufw_r2r( mm2, mm2, 0 );
    pxor_r2r( mm7, mm7 );

    for( i = width/2; i; i-- ) {
        int fg1 = *((unsigned int *) foreground);
        int fg2 = *(((unsigned int *) foreground)+1);

        if( fg1 || fg2 ) {
            /* mm1 = [ cr ][ y ][ cb ][ y ] */
            movd_m2r( *input, mm1 );
            punpcklbw_r2r( mm7, mm1 );

            movq_m2r( *foreground, mm3 );
            movq_r2r( mm3, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpckhbw_r2r( mm7, mm4 );
            /* mm3 and mm4 will be the appropriate colours, mm5 and mm6 for alpha. */

            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0 a ][ 0 a ][ 0 a ][ 0 a ] */
            pshufw_r2r( mm3, mm5, 0 );
            pshufw_r2r( mm4, mm6, 0 );
            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 3 cr ][ 0 a ][ 2 cb ][ 1 y ]  == 11001000 == 201 */
            pshufw_r2r( mm3, mm3, 201 );
            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0 a ][ 1 y ][ 0 a ][ 0 a ]  == 00010000 == 16 */
            pshufw_r2r( mm4, mm4, 16 );

            pand_m2r( alpha1, mm3 );
            pand_m2r( alpha2, mm4 );
            pand_m2r( alpha1, mm5 );
            pand_m2r( alpha2, mm6 );
            por_r2r( mm4, mm3 );
            por_r2r( mm6, mm5 );

            /* now, mm5 is af and mm1 is B.  Need to multiply them. */
            pmullw_r2r( mm1, mm5 );

            /* Multiply by appalpha. */
            pmullw_r2r( mm2, mm3 );
            paddw_m2r( round, mm3 );
            psrlw_i2r( 8, mm3 );
            /* Result is now B + F. */
            paddw_r2r( mm3, mm1 );

            /* Round up appropriately. */
            paddw_m2r( round, mm5 );

            /* mm6 contains our i>>8; */
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );

            /* Add mm6 back into mm5.  Now our result is in the high bytes. */
            paddw_r2r( mm6, mm5 );

            /* Shift down. */
            psrlw_i2r( 8, mm5 );

            /* Multiply by appalpha. */
            pmullw_r2r( mm2, mm5 );
            paddw_m2r( round, mm5 );
            psrlw_i2r( 8, mm5 );

            psubusw_r2r( mm5, mm1 );

            /* mm1 = [ B + F - af*B ] */
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
    const mmx_t alpha2 = { 0x0000FFFF00000000ULL };
    const mmx_t alpha1 = { 0xFFFF0000FFFFFFFFULL };
    const mmx_t round  = { 0x0080008000800080ULL };
    int i;

    SPEEDY_START();
    READ_PREFETCH_2048( input );
    READ_PREFETCH_2048( foreground );

    pxor_r2r( mm7, mm7 );
    for( i = width/2; i; i-- ) {
        int fg1 = *((unsigned int *) foreground);
        int fg2 = *(((unsigned int *) foreground)+1);

        if( (fg1 & 0xff) == 0xff && (fg2 & 0xff) == 0xff ) {
            movq_m2r( *foreground, mm3 );
            movq_r2r( mm3, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpckhbw_r2r( mm7, mm4 );
            /* mm3 and mm4 will be the appropriate colours, mm5 and mm6 for alpha. */
            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 3 cr ][ 0 a ][ 2 cb ][ 1 y ]  == 11001000 == 201 */
            pshufw_r2r( mm3, mm3, 201 );
            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0  a ][ 1 y ][ 0  a ][ 0 a ]  == 00010000 == 16 */
            pshufw_r2r( mm4, mm4, 16 );
            pand_m2r( alpha1, mm3 );
            pand_m2r( alpha2, mm4 );
            por_r2r( mm4, mm3 );
            /* mm1 = [ B + F - af*B ] */
            packuswb_r2r( mm3, mm3 );
            movd_r2m( mm3, *output );
        } else if( fg1 || fg2 ) {

            /* mm1 = [ cr ][ y ][ cb ][ y ] */
            movd_m2r( *input, mm1 );
            punpcklbw_r2r( mm7, mm1 );

            movq_m2r( *foreground, mm3 );
            movq_r2r( mm3, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpckhbw_r2r( mm7, mm4 );
            /* mm3 and mm4 will be the appropriate colours, mm5 and mm6 for alpha. */

            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0 a ][ 0 a ][ 0 a ][ 0 a ] */
            pshufw_r2r( mm3, mm5, 0 );
            pshufw_r2r( mm4, mm6, 0 );
            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 3 cr ][ 0 a ][ 2 cb ][ 1 y ]  == 11001000 == 201 */
            pshufw_r2r( mm3, mm3, 201 );
            /* [ 3 cr ][ 2 cb ][ 1 y ][ 0 a ]  -> [ 0  a ][ 1 y ][ 0  a ][ 0 a ]  == 00010000 == 16 */
            pshufw_r2r( mm4, mm4, 16 );

            pand_m2r( alpha1, mm3 );
            pand_m2r( alpha2, mm4 );
            pand_m2r( alpha1, mm5 );
            pand_m2r( alpha2, mm6 );
            por_r2r( mm4, mm3 );
            por_r2r( mm6, mm5 );

            /* now, mm5 is af and mm1 is B.  Need to multiply them. */
            pmullw_r2r( mm1, mm5 );

            /* Result is now B + F. */
            paddw_r2r( mm3, mm1 );

            /* Round up appropriately. */
            paddw_m2r( round, mm5 );

            /* mm6 contains our i>>8; */
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );

            /* Add mm6 back into mm5.  Now our result is in the high bytes. */
            paddw_r2r( mm6, mm5 );

            /* Shift down. */
            psrlw_i2r( 8, mm5 );

            psubusw_r2r( mm5, mm1 );

            /* mm1 = [ B + F - af*B ] */
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

/**
 * um... just need some scrap paper...
 *   D = (1 - alpha)*B + alpha*F
 *   D = (1 - a)*B + a*textluma
 *     = B - a*B + a*textluma
 *     = B + a*(textluma - B)
 *   Da = (1 - a)*b + a
 */
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
                                       |  (input[ 0 ] + multiply_alpha( a, 0xff - input[ 0 ] ));
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
    const mmx_t round = { 0x0080008000800080ULL };
    const mmx_t fullalpha = { 0x00000000000000ffULL };

    SPEEDY_START();

    movd_m2r( textluma, mm1 );
    movd_m2r( textcb, mm2 );
    movd_m2r( textcr, mm3 );

    /* mm1 = [  0 ][  0 ][ y ][ 0 ] == 11110011 == 243 */
    pshufw_r2r( mm1, mm1, 243 );
    /* mm2 = [  0 ][ cb ][ 0 ][ 0 ] == 11001111 == 207 */
    pshufw_r2r( mm2, mm2, 207 );
    /* mm3 = [ cr ][  0 ][ 0 ][ 0 ] == 00111111 == 63 */
    pshufw_r2r( mm3, mm3, 63 );

    /* mm1 = [ cr ][ cb ][ y ][ 0 ] */
    por_r2r( mm2, mm1 );
    por_r2r( mm3, mm1 );
    movq_r2r( mm1, mm0 );
    /* mm0 = [ cr ][ cb ][ y ][ 0xff ] */
    paddw_m2r( fullalpha, mm0 );

    pxor_r2r( mm7, mm7 );

    while( width-- ) {
        int a = *mask;

        if( a == 0xff ) {
            *((unsigned int *) output) = opaque;
        } else if( (input[ 0 ] == 0x00) ) {
            movd_m2r( a, mm2 );
            movq_r2r( mm2, mm3 );
            pshufw_r2r( mm2, mm2, 0 );
            movq_r2r( mm1, mm5 );
            pmullw_r2r( mm2, mm5 );
            paddw_m2r( round, mm5 );
            movq_r2r( mm5, mm6 );
            psrlw_i2r( 8, mm6 );
            paddw_r2r( mm6, mm5 );
            psrlw_i2r( 8, mm5 );
            por_r2r( mm3, mm5 );
            packuswb_r2r( mm5, mm5 );
            movd_r2m( mm5, *output );
        } else if( a ) {
            /* mm2 = [ a ][ a ][ a ][ a ] */
            movd_m2r( a, mm2 );
            pshufw_r2r( mm2, mm2, 0 );

            /* mm3 = [ cr ][ cb ][ y ][ 0xff ] */
            movq_r2r( mm0, mm3 );

            /*  mm4 = [ i_cr ][ i_cb ][ i_y ][ i_a ] */
            movd_m2r( *input, mm4 );
            punpcklbw_r2r( mm7, mm4 );
            movq_r2r( mm4, mm5 );

            /* Multiply alpha. */
            psubusw_r2r( mm4, mm3 );
            pmullw_r2r( mm2, mm3 );
            paddw_m2r( round, mm3 );
            movq_r2r( mm3, mm2 );
            psrlw_i2r( 8, mm2 );
            paddw_r2r( mm2, mm3 );
            psrlw_i2r( 8, mm3 );
            paddw_r2r( mm3, mm4 );

            /* Write result. */
            packuswb_r2r( mm4, mm4 );
            movd_r2m( mm4, *output );
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
           } else if( input[ 0 ] == 0x00 ) {
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

void premultiply_packed4444_scanline_c( unsigned char *output, unsigned char *input, int width )
{
    SPEEDY_START();

    while( width-- ) {
        unsigned int cur_a = input[ 0 ];

        *((unsigned int *) output) = (multiply_alpha( cur_a, input[ 3 ] ) << 24)
                                   | (multiply_alpha( cur_a, input[ 2 ] ) << 16)
                                   | (multiply_alpha( cur_a, input[ 1 ] ) << 8)
                                   | cur_a;

        output += 4;
        input += 4;
    }

    SPEEDY_END();
}

void premultiply_packed4444_scanline_mmxext( unsigned char *output, unsigned char *input, int width )
{
    const mmx_t round  = { 0x0080008000800080ULL };
    const mmx_t alpha  = { 0x00000000000000ffULL };
    const mmx_t noalp  = { 0xffffffffffff0000ULL };

    SPEEDY_START();

    pxor_r2r( mm7, mm7 );
    while( width-- ) {
        movd_m2r( *input, mm0 );
        punpcklbw_r2r( mm7, mm0 );

        movq_r2r( mm0, mm2 );
        pshufw_r2r( mm2, mm2, 0 );
        movq_r2r( mm2, mm4 );
        pand_m2r( alpha, mm4 );

        pmullw_r2r( mm2, mm0 );
        paddw_m2r( round, mm0 );

        movq_r2r( mm0, mm3 );
        psrlw_i2r( 8, mm3 );
        paddw_r2r( mm3, mm0 );
        psrlw_i2r( 8, mm0 );

        pand_m2r( noalp, mm0 );
        paddw_r2r( mm4, mm0 );

        packuswb_r2r( mm0, mm0 );
        movd_r2m( mm0, *output );

        output += 4;
        input += 4;
    }
    emms();

    SPEEDY_END();
}

void blend_packed422_scanline_c( unsigned char *output, unsigned char *src1,
                                 unsigned char *src2, int width, int pos )
{
    if( pos == 0 ) {
        blit_packed422_scanline( output, src1, width );
    } else if( pos == 256 ) {
        blit_packed422_scanline( output, src2, width );
    } else if( pos == 128 ) {
        interpolate_packed422_scanline( output, src1, src2, width );
    } else {
        width *= 2;
        while( width-- ) {
            *output++ = ( (*src1++ * ( 256 - pos )) + (*src2++ * pos) + 0x80 ) >> 8;
        }
    }
}

void blend_packed422_scanline_mmxext( unsigned char *output, unsigned char *src1,
                                      unsigned char *src2, int width, int pos )
{
    if( pos <= 0 ) {
        blit_packed422_scanline( output, src1, width );
    } else if( pos >= 256 ) {
        blit_packed422_scanline( output, src2, width );
    } else if( pos == 128 ) {
        interpolate_packed422_scanline( output, src1, src2, width );
    } else {
        const mmx_t all256 = { 0x0100010001000100ULL };
        const mmx_t round  = { 0x0080008000800080ULL };

        SPEEDY_START();

        movd_m2r( pos, mm0 );
        pshufw_r2r( mm0, mm0, 0 );
        movq_m2r( all256, mm1 );
        psubw_r2r( mm0, mm1 );
        pxor_r2r( mm7, mm7 );

        for( width /= 2; width; width-- ) {
            movd_m2r( *src1, mm3 );
            movd_m2r( *src2, mm4 );
            punpcklbw_r2r( mm7, mm3 );
            punpcklbw_r2r( mm7, mm4 );

            pmullw_r2r( mm1, mm3 );
            pmullw_r2r( mm0, mm4 );
            paddw_r2r( mm4, mm3 );
            paddw_m2r( round, mm3 );
            psrlw_i2r( 8, mm3 );

            packuswb_r2r( mm3, mm3 );
            movd_r2m( mm3, *output );

            output += 4;
            src1 += 4;
            src2 += 4;
        }
        emms();

        SPEEDY_END();
    }
}

void a8_subpix_blit_scanline_c( unsigned char *output, unsigned char *input,
                                int lasta, int startpos, int width )
{
    int pos = 0xffff - (startpos & 0xffff);
    int prev = lasta;
    int x;

    for( x = 0; x < width; x++ ) {
        output[ x ] = ( ( prev * pos ) + ( input[ x ] * ( 0xffff - pos ) ) ) >> 16;
        prev = input[ x ];
    }
}


static uint32_t speedy_accel;

void setup_speedy_calls( int verbose )
{
    speedy_accel = mm_accel();

    if( verbose ) {
        speedy_print_cpu_info();
    }

    interpolate_packed422_scanline = interpolate_packed422_scanline_c;
    blit_colour_packed422_scanline = blit_colour_packed422_scanline_c;
    blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_c;
    blit_packed422_scanline = blit_packed422_scanline_c;
    composite_packed4444_to_packed422_scanline = composite_packed4444_to_packed422_scanline_c;
    composite_packed4444_alpha_to_packed422_scanline = composite_packed4444_alpha_to_packed422_scanline_c;
    composite_alphamask_to_packed4444_scanline = composite_alphamask_to_packed4444_scanline_c;
    composite_alphamask_alpha_to_packed4444_scanline = composite_alphamask_alpha_to_packed4444_scanline_c;
    premultiply_packed4444_scanline = premultiply_packed4444_scanline_c;
    blend_packed422_scanline = blend_packed422_scanline_c;
    filter_luma_121_packed422_inplace_scanline = filter_luma_121_packed422_inplace_scanline_c;
    filter_luma_14641_packed422_inplace_scanline = filter_luma_14641_packed422_inplace_scanline_c;
    comb_factor_packed422_scanline = 0;
    diff_factor_packed422_scanline = diff_factor_packed422_scanline_c;
    kill_chroma_packed422_inplace_scanline = kill_chroma_packed422_inplace_scanline_c;
    mirror_packed422_inplace_scanline = mirror_packed422_inplace_scanline_c;
    halfmirror_packed422_inplace_scanline = halfmirror_packed422_inplace_scanline_c;
    speedy_memcpy = temp_memcpy;
    diff_packed422_block8x8 = diff_packed422_block8x8_c;
    a8_subpix_blit_scanline = a8_subpix_blit_scanline_c;

    if( speedy_accel & MM_ACCEL_X86_MMXEXT ) {
        if( verbose ) {
            fprintf( stderr, "speedycode: Using MMXEXT optimized functions.\n" );
        }
        interpolate_packed422_scanline = interpolate_packed422_scanline_mmxext;
        blit_colour_packed422_scanline = blit_colour_packed422_scanline_mmxext;
        blit_colour_packed4444_scanline = blit_colour_packed4444_scanline_mmxext;
        blit_packed422_scanline = blit_packed422_scanline_mmxext_billy;
        composite_packed4444_to_packed422_scanline = composite_packed4444_to_packed422_scanline_mmxext;
        composite_packed4444_alpha_to_packed422_scanline = composite_packed4444_alpha_to_packed422_scanline_mmxext;
        composite_alphamask_to_packed4444_scanline = composite_alphamask_to_packed4444_scanline_mmxext;
        premultiply_packed4444_scanline = premultiply_packed4444_scanline_mmxext;
        kill_chroma_packed422_inplace_scanline = kill_chroma_packed422_inplace_scanline_mmx;
        blend_packed422_scanline = blend_packed422_scanline_mmxext;
        diff_factor_packed422_scanline = diff_factor_packed422_scanline_mmx;
        comb_factor_packed422_scanline = comb_factor_packed422_scanline_mmx;
        diff_packed422_block8x8 = diff_packed422_block8x8_mmx;
    } else if( speedy_accel & MM_ACCEL_X86_MMX ) {
        if( verbose ) {
            fprintf( stderr, "speedycode: Using MMX optimized functions.\n" );
        }
        diff_factor_packed422_scanline = diff_factor_packed422_scanline_mmx;
        comb_factor_packed422_scanline = comb_factor_packed422_scanline_mmx;
        kill_chroma_packed422_inplace_scanline = kill_chroma_packed422_inplace_scanline_mmx;
        diff_packed422_block8x8 = diff_packed422_block8x8_mmx;
    } else {
        if( verbose ) {
            fprintf( stderr, "speedycode: No MMX or MMXEXT support detected, using C fallbacks.\n" );
        }
    }
}

int speedy_get_accel( void )
{
    return speedy_accel;
}

unsigned int speedy_get_cycles( void )
{
    return (unsigned int) speedy_time;
}

void speedy_reset_timer( void )
{
    speedy_time = 0;
}

double speedy_measure_cpu_mhz( void )
{
    uint64_t tsc_start, tsc_end;
    struct timeval tv_start, tv_end;
    int usec_delay;

    rdtscll( tsc_start );
    gettimeofday( &tv_start, 0 );
    usleep( 100000 );
    rdtscll( tsc_end );
    gettimeofday( &tv_end, 0 );

    usec_delay = 1000000 * (tv_end.tv_sec - tv_start.tv_sec) + (tv_end.tv_usec - tv_start.tv_usec);

    return (((double) (tsc_end - tsc_start)) / ((double) usec_delay));
}

typedef struct cpuid_regs {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
} cpuid_regs_t;

static cpuid_regs_t cpuid( int func ) {
    cpuid_regs_t regs;
#define CPUID ".byte 0x0f, 0xa2; "
    asm("movl %4,%%eax; " CPUID
        "movl %%eax,%0; movl %%ebx,%1; movl %%ecx,%2; movl %%edx,%3"
            : "=m" (regs.eax), "=m" (regs.ebx), "=m" (regs.ecx), "=m" (regs.edx)
            : "g" (func)
            : "%eax", "%ebx", "%ecx", "%edx");
    return regs;
}

#define X86_VENDOR_INTEL 0
#define X86_VENDOR_CYRIX 1
#define X86_VENDOR_AMD 2
#define X86_VENDOR_UMC 3
#define X86_VENDOR_NEXGEN 4
#define X86_VENDOR_CENTAUR 5
#define X86_VENDOR_RISE 6
#define X86_VENDOR_TRANSMETA 7
#define X86_VENDOR_NSC 8
#define X86_VENDOR_UNKNOWN 0xff

struct cpu_model_info {
    int vendor;
    int family;
    char *model_names[16];
};

/* Naming convention should be: <Name> [(<Codename>)] */
/* This table only is used unless init_<vendor>() below doesn't set it; */
/* in particular, if CPUID levels 0x80000002..4 are supported, this isn't used */
static struct cpu_model_info cpu_models[] = {
    { X86_VENDOR_INTEL,    4,
      { "486 DX-25/33", "486 DX-50", "486 SX", "486 DX/2", "486 SL", 
        "486 SX/2", NULL, "486 DX/2-WB", "486 DX/4", "486 DX/4-WB", NULL, 
        NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_INTEL,    5,
      { "Pentium 60/66 A-step", "Pentium 60/66", "Pentium 75 - 200",
        "OverDrive PODP5V83", "Pentium MMX", NULL, NULL,
        "Mobile Pentium 75 - 200", "Mobile Pentium MMX", NULL, NULL, NULL,
        NULL, NULL, NULL, NULL }},
    { X86_VENDOR_INTEL,    6,
      { "Pentium Pro A-step", "Pentium Pro", NULL, "Pentium II (Klamath)", 
        NULL, "Pentium II (Deschutes)", "Mobile Pentium II",
        "Pentium III (Katmai)", "Pentium III (Coppermine)", NULL,
        "Pentium III (Cascades)", NULL, NULL, NULL, NULL }},
    { X86_VENDOR_AMD,    4,
      { NULL, NULL, NULL, "486 DX/2", NULL, NULL, NULL, "486 DX/2-WB",
        "486 DX/4", "486 DX/4-WB", NULL, NULL, NULL, NULL, "Am5x86-WT",
        "Am5x86-WB" }},
    { X86_VENDOR_AMD,    5, /* Is this this really necessary?? */
      { "K5/SSA5", "K5",
        "K5", "K5", NULL, NULL,
        "K6", "K6", "K6-2",
        "K6-3", NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_AMD,    6, /* Is this this really necessary?? */
      { "Athlon", "Athlon",
        "Athlon", NULL, "Athlon", NULL,
        NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_UMC,    4,
      { NULL, "U5D", "U5S", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_NEXGEN,    5,
      { "Nx586", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL }},
    { X86_VENDOR_RISE,    5,
      { "iDragon", NULL, "iDragon", NULL, NULL, NULL, NULL,
        NULL, "iDragon II", "iDragon II", NULL, NULL, NULL, NULL, NULL, NULL }},
};

/* Look up CPU names by table lookup. */
static char *table_lookup_model( int vendor, int family, int model )
{
    struct cpu_model_info *info = cpu_models;
    int i;

    if( model >= 16 ) {
        return NULL; /* Range check */
    }

    for( i = 0; i < sizeof(cpu_models)/sizeof(struct cpu_model_info); i++ ) {
        if( info->vendor == vendor && info->family == family ) {
            return info->model_names[ model ];
        }
        info++;
    }

    return NULL; /* Not found */
}

int get_cpu_vendor( const char *idstr )
{
    if( !strcmp( idstr, "GenuineIntel" ) ) return X86_VENDOR_INTEL;
    if( !strcmp( idstr, "AuthenticAMD" ) ) return X86_VENDOR_AMD;
    if( !strcmp( idstr, "CyrixInstead" ) ) return X86_VENDOR_CYRIX;
    if( !strcmp( idstr, "Geode by NSC" ) ) return X86_VENDOR_NSC;
    if( !strcmp( idstr, "UMC UMC UMC " ) ) return X86_VENDOR_UMC;
    if( !strcmp( idstr, "CentaurHauls" ) ) return X86_VENDOR_CENTAUR;
    if( !strcmp( idstr, "NexGenDriven" ) ) return X86_VENDOR_NEXGEN;
    if( !strcmp( idstr, "RiseRiseRise" ) ) return X86_VENDOR_RISE;
    if( !strcmp( idstr, "GenuineTMx86" ) || !strcmp( idstr, "TransmetaCPU" ) ) return X86_VENDOR_TRANSMETA;
    return X86_VENDOR_UNKNOWN;
}


static void store32( char *d, unsigned int v )
{
    d[0] =  v        & 0xff;
    d[1] = (v >>  8) & 0xff;
    d[2] = (v >> 16) & 0xff;
    d[3] = (v >> 24) & 0xff;
}

void speedy_print_cpu_info( void )
{
    cpuid_regs_t regs, regs_ext;
    unsigned int max_cpuid;
    unsigned int max_ext_cpuid;
    unsigned int amd_flags;
    int family, model, stepping;
    char idstr[13];
    char *model_name;
    char processor_name[49];
    int i;

    regs = cpuid(0);
    max_cpuid = regs.eax;

    store32(idstr+0, regs.ebx);
    store32(idstr+4, regs.edx);
    store32(idstr+8, regs.ecx);
    idstr[12] = 0;

    regs = cpuid( 1 );
    family = (regs.eax >> 8) & 0xf;
    model = (regs.eax >> 4) & 0xf;
    stepping = regs.eax & 0xf;

    model_name = table_lookup_model( get_cpu_vendor( idstr ), family, model );

    regs_ext = cpuid((1<<31) + 0);
    max_ext_cpuid = regs_ext.eax;

    if (max_ext_cpuid >= (1<<31) + 1) {
        regs_ext = cpuid((1<<31) + 1);
        amd_flags = regs_ext.edx;

        if (max_ext_cpuid >= (1<<31) + 4) {
            for (i = 2; i <= 4; i++) {
                regs_ext = cpuid((1<<31) + i);
                store32(processor_name + (i-2)*16, regs_ext.eax);
                store32(processor_name + (i-2)*16 + 4, regs_ext.ebx);
                store32(processor_name + (i-2)*16 + 8, regs_ext.ecx);
                store32(processor_name + (i-2)*16 + 12, regs_ext.edx);
            }
            processor_name[48] = 0;
            model_name = processor_name;
        }
    } else {
        amd_flags = 0;
    }

    /* Is this dangerous? */
    while( isspace( *model_name ) ) model_name++;

    fprintf( stderr, "speedycode: CPU %s, family %d, model %d, stepping %d.\n", model_name, family, model, stepping );
    fprintf( stderr, "speedycode: CPU measured at %.3fMHz.\n", speedy_measure_cpu_mhz() );
}

