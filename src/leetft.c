/**
 * Copyright (C) 2002,2003 Billy Biggs <vektor@dumbterm.net>.
 * Copyright (C) 2003      Per von Zweigbergk <pvz@e.kth.se>.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>

/* Freetype's build stuff is weird. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "leetft.h"
#include "hashtable.h"

static int ft_lib_refcount = 0;
static FT_Library ft_lib = 0;

#define MAX_STRING_LENGTH 1024
#define HASHTABLE_SIZE 303 /* is a prime number. */

struct ft_font_s
{
    int fontsize;
    int xdpi;
    FT_Face face;
    hashtable_t *glyphdata; // These contain glyph_data_s
    FT_UInt glyphpos[ MAX_STRING_LENGTH ];
    FT_UInt glyphindex[ MAX_STRING_LENGTH ];
    iconv_t cd;
};

typedef struct ft_glyph_data_s ft_glyph_data_t;
struct ft_glyph_data_s
{
    FT_Glyph glyph;
    FT_Glyph bitmap;
};

static int ft_cache_glyph( ft_font_t *font, wchar_t wchar,
                           FT_BBox *glyph_bbox )
{
    FT_Error error;
    FT_UInt glyph_index;
    ft_glyph_data_t *cur;

    if( hashtable_lookup( font->glyphdata, wchar ) ) {
        return 1; // The glyph already is cached, no need to re-cache it.
    }

    glyph_index = FT_Get_Char_Index( font->face, wchar );

    if( !glyph_index )
        return 0;

    cur = malloc( sizeof( *cur ) );
    
    if (cur == NULL) {
        fprintf ( stderr, "leeft: Out of memory\n");
        return 0;
    }
    error = FT_Load_Glyph( font->face, glyph_index, FT_LOAD_NO_HINTING );
    if( error ) {
        fprintf( stderr, "leetft: Can't load glyph %ld\n", wchar );
        return 0;
    }
    
    error = FT_Get_Glyph( font->face->glyph, &cur->glyph );
    if( error ) {
        fprintf( stderr, "leetft: FT_Get_Glyph failure for glyph %ld\n",
                 wchar );
        return 0;
    }
        
    error = FT_Glyph_Copy( cur->glyph, &cur->bitmap );
    if( error ) {
        fprintf( stderr, "leetft: Can't copy glyph %ld\n", wchar );
        FT_Done_Glyph( cur->glyph );
        hashtable_delete( font->glyphdata, wchar );
        return 0;
    }
        
    error = FT_Glyph_To_Bitmap( &cur->bitmap, ft_render_mode_normal, 0, 1 );
    if( error ) {
        fprintf( stderr, "leetft: Can't render glyph %ld\n", wchar );
        FT_Done_Glyph( cur->glyph );
        FT_Done_Glyph( cur->bitmap );
        hashtable_delete( font->glyphdata, wchar );
        return 0;
    }
    
    if( !hashtable_insert( font->glyphdata, wchar, cur ) ) {
        /* No more memory in the hash table, don't insert this glyph. */
        FT_Done_Glyph( cur->glyph );
        FT_Done_Glyph( cur->bitmap );
        hashtable_delete( font->glyphdata, wchar );
        return 0;
    }
    
    if( glyph_bbox ) { // Allow caller not to accept the BBox.
        FT_Glyph_Get_CBox( cur->glyph, ft_glyph_bbox_subpixels, glyph_bbox );
    }

    return 1;
}

ft_font_t *ft_font_new( const char *file, int fontsize, double pixel_aspect )
{
    ft_font_t *font = malloc( sizeof( ft_font_t ) );
    int i;

    if( !font ) return 0;

    font->cd = iconv_open( "WCHAR_T", "UTF-8" );
    if( font->cd == (iconv_t) -1 ) {
	free( font );
	return 0;
    }

    font->glyphdata = hashtable_init( HASHTABLE_SIZE );
    if( !font->glyphdata ) {
	iconv_close( font->cd );
        free( font );
        return 0;
    }

    if( !ft_lib_refcount ) {
        if( FT_Init_FreeType( &ft_lib ) ) {
            fprintf( stderr, "ftfont: Can't load freetype library.\n" );
            hashtable_destroy( font->glyphdata );
	    iconv_close( font->cd );
            free( font );
            return 0;
        }
    }
    ft_lib_refcount++;

    font->fontsize = fontsize;
    if( FT_New_Face( ft_lib, file, 0, &(font->face) ) ) {
        ft_lib_refcount--;
        if( !ft_lib_refcount ) {
            FT_Done_FreeType( ft_lib );
        }
        hashtable_destroy( font->glyphdata );
	iconv_close( font->cd );
        free( font );
        return 0;
    }

    /**
     * Use this to load postscript font metrics for kerning information.
    if( FT_Attach_File( font->face, "../data/cmss17.afm" ) ) {
        fprintf( stderr, "ftfont: AFM attach failed.\n" );
    }
    */

    font->xdpi = (int) ( ( 72.0 * pixel_aspect ) + 0.5 );
    FT_Set_Char_Size( font->face, 0, font->fontsize * 64, font->xdpi, 72 );

    FT_Select_Charmap( font->face, ft_encoding_unicode );

    /* Precache printable ASCII chars for better performance */

    for (i = 32; i < 128; i++)
        ft_cache_glyph (font, i, NULL);

    return font;
}

