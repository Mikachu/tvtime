/**
 * Copyright (C) 2001, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <inttypes.h>
#include "diffcomp.h"

/**
 * This source file implements a simple form of information-preserving
 * differential coding, that is, predictive coding without quantization.
 * The basic idea is to predict the value of a pixel and encode the
 * error.
 *
 * I first got interested in the idea from the 'Huffyuv' codec by Ben
 * Rudiak-Gould and released under the GPL.  See
 * http://www.math.berkeley.edu/~benrg/huffyuv.html for more information
 * about 'Huffyuv'.
 *
 * There are a few ways of making a good prediction.  My reference is
 * 'Image and Video Compression for Multimedia Engineering' by Yun Q.
 * Shi and Huifang Sun.  One good reference they seemed to use was:
 * Haskell, B. G., Frame replenishment coding of television, in 'Image
 * Transmission Techniques', W.K. Pratt (Ed.), Academic Press, New York,
 * 1979.
 *
 * Here are some ideas:  Say we have the following samples:
 *
 *    a b c       m n o
 *    d e f       p q r
 *    g h i       s t u
 *    j k l       v w x
 *      |           +----current frame
 *      previous frame
 *
 * So, say we're trying to encode sample t.  There are a few easy
 * prediction schemes.  The few I played with were:
 *
 * Element difference:        E = t - s
 * Field-based interpolation: E = t - ((n + s) / 2)
 *                         or E = t - ((n + s + m) / 3)
 * Median (field based):      E = med(n, s, (n + s) / 2)
 *                         or E = med(n, s, n + s - m)
 * Frame difference:          E = (t - s) - (h - g)
 * (E = error)
 *
 * Some other neat suggested ones were:
 * Field difference:
 *      E = (t - ((q + w) / 2)) - (h - ((e + k) / 2))
 * Line difference:
 *      E = (t - n) - (h - b)
 *
 * Some of the above names are from Shi and Sun.  Also, some were based
 * on Ben Rudiak-Gould's page.  Basically though, what I found was that
 * the frame difference idea of using the previous frame did not produce
 * a code with much less entropy (<.1 bit).  As well, using anything
 * from 2 scanlines above is another bad idea.  This leaves only the
 * element difference, that is, using the previous pixel value, as the
 * best option.
 *
 * I seem to get an average compression of about 55%-65% of the original
 * uncompressed size.
 *
 * The code below is optimized for codewords < 16 bits.  I first
 * generated a code with a max wordlength of 12 bits based on
 * 'Transformers: The Movie' VHS sampled at 352x480.  I generated the
 * new code below from 'Austin Powers' VHS, and I think I like it
 * better.  Sampling TV at 352x480 gave me longer codes without much
 * better entropy values, so I'm keeping this code for now.
 *
 * I use the source code found at
 * http://www.cs.mu.oz.au/~alistair/abstracts/wads95.html
 * to generate the word lengths for my code.  The code I build at
 * runtime based on these lenghts.  I generate a 'canonical huffman
 * code'.  I got help for coding this (and optimizing it) from the
 * website 'Practical Huffman Coding' at
 * http://www.compressconsult.com/huffman/
 *
 * You'll note that I use the same compression table for both the luma
 * and the chroma.  I know, I could probably do even better by building
 * different codes for each, but I didn't.  Since I'm only sampling at
 * 4:2:0 rates in my app, I'm ok with this situation, for now.
 *
 * - Billy Biggs <vektor@dumbterm.net>
 */

/**
 * Updates:
 * 
 * Sat Apr 12 16:19:03 ADT 2003
 * - Updated to have a first-8-bits lookup for decoding speed.  Works well.
 * - Updated to include a YUY2 compressor and decompressor.
 * - Note the code generation source we used is now here:
 *   http://www.cs.mu.oz.au/~alistair/inplace.c
 */


/**
 * Everything I do needs to hold a number of bits and a value.  Kinda
 * cool, eh?
 */
