/**
 * Copyright (C) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * Common routines for X output drivers.  Uses EWMH code and lots of
 * help from:
 *
 * Ogle - A video player
 * Copyright (C) 2001, 2002 Björn Englund, Håkan Hjort
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__FreeBSD__)
#include <machine/param.h>
#endif
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#ifdef HAVE_XTESTEXTENSION
#include <X11/extensions/XTest.h>
#endif

#include "xfullscreen.h"
#include "speedy.h"
#include "utils.h"
#include "xcommon.h"

/* Every 30 seconds, ping the screensaver. */
#define SCREENSAVER_PING_TIME (30 * 1000 * 1000)

/* Useful. */
#define MIN(x,y)((x)<(y)?(x):(y))
#define MAX(x,y)((x)>(y)?(x):(y))

static Display *display;
static int screen;
static Window wm_window;
static Window fs_window;
static Window output_window;
static GC gc;
static int have_xtest;
static int output_width, output_height;
static int output_aspect;
static int output_on_root;
static int has_ewmh_state_fullscreen;
static int has_ewmh_state_above;
static int has_ewmh_state_below;
static Cursor nocursor;
static int output_fullscreen;
static int xcommon_verbose;
static int xcommon_exposed;
static int xcommon_colourkey;
static int motion_timeout;
static int fullscreen_position;
static int matte_width;
static int matte_height;
static int alwaysontop;
static int has_focus;
static int wm_is_metacity;
static xfullscreen_t *xf;

static Atom net_supporting_wm_check;
static Atom net_supported;
static Atom net_wm_name;
static Atom net_wm_state;
static Atom net_wm_state_above;
static Atom net_wm_state_below;
static Atom net_wm_state_fullscreen;
static Atom utf8_string;
static Atom kwm_keep_on_top;
static Atom wm_protocols;
static Atom wm_delete_window;
static Atom xawtv_station;
static Atom xawtv_remote;

#ifdef HAVE_XTESTEXTENSION
static KeyCode kc_shift_l; /* Fake key to send. */
#endif

static area_t video_area;
static area_t window_area;
static area_t scale_area;

static Time lastpresstime = 0;
static Time lastreleasetime = 0;

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

/**
 * Cache every atom we need in xcommon.  This minimizes round trips
 * to the server.  The number of atoms and their position is hardcoded,
 * but it's small.  Making this look pretty I think would be too
 * annoying to be worth it.
 */
static void load_atoms( Display *dpy )
{
    static char *atom_names[] = {
        "_NET_SUPPORTING_WM_CHECK",
        "_NET_SUPPORTED",
        "_NET_WM_NAME",
        "_NET_WM_STATE",
        "_NET_WM_STATE_ABOVE",
        "_NET_WM_STATE_BELOW",
        "_NET_WM_STATE_FULLSCREEN",
        "UTF8_STRING",
        "KWM_KEEP_ON_TOP",
        "WM_PROTOCOLS",
        "WM_DELETE_WINDOW",
        "_XAWTV_STATION",
        "_XAWTV_REMOTE"
    };
    Atom atoms_return[ 13 ];

    XInternAtoms( display, atom_names, 13, False, atoms_return );
    net_supporting_wm_check = atoms_return[ 0 ];
    net_supported = atoms_return[ 1 ];
    net_wm_name = atoms_return[ 2 ];
    net_wm_state = atoms_return[ 3 ];
    net_wm_state_above = atoms_return[ 4 ];
    net_wm_state_below = atoms_return[ 5 ];
    net_wm_state_fullscreen = atoms_return[ 6 ];
    utf8_string = atoms_return[ 7 ];
    kwm_keep_on_top = atoms_return[ 8 ];
    wm_protocols = atoms_return[ 9 ];
    wm_delete_window = atoms_return[ 10 ];
    xawtv_station = atoms_return[ 11 ];
    xawtv_remote = atoms_return[ 12 ];
}


/**
 * Called after mapping a window - waits until the window is mapped.
 */
static void x11_wait_mapped( Display *dpy, Window win )
{
    XEvent event;
    do {
        XMaskEvent( dpy, StructureNotifyMask, &event );
    } while ( (event.type != MapNotify) || (event.xmap.event != win) );
}