void ft_font_delete( ft_font_t *font )
{
    hashtable_iterator_t *iter = hashtable_iterator_init (font->glyphdata);
    ft_glyph_data_t *cur;
    int index;

    while ((cur = hashtable_iterator_go (iter, 0, 1, &index)) != NULL) {
        FT_Done_Glyph( cur->glyph );
        FT_Done_Glyph( cur->bitmap );
        hashtable_delete( font->glyphdata, index );
    }

    FT_Done_Face( font->face );
    hashtable_iterator_destroy (iter);
    hashtable_destroy( font->glyphdata );
    iconv_close( font->cd );
    free( font );

    ft_lib_refcount--;
    if( !ft_lib_refcount ) {
        FT_Done_FreeType( ft_lib );
        ft_lib = 0;
    }
}

int ft_font_get_size( ft_font_t *font )
{
    return font->fontsize;
}

int ft_font_points_to_subpix_width( ft_font_t *font, int points )
{
    return ( font->xdpi * points * 65536 ) / 72;
}

static FT_BBox prerender_text( ft_font_t *font, const wchar_t *wtext, int len )
{
    FT_Bool use_kerning;
    FT_UInt previous;
    FT_BBox bbox;
    int pen_x, i;
    int prevchar = 0;

    bbox.xMin = bbox.yMin = INT_MAX;
    bbox.xMax = bbox.yMax = -INT_MAX;

    use_kerning = FT_HAS_KERNING( font->face );
    previous = 0;
    pen_x = 0;

    for( i = 0; i < len; i++ ) {
        wchar_t cur = wtext[ i ];
        ft_glyph_data_t *curdata;
	int ret;

	ret = ft_cache_glyph( font, wtext[i], NULL );
        curdata = hashtable_lookup( font->glyphdata, cur );

        if( ret && curdata ) {
            FT_Glyph curglyph = curdata->glyph;
            FT_BBox glyph_bbox;

            font->glyphindex[ i ] = FT_Get_Char_Index( font->face, cur );

            if( use_kerning && previous && font->glyphindex[ i ] ) {
                FT_Vector  delta;
                FT_Get_Kerning( font->face, previous, font->glyphindex[ i ],
                                ft_kerning_unfitted, &delta );

                /* Ignore kerning on numbers for now. */
                if( !((prevchar >= '0' && prevchar <= '9') &&
                      (wtext[ i ] >= '0' && wtext[ i ] <= '9')) ) {
                    pen_x += ( delta.x * 1024 );
                }
            }
            prevchar = wtext[ i ];
            previous = font->glyphindex[ i ];

            /* Save the current pen position. */
            font->glyphpos[ i ] = pen_x;

            /* Advance is in 16.16 format. */
            pen_x += curglyph->advance.x;


            FT_Glyph_Get_CBox( curglyph, ft_glyph_bbox_subpixels,
                               &glyph_bbox );

            glyph_bbox.xMin *= 1024;
            glyph_bbox.xMax *= 1024;

            glyph_bbox.xMin += font->glyphpos[ i ];
            glyph_bbox.xMax += font->glyphpos[ i ];

            if( glyph_bbox.xMin < bbox.xMin ) bbox.xMin = glyph_bbox.xMin;
            if( glyph_bbox.yMin < bbox.yMin ) bbox.yMin = glyph_bbox.yMin;
            if( glyph_bbox.xMax > bbox.xMax ) bbox.xMax = glyph_bbox.xMax;
            if( glyph_bbox.yMax > bbox.yMax ) bbox.yMax = glyph_bbox.yMax;
        }
    }

    // check that we really grew the string bbox
    if( bbox.xMin > bbox.xMax ) {
        bbox.xMin = bbox.yMin = bbox.xMax = bbox.yMax = 0;
    }

    if( bbox.xMax < pen_x ) bbox.xMax = pen_x;

    return bbox;
}