typedef struct mytuple_s {
    int numbits;
    unsigned int value;
} mytuple_t;

/**
 * This table holds the number of bits used by each value in the huffman
 * code.  Since we're using canonical huffman codes, this is sufficient
 * to uniquely describe the code.  The max size variable holds the
 * maximum length of any codeword.
 */
static int maxsize = 15;
static mytuple_t codesizetable[] = {
    { 15, 120 }, { 15, 121 }, { 15, 122 }, { 15, 118 }, { 15, 124 },
    { 15, 119 }, { 15, 123 }, { 15, 125 }, { 15, 117 }, { 15, 128 },
    { 15, 127 }, { 15, 116 }, { 15, 126 }, { 15, 129 }, { 15, 130 },
    { 15, 114 }, { 15, 115 }, { 15, 131 }, { 15, 132 }, { 15, 133 },
    { 15, 134 }, { 15, 135 }, { 15, 113 }, { 15, 112 }, { 15, 111 },
    { 15, 137 }, { 15, 136 }, { 15, 110 },
    { 14, 139 }, { 14, 138 }, { 14, 109 }, { 14, 140 }, { 14, 108 },
    { 14, 141 }, { 14, 107 }, { 14, 142 }, { 14, 143 }, { 14, 106 },
    { 14, 144 }, { 14, 145 }, { 14, 105 }, { 14, 104 }, { 14, 146 },
    { 14, 103 }, { 14, 147 }, { 14, 148 }, { 14, 102 }, { 14, 149 },
    { 14, 150 }, { 14, 101 }, { 14, 151 }, { 14, 100 }, { 14, 152 },
    { 14,  99 }, { 14, 153 }, { 14, 154 }, { 14,  98 }, { 14,  97 },
    { 14, 155 }, { 14, 156 }, { 14,  96 }, { 14, 157 }, { 14,  95 },
    { 14,  94 }, { 14, 158 }, { 14, 159 }, { 14,  93 }, { 14, 160 },
    { 14,  92 }, { 14, 161 }, { 14,  91 }, { 14, 162 }, { 14, 163 },
    { 14,  90 }, { 14, 164 }, { 14,  89 }, { 14, 165 }, { 14,  88 },
    { 13, 166 }, { 13,  87 }, { 13, 167 }, { 13,  86 }, { 13, 168 },
    { 13,  85 }, { 13, 169 }, { 13,  84 }, { 13, 170 }, { 13,  83 },
    { 13, 171 }, { 13,  82 }, { 13, 172 }, { 13,  81 }, { 13, 173 },
    { 13,  80 }, { 13, 174 }, { 13, 175 }, { 13,  79 }, { 13, 176 },
    { 13,  78 }, { 13, 177 }, { 13,  77 }, { 13, 178 }, { 13,  76 },
    { 13,  75 }, { 13, 179 }, { 13, 180 }, { 13,  74 }, { 13, 181 },
    { 13,  73 }, { 13, 182 }, { 13,  72 }, { 13, 183 }, { 13,  71 },
    { 13, 184 }, { 13,  70 }, { 13, 185 }, { 13, 186 }, { 13,  69 },
    { 12,  68 }, { 12, 187 }, { 12,  67 }, { 12, 188 }, { 12,  66 },
    { 12, 189 }, { 12,  65 }, { 12, 190 }, { 12,  64 }, { 12, 191 },
    { 12,  63 }, { 12, 192 }, { 12,  62 }, { 12, 193 }, { 12, 194 },
    { 12,  61 }, { 12, 195 }, { 12,  60 }, { 12, 196 }, { 12,  59 },
    { 12, 197 }, { 12,  58 }, { 12, 198 }, { 12,  57 }, { 12, 199 },
    { 12,  56 }, { 12, 200 }, { 12,  55 }, { 12, 201 }, { 12,  54 },
    { 12, 202 }, { 12,  53 }, { 12, 203 }, { 12,  52 }, { 12, 204 },
    { 12,  51 }, { 12, 205 }, { 12,  50 },
    { 11, 206 }, { 11,  49 }, { 11, 207 }, { 11,  48 }, { 11, 208 },
    { 11,  47 }, { 11, 209 }, { 11,  46 }, { 11, 210 }, { 11,  45 },
    { 11, 211 }, { 11, 212 }, { 11,  44 }, { 11, 213 }, { 11,  43 },
    { 11, 214 }, { 11,  42 }, { 11, 215 }, { 11,  41 }, { 11, 216 },
    { 11,  40 }, { 11, 217 }, { 11,  39 }, { 11, 218 }, { 11,  38 },
    { 11, 219 }, { 11,  37 }, { 11, 220 }, { 11,  36 }, { 11, 221 },
    { 11,  35 },
    { 10, 222 }, { 10,  34 }, { 10, 223 }, { 10,  33 }, { 10, 224 },
    { 10,  32 }, { 10, 225 }, { 10,  31 }, { 10, 226 }, { 10,  30 },
    { 10, 227 }, { 10,  29 }, { 10, 228 }, { 10,  28 }, { 10,  27 },
    { 10, 229 }, { 10,  26 }, { 10, 230 }, { 10,  25 }, { 10, 231 },
    { 10,  24 }, { 10, 232 },
    {  9,  23 }, {  9, 233 }, {  9,  22 }, {  9, 234 }, {  9,  21 },
    {  9, 235 }, {  9,  20 }, {  9, 236 }, {  9,  19 }, {  9, 237 },
    {  9,  18 }, {  9, 238 }, {  9,  17 }, {  9, 239 }, {  9,  16 },
    {  9, 240 },
    {  8,  15 }, {  8, 241 }, {  8,  14 }, {  8, 242 }, {  8,  13 },
    {  8, 243 }, {  8, 244 }, {  8,  12 }, {  8, 245 }, {  8,  11 },
    {  7, 246 }, {  7,  10 }, {  7, 247 }, {  7,   9 }, {  7, 248 },
    {  7,   8 }, {  7, 249 }, {  7,   7 },
    {  6, 250 }, {  6,   6 }, {  6, 251 }, {  6,   5 },
    {  5, 252 }, {  5,   4 }, {  5, 253 }, {  5,   3 },
    {  4, 254 }, {  4,   2 },
    {  3, 255 }, {  3,   1 },
    {  2,   0 },
};

