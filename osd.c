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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "videotools.h"
#include "speedy.h"
#include "pnginput.h"
#include "efs.h"
#include "osd.h"

struct osd_string_s
{
    efont_t *font;
    efont_string_t *efs;
    int frames_left;

    int text_luma;
    int text_cb;
    int text_cr;

    int show_border;
    int border_luma;
    int border_cb;
    int border_cr;

    unsigned char *image4444;
    int image_width;
    int image_height;
    int image_textwidth;
    int image_textheight;
};

osd_string_t *osd_string_new( const char *fontfile, int fontsize,
                              int video_width, int video_height, double video_aspect )
{
    osd_string_t *osds = (osd_string_t *) malloc( sizeof( osd_string_t ) );
    osds->font = efont_new( fontfile, fontsize, video_width, video_height, video_aspect );
    osds->efs = efs_new( osds->font );
    osds->frames_left = 0;

    osds->text_luma = 16;
    osds->text_cb = 128;
    osds->text_cr = 128;

    osds->show_border = 0;
    osds->border_luma = 16;
    osds->border_cb = 128;
    osds->border_cr = 128;

    osds->image4444 = (unsigned char *) malloc( video_width * video_height * 4 );
    osds->image_width = video_width;
    osds->image_height = video_height;
    osds->image_textwidth = 0;
    osds->image_textheight = 0;

    return osds;
}

void osd_string_delete( osd_string_t *osds )
{
    efs_delete( osds->efs );
    efont_delete( osds->font );
    free( osds );
}

void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr )
{
    osds->text_luma = luma;
    osds->text_cb = cb;
    osds->text_cr = cr;
}

void osd_string_show_border( osd_string_t *osds, int show_border )
{
    osds->show_border = show_border;
}

void osd_string_set_border_colour( osd_string_t *osds, int luma, int cb, int cr )
{
    osds->border_luma = luma;
    osds->border_cb = cb;
    osds->border_cr = cr;
}

void osd_string_render_image4444( osd_string_t *osds )
{
    osds->image_textwidth = efs_get_width( osds->efs ) + 4;
    osds->image_textheight = efs_get_height( osds->efs ) + 4;

    blit_colour_packed4444( osds->image4444, osds->image_textwidth,
                            osds->image_textheight, osds->image_width * 4,
                            0, 16, 128, 128 );

    if( osds->show_border ) {
        composite_alphamask_packed4444( osds->image4444, osds->image_width,
                                        osds->image_height, osds->image_width * 4,
                                        efs_get_buffer( osds->efs ),
                                        efs_get_width( osds->efs ),
                                        efs_get_height( osds->efs ),
                                        efs_get_stride( osds->efs ),
                                        osds->border_luma, osds->border_cb,
                                        osds->border_cr, 0, 0 );
        composite_alphamask_packed4444( osds->image4444, osds->image_width,
                                        osds->image_height, osds->image_width * 4,
                                        efs_get_buffer( osds->efs ),
                                        efs_get_width( osds->efs ),
                                        efs_get_height( osds->efs ),
                                        efs_get_stride( osds->efs ),
                                        osds->border_luma, osds->border_cb,
                                        osds->border_cr, 4, 0 );
        composite_alphamask_packed4444( osds->image4444, osds->image_width,
                                        osds->image_height, osds->image_width * 4,
                                        efs_get_buffer( osds->efs ),
                                        efs_get_width( osds->efs ),
                                        efs_get_height( osds->efs ),
                                        efs_get_stride( osds->efs ),
                                        osds->border_luma, osds->border_cb,
                                        osds->border_cr, 0, 4 );
        composite_alphamask_packed4444( osds->image4444, osds->image_width,
                                        osds->image_height, osds->image_width * 4,
                                        efs_get_buffer( osds->efs ),
                                        efs_get_width( osds->efs ),
                                        efs_get_height( osds->efs ),
                                        efs_get_stride( osds->efs ),
                                        osds->border_luma, osds->border_cb,
                                        osds->border_cr, 4, 4 );
    }

    composite_alphamask_packed4444( osds->image4444, osds->image_width,
                                    osds->image_height, osds->image_width * 4,
                                    efs_get_buffer( osds->efs ), efs_get_width( osds->efs ),
                                    efs_get_height( osds->efs ), efs_get_stride( osds->efs ),
                                    osds->text_luma, osds->text_cb, osds->text_cr, 2, 2 );
}

void osd_string_show_text( osd_string_t *osds, const char *text, int timeout )
{
    efs_set_text( osds->efs, text );
    osd_string_render_image4444( osds );
    osds->frames_left = timeout;
}

void osd_string_set_timeout( osd_string_t *osds, int timeout )
{
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
    if( !osds->efs ) return;
    if( !osds->frames_left ) return;

    if( rightjustified ) xpos -= osds->image_textwidth;
    if( osds->frames_left < 50 ) {
        int alpha = (int) ( ( ( ( (double) osds->frames_left ) / 50.0 ) * 256.0 ) + 0.5 );
        composite_packed4444_alpha_to_packed422( output, width, height, stride,
                                                 osds->image4444, osds->image_textwidth,
                                                 osds->image_textheight, osds->image_width*4,
                                                 xpos, ypos, alpha );
    } else {
        composite_packed4444_to_packed422( output, width, height, stride,
                                           osds->image4444, osds->image_textwidth,
                                           osds->image_textheight, osds->image_width*4,
                                           xpos, ypos );
    }
}


/* Shape functions */
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
                    osds->shape_mask[y*shape_width+x] = 255;
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

struct osd_graphic_s
{
    pnginput_t *png;
    int frames_left;

