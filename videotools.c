
#include <math.h>
#include <stdlib.h>
#include "videotools.h"

/**
 * Define this to 1 not fade out the phosphors on the opposite field.
 * Try out 2 to have it faded.
 */
#define BRIGHT_FACTOR 1

void build_plane( unsigned char *output, unsigned char *field,
                  int fieldstride, int width, int height,
                  int bottom_field, int chroma )
{
    int i, j;

    if( bottom_field ) {
        if( chroma ) {
            memset( output, 128, width );
        } else {
            memset( output, 16, width );
        }
        output += width;

        memcpy( output, field, width );
        output += width;
    }

    for( i = 0; i < (height / 2) - 1; i++ ) {
        unsigned char *c;

        if( !bottom_field ) {
            memcpy( output, field, width );
            output += width;
        }

        c = field;
        if( chroma ) {
            for( j = 0; j < width; j++ ) {
                *output = ((*c + *(c+fieldstride) - 256) >> BRIGHT_FACTOR)
                          + 128;
                output++;
                c++;
            }
        } else {
            for( j = 0; j < width; j++ ) {
                *output = (*c + *(c+fieldstride)) >> BRIGHT_FACTOR;
                output++;
                c++;
            }
        }
        field += fieldstride;

        if( bottom_field ) {
            memcpy( output, field, width );
            output += width;
        }
    }

    if( !bottom_field ) {
        memcpy( output, field, width );
        output += width;

        if( chroma ) {
            memset( output, 128, width );
        } else {
            memset( output, 16, width );
        }
    }
}

static inline void clear_scanline_packed_422( unsigned char *output, int size )
{
    static int clear = 128 << 24 | 16 << 16 | 128 << 8 | 16;
    unsigned int *o = (unsigned int *) output;

    for( size /= 4; size; --size ) {
        *o++ = clear;
    }
}

static inline void copy_scanline_packed_422( unsigned char *output, unsigned char *luma,
                                             unsigned char *cb, unsigned char *cr, int width )
{
    unsigned int *o = (unsigned int *) output;

    for( width /= 2; width; --width ) {
        *o = (*cr << 24) | (*(luma + 1) << 16) | (*cb << 8) | (*luma);
        o++;
        luma += 2;
        cb++;
        cr++;
    }
}

static inline void interpolate_scanline_packed_422( unsigned char *output,
                                                    unsigned char *luma, unsigned char *cb,
                                                    unsigned char *cr, int lstride, int cstride, int width )
{
    for( width /= 2; width; --width ) {
        *output = (*luma + *(luma+lstride)) >> BRIGHT_FACTOR;
        output++;
        luma++;

        *output = ((*cb + *(cb+cstride) - 256) >> BRIGHT_FACTOR) + 128;
        output++;
        cb++;

        *output = (*luma + *(luma+lstride)) >> BRIGHT_FACTOR;
        output++;
        luma++;

        *output = ((*cr + *(cr+cstride) - 256) >> BRIGHT_FACTOR) + 128;
        output++;
        cr++;
    }
}

unsigned char *temp_scanline_data = 0;

void build_packed_422_frame( unsigned char *output, unsigned char *fieldluma,
                             unsigned char *fieldcb, unsigned char *fieldcr,
                             int bottom_field, int lstride, int cstride,
                             int width, int height, video_correction_t *vc )
{
    int i;

    if( !temp_scanline_data ) {
        temp_scanline_data = (unsigned char *) malloc( 720 * 2 );
    }

    if( bottom_field ) {
        /* Clear a scanline. */
        clear_scanline_packed_422( output, width * 2 );
        output += width * 2;

        /* Copy a scanline. */
        video_correction_correct_luma_scanline( vc, temp_scanline_data, fieldluma, width );
        copy_scanline_packed_422( output, temp_scanline_data, fieldcb, fieldcr, width );
        output += width * 2;
    }

    for( i = 0; i < (height / 2) - 1; i++ ) {

        /* Correct top scanline. */
        video_correction_correct_luma_scanline( vc, temp_scanline_data, fieldluma, width );

        if( !bottom_field ) {
            /* Copy a scanline. */
            copy_scanline_packed_422( output, temp_scanline_data, fieldcb, fieldcr, width );
            output += width * 2;
        }

        /* Interpolate a scanline. */
        video_correction_correct_luma_scanline( vc, temp_scanline_data + width, fieldluma + lstride, width );
        interpolate_scanline_packed_422( output, temp_scanline_data, fieldcb, fieldcr,
                                         width, cstride, width );
        output += width * 2;

        fieldluma += lstride;
        fieldcb += cstride;
        fieldcr += cstride;

        if( bottom_field ) {
            /* Copy a scanline. */
            copy_scanline_packed_422( output, temp_scanline_data + width, fieldcb, fieldcr, width );
            output += width * 2;
        }
    }

    if( !bottom_field ) {
        /* Copy a scanline. */
        video_correction_correct_luma_scanline( vc, temp_scanline_data, fieldluma, width );
        copy_scanline_packed_422( output, temp_scanline_data, fieldcb, fieldcr, width );
        output += width * 2;

        /* Clear a scanline. */
        clear_scanline_packed_422( output, width * 2 );
    }
}

