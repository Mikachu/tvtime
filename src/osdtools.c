/**
 * Copyright (C) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include <math.h>
#include <ctype.h>
#include "videotools.h"
#include "speedy.h"
#include "pnginput.h"
#include "leetft.h"
#include "osdtools.h"
#include "utils.h"

#define OSD_FADEOUT_TIME 25

#define MEMORY_PER_STRING (256*1024)

#define OSD_LIST_MAX_LINES 16

struct osd_font_s
{
    ft_font_t *font;
};

osd_font_t *osd_font_new( const char *fontfile, int fontsize, double pixel_aspect )
{
    osd_font_t *font = malloc( sizeof( osd_font_t ) );
    char *fontfilename;

    if( !font ) {
        return 0;
    }

    fontfilename = get_tvtime_file( fontfile );
    if( !fontfilename ) {
        fprintf( stderr, "osd_font: Can't find font '%s'.\nosd_font: Looked in %s\n",
                 fontfile, get_tvtime_paths() );
        free( font );
        return 0;
    }

    font->font = ft_font_new( fontfilename, fontsize, pixel_aspect );
    free( fontfilename );

    if( !font->font ) {
        fprintf( stderr, "osd_font: Can't open font '%s'\n", fontfile );
        free( font );
        return 0;
    }

    return font;
}

void osd_font_delete( osd_font_t *font )
{
    ft_font_delete( font->font );
    free( font );
}

void osd_font_set_pixel_aspect( osd_font_t *font, double pixel_aspect )
{
    ft_font_set_pixel_aspect( font->font, pixel_aspect );
}

static ft_font_t *osd_font_get_font( osd_font_t *font )
{
    return font->font;
}

struct osd_string_s
{
    osd_font_t *font;
    ft_string_t *fts;
    int frames_left;
    char curtext[ 256 ];
    int hold;

    int text_luma;
    int text_cb;
    int text_cr;

    int show_border;
    int border_luma;
    int border_cb;
    int border_cr;

    uint8_t image4444[ MEMORY_PER_STRING ];
    int image_size;
    int image_textwidth;
    int image_textheight;
};

osd_string_t *osd_string_new( osd_font_t *font )
{
    osd_string_t *osds = malloc( sizeof( osd_string_t ) );

    if( !osds ) {
        return 0;
    }

    osds->font = font;
    osds->fts = ft_string_new( osd_font_get_font( osds->font ) );
    if( !osds->fts ) {
        fprintf( stderr, "osd_string: Can't create string.\n" );
        free( osds );
        return 0;
    }

    /* Initially, our string is empty. */
    memset( osds->curtext, 0, sizeof( osds->curtext ) );
    osds->frames_left = 0;
    osds->hold = 0;

    osds->text_luma = 16;
    osds->text_cb = 128;
    osds->text_cr = 128;

    osds->show_border = 0;
    osds->border_luma = 16;
    osds->border_cb = 128;
    osds->border_cr = 128;

    osds->image_size = sizeof( osds->image4444 );
    osds->image_textwidth = 0;
    osds->image_textheight = 0;

    return osds;
}

void osd_string_delete( osd_string_t *osds )
{
    ft_string_delete( osds->fts );
    free( osds );
}

void osd_string_set_hold( osd_string_t *osds, int hold )
{
    osds->hold = hold;
}

void osd_string_set_colour( osd_string_t *osds, int luma, int cb, int cr )
{
    osds->text_luma = luma;
    osds->text_cb = cb;
    osds->text_cr = cr;
}