static int have_xtestextention( void )
{  
#ifdef HAVE_XTESTEXTENSION
    int dummy1, dummy2, dummy3, dummy4;
  
    return (XTestQueryExtension( display, &dummy1, &dummy2, &dummy3, &dummy4 ) == True);
#endif
    return 0;
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

static void simplify_fraction( int *n, int *d )
{
    int gcd = calculate_gcd( *n, *d );
    *n /= gcd;
    *d /= gcd;
}

/*
 * Integer division always truncates in C -- it rounds down.
 *
 * To compensate for this, we round up if
 *   n mod d > d/2
 * To avoid adding another truncation error, I multiply
 * this condition by two:
 *   2 * (n mod d) > d
 * And use the property that this evaluates to 1 if true to round up.
 */
static int rounded_int_division( int n, int d ) 
{
    simplify_fraction( &n, &d );
    return ( n/d + (2 * (n%d) > d) );
}

/*
 * Returns the aspect ratio (4:3 or 16:9) for our output,
 * and corrected for rectangular pixels.
 */
static void xv_get_output_aspect( int *px_widthratio, int *px_heightratio )
{
    int metric_widthratio = output_aspect ? 16 : 4;
    int metric_heightratio = output_aspect ? 9 : 3;
    int sar_frac_w, sar_frac_h;

    if( matte_height ) {
        metric_heightratio = matte_height;
        metric_widthratio = matte_width;
    }

    xfullscreen_get_pixel_aspect( xf, &sar_frac_w, &sar_frac_h );

    if( xcommon_verbose ) {
        fprintf( stderr, "xcommon: Pixel aspect ratio %d:%d.\n",
                 sar_frac_w, sar_frac_h );
    }

    /* PX_RATIO = METRIC_RATIO / SAR_FRAC = METRIC_RATIO * (1 / SAR_FRAC) */
    *px_widthratio = metric_widthratio * sar_frac_h;
    *px_heightratio = metric_heightratio * sar_frac_w;
    simplify_fraction( px_widthratio, px_heightratio );
}

static int xv_get_width_for_height( int window_height )
{
    int n, d;

    xv_get_output_aspect( &n, &d );
    return rounded_int_division( window_height * n, d );
}

static int xv_get_height_for_width( int window_width )
{
    int n, d;

    xv_get_output_aspect( &d, &n );
    return rounded_int_division( window_width * n, d );
}

static void x11_aspect_hint( Display *dpy, Window win, int aspect_width, int aspect_height )
{
    /* These hints don't work with current versions of metacity.
     * We'll re-enable this code for metacity when newer versions
     * become more popular.
     */
    if( !wm_is_metacity ) {
        XSizeHints hints;

        hints.flags = PAspect;
        hints.min_aspect.x = aspect_width;
        hints.min_aspect.y = aspect_height;
        hints.max_aspect.x = aspect_width;
        hints.max_aspect.y = aspect_height;

        XSetWMNormalHints( dpy, win, &hints );
    }
}

static void x11_static_gravity( Display *dpy, Window win )
{
    XSizeHints hints;
    hints.flags = PWinGravity;
    hints.win_gravity = StaticGravity;
    XSetWMNormalHints( dpy, win, &hints );
}

static void x11_northwest_gravity( Display *dpy, Window win )
{
    XSizeHints hints;
    hints.flags = PWinGravity;
    hints.win_gravity = NorthWestGravity;
    XSetWMNormalHints( dpy, win, &hints );
}

static void x11_grab_fullscreen_input( Display *dpy, Window win )
{
    int tries = 10;
    int result = -1;

    while( tries && result != GrabSuccess ) {
        result = XGrabPointer( dpy, win, True, 0, GrabModeAsync, GrabModeAsync,
                               win, None, CurrentTime );
        tries--;
    }
    XGrabKeyboard( dpy, win, True, GrabModeAsync, GrabModeAsync, CurrentTime );
    XSync( dpy, False );
}

static void x11_ungrab_fullscreen_input( Display *dpy )
{
    XUngrabPointer( dpy, CurrentTime );
    XUngrabKeyboard( dpy, CurrentTime );
}

/* Used for error handling. */
static unsigned long req_serial;
static int (*prev_xerrhandler)( Display *dpy, XErrorEvent *ev );

static int xprop_errorhandler( Display *dpy, XErrorEvent *ev )
{
    if( ev->serial == req_serial ) {
        /* this is an error to the XGetWindowProperty request
         * most probable the window specified by the property
         * _NET_SUPPORTING_WM_CHECK on the root window no longer exists
         */
        fprintf( stderr, "xprop_errhandler: error in XGetWindowProperty\n" );
        return 0;
    } else {
        /* if we get another error we should handle it,
         * so we give it to the previous errorhandler
         */
        fprintf( stderr, "xprop_errhandler: unexpected error\n" );
        return prev_xerrhandler( dpy, ev );
    }
}

/**
 * Returns the window name for an Extended Window Manager Hints (EWMH)
 * compliant window manager.  Sets the name to "unknown" if no name is
 * set (non-compliant by the spec, but worth handling).
 */
static void get_window_manager_name( Display *dpy, Window wm_window, char **wm_name_return )
{
    Atom type_return;
    int format_return;
    unsigned long bytes_after_return;
    unsigned long nitems_return;
    unsigned char *prop_return = 0;

    if( XGetWindowProperty( dpy, wm_window, net_wm_name, 0,
                            4, False, XA_STRING,
                            &type_return, &format_return,
                            &nitems_return, &bytes_after_return,
                            &prop_return ) != Success ) {
        fprintf( stderr, "xcommon: Can't get window manager name "
                         "property (WM not compliant).\n" );
        *wm_name_return = strdup( "unknown" );
        return;
    }
    
    if( type_return == None ) {
        fprintf( stderr, "xcommon: No window manager name "
                         "property found (WM not compliant).\n" );
        *wm_name_return = strdup( "unknown" );
        XFree( prop_return );
        return;
    }

    if( type_return != XA_STRING ) {
        XFree( prop_return );

        if( type_return == utf8_string ) {
            if( XGetWindowProperty( dpy, wm_window, net_wm_name, 0,
                                    4, False, utf8_string,
                                    &type_return, &format_return,
                                    &nitems_return,
                                    &bytes_after_return,
                                    &prop_return ) != Success ) {
                fprintf( stderr, "wm_name: Can't get window "
                                 "manager name propety "
                                 "(WM not compliant).\n" );
                *wm_name_return = strdup( "unknown" );
                return;
            }

            if( format_return == 8 ) {
                *wm_name_return = strdup( (char *) prop_return );
            }
            XFree( prop_return );
        } else {
            fprintf( stderr, "xcommon: Window manager name not "
                             "ASCII or UTF8, giving up.\n" );
            *wm_name_return = strdup( "unknown" );
            return;
        }
    } else {
        if( format_return == 8 ) {
            *wm_name_return = strdup( (char *) prop_return );
        }
        XFree( prop_return );
    }
}

/**
 * returns 1 if a window manager compliant to the
 * Extended Window Manager Hints (EWMH) spec is running.
 * (version 1.2)
 * Oterhwise returns 0.
 */
static int check_for_EWMH_wm( Display *dpy, char **wm_name_return )
{
    Atom type_return;
    int format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *prop_return = 0;

    if( XGetWindowProperty( dpy, DefaultRootWindow( dpy ), net_supporting_wm_check, 0,
                            1, False, XA_WINDOW,
                            &type_return, &format_return, &nitems_return,
                            &bytes_after_return, &prop_return ) != Success ) {
        fprintf( stderr, "xcommon: Can't get window property to check for EWMH.\n" );
        return 0;
    }
  
    if( type_return == None ) {
        fprintf( stderr, "xcommon: No window properties found for EWMH.\n" );
        return 0;
    }

    if( type_return != XA_WINDOW ) {
        fprintf( stderr, "xcommon: Can't get window property for EWMH.\n" );
        if( prop_return ) XFree( prop_return );
        return 0;
    }

    if( format_return == 32 && nitems_return == 1 && bytes_after_return == 0 ) {
        Window win_id = *( (long *) prop_return );
        int status;

        XFree( prop_return );
        prop_return = 0;

        /* Make sure we don't have any unhandled errors. */
        XSync( dpy, False );

        /* Set error handler so we can check if XGetWindowProperty failed. */
        prev_xerrhandler = XSetErrorHandler( xprop_errorhandler );

        /* get the serial of the XGetWindowProperty request */
        req_serial = NextRequest( dpy );

        /* try to get property
         * this can fail if we have a property with the win_id on the
         * root window, but the win_id is no longer valid.
         * This is to check for a stale window manager
         */
        status = XGetWindowProperty( dpy, win_id, net_supporting_wm_check, 0,
                                     1, False, XA_WINDOW,
                                     &type_return, &format_return, &nitems_return,
                                     &bytes_after_return, &prop_return );

        /* make sure XGetWindowProperty has been processed and any errors
           have been returned to us */
        XSync( dpy, False );

        /* revert to the previous xerrorhandler */
        XSetErrorHandler( prev_xerrhandler );

        if( status != Success || type_return == None ) {
            fprintf( stderr, "xcommon: EWMH check failed on child window, "
                             "stale window manager property on root?\n" );
            return 0;
        }

        if( type_return == XA_CARDINAL ) {
            /* Hack for old and busted metacity. */
            /* Allow this atom to be a CARDINAL. */
            XGetWindowProperty( dpy, win_id, net_supporting_wm_check, 0,
                                1, False, XA_CARDINAL,
                                &type_return, &format_return, &nitems_return,
                                &bytes_after_return, &prop_return );
        } else if( type_return != XA_WINDOW ) {
            fprintf( stderr, "didn't return XA_WINDOW?\n" );
            fprintf( stderr, "xcommon: EWMH check failed on child window, "
                             "stale window manager property on root?\n" );
            if( prop_return ) {
                XFree( prop_return );
            }
            return 0;
        }

        if( format_return == 32 && nitems_return == 1 && bytes_after_return == 0 ) {
            if( win_id == *( (long *) prop_return ) ) {
                /* We have successfully detected a EWMH compliant Window Manager. */
                XFree( prop_return );

                /* Get the name of the wm */
                get_window_manager_name( dpy, win_id, wm_name_return );
                return 1;
            }
            XFree( prop_return );
            return 0;
        } else if( prop_return ) {
            XFree( prop_return );
        }
    } else if( prop_return ) {
        XFree( prop_return );
    }

    return 0;
}

/**
 * Returns 1 if a window manager compliant to the Extended Window
 * Manager Hints (EWMH) specification supports the
 * _NET_WM_STATE_FULLSCREEN window state.  Otherwise, returns 0.
 */
static int check_for_state_fullscreen( Display *dpy )
{
    Atom type_return;
    int format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *prop_return = 0;
    int nr_items = 40;
    int item_offset = 0;
    int supports_net_wm_state = 0;
    int supports_net_wm_state_fullscreen = 0;

    do {
        if( XGetWindowProperty( dpy, DefaultRootWindow( dpy ), net_supported,
                                item_offset, nr_items, False, XA_ATOM,
                                &type_return, &format_return, &nitems_return,
                                &bytes_after_return, &prop_return) != Success ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: Can't check the fullscreen property (WM not compliant).\n" );
            }
            return 0;
        }
    
        if( type_return == None ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: No property for fullscreen support (WM not compliant).\n" );
            }
            return 0;
        }

        if( type_return != XA_ATOM ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: Broken property for fullscreen support (WM not compliant).\n");
            }
            if( prop_return ) XFree( prop_return );
            return 0;
        } else {
            if( format_return == 32 ) {
                int n;

                for( n = 0; n < nitems_return; n++ ) {
                    if( ((long *) prop_return)[ n ] == net_wm_state ) {
                        supports_net_wm_state = 1;
                    } else if( ((long *) prop_return)[ n ] == net_wm_state_fullscreen ) {
                        supports_net_wm_state_fullscreen = 1;
                    }

                    if( supports_net_wm_state && supports_net_wm_state_fullscreen ) {
                        XFree( prop_return );
                        return 1;
                    }
                }

                XFree( prop_return );
            } else {
                XFree( prop_return );
                return 0;
            }
        }

        item_offset += nr_items;
    } while( bytes_after_return > 0 );

    return 0;
}