/**
 * This holds the code table used for encoding.
 */
static mytuple_t codetable[ 256 ];
static int codesgenerated = 0;

/**
 * Here is where I generate the huffman codewords based on the table
 * above.  Real simple.  See the website on 'Practical Huffman Coding'.
 */
static void generate_codes( void )
{
    mytuple_t *curtuple = codesizetable;
    int lastsize = maxsize;
    int curcode = 0;

    while( curtuple->numbits > 0 ) {

        /**
         * Basically, I just go through each value in the codesizetable
         * and assign a codeword.
         */
        if( curtuple->numbits != lastsize ) {
            for(; lastsize > curtuple->numbits; lastsize-- ) {
                curcode >>= 1;
            }
        }
        codetable[ curtuple->value ].numbits = curtuple->numbits;
        codetable[ curtuple->value ].value = curcode;
        curcode++;
        curtuple++;
    }

    codesgenerated = 1;
}

/**
 * This is the table I use for decompression.  Basically, I just make a
 * table of every possible 16bit value I could see in the input and what
 * code word is first in those 16 bits.  Not very fast.
 */
static mytuple_t decompressiontable[ 65536 ];
static int table_generated = 0;
static mytuple_t first_decompressiontable[ 256 ];
static int first_cutoff = 0;

static void generate_decompression_table( void )
{
    int num_hits_first[ 256 ];
    int i;

    if( !codesgenerated ) generate_codes();

    /**
     * For sanity checking our table.  Only bother if you change the
     * code
     */
    /*
    for( i = 0; i < 65536; i++ ) {
        decompressiontable[ i ].numbits = -1;
        decompressiontable[ i ].value = -1;
    }
    */
    for( i = 0; i < 256; i++ ) {
        first_decompressiontable[ i ].numbits = -1;
        num_hits_first[ i ] = 0;
    }

    /**
     * We need to know the cut off point in our table where our first lookup table
     * will fail.
     */
    for( i = 0; i < 256; i++ ) {
        mytuple_t *codeword = &(codetable[ i ]);
        int startword = codeword->value << ( 16 - codeword->numbits );
        int firstbyte = (startword >> 8) & 0xff;
        num_hits_first[ firstbyte ]++;
    }

    for( i = 0; i < 256; i++ ) {
        if( num_hits_first[ i ] < 2 ) {
            first_cutoff = i - 1;
            break;
        }
    }
    fprintf( stderr, "first cutoff: %d\n", first_cutoff );

    /**
     * So, for each codeword, we just fill up every entry which
     * corresponds to it in the decode table.  Pretty simple.
     */
    for( i = 0; i < 256; i++ ) {
        mytuple_t *codeword = &(codetable[ i ]);
        int startword = codeword->value << ( 16 - codeword->numbits );
        int nextword = startword;
        int mask = 0;
        int firstbyte = (startword >> 8) & 0xff;
        int j;

        for( j = 0; j < codeword->numbits; j++ ) {
            mask = ( mask >> 1 ) | 0x8000;
        }

        if( firstbyte > first_cutoff ) {
            int temp = firstbyte;
            while( ( temp & (mask >> 8) ) == firstbyte ) {
                first_decompressiontable[ temp & 0xff ].numbits = codeword->numbits;
                first_decompressiontable[ temp & 0xff ].value = i;
                temp++;
            }
            first_decompressiontable[ firstbyte ].numbits = codeword->numbits;
            first_decompressiontable[ firstbyte ].value = i;
        }

        while( ( nextword & mask ) == startword ) {
            decompressiontable[ nextword & 0xffff ].numbits = codeword->numbits;
            decompressiontable[ nextword & 0xffff ].value = i;
            nextword++;
        }
    }

    /* Sanity check our table. */
    /*
    for( i = 0; i < 65536; i++ ) {
        if( decompressiontable[ i ].numbits == -1 ) {
            fprintf( stderr, "diffcomp: Didn't fill value %d\n", i );
        }
    }
    */

    table_generated = 1;
}

