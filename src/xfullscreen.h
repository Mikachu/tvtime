/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net> and
 *                    Per von Zweigbergk <pvz@e.kth.se>
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

#ifndef XFULLSCREEN_H_INCLUDED
#define XFULLSCREEN_H_INCLUDED

#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This module helps provide advanced fullscreen support in applications
 * by providing information about where the fullscreen window should be
 * placed and what pixel aspect ratio to use.
 */

typedef struct xfullscreen_s xfullscreen_t;

/**
 * Create a new xfullscreen object for the given X display and screen.
 * If verbose is true, we will print debug messages to stderr.
 */
xfullscreen_t *xfullscreen_new( Display *display, int screen, int verbose );

/**
 * Free our internal memory.
 */
void xfullscreen_delete( xfullscreen_t *xf );

/**
 * For debugging, this prints a summary of what we have discovered to
 * stderr.
 */
void xfullscreen_print_summary( xfullscreen_t *xf );

/**
 * This call updates our resolution information from the VidMode
 * extension.  This should be called before switching modes from
 * windowed mode to fullscreen mode and back.
 */
void xfullscreen_update( xfullscreen_t *xf );

/**
 * This returns the x and y position and the width and height to use for
 * a fullscreen window positioned at window_x and window_y.
 */
void xfullscreen_get_position( xfullscreen_t *xf, int window_x,
                               int window_y, int *x, int *y,
                               int *w, int *h );

/**
 * This returns the pixel aspect ratio of the X server at the time of
 * the last update call.
 */
void xfullscreen_get_pixel_aspect( xfullscreen_t *xf, int *aspect_w,
                                   int *aspect_h );

/**
 * This returns the refresh rate of the primary X display at the time of
 * the last update call.  Since this information is not always
 * available, the value is only accurate if the function returns true.
 */
int xfullscreen_get_refresh( xfullscreen_t *xf, double *refresh );

#ifdef __cplusplus
};
#endif
#endif /* XFULLSCREEN_H_INCLUDED */