/**
 * Returns 1 if a window manager compliant to the Extended Window
 * Manager Hints (EWMH) specification supports the _NET_WM_STATE_ABOVE
 * window state.  Otherwise, returns 0.
 */
static int check_for_state_above( Display *dpy )
{
    Atom type_return;
    int format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *prop_return = 0;
    int nr_items = 40;
    int item_offset = 0;
    int supports_net_wm_state = 0;
    int supports_net_wm_state_above = 0;

    do {
        if( XGetWindowProperty( dpy, DefaultRootWindow( dpy ), net_supported,
                                item_offset, nr_items, False, XA_ATOM,
                                &type_return, &format_return, &nitems_return,
                                &bytes_after_return, &prop_return) != Success ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: Can't check the above property (WM not compliant).\n" );
            }
            return 0;
        }
    
        if( type_return == None ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: No property for above support (WM not compliant).\n" );
            }
            return 0;
        }

        if( type_return != XA_ATOM ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: Broken property for above support (WM not compliant).\n");
            }
            if( prop_return ) XFree( prop_return );
            return 0;
        } else {
            if( format_return == 32 ) {
                int n;

                for( n = 0; n < nitems_return; n++ ) {
                    if( ((long *) prop_return)[ n ] == net_wm_state ) {
                        supports_net_wm_state = 1;
                    } else if( ((long *) prop_return)[ n ] == net_wm_state_above ) {
                        supports_net_wm_state_above = 1;
                    }

                    if( supports_net_wm_state && supports_net_wm_state_above ) {
                        XFree( prop_return );
                        return 1;
                    }
                }

                XFree( prop_return );
            } else {
                XFree( prop_return );
                return 0;
            }
        }

        item_offset += nr_items;
    } while( bytes_after_return > 0 );

    return 0;
}

/**
 * Returns 1 if a window manager compliant to the Extended Window
 * Manager Hints (EWMH) specification supports the _NET_WM_STATE_BELOW
 * window state.  Otherwise, returns 0.
 */
