/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <X11/Xlib.h>
#ifdef HAVE_XF86VIDMODE
#include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include "xfullscreen.h"

#define MAX_HEADS 64

typedef struct head_s {
    int x;
    int y;
    int w;
    int h;
} head_t;

struct xfullscreen_s
{
    Display *display;
    int screen;

    int hdisplay;
    int vdisplay;
    int panx;
    int pany;
    double refresh;

    int usevidmode;
    int squarepixel;
    int widthmm;
    int heightmm;

    int nheads;
    head_t heads[ MAX_HEADS ];
};

#ifdef HAVE_XF86VIDMODE
static int get_largest_vidmode_resolution( Display *display, int screen,
                                           int *max_h, int *max_v )
{
    XF86VidModeModeInfo **modelist;
    int nummodes, i;

    *max_h = *max_v = 0;

    if( !XF86VidModeGetAllModeLines( display, screen, &nummodes, &modelist ) ) {
        return 0;
    }

    for( i = 0; i < nummodes; i++ ) {
        if( modelist[ i ]->hdisplay > *max_h ) *max_h = modelist[ i ]->hdisplay;
        if( modelist[ i ]->vdisplay > *max_v ) *max_v = modelist[ i ]->vdisplay;
    }

    return 1;
}

int get_current_modeline_parameters( Display *display, int screen, int *panx,
                                     int *pany, int *hdisp, int *vdisp,
                                     double *refresh )
{
    XF86VidModeModeLine mode_line;
    int dot_clock;

    if( !XF86VidModeGetModeLine( display, screen, &dot_clock, &mode_line ) ||
        !XF86VidModeGetViewPort( display, screen, panx, pany ) ) {
        return 0;
    } else {
        *hdisp = mode_line.hdisplay;
        *vdisp = mode_line.vdisplay;
        *refresh = ( (double) ( (double) dot_clock * 1000.0 ) /
                     (double) ( mode_line.htotal * mode_line.vtotal ) );
    }

    return 1;
}
#endif

xfullscreen_t *xfullscreen_new( Display *display, int screen, int verbose )
{
    xfullscreen_t *xf = malloc( sizeof( xfullscreen_t ) );
    int event_base, error_base;
    int max_h, max_v;

    if( !xf ) {
        return 0;
    }

    xf->display = display;
    xf->screen = screen;

    /* Default settings. */
    xf->usevidmode = 0;
    xf->squarepixel = 1;
    xf->nheads = 1;
    xf->heads[ 0 ].x = 0;
    xf->heads[ 0 ].y = 0;
    xf->heads[ 0 ].w = DisplayWidth( xf->display, xf->screen );
    xf->heads[ 0 ].h = DisplayHeight( xf->display, xf->screen );

#ifdef HAVE_XINERAMA
    if( XineramaQueryExtension( xf->display, &event_base, &error_base ) && XineramaIsActive( xf->display ) ) {
        XineramaScreenInfo *screens;
        int i;

        screens = XineramaQueryScreens( xf->display, &xf->nheads );
        if( xf->nheads > MAX_HEADS ) xf->nheads = MAX_HEADS;
        for( i = 0; i < xf->nheads; i++ ) {
            xf->heads[ i ].x = screens[ i ].x_org;
            xf->heads[ i ].y = screens[ i ].y_org;
            xf->heads[ i ].w = screens[ i ].width;
            xf->heads[ i ].h = screens[ i ].height;
        }

        if( verbose ) {
            fprintf( stderr, "xfullscreen: Using XINERAMA for dual-head information.\n" );
        }
        return xf;
    }
#endif


#ifdef HAVE_XF86VIDMODE
    if( XF86VidModeQueryExtension( xf->display, &event_base, &error_base ) &&
        get_current_modeline_parameters( xf->display, xf->screen, &xf->panx,
                                         &xf->pany, &xf->hdisplay, &xf->vdisplay,
                                         &xf->refresh ) &&
        get_largest_vidmode_resolution( xf->display, xf->screen, &max_h, &max_v ) ) {

        xf->usevidmode = 1;

        if( max_h < DisplayWidth( xf->display, xf->screen ) ||
            max_v < DisplayHeight( xf->display, xf->screen ) ) {

            if( verbose ) {
                fprintf( stderr, "xfullscreen: Desktop larger than primary display, assuming square pixels.\n" );
            }

        } else {
            if( verbose ) {
                fprintf( stderr, "xfullscreen: Single-head detected, pixel aspect will be calculated.\n" );
            }

            xf->squarepixel = 0;
            xf->widthmm = DisplayWidthMM( xf->display, xf->screen );
            xf->heightmm = DisplayHeightMM( xf->display, xf->screen );
        }

        return xf;
    }
#endif

    /* There is no support for XINERAMA or VidMode. */
    if( verbose ) {
        fprintf( stderr, "xfullscreen: No support for the vidmode extension, assuming square pixels.\n" );
    }

    return xf;
}