static inline __attribute__ ((always_inline,const)) mytuple_t *decompress_next_byte( const uint32_t curstuff )
{
    /**
     * Since we have that ereet table, all we need do is look up
     * which value we just read!
     */
    int firstbyte = (curstuff >> 24) & 0xff;
    if( firstbyte > first_cutoff ) {
        return &(first_decompressiontable[ firstbyte ]);
    } else {
        return &(decompressiontable[ ( curstuff >> 16 ) & 0xffff ]);
    }
}


/**
 * Code for keeping statistics useful for building a new code and
 * analyzing the entropy of our codes.  Currently unused.
 */
FILE *statsfile = 0;
static unsigned int frequency[ 256 ];
static unsigned int values[ 256 ];

static void initialize_frequencies( void )
{
    int i;
    for( i = 0; i < 256; i++ ) frequency[ i ] = 0;
    for( i = 0; i < 256; i++ ) values[ i ] = i;
}

/**
 * This is a bad sort, but cheap to code, and it's only 256 values.
 */
static void sort_frequencies( void )
{
    int i, j;
    for( i = 0; i < 256; i++ ) {
        for( j = i + 1; j < 256; j++ ) {
            if( frequency[ j ] < frequency[ i ] ) {
                int tmp = frequency[ j ];
                frequency[ j ] = frequency[ i ];
                frequency[ i ] = tmp;
                tmp = values[ j ];
                values[ j ] = values[ i ];
                values[ i ] = tmp;
            }
        }
    }
}