static int check_for_state_below( Display *dpy )
{
    Atom type_return;
    int format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *prop_return = 0;
    int nr_items = 40;
    int item_offset = 0;
    int supports_net_wm_state = 0;
    int supports_net_wm_state_below = 0;

    do {
        if( XGetWindowProperty( dpy, DefaultRootWindow( dpy ), net_supported,
                                item_offset, nr_items, False, XA_ATOM,
                                &type_return, &format_return, &nitems_return,
                                &bytes_after_return, &prop_return) != Success ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: Can't check the below property (WM not compliant).\n" );
            }
            return 0;
        }
    
        if( type_return == None ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: No property for below support (WM not compliant).\n" );
            }
            return 0;
        }

        if( type_return != XA_ATOM ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: Broken property for below support (WM not compliant).\n");
            }
            if( prop_return ) XFree( prop_return );
            return 0;
        } else {
            if( format_return == 32 ) {
                int n;

                for( n = 0; n < nitems_return; n++ ) {
                    if( ((long *) prop_return)[ n ] == net_wm_state ) {
                        supports_net_wm_state = 1;
                    } else if( ((long *) prop_return)[ n ] == net_wm_state_below ) {
                        supports_net_wm_state_below = 1;
                    }

                    if( supports_net_wm_state && supports_net_wm_state_below ) {
                        XFree( prop_return );
                        return 1;
                    }
                }

                XFree( prop_return );
            } else {
                XFree( prop_return );
                return 0;
            }
        }

        item_offset += nr_items;
    } while( bytes_after_return > 0 );

    return 0;
}


/**
 * Some math:
 *
 * pixel size = pw/ph
 * screen size = sw/sh
 * resolution = rw/rh
 *
 * sw = pw*rw   therefore pw = sw / rw
 * sh = ph*rh   therefore ph = sh / rh
 */

static void calculate_video_area( void )
{
    int curwidth, curheight;

    curwidth = output_width;
    curheight = xv_get_height_for_width( curwidth );
    if( curheight > output_height ) {
        curheight = output_height;
        curwidth = xv_get_width_for_height( curheight );
    }

    video_area.x = ( output_width - curwidth ) / 2;
    video_area.y = ( output_height - curheight ) / 2;
    if( output_fullscreen ) {
        if( fullscreen_position == 1 ) {
            video_area.y = 0;
        } else if( fullscreen_position == 2 ) {
            video_area.y = output_height - curheight;
        }
    }
    video_area.width = curwidth;
    video_area.height = curheight;

    if( xcommon_verbose ) {
        fprintf( stderr, "xcommon: Displaying in a %dx%d window inside %dx%d space.\n",
                 curwidth, curheight, output_width, output_height );
    }
}