struct video_correction_s
{
    int source_black_level;
    int target_black_level;
    int source_white_level;
    int target_white_level;
    double luma_correction;
    unsigned char *luma_table;
    unsigned char *chroma_table;
};

static double intensity_to_voltage( video_correction_t *vc, double val )
{   
    /* Handle invalid values before doing a gamma transform. */
    if( val < 0.0 ) return 0.0;
    if( val > 1.0 ) return 1.0;

    return pow( val, 1.0 / vc->luma_correction );
}

static void video_correction_update_table( video_correction_t *vc )
{
    int i;

    for( i = 0; i < 256; i++ ) {
        if( i < vc->source_black_level ) {
            vc->luma_table[ i ] = vc->target_black_level;
        } else if( i > vc->source_white_level ) {
            vc->luma_table[ i ] = vc->target_white_level;
        } else {
            double curval, target;

            curval = ( (double) (i - vc->source_black_level) ) / ( (double) (vc->source_white_level - vc->source_black_level) );
            curval = intensity_to_voltage( vc, curval );
            target = curval * ( (double) (vc->target_white_level - vc->target_black_level) );
            vc->luma_table[ i ] = ( (int) (target + 0.5) ) + vc->target_black_level;
        }
    }
}

video_correction_t *video_correction_new( void )
{
    video_correction_t *vc = (video_correction_t *) malloc( sizeof( video_correction_t ) );
    if( !vc ) {
        return 0;
    }

    vc->source_black_level = 16;
    vc->source_white_level = 253;
    vc->target_black_level = 16;
    vc->target_white_level = 235;
    vc->luma_correction = 1.0;
    vc->luma_table = (unsigned char *) malloc( 256 );
    vc->chroma_table = (unsigned char *) malloc( 256 );
    if( !vc->luma_table || !vc->chroma_table ) {
        if( vc->luma_table ) free( vc->luma_table );
        if( vc->chroma_table ) free( vc->chroma_table );
        free( vc );
        return 0;
    }
    video_correction_update_table( vc );

    return vc;
}

void video_correction_delete( video_correction_t *vc )
{
    free( vc->luma_table );
    free( vc->chroma_table );
    free( vc );
}

void video_correction_set_luma_power( video_correction_t *vc, double power )
{
    vc->luma_correction = power;
    video_correction_update_table( vc );
}

void video_correction_correct_luma_plane( video_correction_t *vc, unsigned char *luma, int width, int height, int stride )
{
    int y;

    for( y = 0; y < height; y++ ) {
        unsigned char *scanline = luma + (y * stride);
        int x;

        for( x = 0; x < width; x++ ) {
            scanline[ x ] = vc->luma_table[ scanline[ x ] ];
        }
    }
}

void video_correction_correct_luma_scanline( video_correction_t *vc, unsigned char *output, unsigned char *luma, int width )
{
    while( width-- ) {
        *output++ = vc->luma_table[ *luma++ ];
    }
}

const int filterkernel[] = { -1, 3, -6, 12, -24, 80, 80, -24, 12, -6, 3, -1 };
const int kernelsize = sizeof( filterkernel ) / sizeof( int );

void scanline_chroma422_to_chroma444_rec601( unsigned char *dest, unsigned char *src, int srcwidth )
{
    if( srcwidth ) {
        int halfksize = kernelsize / 2;
        int i;

        /* Handle the first few samples. */
        for( i = 0; i < halfksize; i++ ) {
            int sum = 0;
            int j;

            for( j = 0; j < kernelsize; j++ ) {
                int pos = (i - halfksize + 1) + j;

                if( pos >= 0 && pos < srcwidth ) {
                    sum += filterkernel[ j ] * src[ pos ];
                }
            }

            dest[ i*2 ] = src[ i ];
            dest[ i*2 + 1 ] = ( sum + 128 ) >> 8;
        }

        /* Handle samples where we have the full padding. */
        for( i = halfksize; i < srcwidth - halfksize; i++ ) {
            dest[ i*2 ] = src[ i ];
            dest[ i*2 + 1 ] = ((  (160*(src[i  ] + src[i+1]))
                                - ( 48*(src[i-1] + src[i+2]))
                                + ( 24*(src[i-2] + src[i+3]))
                                - ( 12*(src[i-3] + src[i+4]))
                                + (  6*(src[i-4] + src[i+5]))
                                - (  2*(src[i-5] + src[i+6]))) + 128) >> 8;
        }

        /* Handle last few samples. */
        for( i = srcwidth - halfksize; i < srcwidth; i++ ) {
            int sum = 0;
            int j;

            for( j = 0; j < kernelsize; j++ ) {
                int pos = (i - halfksize + 1) + j;

                if( pos >= 0 && pos < srcwidth ) {
                    sum += filterkernel[ j ] * src[ pos ];
                }
            }

            dest[ i*2 ] = src[ i ];
            dest[ i*2 + 1 ] = ( sum + 128 ) >> 8;
        }
    }
}