void osd_string_set_colour_rgb( osd_string_t *osds, int r, int g, int b )
{
    uint8_t rgb[ 3 ];
    uint8_t ycbcr[ 3 ];

    rgb[ 0 ] = r; rgb[ 1 ] = g; rgb[ 2 ] = b;
    rgb24_to_packed444_rec601_scanline( ycbcr, rgb, 1 );

    osds->text_luma = ycbcr[ 0 ];
    osds->text_cb = ycbcr[ 1 ];
    osds->text_cr = ycbcr[ 2 ];
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

static void osd_string_render_bordered_image4444( osd_string_t *osds, const char *text )
{
    int stringwidth, stringheight;
    ft_font_t *font = osd_font_get_font( osds->font );
    int left_x, right_x;
    int top_y, bottom_y;
    int textpos_x, textpos_y;

    left_x = top_y = 0;
    right_x = ft_font_points_to_subpix_width( font, 2 );
    bottom_y = 2;
    textpos_x = ft_font_points_to_subpix_width( font, 1 );
    textpos_y = 1;

    ft_string_set_text( osds->fts, text, 0 );
    stringwidth = ft_string_get_width( osds->fts );
    stringheight = ft_string_get_height( osds->fts );

    osds->image_textwidth = ft_string_get_width( osds->fts ) + ( (right_x + 32768) / 65536 );
    osds->image_textheight = ft_string_get_height( osds->fts ) + bottom_y;

    if( osds->image_textwidth * osds->image_textheight * 4 > osds->image_size ) {
        osds->image_textwidth = (((osds->image_size/4) / osds->image_textheight) | 0xf) - 0xf;
    }

    /* TODO: Only blit size of data if < full text size. */
    blit_colour_packed4444( osds->image4444, osds->image_textwidth,
                            osds->image_textheight, osds->image_textwidth * 4,
                            0, 0, 0, 0 );

    composite_alphamask_to_packed4444( osds->image4444, osds->image_textwidth,
                                       osds->image_textheight, osds->image_textwidth * 4,
                                       ft_string_get_buffer( osds->fts ),
                                       stringwidth, stringheight,
                                       ft_string_get_stride( osds->fts ),
                                       osds->border_luma, osds->border_cb,
                                       osds->border_cr, 0, 0 );
    composite_alphamask_to_packed4444( osds->image4444, osds->image_textwidth,
                                       osds->image_textheight, osds->image_textwidth * 4,
                                       ft_string_get_buffer( osds->fts ),
                                       stringwidth, stringheight,
                                       ft_string_get_stride( osds->fts ),
                                       osds->border_luma, osds->border_cb,
                                       osds->border_cr, 0, bottom_y );
    ft_string_set_text( osds->fts, text, right_x );
    stringwidth = ft_string_get_width( osds->fts );
    stringheight = ft_string_get_height( osds->fts );
    composite_alphamask_to_packed4444( osds->image4444, osds->image_textwidth,
                                       osds->image_textheight, osds->image_textwidth * 4,
                                       ft_string_get_buffer( osds->fts ),
                                       stringwidth, stringheight,
                                       ft_string_get_stride( osds->fts ),
                                       osds->border_luma, osds->border_cb,
                                       osds->border_cr, 0, 0 );
    composite_alphamask_to_packed4444( osds->image4444, osds->image_textwidth,
                                       osds->image_textheight, osds->image_textwidth * 4,
                                       ft_string_get_buffer( osds->fts ),
                                       stringwidth, stringheight,
                                       ft_string_get_stride( osds->fts ),
                                       osds->border_luma, osds->border_cb,
                                       osds->border_cr, 0, bottom_y );

    ft_string_set_text( osds->fts, text, textpos_x );
    stringwidth = ft_string_get_width( osds->fts );
    stringheight = ft_string_get_height( osds->fts );

    composite_alphamask_to_packed4444( osds->image4444, osds->image_textwidth,
                                       osds->image_textheight, osds->image_textwidth * 4,
                                       ft_string_get_buffer( osds->fts ),
                                       stringwidth, stringheight,
                                       ft_string_get_stride( osds->fts ),
                                       osds->text_luma, osds->text_cb, osds->text_cr, 0, textpos_y );
}

static void osd_string_render_plain_image4444( osd_string_t *osds, const char *text )
{
    int stringwidth, stringheight;

    ft_string_set_text( osds->fts, text, 0 );
    stringwidth = ft_string_get_width( osds->fts );
    stringheight = ft_string_get_height( osds->fts );

    osds->image_textwidth = ft_string_get_width( osds->fts );
    osds->image_textheight = ft_string_get_height( osds->fts );

    if( osds->image_textwidth * osds->image_textheight * 4 > osds->image_size ) {
        osds->image_textwidth = (((osds->image_size/4) / osds->image_textheight) | 0xf) - 0xf;
    }

    blit_colour_packed4444( osds->image4444, osds->image_textwidth,
                            osds->image_textheight, osds->image_textwidth * 4,
                            0, 0, 0, 0 );

    composite_alphamask_to_packed4444( osds->image4444, osds->image_textwidth,
                                       osds->image_textheight, osds->image_textwidth * 4,
                                       ft_string_get_buffer( osds->fts ),
                                       stringwidth, stringheight,
                                       ft_string_get_stride( osds->fts ),
                                       osds->text_luma, osds->text_cb, osds->text_cr, 0, 0 );
}

void osd_string_show_text( osd_string_t *osds, const char *text, int timeout )
{
    if( strcmp( text, osds->curtext ) ) {
        if( osds->show_border ) {
            osd_string_render_bordered_image4444( osds, text );
        } else {
            osd_string_render_plain_image4444( osds, text );
        }
        snprintf( osds->curtext, sizeof( osds->curtext ), "%s", text );
    }
    osds->frames_left = timeout;
}

void osd_string_rerender( osd_string_t *osds )
{
    if( osds->show_border ) {
        osd_string_render_bordered_image4444( osds, osds->curtext );
    } else {
        osd_string_render_plain_image4444( osds, osds->curtext );
    }
}

int osd_string_get_ascent( osd_string_t *osds )
{
    return ft_string_get_ascent( osds->fts );
}

int osd_string_get_width( osd_string_t *osds )
{
    return osds->image_textwidth;
}

int osd_string_get_height( osd_string_t *osds )
{
    return osds->image_textheight;
}

void osd_string_set_timeout( osd_string_t *osds, int timeout )
{
    osds->frames_left = timeout;
}

int osd_string_visible( osd_string_t *osds )
{
    return (osds->frames_left > 0);
}

void osd_string_advance_frame( osd_string_t *osds )
{
    if( !osds->hold && osds->frames_left > 0 ) {
        osds->frames_left--;
    }
}

int osd_string_get_frames_left( osd_string_t *osds )
{
    return osds->frames_left;
}

void osd_string_composite_packed422_scanline( osd_string_t *osds,
                                              uint8_t *output,
                                              uint8_t *background,
                                              int width, int xpos,
                                              int scanline )
{
    if( osds->frames_left ) {
        if( scanline < osds->image_textheight && xpos < osds->image_textwidth ) {

            if( (xpos+width) > osds->image_textwidth ) {
                width = osds->image_textwidth - xpos;
            }

            if( osds->frames_left < OSD_FADEOUT_TIME ) {
                int alpha;
                alpha = (int) (((((double) osds->frames_left) / ((double) OSD_FADEOUT_TIME)) * 256.0) + 0.5);
                composite_packed4444_alpha_to_packed422_scanline( output, background,
                    osds->image4444 + (osds->image_textwidth*4*scanline) + (xpos*4),
                    width, alpha );
            } else {
                composite_packed4444_to_packed422_scanline( output, background,
                    osds->image4444 + (osds->image_textwidth*4*scanline) + (xpos*4),
                    width );
            }
        }
    }
}

static int load_png_to_packed4444( uint8_t *buffer, int *active,
                                   int width, int height, int stride,
                                   double pixel_aspect, pnginput_t *pngin )
{
    int has_alpha = pnginput_has_alpha( pngin );
    int pngwidth, pngheight;
    uint8_t *cb444;
    uint8_t *curout;
    int i;

    pngwidth = pnginput_get_width( pngin );
    pngheight = pnginput_get_height( pngin );

    cb444 = malloc( pngwidth * 3 );
    if( !cb444 ) return 0;

    curout = malloc( pngwidth * 4 );
    if( !curout ) {
        free( cb444 );
        return 0;
    }

    for( i = 0; i < height; i++ ) {
        uint8_t *scanline = pnginput_get_scanline( pngin, i );
        uint8_t *outputscanline = buffer + (stride * i);

        if( has_alpha ) {
            rgba32_to_packed4444_rec601_scanline( curout, scanline, pngwidth );
            premultiply_packed4444_scanline( curout, curout, pngwidth );
        } else {
            rgb24_to_packed444_rec601_scanline( cb444, scanline, pngwidth );
            packed444_to_nonpremultiplied_packed4444_scanline( curout, cb444, pngwidth, 255 );
        }

        aspect_adjust_packed4444_scanline( outputscanline, curout, pngwidth, pixel_aspect );
        if( active ) {
            int x;

            active[ i ] = 0;
            for( x = 0; x < width; x++ ) {
                if( outputscanline[ (x * 4) ] ) {
                    active[ i ] = 1;
                    break;
                }
            }
        }
    }

    free( curout );
    free( cb444 );

    return 1;
}

/* Graphic functions */
struct osd_animation_s
{
    int numframes;
    uint8_t *frames4444;
    int paused;
    int curtime;
    int image_width;
    int image_height;
    int image_size;
    int frames_left;
    int alpha;
    int frametime;
    int hold;
    uint8_t *curframe;
};

static int animation_get_num_frames( const char *filename_base )
{
    int numframes = 0;

    for(;;) {
        char curfilename[ 1024 ];
        char *fullfilename;

        snprintf( curfilename, sizeof( curfilename ), "%s_%04d.png", filename_base, numframes );
        fullfilename = get_tvtime_file( curfilename );
        if( !fullfilename ) {
            return numframes;
        }

        free( fullfilename );
        numframes++;
    }
}

osd_animation_t *osd_animation_new( const char *filename_base,
                                    double pixel_aspect, int alpha, int frametime )
{
    osd_animation_t *osda = malloc( sizeof( osd_animation_t ) );
    char curfilename[ 1024 ];
    char *fullfilename;
    pnginput_t *pngin;
    int i;

    if( !osda ) {
        return 0;
    }

    osda->numframes = animation_get_num_frames( filename_base );
    if( !osda->numframes ) {
        free( osda );
        return 0;
    }

    /* Get info from the first frame. */
    snprintf( curfilename, sizeof( curfilename ), "%s_%04d.png", filename_base, 0 );
    fullfilename = get_tvtime_file( curfilename );
    pngin = pnginput_new( fullfilename );
    free( fullfilename );

    if( !pngin ) {
        free( osda );
        return 0;
    }

    osda->hold = 0;
    osda->curtime = 0;
    osda->paused = 0;
    osda->frametime = frametime;
    osda->frames_left = 0;
    osda->alpha = alpha;
    osda->image_width = (int) (( ((double) pnginput_get_width( pngin )) * pixel_aspect ) + 1.5);
    osda->image_height = pnginput_get_height( pngin );
    osda->image_size = osda->image_width * osda->image_height * 4;
    pnginput_delete( pngin );

    osda->frames4444 = malloc( osda->numframes * osda->image_size );
    if( !osda->frames4444 ) {
        free( osda );
        return 0;
    }
    osda->curframe = osda->frames4444;

    /* Load each frame. */
    for( i = 0; i < osda->numframes; i++ ) {
        snprintf( curfilename, sizeof( curfilename ), "%s_%04d.png", filename_base, i );
        fullfilename = get_tvtime_file( curfilename );
        pngin = pnginput_new( fullfilename );
        free( fullfilename );

        if( !load_png_to_packed4444( osda->frames4444 + (i * osda->image_size), 0,
                                     osda->image_width, osda->image_height, osda->image_width * 4,
                                     pixel_aspect, pngin ) ) {
            fprintf( stderr, "osd_animation: Can't render image '%s'.\n", curfilename );
            pnginput_delete( pngin );
            free( osda->frames4444 );
            free( osda );
            return 0;
        }
        pnginput_delete( pngin );
    }

    return osda;
}

void osd_animation_delete( osd_animation_t *osda )
{
    free( osda->frames4444 );
    free( osda );
}

int osd_animation_get_width( osd_animation_t *osda )
{
    return osda->image_width;
}

int osd_animation_get_height( osd_animation_t *osda )
{
    return osda->image_height;
}

void osd_animation_pause( osd_animation_t *osda, int pause )
{
    osda->paused = pause;
}

void osd_animation_seek( osd_animation_t *osda, double pos )
{
    osda->curtime = osda->frametime * ((int) ((pos * (double) (osda->numframes - 1)) + 0.5));
    osda->curframe = osda->frames4444 + ((osda->curtime / osda->frametime) * osda->image_size);
}

void osd_animation_set_hold( osd_animation_t *osda, int hold )
{
    osda->hold = hold;
}

void osd_animation_set_timeout( osd_animation_t *osda, int timeout )
{
    osda->frames_left = timeout;
}

int osd_animation_visible( osd_animation_t *osda )
{
    return (osda->frames_left > 0);
}

void osd_animation_advance_frame( osd_animation_t *osda )
{
    if( osda->frames_left > 0 ) {
        if( !osda->paused ) {
            osda->curtime = (osda->curtime + 1) % (osda->frametime * osda->numframes);
            osda->curframe = osda->frames4444 + ((osda->curtime / osda->frametime) * osda->image_size);
        }
        if( !osda->hold ) osda->frames_left--;
    }
}

void osd_animation_composite_packed422_scanline( osd_animation_t *osda,
                                                 uint8_t *output,
                                                 uint8_t *background,
                                                 int width, int xpos,
                                                 int scanline )
{
    if( osda->frames_left ) {
        if( scanline < osda->image_height && xpos < osda->image_width ) {
            int alpha;

            if( (xpos+width) > osda->image_width ) {
                width = osda->image_width - xpos;
            }

            if( osda->frames_left < OSD_FADEOUT_TIME ) {
                alpha = (int) ( ( ( ( (double) osda->frames_left ) / ((double) OSD_FADEOUT_TIME) ) * osda->alpha ) + 0.5 );
            } else {
                alpha = osda->alpha;
            }

            composite_packed4444_alpha_to_packed422_scanline( output, background,
                osda->curframe + (osda->image_width*scanline*4) + (xpos*4),
                width, alpha );
        }
    }
}

struct osd_list_s
{
    double aspect;
    int frames_left;
    int hold;
    int numlines;
    int hilight;
    int width;
    int minwidth;
    int height;
    osd_string_t *lines[ OSD_LIST_MAX_LINES ];
};

osd_list_t *osd_list_new( osd_font_t *font, double pixel_aspect )
{
    osd_list_t *osdl = malloc( sizeof( osd_list_t ) );
    int i;

    if( !osdl ) {
        return 0;
    }

    osdl->hilight = -1;
    osdl->numlines = 0;
    osdl->frames_left = 0;
    osdl->width = 0;
    osdl->minwidth = 0;
    osdl->height = 0;
    osdl->hold = 0;
    memset( osdl->lines, 0, sizeof( osdl->lines ) );

    for( i = 0; i < OSD_LIST_MAX_LINES; i++ ) {
        osdl->lines[ i ] = osd_string_new( font );
        if( !osdl->lines[ i ] ) {
            osd_list_delete( osdl );
            return 0;
        }
        osd_string_show_text( osdl->lines[ i ], "Blank", 100 );
        if( !i ) {
            /* white */
            osd_string_set_colour( osdl->lines[ i ], 235, 128, 128 );
        } else {
            /* wheat */
            osd_string_set_colour_rgb( osdl->lines[ i ], 0xf5, 0xde, 0xb3 );
        }
        osd_string_show_border( osdl->lines[ i ], 1 );
    }

    return osdl;
}

void osd_list_delete( osd_list_t *osdl )
{
    int i;

    for( i = 0; i < OSD_LIST_MAX_LINES; i++ ) {
        if( osdl->lines[ i ] ) osd_string_delete( osdl->lines[ i ] );
    }
    free( osdl );
}

void osd_list_set_text( osd_list_t *osdl, int line, const char *text )
{
    if( line < OSD_LIST_MAX_LINES ) {
        osd_string_show_text( osdl->lines[ line ], text, 100 );
    }
}

int osd_list_get_width( osd_list_t *osdl )
{
    return osdl->width;
}

void osd_list_set_lines( osd_list_t *osdl, int numlines )
{
    if( numlines > OSD_LIST_MAX_LINES ) numlines = OSD_LIST_MAX_LINES;
    osdl->numlines = numlines;
}

void osd_list_set_hilight( osd_list_t *osdl, int pos )
{
    if( pos < OSD_LIST_MAX_LINES ) {
        if( osdl->hilight >= 0 ) {
            osd_string_set_colour_rgb( osdl->lines[ osdl->hilight ], 0xf5, 0xde, 0xb3 );
            osd_string_rerender( osdl->lines[ osdl->hilight ] );
        }
        osdl->hilight = pos;
        if( osdl->hilight >= 0 ) {
            osd_string_set_colour_rgb( osdl->lines[ osdl->hilight ],
                                       255, 255, 0 );
            osd_string_rerender( osdl->lines[ osdl->hilight ] );
        }
    }
}

int osd_list_get_hilight( osd_list_t *osdl )
{
    return osdl->hilight;
}

int osd_list_get_numlines( osd_list_t *osdl )
{
    return osdl->numlines;
}

void osd_list_set_timeout( osd_list_t *osdl, int timeout )
{
    int i;

    osdl->frames_left = timeout;
    for( i = 0; i < osdl->numlines; i++ ) {
        osd_string_set_timeout( osdl->lines[ i ], timeout );
    }

    if( timeout ) {
        osdl->width = osdl->minwidth;
        osdl->height = 0;
        for( i = 0; i < osdl->numlines; i++ ) {
            int width = osd_string_get_width( osdl->lines[ i ] );
            int height = osd_string_get_height( osdl->lines[ i ] );
            if( width > osdl->width ) osdl->width = width;
            if( height > osdl->height ) osdl->height = height;
        }
    }
}

int osd_list_get_line_pos( osd_list_t *osdl, int y )
{
    int i;

    for( i = 0; i < osdl->numlines && y >= 0; i++ ) {
        if( y < osdl->height ) {
            if( !i ) return 1;
            return i;
        }
        y -= osdl->height;
    }

    return osdl->numlines - 1;
}

void osd_list_get_bounding_box( osd_list_t *osdl, int *x, int *y,
                                int *width, int *height )
{
    *x = 0;
    *y = 0;
    *width = osdl->width;
    *height = osdl->numlines * osdl->height;
}

int osd_list_visible( osd_list_t *osdl )
{
    return (osdl->numlines > 0 && osd_string_visible( osdl->lines[ 0 ] ));
}

void osd_list_rerender( osd_list_t *osdl )
{
    int i;

    for( i = 0; i < OSD_LIST_MAX_LINES; i++ ) {
        osd_string_rerender( osdl->lines[ i ] );
    }
}

void osd_list_set_hold( osd_list_t *osdl, int hold )
{
    osdl->hold = hold;
}

void osd_list_advance_frame( osd_list_t *osdl )
{
    int i;

    if( !osdl->hold && osdl->frames_left > 0 ) {
        osdl->frames_left--;
    }

    if( !osdl->hold ) {
        for( i = 0; i < osdl->numlines; i++ ) {
            osd_string_advance_frame( osdl->lines[ i ] );
        }
    }
}

void osd_list_composite_packed422_scanline( osd_list_t *osdl,
                                            uint8_t *output,
                                            uint8_t *background,
                                            int width, int xpos,
                                            int scanline )
{
    int i;

    for( i = 0; i < osdl->numlines && scanline >= 0; i++ ) {
        if( scanline < osdl->height ) {
            int bgwidth = osdl->width - xpos;
            int alpha255, alpha200;

            if( bgwidth > width ) bgwidth = width;

            alpha255 = (int) (((((double) osdl->frames_left) / ((double) OSD_FADEOUT_TIME)) * 255.0) + 0.5);
            alpha200 = (int) (((((double) osdl->frames_left) / ((double) OSD_FADEOUT_TIME)) * 200.0) + 0.5);

            if( alpha255 > 255 ) alpha255 = 255;
            if( alpha200 > 200 ) alpha200 = 200;

            if( !i ) {
                /* tvtime blue */
                composite_colour4444_alpha_to_packed422_scanline( output, output, 255, 123, 150, 124, bgwidth, alpha255 );
            } else if( i == osdl->hilight ) {
                /* white */
                composite_colour4444_alpha_to_packed422_scanline( output, output, 255, 235, 128, 128, bgwidth, alpha200 );
            } else {
                /* gray */
                composite_colour4444_alpha_to_packed422_scanline( output, output, 255,  100, 140, 128, bgwidth, alpha200 );
            }

            osd_string_composite_packed422_scanline( osdl->lines[ i ],
                                                     output, background,
                                                     width, xpos, scanline );
        }
        scanline -= osdl->height;
    }
}

struct osd_rect_s
{
    int frames_left;
    int width;
    int height;
    int a, l, cb, cr;
};

osd_rect_t *osd_rect_new( void )
{
    osd_rect_t *osdr = malloc( sizeof( osd_rect_t ) );

    if( !osdr ) {
        return 0;
    }

    osdr->frames_left = 0;
    osdr->width = 0;
    osdr->height = 0;
    return osdr;
}

void osd_rect_delete( osd_rect_t *osdr )
{
    free( osdr );
}

void osd_rect_set_colour( osd_rect_t *osdr, int alpha, int luma, int cb, int cr )
{
    osdr->a = alpha;
    osdr->l = luma;
    osdr->cb = cb;
    osdr->cr = cr;
}

void osd_rect_set_colour_rgb( osd_rect_t *osdr, int alpha, int r, int g, int b )
{
    uint8_t rgb[ 3 ];
    uint8_t ycbcr[ 3 ];

    rgb[ 0 ] = r; rgb[ 1 ] = g; rgb[ 2 ] = b;
    rgb24_to_packed444_rec601_scanline( ycbcr, rgb, 1 );

    osdr->a = alpha;
    osdr->l = ycbcr[ 0 ];
    osdr->cb = ycbcr[ 1 ];
    osdr->cr = ycbcr[ 2 ];
}

void osd_rect_set_timeout( osd_rect_t *osdr, int timeout )
{
    osdr->frames_left = timeout;
}

int osd_rect_visible( osd_rect_t *osdr )
{
    return (osdr->frames_left > 0);
}

void osd_rect_advance_frame( osd_rect_t *osdr )
{
    if( osdr->frames_left > 0 ) {
        osdr->frames_left--;
    }
}

void osd_rect_set_size( osd_rect_t *osdr, int width, int height )
{
    osdr->width = width;
    osdr->height = height;
}

void osd_rect_composite_packed422_scanline( osd_rect_t *osdr, uint8_t *output,
                                            uint8_t *background, int width, int xpos,
                                            int scanline )
{
    if( scanline < osdr->height && xpos < osdr->width ) {
        int alpha = (int) (((((double) osdr->frames_left) / ((double) OSD_FADEOUT_TIME)) * 256.0) + 0.5);
        if( alpha > 256 ) alpha = 256;
        composite_colour4444_alpha_to_packed422_scanline( output, output, osdr->a, osdr->l,
                                                          osdr->cb, osdr->cr, osdr->width - xpos, alpha );
    }
}

void osd_list_set_minimum_width( osd_list_t *osdl, int minwidth )
{
    osdl->minwidth = minwidth;
}

int osd_list_set_multitext( osd_list_t *osdl, int cur, const char *textarg,
                             int numlines, int maxwidth )
{
    static const char ellipsis[] = "...";
    const size_t ellipsislen = strlen( ellipsis );
    int textlen;
    int lastline = cur + numlines;
    char textbuf[4096];
    char *text = textbuf;
    int width;
    int breakpos;
    int maxwidthpos;
    int pos;

    /*
     * We make a copy of the textarg, since we'll want to be able to do
     * changes to our local copy.
     */
    snprintf( text, 4096, "%s", textarg );
    
    while( cur <= lastline ) {
        /* Eat leading whitespace */
        while( isspace( *text ) )
            text++, textarg++;
        
        textlen = strlen( text );
        if( textlen == 0 )
            break;

        /*
         * Seek to the end of the line. This has to been done iteratively,
         * since we have to decode the UTF-8 to skip over any tail bytes.
         */
        pos = 0;
        width = 0;
        while( width < maxwidth ) {
            width++;
            /* Skip tail bytes */
            while( ( text[ ++pos ] & 0xc0 ) == 0x80 )
                ;
            if( text[pos] == '\0' ) {
                breakpos = pos;
                goto breaknow;
            }
        }
        maxwidthpos = pos;

        breakpos = -1;
        for( pos = 0; pos <= maxwidthpos; pos++ ) {
            if( isspace( text[ pos ] ) || text[ pos ] == '\0' ) {
                breakpos = pos;
                if( text[ pos ] == '\n' )
                    break;
            }
        }
        if( breakpos == -1 ) {
            /* This place is as good as any to break... */
            breakpos = maxwidthpos;
        }

        if( cur == lastline ) {
            if( breakpos < textlen ) {
                /* Seek back to fit the ellipsis. */
                pos = breakpos;
                pos -= ellipsislen;
                /* Make sure to land at the start of a UTF-8 character. */
                while( ( text[ pos ] & 0xc0 ) == 0x80 )
                    pos--;
                /* Write the ellipsis. Bounds have been checked. */
                strcpy (text+pos, ellipsis);
            }
        }
    breaknow:
        text[ breakpos ] = '\0';
        osd_list_set_text( osdl, cur++, text );
        text[ breakpos ] = textarg[ breakpos ];
        text = &text[ breakpos ];
        textarg = &textarg[ breakpos ];
    }
    return cur;
}

