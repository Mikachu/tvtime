
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

#include "display.h"
#include "input.h"
#include "speedy.h"
#include "xcommon.h"

/* Every 30 seconds, ping the screensaver. */
#define SCREENSAVER_PING_TIME (30 * 1000 * 1000)

static Display *display;
static int screen;
static Window wm_window;
static Window fs_window;
static Window output_window;
static Window windowed_output_window;
static GC gc;
static int have_xtest = 0;
static int output_width, output_height;
static int output_aspect = 0;
static int output_on_root = 0;
static int has_ewmh_state_fullscreen = 0;
static Cursor nocursor;
static int output_fullscreen = 0;
static int xcommon_verbose = 0;
static int xcommon_exposed = 0;
static int xcommon_colourkey = 0;
static int motion_timeout = 0;
static int kicked_out_of_fullscreen = 0;

static Atom wmProtocolsAtom;
static Atom wmDeleteAtom;

#ifdef HAVE_XTESTEXTENSION
static KeyCode kc_shift_l; /* Fake key to send. */
#endif

static area_t video_area;
static area_t window_area;

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
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

static int xv_get_width_for_height( int window_height )
{
    int widthratio = output_aspect ? 16 : 4;
    int heightratio = output_aspect ? 9 : 3;
    int sar_frac_n, sar_frac_d;

    if( DpyInfoGetSAR( display, screen, &sar_frac_n, &sar_frac_d ) ) {
        if( xcommon_verbose ) {
            fprintf( stderr, "xvoutput: Sample aspect ratio %d/%d.\n",
                     sar_frac_n, sar_frac_d );
        }
    } else {
        /* Assume 4:3 aspect ? */
        if( xcommon_verbose ) {
            fprintf( stderr, "xvoutput: Assuming square pixel display.\n" );
        }
        sar_frac_n = 1;
        sar_frac_d = 1;
    }

    /* Correct for our non-square pixels, and for current aspect ratio (4:3 or 16:9). */
    return ( window_height * sar_frac_n * widthratio ) / ( sar_frac_d * heightratio );
}

static void x11_aspect_hint( Display *dpy, Window win, int aspect_width, int aspect_height )
{
   /* These hints don't work with current versions of metacity.
    * I will be doing some detection code, but in the mean time,
    * let's not use the hints.  -Billy
    */
/*
    XSizeHints hints;

    hints.flags = PAspect;
    hints.min_aspect.x = aspect_width;
    hints.min_aspect.y = aspect_height;
    hints.max_aspect.x = aspect_width;
    hints.max_aspect.y = aspect_height;

    XSetWMNormalHints( dpy, win, &hints );
*/
}

/**
 * returns 1 if a window manager compliant to the
 * Extended Window Manager Hints (EWMH) spec.
 * supports the _NET_WM_STATE_FULLSCREEN window state
 * Oterhwise returns 0.
 */
