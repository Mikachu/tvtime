/**
 * This file is originally from mythtv by Isaac Richards <ijr@po.cwru.edu>.
 * http://mythtv.sourceforge.net/
 *
 * MythTV is distributed under the terms of the GPL, version 2 only.
 * If you don't have a copy of the GPL, get one at:
 *    http://www.gnu.org/licenses/gpl.txt
 */

/*
 * Copyright (C) 1999 Carsten Haitzler and various contributors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ttfont.h"


/**
 * define this to enable kerning
 * #define USE_KERNING
 */

static char         have_library = 0;
static FT_Library   the_library;

#define FT_VALID(handle) ((handle) && (handle)->clazz != NULL)

struct Raster_Map
{
    int width;
    int rows;
    int cols;
    int size;
    unsigned char *bitmap;
};

Raster_Map *TTFFont::create_font_raster(int width, int height)
{
   Raster_Map      *rmap;

   rmap = new Raster_Map;
   rmap->width = (width + 3) & -4;
   rmap->rows = height;
   rmap->cols = rmap->width;
   rmap->size = rmap->rows * rmap->width;
   if (rmap->size <= 0)
   {
        delete rmap;
        return NULL;
   }
   rmap->bitmap = new unsigned char[rmap->size];
   if (!rmap->bitmap)
   {
        delete rmap;
        return NULL;
   }
   memset(rmap->bitmap, 0, rmap->size);
   return rmap;
}

Raster_Map *TTFFont::duplicate_raster(FT_BitmapGlyph bmap)
{
    Raster_Map      *new_rmap;

    new_rmap = new Raster_Map;

    new_rmap->width = bmap->bitmap.width;
    new_rmap->rows = bmap->bitmap.rows;
    new_rmap->cols = bmap->bitmap.pitch;
    new_rmap->size = new_rmap->cols * new_rmap->rows;

    if( new_rmap->size > 0 ) {
        new_rmap->bitmap = new unsigned char[new_rmap->size];
        memcpy(new_rmap->bitmap, bmap->bitmap.buffer, new_rmap->size);
    } else {
        new_rmap->bitmap = NULL;
    }

    return new_rmap;
}

void TTFFont::destroy_font_raster(Raster_Map * rmap)
{
   if (!rmap)
      return;
   if (rmap->bitmap)
      delete [] rmap->bitmap;
   delete rmap;
}

void TTFFont::calc_size(int *width, int *height, const char *text)
{
   int                 i, pw, ph;

   pw = 0;
   ph = ((max_ascent) - max_descent) / 64;

   for (i = 0; text[i]; i++)
   {
       unsigned char       j = text[i];

       if (!FT_VALID(glyphs[j]))
           continue;
       if (i == 0)
       {
           FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
           pw += 2; //((face->glyph->metrics.horiBearingX) / 64);
       }
       if (text[i + 1] == 0)
       {
           FT_BBox bbox;
           FT_Glyph_Get_CBox(glyphs[j], ft_glyph_bbox_subpixels, &bbox);
           pw += (bbox.xMax / 64);
       }
       else
           pw += glyphs[j]->advance.x / 65535;
   }
   *width = pw;
   *height = ph;
}

void TTFFont::render_text( Raster_Map *rmap, const char *text )
{
   FT_F26Dot6          x, y, xmin, ymin, xmax, ymax;
   FT_BBox             bbox;
   int                 i, ioff, iread;
   char               *off, *read, *_off, *_read;
   int                 x_offset, y_offset;
   unsigned char       j, previous;

   j = text[0];
   FT_Load_Glyph(face, j, FT_LOAD_DEFAULT);
   x_offset = 2; //(face->glyph->metrics.horiBearingX) / 64;

   y_offset = -(max_descent / 64);

   previous = 0;

   for (i = 0; text[i]; i++)
   {
       Raster_Map *rtmp;
       j = text[i];

       if (!FT_VALID(glyphs[j]))
           continue;

       FT_Glyph_Get_CBox(glyphs[j], ft_glyph_bbox_subpixels, &bbox);
       xmin = bbox.xMin & -64;
       ymin = bbox.yMin & -64;
       xmax = (bbox.xMax + 63) & -64;
       ymax = (bbox.yMax + 63) & -64;

       if( !glyphs_cached[j] ) {
           FT_Vector origin;
           FT_BitmapGlyph bmap;

           origin.x = -xmin;
           origin.y = -ymin;

           FT_Glyph_To_Bitmap(&glyphs[j], ft_render_mode_normal, &origin, 1);
           bmap = (FT_BitmapGlyph)(glyphs[j]);

           glyphs_cached[j] = duplicate_raster(bmap);
       }
       rtmp = glyphs_cached[j];
       // Blit-or the resulting small pixmap into the biggest one
       // We do that by hand, and provide also clipping.

#ifdef USE_KERNING
       if (use_kerning && previous && j)
       {
           FT_Vector delta;
           FT_Get_Kerning(face, previous, j, FT_KERNING_DEFAULT, &delta);
           x_offset += delta.x >> 6;
       }
#endif // USE_KERNING

       xmin = (xmin >> 6) + x_offset;
       ymin = (ymin >> 6) + y_offset;
       xmax = (xmax >> 6) + x_offset;
       ymax = (ymax >> 6) + y_offset;

       // Take care of comparing xmin and ymin with signed values!
       // This was the cause of strange misplacements when Bit.rows
       // was unsigned.

       if (xmin >= (int)rmap->width ||
           ymin >= (int)rmap->rows ||
           xmax < 0 || ymax < 0)
           continue;

       // Note that the clipping check is performed _after_ rendering
       // the glyph in the small bitmap to let this function return
       // potential error codes for all glyphs, even hidden ones.

       // In exotic glyphs, the bounding box may be larger than the
       // size of the small pixmap.  Take care of that here.

       if (xmax - xmin + 1 > rtmp->width)
           xmax = xmin + rtmp->width - 1;

       if (ymax - ymin + 1 > rtmp->rows)
           ymax = ymin + rtmp->rows - 1;

       // set up clipping and cursors
 
       iread = 0;
       if (ymin < 0)
       {
           iread -= ymin * rtmp->cols;
           ioff = 0;
           ymin = 0;
       }
       else
           ioff = (rmap->rows - ymin - 1) * rmap->cols;

       if (ymax >= rmap->rows)
           ymax = rmap->rows - 1;

       if (xmin < 0)
       {
           iread -= xmin;
           xmin = 0;
       }
       else
           ioff += xmin;

       if (xmax >= rmap->width)
           xmax = rmap->width - 1;

       iread = (ymax - ymin) * rtmp->cols + iread;

       _read = (char *)rtmp->bitmap + iread;
       _off = (char *)rmap->bitmap + ioff;

       for (y = ymin; y <= ymax; y++)
       {
           read = _read;
           off = _off;

           for (x = xmin; x <= xmax; x++)
           {
               *off = *read;
               off++;
               read++;
           }
           _read -= rtmp->cols;
           _off -= rmap->cols;
       }
       x_offset += (glyphs[j]->advance.x / 65535);
       previous = j;
    }
}