int xcommon_open_display( const char *user_geometry, int aspect, int verbose )
{
    Pixmap curs_pix;
    XEvent xev;
    XSizeHints hint;
    XClassHint classhint;
    XColor curs_col;
    XSetWindowAttributes xswa;
    const char *hello = "tvtime";
    unsigned long mask;
    char *wmname;
    int wx, wy;
    unsigned int ww, wh;
    int gflags;

    output_aspect = aspect;
    output_height = 576;

    have_xtest = 0;
    output_on_root = 0;
    has_ewmh_state_fullscreen = 0;
    has_ewmh_state_above = 0;
    has_ewmh_state_below = 0;
    output_fullscreen = 0;
    xcommon_verbose = verbose;
    xcommon_exposed = 0;
    xcommon_colourkey = 0;
    motion_timeout = 0;
    fullscreen_position = 0;
    matte_width = 0;
    matte_height = 0;
    alwaysontop = 0;
    has_focus = 0;
    wm_is_metacity = 0;

    display = XOpenDisplay( 0 );
    if( !display ) {
        if( getenv( "DISPLAY" ) ) {
            fprintf( stderr, "xcommon: Cannot open display '%s'.\n", getenv( "DISPLAY" ) );
        } else {
            fprintf( stderr, "xcommon: No DISPLAY set, so no output possible!\n" );
        }
        return 0;
    }

    if( xcommon_verbose ) {
        fprintf( stderr, "xcommon: Display %s, vendor %s, ",
        DisplayString( display ), ServerVendor( display ) );

        if( strstr( ServerVendor( display ), "XFree86" ) ) {
            int vendrel = VendorRelease( display );

            fprintf( stderr, "XFree86 " );
            if( vendrel < 336 ) {
                /*
                 * vendrel was set incorrectly for 3.3.4 and 3.3.5, so handle
                 * those cases here.
                 */
                fprintf( stderr, "%d.%d.%d", vendrel / 100, (vendrel / 10) % 10, vendrel % 10);
            } else if( vendrel < 3900 ) {
                /* 3.3.x versions, other than the exceptions handled above */
                fprintf( stderr, "%d.%d", vendrel / 1000, (vendrel / 100) % 10);
                if (((vendrel / 10) % 10) || (vendrel % 10)) {
                    fprintf( stderr, ".%d", (vendrel / 10) % 10);
                    if (vendrel % 10) {
                        fprintf( stderr, ".%d", vendrel % 10);
                    }
                }
            } else if( vendrel < 40000000 ) {
                /* 4.0.x versions */
                fprintf( stderr, "%d.%d", vendrel / 1000, (vendrel / 10) % 10);
                if( vendrel % 10 ) {
                    fprintf( stderr, ".%d", vendrel % 10 );
                }
            } else {
                /* post-4.0.x */
                fprintf( stderr, "%d.%d.%d", vendrel / 10000000, (vendrel / 100000) % 100, (vendrel / 1000) % 100);
                if( vendrel % 1000 ) {
                    fprintf( stderr, ".%d", vendrel % 1000);
                }
            }
            fprintf( stderr, "\n" );
        } else {
            fprintf( stderr, "vendor release %d\n", VendorRelease( display ) );
        }
    }

    load_atoms( display );

    screen = DefaultScreen( display );

    xf = xfullscreen_new( display, screen, verbose );
    if( !xf ) {
        fprintf( stderr, "xcommon: Can't initialize fullscreen information object.\n" );
        XCloseDisplay( display );
        return 0;
    }

    if( xcommon_verbose ) {
        xfullscreen_print_summary( xf );
    }

#ifdef HAVE_XTESTEXTENSION
    kc_shift_l = XKeysymToKeycode( display, XK_Shift_L );
#endif
    have_xtest = have_xtestextention();
    if( have_xtest && xcommon_verbose ) {
        fprintf( stderr, "xcommon: Have XTest, will use it to ping the screensaver.\n" );
    }

    /* Initially, get the best width for our height. */
    output_width = xv_get_width_for_height( output_height );

    window_area.x = 0;
    window_area.y = 0;
    window_area.width = output_width;
    window_area.height = output_height;

    xswa.override_redirect = False;
    xswa.backing_store = NotUseful;
    xswa.save_under = False;
    xswa.background_pixel = BlackPixel( display, screen );
    xswa.event_mask = ButtonPressMask | ButtonReleaseMask |
                      StructureNotifyMask | KeyPressMask | PointerMotionMask |
                      VisibilityChangeMask | FocusChangeMask |
                      PropertyChangeMask | ExposureMask;

    mask = (CWBackPixel | CWSaveUnder | CWBackingStore | CWEventMask);

    gflags = XParseGeometry( user_geometry, &wx, &wy, &ww, &wh );
    if( !(gflags & WidthValue) ) {
        ww = output_width;
    }
    if( !(gflags & HeightValue) ) {
        wh = output_height;
    } else if( ww == 0 ) {
        ww = xv_get_width_for_height( wh );
    }

    window_area.width = ww;
    window_area.height = wh;
    output_width = ww;
    output_height = wh;

    wm_window = XCreateWindow( display, RootWindow( display, screen ), 0, 0,
                               ww, wh, 0, CopyFromParent, InputOutput,
                               CopyFromParent, mask, &xswa);

    output_window = XCreateWindow( display, wm_window, 0, 0,
                                   ww, wh, 0, CopyFromParent, InputOutput,
                                   CopyFromParent, mask, &xswa);

    xswa.override_redirect = True;
    xswa.border_pixel = 0;

    mask = (CWBackPixel | CWOverrideRedirect | CWEventMask);

    fs_window = XCreateWindow( display, RootWindow( display, screen ), 0, 0,
                               output_width, output_height, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               mask, &xswa );

    /* Tell KDE to keep the fullscreen window on top */
    {
        XEvent ev;
        long mask;

        memset( &ev, 0, sizeof( ev ) );
        ev.xclient.type = ClientMessage;
        ev.xclient.window = DefaultRootWindow( display );
        ev.xclient.message_type = kwm_keep_on_top;
        ev.xclient.format = 32;
        ev.xclient.data.l[ 0 ] = fs_window;
        ev.xclient.data.l[ 1 ] = CurrentTime;
        mask = SubstructureRedirectMask;
        XSendEvent( display, DefaultRootWindow( display ), False, mask, &ev );
    }

    if( check_for_EWMH_wm( display, &wmname ) ) {
        if( xcommon_verbose ) {
            fprintf( stderr, "xcommon: Window manager is %s and is EWMH compliant.\n", wmname );
        }

        if( !strcasecmp( wmname, "metacity" ) ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: You are using metacity.  Disabling aspect ratio hints\n"
                                 "xcommon: since most deployed versions of metacity are still broken.\n" );
            }
            wm_is_metacity = 1;
        }
        free( wmname );


        /**
         * If we have an EWMH compliant window manager, check for
         * fullscreen, above and below states.
         */
        has_ewmh_state_fullscreen = check_for_state_fullscreen( display );
        if( has_ewmh_state_fullscreen && xcommon_verbose ) {
            fprintf( stderr, "xcommon: Using EWMH state fullscreen property.\n" );
        }
        has_ewmh_state_above = check_for_state_above( display );
        if( has_ewmh_state_above && xcommon_verbose ) {
            fprintf( stderr, "xcommon: Using EWMH state above property.\n" );
        }
        has_ewmh_state_below = check_for_state_below( display );
        if( has_ewmh_state_below && xcommon_verbose ) {
            fprintf( stderr, "xcommon: Using EWMH state below property.\n" );
        }
    } else if( xcommon_verbose ) {
        fprintf( stderr, "xcommon: Window manager is not EWMH compliant.\n" );
    }

    gc = DefaultGC( display, screen );

    hint.width = output_width;
    hint.height = output_height;
    hint.flags = PSize;

    XSetStandardProperties( display, wm_window, hello, hello, None, 0, 0, &hint );

    /* The class hint is useful for window managers like WindowMaker. */
    classhint.res_class = "tvtime";
    classhint.res_name = "TVWindow";
    XSetClassHint( display, wm_window, &classhint );
    classhint.res_name = "TVFullscreen";
    XSetClassHint( display, fs_window, &classhint );

    /* collaborate with the window manager for close requests */
    XSetWMProtocols( display, wm_window, &wm_delete_window, 1 );

    calculate_video_area();
    x11_aspect_hint( display, wm_window, video_area.width, video_area.height );

    XMapWindow( display, output_window );
    XMapWindow( display, wm_window );

    /* FIXME: Do I need to wait for the window to be mapped? */
    /* x11_wait_mapped( display, wm_window ); */

    /* Wait for map. */
    XMaskEvent( display, StructureNotifyMask, &xev );

    /* Create a 1 pixel cursor to use in full screen mode */
    curs_pix = XCreatePixmap( display, output_window, 1, 1, 1 );
    curs_col.pixel = 0;
    curs_col.red = 0;
    curs_col.green = 0;
    curs_col.blue = 0;
    curs_col.flags = 0;
    curs_col.pad = 0;
    nocursor = XCreatePixmapCursor( display, curs_pix, curs_pix, &curs_col, &curs_col, 1, 1 );
    XDefineCursor( display, output_window, nocursor );
    XSetIconName( display, wm_window, "tvtime" );

    return 1;
}

static struct timeval last_ping_time;
static int time_initialized = 0;

void xcommon_ping_screensaver( void )
{
    struct timeval curtime;

    if( !time_initialized ) {
        gettimeofday( &last_ping_time, 0 );
        time_initialized = 1;
    }

    gettimeofday( &curtime, 0 );
    if( timediff( &curtime, &last_ping_time ) > SCREENSAVER_PING_TIME ) { 
        last_ping_time = curtime;
#ifdef HAVE_XTESTEXTENSION
        if( have_xtest ) {
            XTestFakeKeyEvent( display, kc_shift_l, True, CurrentTime );
            XTestFakeKeyEvent( display, kc_shift_l, False, CurrentTime );
        } else 
#endif
        {
            XResetScreenSaver( display );
        }
    }
}

void xcommon_frame_drawn( void )
{
    if( motion_timeout ) {
        motion_timeout--;
        if( !motion_timeout ) {
            XDefineCursor( display, output_window, nocursor );
        }
    }
}

void xcommon_set_window_position( int x, int y )
{
    if( output_fullscreen ) {
        xcommon_toggle_fullscreen( 0, 0 );
    }

    if( xcommon_verbose ) {
        fprintf( stderr, "xcommon: Target window position %d,%d.\n", x, y );
    }

    x11_northwest_gravity( display, wm_window );
    XMoveWindow( display, wm_window, x, y );
}