void xfullscreen_delete( xfullscreen_t *xf )
{
    free( xf );
}

void xfullscreen_print_summary( xfullscreen_t *xf )
{
    if( xf->squarepixel ) {
        fprintf( stderr, "xfullscreen: Pixels are square.\n" );
    } else {
        fprintf( stderr, "xfullscreen: Pixel aspect ratio on the primary head is: %.2f.\n",
                 ((double) (xf->heightmm * xf->hdisplay)) / ((double) (xf->widthmm * xf->vdisplay)) );
    }

    if( xf->usevidmode ) {
        fprintf( stderr, "xfullscreen: Using the XFree86-VidModeExtension to calculate fullscreen size.\n" );
        fprintf( stderr, "xfullscreen: Fullscreen to %d,%d with size %dx%d.\n",
                 xf->panx, xf->pany, xf->hdisplay, xf->vdisplay );
    } else {
        int i;

        fprintf( stderr, "xfullscreen: Fullscreen uses display information.\n" );
        fprintf( stderr, "xfullscreen: Number of displays is %d.\n", xf->nheads );

        for( i = 0; i < xf->nheads; i++ ) {
            fprintf( stderr, "xfullscreen: Head %d at %d,%d with size %dx%d.\n",
                     i, xf->heads[ i ].x, xf->heads[ i ].y, xf->heads[ i ].w, xf->heads[ i ].h );
        }
    }
}

void xfullscreen_update( xfullscreen_t *xf )
{
#ifdef HAVE_XF86VIDMODE
    if( xf->usevidmode ) {
        get_current_modeline_parameters( xf->display, xf->screen, &xf->panx,
                                         &xf->pany, &xf->hdisplay, &xf->vdisplay,
                                         &xf->refresh );
    }
#endif
}

void xfullscreen_get_position( xfullscreen_t *xf, int window_x, int window_y,
                               int *x, int *y, int *w, int *h )
{
    if( xf->usevidmode ) {
        *x = xf->panx;
        *y = xf->pany;
        *w = xf->hdisplay;
        *h = xf->vdisplay;
    } else {
        int i;

        for( i = 0; i < xf->nheads; i++ ) {
            if( (xf->heads[ i ].x <= window_x) &&
                (window_x < (xf->heads[ i ].x + xf->heads[ i ].w)) &&
                (xf->heads[ i ].y <= window_y) &&
                (window_y < (xf->heads[ i ].y + xf->heads[ i ].h)) ) {
                *x = xf->heads[ i ].x;
                *y = xf->heads[ i ].y;
                *w = xf->heads[ i ].w;
                *h = xf->heads[ i ].h;
                return;
            }
        }
    }
}

static int calculate_gcd( int x, int y )
{
    if( y > x ) return calculate_gcd( y, x );
    if( y < 0 ) return calculate_gcd( -y, 0 );

    while( y ) {
        int tmp = y;
        y = x % y;
        x = tmp;
    }

    return x;
}

void xfullscreen_get_pixel_aspect( xfullscreen_t *xf, int *aspect_n, int *aspect_d )
{
    if( xf->squarepixel ) {
        *aspect_n = *aspect_d = 1;
    } else {
        int gcd;

        *aspect_n = xf->heightmm * xf->hdisplay;
        *aspect_d = xf->widthmm * xf->vdisplay;

        gcd = calculate_gcd( *aspect_n, *aspect_d );
        if( gcd ) {
            *aspect_n /= gcd;
            *aspect_d /= gcd;
        }
    }
}

int xfullscreen_get_refresh( xfullscreen_t *xf, double *refresh )
{
    if( xf->usevidmode ) {
        *refresh = xf->refresh;
        return 1;
    }

    return 0;
}

