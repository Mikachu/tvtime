/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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
#include <stdlib.h>
#include <string.h>

/* Freetype's build stuff is weird. */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "leetft.h"

static int ft_lib_refcount = 0;
static FT_Library ft_lib = 0;

#define MAX_STRING_LENGTH 1024

struct ft_font_s
{
    int fontsize;
    FT_Face face;
    FT_Glyph glyphs[ 256 ];
    FT_UInt glyphpos[ MAX_STRING_LENGTH ];
    FT_UInt glyphindex[ MAX_STRING_LENGTH ];
};

ft_font_t *ft_font_new( const char *file, int fontsize, double pixel_aspect )
{
    ft_font_t *font = (ft_font_t *) malloc( sizeof( ft_font_t ) );
    FT_Error error;
    int xdpi, i;

    if( !font ) return 0;

    if( !ft_lib_refcount ) {
        if( FT_Init_FreeType( &ft_lib ) ) {
           fprintf( stderr, "ftfont: Can't load freetype library.\n" );
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
        free( font );
        return 0;
    }

    /*
    if( FT_Attach_File( font->face, "../data/cmss17.afm" ) ) {
        fprintf( stderr, "attach failed.\n" );
    }
    */

    xdpi = (int) ( ( 72.0 * pixel_aspect ) + 0.5 );
    FT_Set_Char_Size( font->face, 0, font->fontsize * 64, xdpi, 72 );

    FT_Set_Charmap( font->face, font->face->charmaps[ 0 ] );
    for( i = 0; i < font->face->num_charmaps; i++ ) {
        FT_CharMap char_map;
        char_map = font->face->charmaps[ i ];
        if( ( char_map->platform_id == 3 && char_map->encoding_id == 1 ) ||
            ( char_map->platform_id == 0 && char_map->encoding_id == 0 ) ) {
            FT_Set_Charmap( font->face, char_map );
            break;
        }
    }

    /* Load glyphs. */
    for( i = 0; i < 256; i++ ) {
        FT_UInt glyph_index = FT_Get_Char_Index( font->face, i );

        error = FT_Load_Glyph( font->face, glyph_index, FT_LOAD_NO_HINTING );
        if( error ) {
            font->glyphs[ i ] = 0;
        } else {
            FT_Get_Glyph( font->face->glyph, &(font->glyphs[ i ]) );
        }
    }

    return font;
}

void ft_font_delete( ft_font_t *font )
{
    int i;

    for( i = 0; i < 256; i++ ) {
        if( font->glyphs[ i ] ) {
            FT_Done_Glyph( font->glyphs[ i ] );
        }
    }
    FT_Done_Face( font->face );
    free( font );

    ft_lib_refcount--;
    if( !ft_lib_refcount ) {
        FT_Done_FreeType( ft_lib );
        ft_lib = 0;
    }
}

static FT_BBox prerender_text( FT_Face face, FT_Glyph *glyphs, FT_UInt *glyphpos,
                               FT_UInt *glyphindex, const char *text, int len )
{
    FT_Bool use_kerning;
    FT_UInt previous;
    FT_BBox bbox;
    int pen_x, i;
    int prev = 0;

    bbox.xMin = bbox.yMin = INT_MAX;
    bbox.xMax = bbox.yMax = -INT_MAX;

    use_kerning = FT_HAS_KERNING( face );
    previous = 0;
    pen_x = 0;

    for( i = 0; i < len; i++ ) {
        int cur = text[ i ];

        if( glyphs[ cur ] ) {
            FT_BBox glyph_bbox;

            glyphindex[ i ] = FT_Get_Char_Index( face, cur );

            if( use_kerning && previous && glyphindex[ i ] ) {
                FT_Vector  delta;
                FT_Get_Kerning( face, previous, glyphindex[ i ], ft_kerning_unfitted, &delta );
                pen_x += ( delta.x * 1024 );
            }
            prev = text[ i ];

            /* Save the current pen position. */
            glyphpos[ i ] = pen_x;

            /* Advance is in 16.16 format. */
            pen_x += glyphs[ cur ]->advance.x;
            previous = glyphindex[ i ];


            FT_Glyph_Get_CBox( glyphs[ cur ], ft_glyph_bbox_subpixels, &glyph_bbox );

            glyph_bbox.xMin *= 1024;
            glyph_bbox.xMax *= 1024;

            glyph_bbox.xMin += glyphpos[ i ];
            glyph_bbox.xMax += glyphpos[ i ];

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

    return bbox;
}

/*
static void blit_stuff( unsigned char *dst, int dst_width, int dst_height, int dst_stride,
                        unsigned char *src, int src_width, int src_height, int src_stride,
                        int dst_xpos, int dst_ypos, int src_xpos, int src_ypos )
{
    int blit_width = (dst_width - dst_xpos);
    int blit_height = (dst_height - dst_ypos);

    if( blit_width > src_width ) blit_width = src_width;
    if( blit_height > src_height ) blit_height = src_height;

    blit_width -= src_xpos;
    blit_height -= src_ypos;

    if( blit_width >= 0 && blit_height >= 0 ) {
        int y;

        for( y = 0; y < blit_height; y++ ) {
            unsigned char *curdst = dst + ((dst_ypos + y)*dst_stride) + dst_xpos;
            unsigned char *cursrc = src + ((src_ypos + y)*src_stride) + src_xpos;
            int x;

            for( x = 0; x < blit_width; x++ ) {
                if( *cursrc ) *curdst = *cursrc;
                curdst++;
                cursrc++;
            }
        }
    }
}
*/

static void blit_stuff_subpix( unsigned char *dst, int dst_width, int dst_height, int dst_stride,
                               unsigned char *src, int src_width, int src_height, int src_stride,
                               int dst_xpos_subpix, int dst_ypos )
{
    int blit_width = src_width; // (dst_width - dst_xpos);
    int blit_height = (dst_height - dst_ypos);

    if( blit_width > src_width ) blit_width = src_width;
    if( blit_height > src_height ) blit_height = src_height;

    if( blit_width >= 0 && blit_height >= 0 ) {
        int y;

        for( y = 0; y < blit_height; y++ ) {
            unsigned char *curdst = dst + ((dst_ypos + y)*dst_stride) + ( dst_xpos_subpix >> 16 );
            unsigned char *cursrc = src + (y*src_stride);
            int prev = 0;
            int pos = dst_xpos_subpix & 0xffff;
            int x;
            int tmp;

            for( x = 0; x < blit_width; x++ ) {
                tmp = ( ( prev * pos ) + ( cursrc[ x ] * ( 0xffff - pos ) ) ) / 65535;
                tmp += curdst[ x ];
                curdst[ x ] = (tmp > 255) ? 255 : tmp;
                // curdst[ x ] = ( ( prev * pos ) + ( cursrc[ x ] * ( 0xffff - pos ) ) ) / 65535;
                prev = cursrc[ x ];
            }
            tmp = ( prev * pos ) / 65535;
            tmp += curdst[ blit_width ];
            curdst[ x ] = (tmp > 255) ? 255 : tmp;
            // curdst[ blit_width ] = ( prev * pos ) / 65535;
        }
    }
}

void ft_font_render( ft_font_t *font, unsigned char *output, const char *text,
                     int *width, int *height, int outsize )
{
    FT_BBox string_bbox;
    int len;
    int push_x, i;

    if( !text || !(*text) ) {
        *width = *height = 0;
        return;
    }

    len = strlen( text );
    string_bbox = prerender_text( font->face, font->glyphs, font->glyphpos, font->glyphindex, text, len );

    /**
     * Temporary hack.  I'm worried about strings where the bounding box
     * starts at a negative position, so I'm pushing everything over to
     * the left if that is the case.
     */
    push_x = 0;
    *width = string_bbox.xMax;
    if( string_bbox.xMin < 0 ) {
        fprintf( stderr, "leetft: Negative xmin?  Sigh..\n" );
        *width += -string_bbox.xMin;
        push_x = -string_bbox.xMin;
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
        FT_Glyph image;
        FT_Error error;
        int cur = text[ i ];

        // create a copy of the original glyph
        error = FT_Glyph_Copy( font->glyphs[ cur ], &image );
        if( !error ) {
            // convert glyph image to bitmap (destroy the glyph copy !!)
            error = FT_Glyph_To_Bitmap( &image, ft_render_mode_normal, 0, 1 );
            if( !error ) {
                FT_BitmapGlyph bit = (FT_BitmapGlyph) image;

                blit_stuff_subpix( output, *width, *height, *width,
                                   bit->bitmap.buffer, bit->bitmap.width, bit->bitmap.rows, bit->bitmap.pitch,
                            (push_x*0xffff) + font->glyphpos[ i ] + (bit->left*65536), font->fontsize - bit->top );

                FT_Done_Glyph( image );
            }
        }
    }
}

struct ft_string_s
{
    ft_font_t *font;
    unsigned char *data;
    int datasize;
    int width;
    int height;
};

ft_string_t *ft_string_new( ft_font_t *font )
{
    ft_string_t *fts = (ft_string_t *) malloc( sizeof( ft_string_t ) );
    if( !fts ) return 0;

    fts->font = font;
    fts->datasize = 65536;
    fts->data = (unsigned char *) malloc( fts->datasize );
    if( !fts->data ) {
        free( fts );
        return 0;
    }

    return fts;
}

void ft_string_delete( ft_string_t *fts )
{
    free( fts->data );
    free( fts );
}

void ft_string_set_text( ft_string_t *fts, const char *text )
{
    ft_font_render( fts->font, fts->data, text, &fts->width, &fts->height, fts->datasize );
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

unsigned char *ft_string_get_buffer( ft_string_t *fts )
{
    return fts->data;
}

