/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdlib.h>
#include "ttfont.h"
#include "videotools.h"
#include "osd.h"

struct osd_font_s
{
    Efont *font;
};

osd_font_t *osd_font_new( const char *filename )
{
    osd_font_t *osdf = (osd_font_t *) malloc( sizeof( osd_font_t ) );
    if( !osdf ) {
        return 0;
    }

    osdf->font = Efont_load( filename, 80, 720, 480 );
    if( !osdf->font ) {
        free( osdf );
        return 0;
    }

    return osdf;
}

void osd_font_delete( osd_font_t *osdf )
{
    Efont_free( osdf->font );
    free( osdf );
}

Efont *osd_font_get_font( osd_font_t *osdf )
{
    return osdf->font;
}

struct osd_string_s
{
    osd_font_t *font;
    efont_string_t *efs;
    int frames_left;
    int text_luma;
    int text_cb;
    int text_cr;
};

osd_string_t *osd_string_new( osd_font_t *font )
{
    osd_string_t *osds = (osd_string_t *) malloc( sizeof( osd_string_t ) );
    osds->font = font;
    osds->efs = 0;
    osds->frames_left = 0;
    osds->text_luma = 16;
    osds->text_cb = 128;
    osds->text_cr = 128;
    return osds;
}

void osd_string_delete( osd_string_t *osds )
{
    if( osds->efs ) {
        efs_delete( osds->efs );
    }
    free( osds );
}

void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr )
{
    osds->text_luma = luma;
    osds->text_cb = cb;
    osds->text_cr = cr;
}

void osd_string_show_text( osd_string_t *osds, const char *text, int timeout )
{
    if( osds->efs ) {
        efs_delete( osds->efs );
    }
    osds->efs = efs_new( osd_font_get_font( osds->font ), text );
    osds->frames_left = timeout;
}

int osd_string_visible( osd_string_t *osds )
{
    return ( osds->efs && (osds->frames_left != 0) );
}

void osd_string_advance_frame( osd_string_t *osds )
{
    if( osds->frames_left > 0 ) {
        osds->frames_left--;
    }
}

void osd_string_composite_packed422( osd_string_t *osds, unsigned char *output,
                                     int width, int height, int stride,
                                     int xpos, int ypos, int rightjustified )
{
    int dest_x, dest_y, src_x, src_y, blit_w, blit_h;

    if( !osds->efs ) return;

    src_x = 0;
    src_y = 0;
    if( rightjustified ) {
        dest_x = xpos - efs_get_width( osds->efs );
    } else {
        dest_x = xpos;
    }
    dest_y = ypos;
    blit_w = efs_get_width( osds->efs );
    blit_h = efs_get_height( osds->efs );

    if( dest_x < 0 ) {
        src_x = -dest_x;
        blit_w += dest_x;
        dest_x = 0;
    }
    if( dest_y < 0 ) {
        src_y = -dest_y;
        blit_h += dest_y;
        dest_y = 0;
    }
    if( dest_x + blit_w > width ) {
        blit_w = width - dest_x;
    }
    if( dest_y + blit_h > height ) {
        blit_h = height - dest_y;
    }

    if( blit_w > 0 && blit_h > 0 ) {
        double textalpha = 1.0;
        int i;

        output += dest_y * stride;
        for( i = 0; i < blit_h; i++ ) {
            composite_textmask_packed422_scanline( output + ((dest_x) * 2),
                                                   output + ((dest_x) * 2),
                                                   efs_get_scanline( osds->efs, src_y + i ),
                                                   blit_w, osds->text_luma, osds->text_cb,
                                                   osds->text_cr, textalpha );
            output += stride;
        }
    }
}