void xcommon_set_window_height( int window_height )
{
    XWindowChanges win_changes;
    int window_width;

    if( output_fullscreen ) {
        xcommon_toggle_fullscreen( 0, 0 );
    }

    window_width = xv_get_width_for_height( window_height );

    if( xcommon_verbose ) {
        fprintf( stderr, "xcommon: Target window size %dx%d.\n", window_width, window_height );
    }

    win_changes.width = window_width;
    win_changes.height = window_height;

    XReconfigureWMWindow( display, wm_window, 0,
                          CWWidth | CWHeight, &win_changes );
}

void xcommon_clear_screen( void )
{
    XSetForeground( display, gc, BlackPixel( display, screen ) );
    XClearWindow( display, output_window );
    XSetForeground( display, gc, xcommon_colourkey );
    XFillRectangle( display, output_window, gc, video_area.x, video_area.y,
                    video_area.width, video_area.height );
    /* This is how to draw text.
    XSetForeground( display, gc, WhitePixel( display, screen ) );
    XDrawString( display, output_window, gc,
                 video_area.x + (video_area.width/3),
                 video_area.y + (video_area.height/3),
                 text, strlen( text ) );
     */
    XSync( display, False );
}

void xcommon_clear_area( int x, int y, int w, int h )
{
    int dx, dy, dw, dh;

    /* First fill the new area to black. */
    XSetForeground( display, gc, BlackPixel( display, screen ) );
    XFillRectangle( display, output_window, gc, x, y, w, h );

    /**
     * Only fill the intersection of the video area and the exposed
     * area with the colour key.
     */
    dx = MAX( x, video_area.x );
    dy = MAX( y, video_area.y );
    dw = MIN( x + w, video_area.x + video_area.width ) - dx;
    dh = MIN( y + h, video_area.y + video_area.height ) - dy;
    if( dx >= 0 && dy >= 0 && dw >= 0 && dh >= 0 ) {
        XSetForeground( display, gc, xcommon_colourkey );
        XFillRectangle( display, output_window, gc, dx, dy, dw, dh );
    }
}

/**
 * Called after unmapping a window - waits until the window is unmapped.
 */
static void x11_wait_unmapped( Display *dpy, Window win )
{
    XEvent event;
    do {
        XMaskEvent( dpy, StructureNotifyMask, &event );
    } while ( (event.type != UnmapNotify) || (event.xunmap.event != win) );
}

int xcommon_toggle_alwaysontop( void )
{
    if( has_ewmh_state_above ) {
        XEvent ev;

        alwaysontop = !alwaysontop;

        ev.type = ClientMessage;
        ev.xclient.window = wm_window;
        ev.xclient.message_type = net_wm_state;
        ev.xclient.format = 32;
        ev.xclient.data.l[ 0 ] = alwaysontop ? 1 : 0;
        ev.xclient.data.l[ 1 ] = net_wm_state_above;
        ev.xclient.data.l[ 2 ] = 0;

        XSendEvent( display, DefaultRootWindow( display ), False,
                    SubstructureNotifyMask|SubstructureRedirectMask, &ev );
    }

    return alwaysontop;
}

int xcommon_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
    output_fullscreen = !output_fullscreen;
    if( output_fullscreen ) {
        int x, y, w, h;
  
        xfullscreen_update( xf );
        xfullscreen_get_position( xf, window_area.x, window_area.y,
                                  &x, &y, &w, &h );

        output_width = w;
        output_height = h;

        window_area.x = x;
        window_area.y = y;
        window_area.width = w;
        window_area.height = h;

        if( has_ewmh_state_fullscreen ) {
            XEvent ev;

            ev.type = ClientMessage;
            ev.xclient.display = display;
            ev.xclient.window = wm_window;
            ev.xclient.message_type = net_wm_state;
            ev.xclient.format = 32;
            ev.xclient.data.l[ 0 ] = 1;
            ev.xclient.data.l[ 1 ] = net_wm_state_fullscreen;
            ev.xclient.data.l[ 2 ] = 0;
            ev.xclient.data.l[ 3 ] = 0;
            ev.xclient.data.l[ 4 ] = 0;

            XSendEvent( display, DefaultRootWindow( display ), False,
                        SubstructureNotifyMask | SubstructureRedirectMask,
                        &ev );
        } else {
            /* Show our fullscreen window. */
            XMoveResizeWindow( display, fs_window, x, y, w, h );
            XMapRaised( display, fs_window );
            x11_wait_mapped( display, fs_window );
            XRaiseWindow( display, fs_window );

            /* Since we just mapped the window and got our
             * Map event, we mark this here. */
            xcommon_exposed = 1;

            XReparentWindow( display, output_window, fs_window, 0, 0);

            /* Grab the pointer, grab input focus, then ungrab. */
            x11_grab_fullscreen_input( display, fs_window );
            XSetInputFocus( display, fs_window, RevertToPointerRoot, CurrentTime );
            x11_ungrab_fullscreen_input( display );
        }
    } else {
        XWindowAttributes attrs;

        XGetWindowAttributes( display, wm_window, &attrs );

        output_width = attrs.width;
        output_height = attrs.height;
        window_area.width = attrs.width;
        window_area.height = attrs.height;

        if( has_ewmh_state_fullscreen ) {
            XEvent ev;

            ev.type = ClientMessage;
            ev.xclient.display = display;
            ev.xclient.window = wm_window;
            ev.xclient.message_type = net_wm_state;
            ev.xclient.format = 32;
            ev.xclient.data.l[ 0 ] = 0;
            ev.xclient.data.l[ 1 ] = net_wm_state_fullscreen;
            ev.xclient.data.l[ 2 ] = 0;
            ev.xclient.data.l[ 3 ] = 0;
            ev.xclient.data.l[ 4 ] = 0;

            XSendEvent( display, DefaultRootWindow( display ), False,
                        SubstructureNotifyMask | SubstructureRedirectMask,
                        &ev );
        } else {
            XReparentWindow( display, output_window, wm_window, 0, 0 );
            XUnmapWindow( display, fs_window );
            x11_wait_unmapped( display, fs_window );
        }
    }
    XResizeWindow( display, output_window, output_width, output_height );
    calculate_video_area();
    xcommon_clear_screen();
    XFlush( display );
    XSync( display, False );

    return output_fullscreen;
}

