/*
 * Copyright (c) 2003 Achim Schneider <batchall@mordor.ch>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * See COPYING in the program root directory for more details.
 */

#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <libzvbi.h>
#include "leetft.h"
#include "osdtools.h"
#include "vtscreen.h"
#include "speedy.h"
#include "videotools.h"

#define FONT_SIZE 20
#define VT_ROWS 25

#define RENDER_NOTHING 0
#define RENDER_HEADER 1
#define RENDER_PAGE 2
struct vtscreen_s {
    vbi_pgno pgno;
    vbi_subno subno;
    vbi_decoder *dec;
    osd_font_t *medium;
    osd_font_t *mediumw;
    osd_font_t *mediumh;
    osd_font_t *mediumwh;
    int verbose;
    vbi_page pg;
    int rerender;
    uint8_t *buf4444;
    int cellh, cellw; // cellh % 3 == cellw % 2 == 0
    int ofsx, ofsy;
    int screenh, screenw;
};

// FIXME
struct osd_font_s
{
    ft_font_t *font;
};

vtscreen_t *vtscreen_new( int video_width, int video_height, 
                            double pixel_aspect, int verbose )
{
    vtscreen_t *vts= malloc( sizeof( vtscreen_t ) );
    if( !vts ) {
        return 0;
    }

    vts->medium= osd_font_new( "FreeMono.ttf", FONT_SIZE, pixel_aspect );
    if( !vts->medium ) {
        vtscreen_delete( vts );
        return 0;
    }
    vts->mediumw= osd_font_new( "FreeMono.ttf", FONT_SIZE, pixel_aspect / 2 );
    if( !vts->mediumw ) {
        vtscreen_delete( vts );
        return 0;
    }
    vts->mediumh= osd_font_new( "FreeMono.ttf", FONT_SIZE, pixel_aspect * 2 );
    if( !vts->mediumh ) {
        vtscreen_delete( vts );
        return 0;
    }
    vts->mediumwh= osd_font_new( "FreeMono.ttf", FONT_SIZE<<1, pixel_aspect );
    if( !vts->mediumwh ) {
        vtscreen_delete( vts );
        return 0;
    }


    vts->cellh= 6*3;
    vts->cellw= 6*2;
    vts->ofsx= 100;
    vts->ofsy= 100;
    vts->screenh= video_height;
    vts->screenw= video_width;
    vts->rerender= RENDER_PAGE;
    vts->pgno= vbi_dec2bcd( 100 );
    vts->subno= vbi_dec2bcd( 0 );
    
    vts->buf4444= malloc( 4 * video_width * video_height );
    if( !vts->buf4444 ) {
        vtscreen_delete( vts );
        return 0;
    }
    
    return vts;
}

void vtscreen_delete( vtscreen_t *vts )
{
}

void vtscreen_register_decoder( vtscreen_t *vts, vbi_decoder *dec ) 
{
    vts->dec= dec;
}


void vtscreen_notify_update( vtscreen_t *vts, vbi_pgno pgno, vbi_subno subno ) 
{  
    if( vts->pgno == pgno && vts->subno == subno ) {
        vts->rerender= RENDER_PAGE;
        fprintf(stderr, "%d\n", vbi_bcd2dec(pgno));
    }
    else {
        vts->rerender= RENDER_HEADER;
    }
}

uint8_t *cell2screen( vtscreen_t *vts, int x, int y )
{
    return vts->buf4444 + ((( y*vts->cellh + vts->ofsy ) * vts->screenw)<<2) + (( x*vts->cellw + vts->ofsx )<<2);
}

