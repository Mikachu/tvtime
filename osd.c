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
#include <math.h>
#include "ttfont.h"
#include "videotools.h"
#include "osd.h"

typedef struct osd_font_s osd_font_t;

struct osd_font_s
{
    Efont *font;
};

osd_font_t *osd_font_new( const char *filename, int fontsize, int video_width, int video_height )
{
    osd_font_t *osdf = (osd_font_t *) malloc( sizeof( osd_font_t ) );
    if( !osdf ) {
        return 0;
    }

    osdf->font = Efont_load( filename, fontsize, video_width, video_height );
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

struct osd_shape_s
{
    int type;
    int frames_left;
    int shape_luma;
    int shape_cb;
    int shape_cr;
    int shape_height;
    int shape_width;
    unsigned char *shape_mask;
};

osd_string_t *osd_string_new( const char *fontfile, int fontsize,
                              int video_width, int video_height )
{
    osd_string_t *osds = (osd_string_t *) malloc( sizeof( osd_string_t ) );
    osds->font = osd_font_new( fontfile, fontsize, video_width, video_height );
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
        int i;

        output += dest_y * stride;
        for( i = 0; i < blit_h; i++ ) {
            composite_textmask_packed422_scanline( output + ((dest_x) * 2),
                                                   output + ((dest_x) * 2),
                                                   efs_get_scanline( osds->efs, src_y + i ),
                                                   blit_w, osds->text_luma, osds->text_cb,
                                                   osds->text_cr, ((double) osds->frames_left) / 50.0 );
            output += stride;
        }
    }
}


/* Shape functions */

osd_shape_t *osd_shape_new( OSD_Shape shape_type, int shape_width, int shape_height )
{

    int x,y;
    double radius_sqrd,x0;

    osd_shape_t *osds = (osd_shape_t *) malloc( sizeof( osd_shape_t ) );
    osds->frames_left = 0;
    osds->shape_luma = 16;
    osds->shape_cb = 128;
    osds->shape_cr = 128;
    osds->shape_width = shape_width;
    osds->shape_height = shape_height;
    osds->type = shape_type;

    switch( shape_type ) {
    case OSD_Rect:
        osds->shape_mask = (unsigned char*)malloc( osds->shape_width );
        memset( osds->shape_mask, 5, osds->shape_width );
        break;

    case OSD_Circle:
        osds->shape_mask = (unsigned char*)malloc( shape_width * shape_width );
        memset( osds->shape_mask, 0, shape_width * shape_width );

        x0 = (double)(((double)shape_width)/2.0);
        radius_sqrd = pow((double)x0,(double)2);
        for( x = 0; x < shape_width; x++ ) {
            for( y = 0; y < shape_width; y++ ) {
                if( (pow((x-x0),(double)2) + pow((double)(y-x0),(double)2)) <= radius_sqrd ) {
                    osds->shape_mask[y*shape_width+x] = (char)5;
                }
            }
        }

        break;

    default:
        osds->shape_mask = NULL;
        break;
    }

    return osds;
}

void osd_shape_delete( osd_shape_t *osds )
{
    if( osds->shape_mask ) free( osds->shape_mask );
    free( osds );
}

void osd_shape_set_colour( osd_shape_t *osds, int luma, int cb, int cr )
{
    osds->shape_luma = luma;
    osds->shape_cb = cb;
    osds->shape_cr = cr;
}

void osd_shape_show_shape( osd_shape_t *osds, int timeout )
{
    osds->frames_left = timeout;
}

int osd_shape_visible( osd_shape_t *osds )
{
    return ( osds->frames_left != 0 );
}

void osd_shape_advance_frame( osd_shape_t *osds )
{
    if( osds->frames_left > 0 ) {
        osds->frames_left--;
    }
}


unsigned char *osd_shape_get_scanline( osd_shape_t *osds, int line )
{
    unsigned char *scanline;

    switch( osds->type ) {
    case OSD_Rect:
        scanline = osds->shape_mask;
        break;
    case OSD_Circle:
        scanline = (unsigned char*)(osds->shape_mask + osds->shape_width*line);
        break;
    default:
        scanline = NULL;
        break;
    }

    return scanline;
}

void osd_shape_composite_packed422( osd_shape_t *osds, unsigned char *output,
                                    int width, int height, int stride,
                                    int xpos, int ypos )
{
    int dest_x, dest_y, src_x, src_y, blit_w, blit_h;
    unsigned char *scanline;


    src_x = 0;
    src_y = 0;
    dest_x = xpos;
    dest_y = ypos;
    blit_w = osds->shape_width;
    blit_h = osds->shape_height;

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
        int i;

        output += dest_y * stride;
        for( i = 0; i < blit_h; i++ ) {
            scanline = osd_shape_get_scanline( osds, i );

            if( scanline )
            composite_textmask_packed422_scanline( output + ((dest_x) * 2),
                                                   output + ((dest_x) * 2),
                                                   scanline,
                                                   blit_w, osds->shape_luma, 
                                                   osds->shape_cb, 
                                                   osds->shape_cr, 
                                                   ((double) osds->frames_left) / 50.0 );
            output += stride;
        }
    }
}