int xcommon_toggle_root( int fullscreen_width, int fullscreen_height )
{
    return 0;
}

int xcommon_toggle_aspect( void )
{
    output_aspect = !output_aspect;
    calculate_video_area();
    x11_aspect_hint( display, wm_window, video_area.width, video_area.height );
    xcommon_clear_screen();
    return output_aspect;
}

void xcommon_poll_events( input_t *in )
{
    XEvent event;
    int reconfigure = 0;
    int reconfwidth = 0;
    int reconfheight = 0;
    int getfocus = 0;
    int motion = 0;
    int motion_x = 0;
    int motion_y = 0;
    int bx = -1;
    int by = -1;
    int bw = -1;
    int bh = -1;

    while( XPending( display ) ) {
        KeySym mykey;
        char mykey_string;
        int arg = 0;

        XNextEvent( display, &event );

        switch( event.type ) {
        case ClientMessage:
            /* X sucks this way.  Thanks to walken for help on this one. */
            if( ( event.xclient.message_type == wm_protocols ) && ( event.xclient.format == 32 ) &&
                ( event.xclient.data.l[ 0 ] == wm_delete_window ) ) {
                input_callback( in, I_QUIT, 0 );
            }
            break;

        case Expose:
            if( bx < 0 ) {
                bx = event.xexpose.x;
                by = event.xexpose.y;
                bw = event.xexpose.width;
                bh = event.xexpose.height;
            } else {
                int dx, dy, dw, dh;
                int cx, cy, cw, ch;
                cx = event.xexpose.x;
                cy = event.xexpose.y;
                cw = event.xexpose.width;
                ch = event.xexpose.height;
                dx = MIN(bx, cx);
                dy = MIN(by, cy);
                dw = MAX(bx + bw, cx + cw) - dx;
                dh = MAX(by + bh, cy + ch) - dy;
                bx = dx;
                by = dy;
                bw = dw;
                bh = dh;
            }
            break;
        case DestroyNotify:
            if( event.xdestroywindow.window != wm_window ) {
                break;
            }
            input_callback( in, I_QUIT, 0 );
            break;
        case MotionNotify:
            if( event.xmotion.x >= video_area.x && event.xmotion.x < (video_area.x + video_area.width) &&
                event.xmotion.y >= video_area.y && event.xmotion.y < (video_area.y + video_area.height) &&
                video_area.width && video_area.height ) {

                int videox = event.xmotion.x - video_area.x;
                int videoy = event.xmotion.y - video_area.y;
                int imagex = scale_area.x + ((scale_area.width * videox) / video_area.width);
                int imagey = scale_area.y + ((scale_area.height * videoy) / video_area.height);

                motion = 1;
                motion_x = imagex;
                motion_y = imagey;
            }
            XUndefineCursor( display, output_window );
            motion_timeout = 30;
            break;
        case EnterNotify:
            getfocus = 1;
            break;
        case FocusIn:
            has_focus = 1;
            break;
        case FocusOut:
            has_focus = 0;
            break;
        case MapNotify:
            if( !output_on_root ) {
                xcommon_exposed = 1;
                xcommon_clear_screen();
                if( xcommon_verbose ) {
                    fprintf( stderr, "xcommon: Received a map, marking window as visible (%lu).\n",
                             event.xany.serial );
                }
            }
            break;
        case UnmapNotify:
            if( !output_on_root ) {
                xcommon_exposed = 0;
                if( xcommon_verbose ) {
                    fprintf( stderr, "xcommon: Received an unmap, marking window as hidden (%lu).\n",
                             event.xany.serial );
                }
            }
            break;
        case VisibilityNotify:
            if( !output_on_root ) {
                if( event.xvisibility.state == VisibilityFullyObscured ) {
                    if( xcommon_exposed ) {
                        xcommon_exposed = 0;
                        if( xcommon_verbose ) {
                            fprintf( stderr, "xcommon: Window fully obscured, marking window as hidden (%lu).\n",
                                     event.xany.serial );
                        }
                    }
                } else if( !xcommon_exposed ) {
                    xcommon_exposed = 1;
                    xcommon_clear_screen();
                    if( xcommon_verbose ) {
                        fprintf( stderr, "xcommon: Window made visible, marking window as visible (%lu).\n",
                                 event.xany.serial );
                    }
                }
            }
            break;
        case ConfigureNotify:
            if( event.xconfigure.window == wm_window ) {
                window_area.x = event.xconfigure.x;
                window_area.y = event.xconfigure.y;
                window_area.width = event.xconfigure.width;
                window_area.height = event.xconfigure.height;
            }
            reconfwidth = event.xconfigure.width;
            reconfheight = event.xconfigure.height;
            if( reconfwidth != output_width || reconfheight != output_height ) {
                reconfigure = 1;
            }
            break;
        case KeyPress:
            /* if( event.xkey.state & ShiftMask ) arg |= I_SHIFT; */
            /* this alternate approach allows handling of keys like '<' and '>' -- mrallen */
            if( event.xkey.state & ShiftMask ) {
                mykey = XKeycodeToKeysym( display, event.xkey.keycode, 1 );
            } else {
                mykey = XKeycodeToKeysym( display, event.xkey.keycode, 0 );
            }
            if( event.xkey.state & ControlMask ) arg |= I_CTRL;
            if( event.xkey.state & Mod1Mask ) arg |= I_META;

            if( mykey >= XK_a && mykey <= XK_z ) {
                arg |= mykey - XK_a + 'a';
            } else if( mykey >= XK_A && mykey <= XK_Z ) {
                arg |= mykey - XK_A + 'a';
            } else if( mykey >= XK_0 && mykey <= XK_9 ) {
                arg |= mykey - XK_0 + '0';
            } else {
                XLookupString( &event.xkey, &mykey_string, 1, 0, 0 );
                arg |= mykey_string;
            }

            switch( mykey ) {
            case XK_Escape: arg |= I_ESCAPE; break;

            case XK_Home:      arg |= I_HOME; break;
            case XK_End:       arg |= I_END; break;
            case XK_Left:      arg |= I_LEFT; break;
            case XK_Up:        arg |= I_UP; break;
            case XK_Right:     arg |= I_RIGHT; break;
            case XK_Down:      arg |= I_DOWN; break;
            case XK_Page_Up:   arg |= I_PGUP; break;
            case XK_Page_Down: arg |= I_PGDN; break;
            case XK_Insert:    arg |= I_INSERT; break;

            case XK_F1: arg |= I_F1; break;
            case XK_F2: arg |= I_F2; break;
            case XK_F3: arg |= I_F3; break;
            case XK_F4: arg |= I_F4; break;
            case XK_F5: arg |= I_F5; break;
            case XK_F6: arg |= I_F6; break;
            case XK_F7: arg |= I_F7; break;
            case XK_F8: arg |= I_F8; break;
            case XK_F9: arg |= I_F9; break;
            case XK_F10: arg |= I_F10; break;
            case XK_F11: arg |= I_F11; break;
            case XK_F12: arg |= I_F12; break;

            case XK_KP_Enter: arg |= I_ENTER; break;
            case XK_KP_0: arg |= '0'; break;
            case XK_KP_1: arg |= '1'; break;
            case XK_KP_2: arg |= '2'; break;
            case XK_KP_3: arg |= '3'; break;
            case XK_KP_4: arg |= '4'; break;
            case XK_KP_5: arg |= '5'; break;
            case XK_KP_6: arg |= '6'; break;
            case XK_KP_7: arg |= '7'; break;
            case XK_KP_8: arg |= '8'; break;
            case XK_KP_9: arg |= '9'; break;
            case XK_KP_Multiply: arg |= '*'; break;
            case XK_KP_Subtract: arg |= '-'; break;
            case XK_KP_Add: arg |= '+'; break;
            case XK_KP_Divide: arg |= '/'; break;

            case XK_KP_Insert: arg |= '0'; break;
            case XK_KP_End: arg |= '1'; break;
            case XK_KP_Down: arg |= '2'; break;
            case XK_KP_Next: arg |= '3'; break;
            case XK_KP_Left: arg |= '4'; break;
            case XK_KP_Begin: arg |= '5'; break;
            case XK_KP_Right: arg |= '6'; break;
            case XK_KP_Home: arg |= '7'; break;
            case XK_KP_Up: arg |= '8'; break;
            case XK_KP_Prior: arg |= '9'; break;

            default: break;
            }
            input_callback( in, I_KEYDOWN, arg );
            break;
        case ButtonPress:
            if( event.xbutton.time != lastpresstime ) {
                input_callback( in, I_BUTTONPRESS, event.xbutton.button );
                lastpresstime = event.xbutton.time;
            }
            getfocus = 1;
            break;
        case ButtonRelease:
            if( event.xbutton.time != lastreleasetime ) {
                input_callback( in, I_BUTTONRELEASE, event.xbutton.button );
                lastreleasetime = event.xbutton.time;
            }
            getfocus = 1;
            break;
        default: break;
        }
    }

    if( motion ) {
        input_callback( in, I_MOUSEMOVE, ((motion_x & 0xffff) << 16 | (motion_y & 0xffff)) );
    }

    if( getfocus ) {
        XSetInputFocus( display, wm_window, RevertToPointerRoot, CurrentTime );
    }

    if( !has_ewmh_state_fullscreen && output_fullscreen ) {
        /**
         * We used to check if we still have focus too.
         * Let's ignore that for now and see how well this
         * works.
         */
        if( !xcommon_exposed /* || !has_focus */ ) {
            /**
             * Switch back to windowed mode if we have lost
             * input focus or window visibility.
             */
            xcommon_toggle_fullscreen( 0, 0 );
        }
    }

    if( bx >= 0 ) {
        xcommon_clear_area( bx, by, bw, bh );
    }

    if( reconfigure ) {
        if( !output_on_root && !output_fullscreen ) {
            output_width = reconfwidth;
            output_height = reconfheight;
            XResizeWindow( display, output_window,
                           output_width, output_height );
        }

        calculate_video_area();
        xcommon_clear_screen();
        XSync( display, False );
    }
}