    unsigned char *image4444;
    int image_width;
    int image_height;
    double image_aspect;
    int image_adjusted_width;
    int alpha;
};

osd_graphic_t *osd_graphic_new( const char *filename, int video_width,
                                int video_height, double video_aspect )
{
    osd_graphic_t *osdg = (osd_graphic_t *) malloc( sizeof( struct osd_graphic_s ) );
    if( !osdg ) {
        return NULL;
    }

    osdg->png = pnginput_new( filename );
    if( !osdg->png ) {
        free( osdg );
        return NULL;
    }

    osdg->frames_left = 0;
    osdg->image4444 = (unsigned char *)malloc( video_width * video_height * 4);
    if( !osdg->image4444 ) {
        pnginput_delete( osdg->png );
        free( osdg );
        return NULL;
    }
    osdg->image_width = video_width;
    osdg->image_height = video_height;
    osdg->image_aspect = video_aspect / (double)(((double)osdg->image_width)/((double)osdg->image_height));
    osdg->alpha = 255;

    return osdg;
}

void osd_graphic_delete( osd_graphic_t *osdg )
{
    if( !osdg ) return;

    pnginput_delete( osdg->png );
    free( osdg->image4444 );
    free( osdg );
}

void composite_packed444_to_packed4444_alpha_scanline( unsigned char *output, 
                                                       unsigned char *input,
                                                       int width, int alpha )
{
    int i;

    if( !alpha ) return;

    for( i = 0; i < width; i++ ) {
        output[ 0 ] = alpha & 0xff;
        output[ 1 ] = input[ 0 ] & 0xff;
        output[ 2 ] = input[ 1 ] & 0xff;
        output[ 3 ] = input[ 2 ] & 0xff;

        output += 4;
        input += 3;
    }

}

int aspect_adjust_packed4444_scanline( unsigned char *output,
                                       unsigned char *input, 
                                       int width,
                                       double aspectratio )
{
    double i;
    int w=0, prev_i=0, j;
    double avg_a=0.0, avg_y=0.0, avg_cb=0.0, avg_cr=0.0, c=0.0;
    unsigned char *curin;

    for( i=0; i < width; i += aspectratio ) {
        curin = input + ((int)i)*4;

        if( !prev_i ) {
            output[ 0 ] = curin[ 0 ];
            output[ 1 ] = curin[ 1 ];
            output[ 2 ] = curin[ 2 ];
            output[ 3 ] = curin[ 3 ];
        } else {
            avg_a = 0.0;
            avg_y = 0.0;
            avg_cb = 0.0;
            avg_cr = 0.0;
            for( c=0,j=prev_i; j <= (int)i; j++,c++ ) {
                avg_a += input[ j*4 ];
                avg_y += input[ j*4 + 1 ];
                avg_cb += input[ j*4 + 2 ];
                avg_cr += input[ j*4 + 3 ];
            }
            avg_a /= c;
            avg_y /= c;
            avg_cb /= c;
            avg_cr /= c;
            output[ 0 ] = avg_a;
            output[ 1 ] = avg_y;
            output[ 2 ] = avg_cb;
            output[ 3 ] = avg_cr;
        }
        output += 4;
        prev_i = (int)i;
        w++;
    }

    return w;
}

void osd_graphic_render_image4444( osd_graphic_t *osdg )
{
    int i, width, height;
    unsigned char *scanline;
    unsigned char *cb444;
    unsigned char *curout;
    int has_alpha = pnginput_has_alpha( osdg->png );

    width = pnginput_get_width( osdg->png );
    height = pnginput_get_height( osdg->png );

    cb444 = (unsigned char *) malloc( (width * 3) );
    if( !cb444 ) return;


    curout = (unsigned char *)malloc( width*4 );
    if( !curout) return;


    for( i=0; i < height; i++ ) {

        scanline = pnginput_get_scanline( osdg->png, i );
        if( has_alpha ) {
            rgba32_to_packed4444_rec601_scanline( curout, scanline, width );
        } else {
            rgb24_to_packed444_rec601_scanline( cb444, scanline, width );
            composite_packed444_to_packed4444_alpha_scanline( curout, 
                                                              cb444, 
                                                              width, 
                                                              255 );
        }
        osdg->image_adjusted_width = aspect_adjust_packed4444_scanline( 
                                           osdg->image4444+(i*osdg->image_width*4),
                                           curout, 
                                           width, 
                                           osdg->image_aspect );
    }

    free( curout );
    free( cb444 );
}

void osd_graphic_show_graphic( osd_graphic_t *osdg, int timeout, int alpha )
{
    osdg->frames_left = timeout;
    osdg->alpha = alpha;
    osd_graphic_render_image4444( osdg );
}

int osd_graphic_visible( osd_graphic_t *osdg )
{
    return (osdg->frames_left != 0);
}

void osd_graphic_advance_frame( osd_graphic_t *osdg )
{
    if( osdg->frames_left > 0)
        osdg->frames_left--;
}



void osd_graphic_composite_packed422( osd_graphic_t *osdg, 
                                      unsigned char *output,
                                      int width, int height, int stride,
                                      int xpos, int ypos )
{
    int alpha;

    if( !osdg->png ) return;
    if( !osdg->frames_left ) return;

    if( osdg->frames_left < 50 ) {
        alpha = (int) ( ( ( ( (double) osdg->frames_left ) / 50.0 ) * osdg->alpha ) + 0.5 );
    } else {
        alpha = osdg->alpha;
    }

    composite_packed4444_alpha_to_packed422( output, width, height, stride,
                                       osdg->image4444, 
                                       osdg->image_adjusted_width,
                                       pnginput_get_height( osdg->png ),
                                       osdg->image_width*4,
                                       xpos, ypos, alpha );
}