TTFFont::~TTFFont( void )
{
   int                 i;

   if (!valid)
       return;

   FT_Done_Face(face);
   for (i = 0; i < num_glyph; i++)
     {
        if( glyphs_cached[ i ] ) {
            destroy_font_raster(glyphs_cached[i]);
        }
        if( FT_VALID( glyphs[ i ] ) ) {
            FT_Done_Glyph( glyphs[ i ] );
        }
     }
   if (glyphs)
      free(glyphs);
   if (glyphs_cached)
      free(glyphs_cached);

   have_library--;

   if (!have_library)
       FT_Done_FreeType(the_library);
}

TTFFont::TTFFont(const char *file, int size, int video_width, int video_height, double aspect_ratio)
{
   FT_Error            error;
   FT_CharMap          char_map;
   FT_BBox             bbox;
   int                 xdpi = 72, ydpi = 72;
   unsigned short      i, n, code;

   valid = false;

   if (!have_library)
   {
        error = FT_Init_FreeType(&the_library);
        if (error) {
           return;
        }
   }
   have_library++;

   fontsize = size;
   library = the_library;
   error = FT_New_Face(library, file, 0, &face);
   if (error)
   {
        have_library--;
 
        if (!have_library)
            FT_Done_FreeType(the_library);

        return;
   }

   vid_width = video_width;
   vid_height = video_height;
   vid_aspect = aspect_ratio;
   xdpi = (int) ((((double)xdpi) * (double)(video_width / (double)(video_height * vid_aspect)))+0.5);

   FT_Set_Char_Size(face, 0, size * 64, xdpi, ydpi);

   n = face->num_charmaps;

   for (i = 0; i < n; i++)
   {
        char_map = face->charmaps[i];
        if ((char_map->platform_id == 3 && char_map->encoding_id == 1) ||
            (char_map->platform_id == 0 && char_map->encoding_id == 0))
        {
             FT_Set_Charmap(face, char_map);
             break;
        }
   }
   if (i == n)
      FT_Set_Charmap(face, face->charmaps[0]);

   num_glyph = face->num_glyphs;
   num_glyph = 256;
   glyphs = (FT_Glyph *) malloc((num_glyph + 1) * sizeof(FT_Glyph));
   memset(glyphs, 0, num_glyph * sizeof(FT_Glyph));
   glyphs_cached = (Raster_Map **) malloc(num_glyph * sizeof(Raster_Map *));
   memset(glyphs_cached, 0, num_glyph * sizeof(Raster_Map *));

   max_descent = 0;
   max_ascent = 0;

   for (i = 0; i < num_glyph; ++i)
   {
        if (FT_VALID(glyphs[i]))
            continue;

        code = FT_Get_Char_Index(face, i);

        FT_Load_Glyph(face, code, FT_LOAD_DEFAULT);
        FT_Get_Glyph(face->glyph, &glyphs[i]);

        FT_Glyph_Get_CBox(glyphs[i], ft_glyph_bbox_subpixels, &bbox);

        if ((bbox.yMin & -64) < max_descent)
           max_descent = (bbox.yMin & -64);
        if (((bbox.yMax + 63) & -64) > max_ascent)
           max_ascent = ((bbox.yMax + 63) & -64);
   }

#ifdef USE_KERNING
   use_kerning = FT_HAS_KERNING(face);
#else
   use_kerning = 0;
#endif

   valid = true;
}

void TTFFont::RenderString( unsigned char *output, const char *text,
                            int *width, int *height, int maxx, int maxy )
{
   int w, h;
   Raster_Map rmap;

   calc_size( &w, &h, text );
   if( w <= 0 || h <= 0 ) {
       *width = *height = 0;
       return;
   }

   *width = w;
   if( *width > maxx ) *width = maxx;
   *height = h;
   if( *height > maxy ) *height = maxy;

   rmap.width = w;
   rmap.rows = h;
   rmap.cols = w;
   rmap.size = w * h;
   rmap.bitmap = output;
   memset( output, 0, w * h );

   render_text( &rmap, text );
}