/**
 * Prints out the frequencies in a form suitable for the app I'm using
 * to generate my code lengths.
 */
static void print_frequencies( void )
{
    int i;
    fprintf( statsfile, "256\n" );
    for( i = 0; i < 256; i++ ) {
        fprintf( statsfile, "%d %d\n", frequency[ i ], values[ i ] );
    }
}

int diffcomp_compress_plane( unsigned char *dst, unsigned char *src,
                             int width, int height )
{
    unsigned char lastcode = 0;
    uint32_t curout = 0;
    int bitsused = 0;
    int outsize = 0;
    int i;

    if( !codesgenerated ) generate_codes();

    for( i = 0; i < width * height; i++ ) {
        unsigned char diff = ( *src - lastcode ) & 0xff;
        mytuple_t *nextout = &(codetable[ diff ]);

        /**
         * If we were trying to measure the input one would
         * do:
         *   frequency[ diff ]++;
         * around here.
         */

        /**
         * If we're going to overflow, write out some bytes.
         */
        while( ( nextout->numbits + bitsused ) > 32 ) {
            bitsused -= 8;
            *dst++ = ( curout >> bitsused );
            outsize++;
        }

        /**
         * Shift over enough space for our codeword, then or in the
         * value of the codeword.
         */
        curout = curout << nextout->numbits;
        curout = curout | nextout->value;
        bitsused += nextout->numbits;
        lastcode = *src;
        src++;
    }

    /* Write what's left, and write some buffer room for safety. */
    curout = curout << ( 32 - bitsused );
    *dst++ = ( curout >> 24 ) & 0xff;
    *dst++ = ( curout >> 16 ) & 0xff;
    *dst++ = ( curout >>  8 ) & 0xff;
    *dst++ = ( curout       ) & 0xff;
    *dst++ = 0;
    *dst++ = 0;
    *dst++ = 0;
    *dst++ = 0;
    outsize += 8;
    return outsize;
}

void diffcomp_decompress_plane( unsigned char *dst, unsigned char *src,
                                int width, int height )
{
    unsigned char lastvalue = 0;
    uint32_t curstuff = 0;
    int bitsused = 0;
    int i;

    if( !table_generated ) generate_decompression_table();

    for( i = 0; i < width * height; i++ ) {
        mytuple_t *decodedword;

        /**
         * Any time we have less than 16 bits, load up some more data
         * from the compressed image.
         */
        if( bitsused < 16 ) {
            unsigned int coolstuff = *src << 8 | *(src + 1);
            curstuff = curstuff | ( coolstuff << ( 16 - bitsused ) );
            bitsused += 16;
            src += 2;
        }

        /**
         * Since we have that ereet table, all we need do is look up
         * which value we just read!
         */
        decodedword = decompress_next_byte( curstuff );
        curstuff = curstuff << decodedword->numbits;
        bitsused -= decodedword->numbits;
        *dst = ( lastvalue + decodedword->value ) & 0xff;
        lastvalue = *dst;
        dst++;
    }
}

void compress_next_byte( unsigned char **dst, uint32_t *curout, int *bitsused, int *outsize,
                         unsigned char diff )
{
    mytuple_t *nextout = &(codetable[ diff ]);

    /**
     * If we're going to overflow, write out some bytes.
     */
    while( ( nextout->numbits + *bitsused ) > 32 ) {
        *bitsused -= 8;
        **dst = ( *curout >> *bitsused );
        *dst += 1;
        *outsize = *outsize + 1;
    }

    /**
     * Shift over enough space for our codeword, then or in the
     * value of the codeword.
     */
    *curout = *curout << nextout->numbits;
    *curout = *curout | nextout->value;
    *bitsused += nextout->numbits;
}

