/**
 * This file is originally from mythtv by Isaac Richards <ijr@po.cwru.edu>.
 * http://mythtv.sourceforge.net/
 *
 * MythTV is distributed under the terms of the GPL, version 2 only.
 * If you don't have a copy of the GPL, get one at:
 *    http://www.gnu.org/licenses/gpl.txt
 */

#ifndef INCLUDED_TTFONT__H_
#define INCLUDED_TTFONT__H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

struct Raster_Map;

class TTFFont
{
  public:
     TTFFont( const char *file, int size, int video_width, int video_height, double aspect_ratio );
    ~TTFFont( void );

     bool isValid( void ) const { return valid; }

     void RenderString( unsigned char *output, const char *text, int *width, int *height, int maxx, int maxy );

  private:
     Raster_Map *create_font_raster(int width, int height);
     Raster_Map *duplicate_raster(FT_BitmapGlyph bmap);
     void clear_raster(Raster_Map *rmap);
     void destroy_font_raster(Raster_Map *rmap);
     void calc_size(int *width, int *height, const char *text);
     void render_text( Raster_Map *rmap, const char *text );

     bool         valid;
     FT_Library   library;
     FT_Face      face;
     int          num_glyph;
     FT_Glyph    *glyphs;
     Raster_Map **glyphs_cached;
     int          max_descent;
     int          max_ascent;
     int          fontsize;
     int          vid_width;
     int          vid_height;
     double       vid_aspect;
     bool         use_kerning;
};

#endif