void xcommon_close_display( void )
{
    XDestroyWindow( display, output_window );
    XDestroyWindow( display, wm_window );
    XDestroyWindow( display, fs_window );
    XCloseDisplay( display );
    xfullscreen_delete( xf );
}

void xcommon_set_window_caption( const char *caption )
{
    XStoreName( display, wm_window, caption );
    XChangeProperty( display, wm_window, net_wm_name, utf8_string, 8,
                     PropModeReplace, (const unsigned char *) caption,
                     strlen( caption ) );
}

Display *xcommon_get_display( void )
{
    return display;
}

Window xcommon_get_output_window( void )
{
    return output_window;
}

GC xcommon_get_gc( void )
{
    return gc;
}

int xcommon_is_fullscreen( void )
{
    return output_fullscreen;
}

int xcommon_is_alwaysontop( void )
{
    return alwaysontop;
}

int xcommon_is_fullscreen_supported( void )
{
    return 1;
}

int xcommon_is_alwaysontop_supported( void )
{
    return has_ewmh_state_above;
}

area_t xcommon_get_video_area( void )
{
    return video_area;
}

int xcommon_is_exposed( void )
{
    return xcommon_exposed;
}

void xcommon_set_colourkey( int colourkey )
{
    xcommon_colourkey = colourkey;
}

int xcommon_get_visible_width( void )
{
    return video_area.width;
}

int xcommon_get_visible_height( void )
{
    return video_area.height;
}

area_t xcommon_get_window_area( void )
{
    return window_area;
}

void xcommon_set_fullscreen_position( int pos )
{
    fullscreen_position = pos;
    calculate_video_area();
    xcommon_clear_screen();
}

void xcommon_set_matte( int width, int height )
{
    if( width ) {
        x11_aspect_hint( display, wm_window, width, height );
    } else {
        x11_aspect_hint( display, wm_window, video_area.width, video_area.height );
    }
    matte_width = width;
    matte_height = height;
    calculate_video_area();
    xcommon_clear_screen();
    XSync( display, False );
}

void xcommon_set_video_scale( area_t scalearea )
{
    scale_area = scalearea;
}

void xcommon_update_xawtv_station( int frequency, int channel_id,
                                   const char *channel_name )
{
    char data[ 1024 ];
    int length;

    length  = sprintf( data, "%.3f", ((float) frequency) / 1000.0 ) + 1;
    length += sprintf( data + length, "%d", channel_id ) + 1;
    length += sprintf( data + length, "%s", channel_name ) + 1;

    XChangeProperty( display, wm_window, xawtv_station,
                     XA_STRING, 8, PropModeReplace,
                     (unsigned char *) data, length );
}