int render( vtscreen_t *vts )
{
    int x,y;
    int bgalpha, fgalpha;
    uint32_t abgr;
    uint8_t rgb[ 3 ];
    uint8_t bgycbcr[ 3 ];
    uint8_t fgycbcr[ 3 ];
    uint8_t *out;
    int width, height;
    int stride= vts->screenw<<2;
    vbi_page *pg= &(vts->pg);
    
    if( vbi_fetch_vt_page( vts->dec, pg, 
                           vts->pgno, vts->subno,
                           VBI_WST_LEVEL_1, 25, 0 ) ) {
        for( y=0; y < pg->rows; ++y ) {
            vbi_char *line= pg->text + y * pg->columns;
            for( x= 0; x < pg->columns; ++x ) {
                
                switch( line[x].opacity ) {
                    case VBI_TRANSPARENT_SPACE:
                        bgalpha= 0;
                        fgalpha= 0;
                        break;
                    case VBI_TRANSPARENT_FULL:
                        bgalpha= 0;
                        fgalpha= 255;
                        break;
                    case VBI_SEMI_TRANSPARENT:
                        bgalpha= 128;
                        fgalpha= 255;
                        break;
                    case VBI_OPAQUE:
                    default:
                        bgalpha= 255;
                        fgalpha= 255;
                }
                
                abgr= pg->color_map[line[x].background];
                rgb[ 0 ] = abgr & 0xff; rgb[ 1 ] = (abgr>>8) & 0xff; rgb[ 2 ] = (abgr>>16) & 0xff;
                rgb24_to_packed444_rec601_scanline( bgycbcr, rgb, 1 );
                
                abgr= pg->color_map[line[x].foreground];
                rgb[ 0 ] = abgr & 0xff; rgb[ 1 ] = (abgr>>8) & 0xff; rgb[ 2 ] = (abgr>>16) & 0xff;
                rgb24_to_packed444_rec601_scanline( fgycbcr, rgb, 1 );
                
                out= cell2screen( vts, x, y );
                switch( line[x].size ) {
                    case VBI_NORMAL_SIZE:
                        width= vts->cellw;
                        height= vts->cellh;
                        break;
                    case VBI_DOUBLE_WIDTH:
                        width= vts->cellw<<1;
                        height= vts->cellh;
                        break;
                    case VBI_DOUBLE_HEIGHT:
                        width= vts->cellw;
                        height= vts->cellh<<1;
                        break;
                    case VBI_DOUBLE_SIZE:
                        width= vts->cellw<<1;
                        height= vts->cellh<<1;
                        break;
                    case VBI_OVER_TOP:
                    case VBI_OVER_BOTTOM:
                    case VBI_DOUBLE_HEIGHT2:
                    case VBI_DOUBLE_SIZE2:
                    default:
						continue; 
                }
				blit_colour_packed4444( out, width, height, stride, 
											bgalpha, bgycbcr[0], bgycbcr[1], bgycbcr[2] ); 
                
				if( fgalpha ) {
					if( line[x].unicode >= 0xEE00 && line[x].unicode <= 0xEE7F ) {
						// box glyph
						int mask= line[x].unicode & 0xff;
						int w= width / 2;
						int h= height / 3;
						if( mask & 0x20 ) {
							// continous
							if( mask & 0x1 )
								blit_colour_packed4444( out, w, h, 
														stride, fgalpha, fgycbcr[0], fgycbcr[1], fgycbcr[2] ); 
							if( mask & 0x2 )
								blit_colour_packed4444( out + (w<<2), w, h, 
														stride, fgalpha, fgycbcr[0], fgycbcr[1], fgycbcr[2] ); 
							if( mask & 0x4 ) 
								blit_colour_packed4444( out + h * stride, w, h, 
														stride, fgalpha, fgycbcr[0], fgycbcr[1], fgycbcr[2] ); 
							if( mask & 0x8 )
								blit_colour_packed4444( out + h * stride + (w<<2), w, h, 
														stride, fgalpha, fgycbcr[0], fgycbcr[1], fgycbcr[2] ); 
							if( mask & 0x10 )
								blit_colour_packed4444( out + h * (stride<<1), w, h, 
														stride, fgalpha, fgycbcr[0], fgycbcr[1], fgycbcr[2] ); 
							if( mask & 0x40 )
								blit_colour_packed4444( out + h * (stride<<1) + (w<<2), w, h, 
														stride, fgalpha, fgycbcr[0], fgycbcr[1], fgycbcr[2] ); 
						}
						else {
							// seperated
						}
					}
					else {
						// unicode glyph
					}
				} // if fgalpha
                    
            } // for x
        } // for y
        
        vbi_unref_page( &(vts->pg) );
        return 1;
    }
    return 0;
}

void vtscreen_set_page( vtscreen_t *vts, int pgno )
{
    if( vts->pgno != pgno ) {
        vts->pgno= pgno;
        vts->subno= 0;
        render( vts );
    }
}

void vtscreen_advance_subpage( vtscreen_t *vts )
{ // FIXME
    if( ++(vts->subno) > vbi_cache_hi_subno( vts->dec, vts->pgno ) )
        vts->subno= 0;
}

void vtscreen_composite_packed422_scanline( vtscreen_t *vts,
                                             uint8_t *output,
                                             int width, int xpos, 
                                             int scanline )
{
    if( vts->rerender != RENDER_NOTHING ) {
        if( render( vts ) )
            vts->rerender= RENDER_NOTHING;
    }
    composite_packed4444_to_packed422_scanline( output, 
                                                output, 
                                                vts->buf4444 + ((vts->screenw*scanline)<<2)+ (xpos),
                                                width );
}