static int check_for_state_fullscreen( Display *dpy )
{
    Atom net_supported, net_wm_state, net_wm_state_fullscreen;
    Atom type_return;
    int format_return;
    unsigned long nitems_return;
    unsigned long bytes_after_return;
    unsigned char *prop_return = 0;
    int nr_items = 40;
    int item_offset = 0;
    int supports_net_wm_state = 0;
    int supports_net_wm_state_fullscreen = 0;

    net_supported = XInternAtom( dpy, "_NET_SUPPORTED", False );
    if( net_supported == None ) {
        return 0;
    }

    net_wm_state = XInternAtom( dpy, "_NET_WM_STATE", False );
    if( net_wm_state == None ) {
        return 0;
    }

    net_wm_state_fullscreen = XInternAtom( dpy, "_NET_WM_STATE_FULLSCREEN", False );
    if( net_wm_state_fullscreen == None ) {
        return 0;
    }

    do {
        if( XGetWindowProperty( dpy, DefaultRootWindow( dpy ), net_supported,
                                item_offset, nr_items, False, XA_ATOM,
                                &type_return, &format_return, &nitems_return,
                                &bytes_after_return, &prop_return) != Success ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xcommon: XGetWindowProperty failed in check_for_state_fullscreen\n" );
            }
            return 0;
        }
    
        if( type_return == None ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xvoutput: check_for_state_fullscreen: property does not exist\n" );
            }
            return 0;
        }

        if( type_return != XA_ATOM ) {
            if( xcommon_verbose ) {
                fprintf( stderr, "xvoutput: check_for_state_fullscreen: XA_ATOM property has wrong type\n");
            }
            if( prop_return ) XFree( prop_return );
            return 0;
        } else {
            if( format_return == 32 ) {
                int n;

                for( n = 0; n < nitems_return; n++ ) {
                    if( ((long *) prop_return)[n] == net_wm_state ) {
                        supports_net_wm_state = 1;
                    } else if( ((long *) prop_return)[n] == net_wm_state_fullscreen ) {
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
    int widthratio = output_aspect ? 16 : 4;
    int heightratio = output_aspect ? 9 : 3;
    int sar_frac_n, sar_frac_d;

    if( DpyInfoGetSAR( display, screen, &sar_frac_n, &sar_frac_d ) ) {
        if( xcommon_verbose ) {
            fprintf( stderr, "xvoutput: Sample aspect ratio %d/%d.\n",
                     sar_frac_n, sar_frac_d );
        }
    } else {
        /* Assume 4:3 aspect ? */
        if( xcommon_verbose ) {
            fprintf( stderr, "xvoutput: Assuming square pixel display.\n" );
        }
        sar_frac_n = 1;
        sar_frac_d = 1;
    }

    /* Correct for our non-square pixels, and for current aspect ratio (4:3 or 16:9). */
    curwidth = output_width;
    curheight = ( output_width * sar_frac_d * heightratio ) / ( sar_frac_n * widthratio );
    if( curheight > output_height ) {
        curheight = output_height;
        curwidth = ( output_height * sar_frac_n * widthratio ) / ( sar_frac_d * heightratio );
    }

    video_area.x = ( output_width - curwidth ) / 2;
    video_area.y = ( output_height - curheight ) / 2;
    video_area.width = curwidth;
    video_area.height = curheight;

    if( xcommon_verbose ) {
        fprintf( stderr, "xvoutput: Displaying in a %dx%d window inside %dx%d space.\n",
                 curwidth, curheight, output_width, output_height );
    }
}


int xcommon_open_display( int aspect, int init_height, int verbose )
{
    Pixmap curs_pix;
    XEvent xev;
    XSizeHints hint;
    XClassHint classhint;
    XColor curs_col;
    XSetWindowAttributes xswa;
    const char *hello = "tvtime";
    unsigned long mask;
    int displaywidthratio;
    int displayheightratio;

    output_aspect = aspect;
    output_height = init_height;
    xcommon_verbose = verbose;

    display = XOpenDisplay( 0 );
    if( !display ) {
        if( getenv( "DISPLAY" ) ) {
            fprintf( stderr, "xvoutput: Cannot open display '%s'.\n", getenv( "DISPLAY" ) );
        } else {
            fprintf( stderr, "xvoutput: No DISPLAY set, so no output possible!\n" );
        }
        return 0;
    }

    if( xcommon_verbose ) {
        fprintf( stderr, "xvoutput: Display %s, vendor %s, ",
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

    screen = DefaultScreen( display );

    DpyInfoInit( display, screen );
    DpyInfoSetUpdateResolution( display, screen, DpyInfoOriginXinerama );
    DpyInfoSetUpdateGeometry( display, screen, DpyInfoOriginXinerama );

    DpyInfoUpdateResolution( display, screen, 0, 0 );
    DpyInfoUpdateGeometry( display, screen );

    if( xcommon_verbose && DpyInfoGetGeometry( display, screen, &displaywidthratio, &displayheightratio ) ) {
        fprintf( stderr, "xvoutput: Geometry %dx%d, display aspect ratio %.2f\n",
                 displaywidthratio, displayheightratio,
                 (double) displaywidthratio / (double) displayheightratio );
    }

#ifdef HAVE_XTESTEXTENSION
    kc_shift_l = XKeysymToKeycode( display, XK_Shift_L );
#endif
    have_xtest = have_xtestextention();
    if( have_xtest && xcommon_verbose ) {
        fprintf( stderr, "xvoutput: Have XTest, will use it to ping the screensaver.\n" );
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
    xswa.event_mask = ButtonPressMask | StructureNotifyMask | KeyPressMask | PointerMotionMask |
                      VisibilityChangeMask | PropertyChangeMask;

    mask = (CWBackPixel | CWSaveUnder | CWBackingStore | CWEventMask);

    wm_window = XCreateWindow( display, RootWindow( display, screen ), 0, 0,
                               output_width, output_height, 0,
                               CopyFromParent, InputOutput, CopyFromParent, mask, &xswa);

    output_window = XCreateWindow( display, wm_window, 0, 0,
                                   output_width, output_height, 0,
                                   CopyFromParent, InputOutput, CopyFromParent, mask, &xswa);

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

        memset(&ev, 0, sizeof(ev));
        ev.xclient.type = ClientMessage;
        ev.xclient.window = DefaultRootWindow( display );
        ev.xclient.message_type = XInternAtom( display, "KWM_KEEP_ON_TOP", False );
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = fs_window;
        ev.xclient.data.l[1] = CurrentTime;
        mask = SubstructureRedirectMask;
        XSendEvent( display, DefaultRootWindow( display ), False, mask, &ev );
    }

    has_ewmh_state_fullscreen = check_for_state_fullscreen( display );
    if( has_ewmh_state_fullscreen && xcommon_verbose ) {
        fprintf( stderr, "xvoutput: Using EWMH state fullscreen property.\n" );
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

    XMapWindow( display, output_window );
    XMapWindow( display, wm_window );
    // x11_wait_mapped( display, wm_window );

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

    wmProtocolsAtom = XInternAtom( display, "WM_PROTOCOLS", False );
    wmDeleteAtom = XInternAtom( display, "WM_DELETE_WINDOW", False );

    /* collaborate with the window manager for close requests */
    XSetWMProtocols( display, wm_window, &wmDeleteAtom, 1 );

    calculate_video_area();
    x11_aspect_hint( display, wm_window, video_area.width, video_area.height );

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

void xcommon_set_window_height( int window_height )
{
    XWindowChanges win_changes;
    int window_width;

    if( output_fullscreen ) {
        xcommon_toggle_fullscreen( 0, 0 );
    }

    window_width = xv_get_width_for_height( window_height );

    if( xcommon_verbose ) {
        fprintf( stderr, "xvoutput: Target window size %dx%d.\n", window_width, window_height );
    }

    win_changes.width = window_width;
    win_changes.height = window_height;

    XResizeWindow( display, wm_window, window_width, window_height );
    XReconfigureWMWindow( display, wm_window, 0, CWWidth | CWHeight, &win_changes );
}

void xcommon_clear_screen( void )
{
    XSetForeground( display, gc, BlackPixel( display, screen ) );
    XClearWindow( display, output_window );
    XSetForeground( display, gc, xcommon_colourkey );
    XFillRectangle( display, output_window, gc, video_area.x, video_area.y,
                    video_area.width, video_area.height );
    XSync( display, False );
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


int xcommon_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
    XWindowAttributes attrs;
    XGetWindowAttributes( display, wm_window, &attrs );

    output_fullscreen = !output_fullscreen;
    if( output_fullscreen ) {
        if( has_ewmh_state_fullscreen ) {
            XEvent ev;

            ev.type = ClientMessage;
            ev.xclient.window = wm_window;
            ev.xclient.message_type = XInternAtom( display, "_NET_WM_STATE", False );
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD not an atom just a define
            ev.xclient.data.l[1] = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", False );
            ev.xclient.data.l[2] = 0;

            XSendEvent( display, DefaultRootWindow( display ), False, SubstructureNotifyMask, &ev );
        } else {
            int x, y, w, h;
            double refresh;

            DpyInfoGetScreenOffset( display, XScreenNumberOfScreen( attrs.screen ), &x, &y );
            DpyInfoGetResolution( display, XScreenNumberOfScreen( attrs.screen ), &w, &h, &refresh );

            /* Show our fullscreen window. */
            XMoveResizeWindow( display, fs_window, x, y, w, h );

            XMapWindow( display, fs_window );
            {   /* stack the window on top or below the four windows 
                   used to change desktops, if detected. */
                Window root;
                Window parent;
                Window *children;
                Window stacking[2];
                unsigned int nchildren;
                int x1, y1;
                unsigned int width, height;
                unsigned int border;
                unsigned int depth;
                int i, score= 0;
                
                XQueryTree(display, wm_window, &root, &parent, &children, &nchildren);
                XFree( children );
                XQueryTree(display, root, &root, &parent, &children, &nchildren);
                if( nchildren > 4 ) {
                    for( i= 1; i <= 4; ++i ) {
                        XGetGeometry( display, children[nchildren-i], &root, &x1, &y1, 
                                &width, &height, &border, &depth);
                        if( ( ( width == w ) && ( height < 10 ) && ( x1 == 0 ) ) 
                                || ( ( height == h ) && ( width < 10 ) && ( y1 == 0 ) ) ) {
                            ++score;
                        }
                    }
                } 

                if( score == 4 ) {
                    stacking[0]= children[nchildren-4];
                    stacking[1]= fs_window;
                    XRestackWindows( display, stacking, 2 );
                } else {
                    XRaiseWindow( display, fs_window );
                }
                XFree( children );
            }
            x11_wait_mapped( display, fs_window );

            /* Since we just mapped the window and got our
             * Map event, we mark this here. */
            xcommon_exposed = 1;

            XReparentWindow( display, output_window, fs_window, 0, 0);
            XMoveWindow( display, fs_window, x, y );
            XSetInputFocus( display, wm_window, RevertToPointerRoot, CurrentTime );
            output_width = w;
            output_height = h;
        }
    } else {
        if( has_ewmh_state_fullscreen ) {
            XEvent ev;

            ev.type = ClientMessage;
            ev.xclient.window = wm_window;
            ev.xclient.message_type = XInternAtom( display, "_NET_WM_STATE", False );
            ev.xclient.format = 32;
            ev.xclient.data.l[0] = 0; // _NET_WM_STATE_REMOVE not an atom just a define
            ev.xclient.data.l[1] = XInternAtom( display, "_NET_WM_STATE_FULLSCREEN", False );
            ev.xclient.data.l[2] = 0;

            XSendEvent( display, DefaultRootWindow( display ), False, SubstructureNotifyMask, &ev );
        } else {
            XReparentWindow( display, output_window, wm_window, 0, 0);
            XUnmapWindow( display, fs_window );
            x11_wait_unmapped( display, fs_window );
            output_width = attrs.width;
            output_height = attrs.height;
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
    XWindowAttributes attrs;
    XGetWindowAttributes( display, wm_window, &attrs );

    output_fullscreen = !output_fullscreen;
    if( output_fullscreen ) {
        int x, y, w, h;
        double refresh;

        DpyInfoGetScreenOffset( display, XScreenNumberOfScreen( attrs.screen ), &x, &y );
        DpyInfoGetResolution( display, XScreenNumberOfScreen( attrs.screen ), &w, &h, &refresh );

        windowed_output_window = output_window;
        output_window = RootWindow( display, screen );

        /* Set up our satellite window. */
        XResizeWindow( display, wm_window, 100, 100 );
        XSetForeground( display, gc, BlackPixel( display, screen ) );
        XClearWindow( display, wm_window );

        output_on_root = 1;
        output_width = w;
        output_height = h;
        xcommon_exposed = 1;
    } else {
        /* Clear the root window. */
        XSetForeground( display, gc, BlackPixel( display, screen ) );
        XClearWindow( display, output_window );

        /* Back to normal. */
        output_window = windowed_output_window;
        output_on_root = 0;
        output_width = attrs.width;
        output_height = attrs.height;
    }
    XResizeWindow( display, output_window, output_width, output_height );
    calculate_video_area();
    xcommon_clear_screen();
    XFlush( display );
    XSync( display, False );
    return output_fullscreen;
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

    while( XPending( display ) ) {
        KeySym mykey;
        char mykey_string;
        int arg = 0;

        XNextEvent( display, &event );

        switch( event.type ) {
        case ClientMessage:
            /* X sucks this way.  Thanks to walken for help on this one. */
            if( ( event.xclient.message_type == wmProtocolsAtom ) && ( event.xclient.format == 32 ) &&
                ( event.xclient.data.l[ 0 ] == wmDeleteAtom ) ) {
                input_callback( in, I_QUIT, 0 );
            }
            break;

        case DestroyNotify:
            if( event.xdestroywindow.window != wm_window ) {
                break;
            }
            input_callback( in, I_QUIT, 0 );
            break;
        case MotionNotify:
            XUndefineCursor( display, output_window );
            motion_timeout = 30;
            break;
        case EnterNotify:
            XSetInputFocus( display, wm_window, RevertToPointerRoot, CurrentTime );
            break;
        case MapNotify:
            if( !output_on_root ) {
                xcommon_exposed = 1;
                xcommon_clear_screen();
                if( xcommon_verbose ) {
                    fprintf( stderr, "xvoutput: Received a map, marking window as visible (%lu).\n",
                             event.xany.serial );
                }
            }
            break;
        case UnmapNotify:
            if( !output_on_root ) {
                xcommon_exposed = 0;
                if( xcommon_verbose ) {
                    fprintf( stderr, "xvoutput: Received an unmap, marking window as hidden (%lu).\n",
                             event.xany.serial );
                }
            }
            break;
        case VisibilityNotify:
            if( !output_on_root ) {
                if( event.xvisibility.state == VisibilityFullyObscured ) {
                    if( !xcommon_exposed ) {
                        xcommon_exposed = 0;
                        if( xcommon_verbose ) {
                            fprintf( stderr, "xvoutput: Window fully obscured, marking window as hidden (%lu).\n",
                                     event.xany.serial );
                        }
                    }
                } else if( !xcommon_exposed ) {
                    xcommon_exposed = 1;
                    xcommon_clear_screen();
                    if( xcommon_verbose ) {
                        fprintf( stderr, "xvoutput: Window made visible, marking window as visible (%lu).\n",
                                 event.xany.serial );
                    }
                }
            }
            break;
        case ConfigureNotify:
            window_area.x = event.xconfigure.x;
            window_area.y = event.xconfigure.y;
            window_area.width = event.xconfigure.width;
            window_area.height = event.xconfigure.height;
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
            input_callback( in, I_BUTTONPRESS, event.xbutton.button );
            break;
        default: break;
        }
    }

    if( !has_ewmh_state_fullscreen ) {
        Window focus_win;
        int focus_revert;

        XGetInputFocus( display, &focus_win, &focus_revert );
        if( output_fullscreen ) {
            if( reconfigure || !xcommon_exposed || focus_win != wm_window ) {
                /* Switch back to windowed mode if we've lost focus or visibility. */
                xcommon_toggle_fullscreen( 0, 0 );
                kicked_out_of_fullscreen = 1;
            }
        } else if( kicked_out_of_fullscreen && ( xcommon_exposed && focus_win == wm_window ) ) {
            /* Switch back to fullscreen mode if we regain visibility
             * after being kicked out of fullscreen mode. */
            xcommon_toggle_fullscreen( 0, 0 );
            kicked_out_of_fullscreen = 0;
        }
    }

    if( !output_on_root && reconfigure ) {
        output_width = reconfwidth;
        output_height = reconfheight;
        XResizeWindow( display, output_window, output_width, output_height );
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
}

void xcommon_set_window_caption( const char *caption )
{
    XStoreName( display, wm_window, caption );
    XSetIconName( display, wm_window, caption );
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