static void blit_glyph_subpix( uint8_t *dst, int dst_width, int dst_height,
                               int dst_stride, uint8_t *src, int src_width,
                               int src_height, int src_stride,
                               int dst_xpos_subpix, int dst_ypos )
{
    int blit_width = dst_width - ( dst_xpos_subpix >> 16 );
    int blit_height = dst_height - dst_ypos;

    if( blit_width > src_width ) blit_width = src_width;
    if( blit_height > src_height ) blit_height = src_height;

    if( blit_width >= 0 && blit_height >= 0 ) {
        int y;

        for( y = 0; y < blit_height; y++ ) {
            uint8_t *curdst = dst + ((dst_ypos + y)*dst_stride) + ( dst_xpos_subpix >> 16 );
            uint8_t *cursrc = src + (y*src_stride);
            int prev = 0;
            int pos = dst_xpos_subpix & 0xffff;
            int tmp;
            int x;

            for( x = 0; x < blit_width; x++ ) {
                tmp = ( ( prev * pos ) + ( cursrc[ x ] * ( 0xffff - pos ) ) )
                    / 65535;
                tmp += curdst[ x ];
                curdst[ x ] = (tmp > 255) ? 255 : tmp;
                prev = cursrc[ x ];
            }
            tmp = ( prev * pos ) / 65535;
            tmp += curdst[ blit_width ];
            curdst[ blit_width ] = (tmp > 255) ? 255 : tmp;
        }
    }
}

void ft_font_render( ft_font_t *font, uint8_t *output, const char *ntext,
                     int subpix_pos, int *width, int *height, int outsize )
{
    FT_BBox string_bbox;
    int push_x, i;
    const char *inbuf = ntext;
    size_t inbytesleft;
    wchar_t wtext[ 512 ];
    wchar_t *outbuf = wtext;
    size_t outbytesleft = sizeof( wtext );
    int len;

    if( !ntext || !(*ntext) ) {
        *width = *height = 0;
        return;
    }

    /* Yes, strlen. I want to know how many bytes, not how many characters. */
    inbytesleft = strlen( ntext );
    
    while( inbytesleft > 0 ) {
        int ret = iconv( font->cd, (char **) &inbuf, &inbytesleft,
                         (char **) &outbuf, &outbytesleft );
        if( ret == -1 ) {
            perror( "leetft: Can't convert UTF-8 to wide string, iconv says" );
            return;
        }
    }
    /* Calculate length of the string generated by iconv. */
    len = outbuf - wtext;

    string_bbox = prerender_text( font, wtext, len );

    /**
     * Temporary hack.  I'm worried about strings where the bounding box
     * starts at a negative position, so I'm pushing everything over to
     * the left if that is the case.
     */
    push_x = subpix_pos;
    *width = subpix_pos + string_bbox.xMax;
    if( string_bbox.xMin < 0 ) {
        /* I should figure out if this is a bug. */
        /* fprintf( stderr, "leetft: Negative xmin?  Sigh..\n" ); */
        *width += -string_bbox.xMin;
        push_x += -string_bbox.xMin;
    }
    *width = ( (*width + 32768) >> 16 ) + 1;

    /* The numbers I get for a height all seem to make sense. */
    *height = font->fontsize - ((string_bbox.yMin + 32) >> 6);

    if( *width * *height > outsize ) {
        *width = *height = 0;
        return;
    }
    memset( output, 0, *width * *height );

    for( i = 0; i < len; i++ ) {
        ft_glyph_data_t *curdata;
        int cur = wtext[ i ];

        curdata = hashtable_lookup( font->glyphdata, cur );
        if( curdata ) {
            FT_BitmapGlyph curglyph = (FT_BitmapGlyph) curdata->bitmap;

            blit_glyph_subpix( output, *width, *height, *width,
                               curglyph->bitmap.buffer, curglyph->bitmap.width,
                               curglyph->bitmap.rows, curglyph->bitmap.pitch,
                               push_x + font->glyphpos[ i ] + (curglyph->left*65536),
                               font->fontsize - curglyph->top );
        }
    }
}

struct ft_string_s
{
    ft_font_t *font;
    int width;
    int height;
    int datasize;
    uint8_t data[ 256 * 1024 ];
};

ft_string_t *ft_string_new( ft_font_t *font )
{
    ft_string_t *fts = malloc( sizeof( ft_string_t ) );
    if( !fts ) return 0;

    fts->font = font;
    fts->datasize = sizeof( fts->data );

    return fts;
}

void ft_string_delete( ft_string_t *fts )
{
    free( fts->data );
    free( fts );
}

void ft_string_set_text( ft_string_t *fts, const char *text, int subpix_pos )
{
    ft_font_render( fts->font, fts->data, text, subpix_pos,
                    &fts->width, &fts->height, fts->datasize );
}

int ft_string_get_width( ft_string_t *fts )
{
    return fts->width;
}

int ft_string_get_height( ft_string_t *fts )
{
    return fts->height;
}

int ft_string_get_stride( ft_string_t *fts )
{
    return fts->width;
}

uint8_t *ft_string_get_buffer( ft_string_t *fts )
{
    return fts->data;
}