int diffcomp_compress_packed422( unsigned char *dst, unsigned char *src,
                                 int width, int height )
{
    unsigned char lastcode_y = 0;
    unsigned char lastcode_cb = 0;
    unsigned char lastcode_cr = 0;
    uint32_t curout = 0;
    int bitsused = 0;
    int outsize = 0;
    int i;

    if( !codesgenerated ) generate_codes();

    for( i = 0; i < width * height / 2; i++ ) {
        compress_next_byte( &dst, &curout, &bitsused, &outsize, (*src - lastcode_y) & 0xff );
        lastcode_y = *src;
        src++;

        compress_next_byte( &dst, &curout, &bitsused, &outsize, (*src - lastcode_cb) & 0xff );
        lastcode_cb = *src;
        src++;

        compress_next_byte( &dst, &curout, &bitsused, &outsize, (*src - lastcode_y) & 0xff );
        lastcode_y = *src;
        src++;

        compress_next_byte( &dst, &curout, &bitsused, &outsize, (*src - lastcode_cr) & 0xff );
        lastcode_cr = *src;
        src++;
    }

    /* Write what's left, and write some buffer room for safety. */
    curout = curout << ( 32 - bitsused );
    *dst++ = ( curout >> 24 ) & 0xff;
    *dst++ = ( curout >> 16 ) & 0xff;
    *dst++ = ( curout >>  8 ) & 0xff;
    *dst++ = ( curout       ) & 0xff;
    *dst++ = 0;
    *dst++ = 0;
    *dst++ = 0;
    *dst++ = 0;
    outsize += 8;
    return outsize;
}

void diffcomp_decompress_packed422( unsigned char *dst, unsigned char *src,
                                    int width, int height )
{
    unsigned char lastvalue_y = 0;
    unsigned char lastvalue_cb = 0;
    unsigned char lastvalue_cr = 0;
    uint32_t curstuff = 0;
    int bitsused = 0;
    int i;

    if( !table_generated ) generate_decompression_table();

    for( i = 0; i < width * height / 2; i++ ) {
        mytuple_t *decodedword;

        if( bitsused < 16 ) {
            unsigned int coolstuff = (*src << 8) | *(src + 1);
            curstuff = curstuff | ( coolstuff << ( 16 - bitsused ) );
            bitsused += 16;
            src += 2;
        }
        decodedword = decompress_next_byte( curstuff );
        curstuff = curstuff << decodedword->numbits;
        bitsused -= decodedword->numbits;
        *dst = ( lastvalue_y + decodedword->value ) & 0xff;
        lastvalue_y = *dst;
        dst++;

        if( bitsused < 16 ) {
            unsigned int coolstuff = (*src << 8) | *(src + 1);
            curstuff = curstuff | ( coolstuff << ( 16 - bitsused ) );
            bitsused += 16;
            src += 2;
        }
        decodedword = decompress_next_byte( curstuff );
        curstuff = curstuff << decodedword->numbits;
        bitsused -= decodedword->numbits;
        *dst = ( lastvalue_cb + decodedword->value ) & 0xff;
        lastvalue_cb = *dst;
        dst++;

        if( bitsused < 16 ) {
            unsigned int coolstuff = (*src << 8) | *(src + 1);
            curstuff = curstuff | ( coolstuff << ( 16 - bitsused ) );
            bitsused += 16;
            src += 2;
        }
        decodedword = decompress_next_byte( curstuff );
        curstuff = curstuff << decodedword->numbits;
        bitsused -= decodedword->numbits;
        *dst = ( lastvalue_y + decodedword->value ) & 0xff;
        lastvalue_y = *dst;
        dst++;

        if( bitsused < 16 ) {
            unsigned int coolstuff = (*src << 8) | *(src + 1);
            curstuff = curstuff | ( coolstuff << ( 16 - bitsused ) );
            bitsused += 16;
            src += 2;
        }
        decodedword = decompress_next_byte( curstuff );
        curstuff = curstuff << decodedword->numbits;
        bitsused -= decodedword->numbits;
        *dst = ( lastvalue_cr + decodedword->value ) & 0xff;
        lastvalue_cr = *dst;
        dst++;
    }
}

