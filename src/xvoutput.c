

#include "config.h"
#include "xvoutput.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#if defined(__FreeBSD__)
#include <machine/param.h>
#endif
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

#define FOURCC_YUY2 0x32595559

static XvImage *image;
static unsigned char *image_data;
static XShmSegmentInfo shminfo;
static Window window;
static Display *display;
static GC gc;
static XvPortID xv_port;
static int completion_type;
static XWindowAttributes attribs;

static int output_width, output_height;
static int output_x, output_y;
static int input_width, input_height;
static int output_aspect = 0;
static Cursor nocursor;
static int output_fullscreen = 0;

static int window_width, window_height;
static int window_x, window_y;

int HandleXError( Display *display, XErrorEvent *xevent )
{
    char str[ 1024 ];

    XGetErrorText( display, xevent->error_code, str, 1024 );
    fprintf( stderr, "xvoutput: Received X error event: %s\n", str );
    return 0;
}

static void x11_InstallXErrorHandler( void )
{
    XSetErrorHandler( HandleXError );
    XFlush( display );
}

static int xv_port_has_yuy2( XvPortID port )
{
    XvImageFormatValues *formatValues;
    int formats;
    int i;

    formatValues = XvListImageFormats( display, port, &formats );
    for( i = 0; i < formats; i++ ) {
        if((formatValues[ i ].id == FOURCC_YUY2) && (!(strcmp( formatValues[ i ].guid, "YUY2" )))) {
            XFree (formatValues);
            return 1;
        }
    }
    XFree( formatValues );
    return 0;
}

static int xv_check_extension( void )
{
    unsigned int version;
    unsigned int release;
    unsigned int dummy;
    unsigned int adaptors;
    unsigned int i;
    unsigned long j;
    XvAdaptorInfo *adaptorInfo;

    if( ( XvQueryExtension( display, &version, &release,
                            &dummy, &dummy, &dummy) != Success ) ||
         ( version < 2 ) || ( ( version == 2 ) && ( release < 2 ) ) ) {

        fprintf( stderr, "xvoutput: xv extension not found.\n");
        return 1;
    }

    XvQueryAdaptors( display, window, &adaptors, &adaptorInfo );

    for( i = 0; i < adaptors; i++ ) {
        if( adaptorInfo[ i ].type & XvImageMask ) {
            for( j = 0; j < adaptorInfo[ i ].num_ports; j++ ) {
                if( xv_port_has_yuy2( adaptorInfo[ i ].base_id + j ) ) {
                    if( XvGrabPort( display, adaptorInfo[ i ].base_id + j, 0 ) == Success ) {
                        xv_port = adaptorInfo[ i ].base_id + j;
                        XvFreeAdaptorInfo( adaptorInfo );
                        return 1;
                    }
                }
            }
        }
    }

    XvFreeAdaptorInfo( adaptorInfo );
    fprintf( stderr, "Cannot find xv port\n" );
    return 0;
}

static int open_display( void )
{
    Pixmap curs_pix;
    XEvent xev;
    int major;
    int minor;
    Bool pixmaps;
    int screen;
    unsigned int fg, bg;
    char *hello = "tvtime";
    XSizeHints hint;
    XColor curs_col;
    XSetWindowAttributes xswa;
    unsigned long mask;

    display = XOpenDisplay( 0 );
    if( !display ) {
	fprintf( stderr, "Can not open display\n" );
	return 0;
    }

    if( ( XShmQueryVersion( display, &major, &minor, &pixmaps) == 0 ) ||
	 (major < 1) || ((major == 1) && (minor < 1))) {
	fprintf (stderr, "No xshm extension\n");
	return 0;
    }

    completion_type = XShmGetEventBase( display ) + ShmCompletion;

    screen = DefaultScreen(display);
    bg = BlackPixel( display, screen );
    fg = BlackPixel( display, screen );
    XGetWindowAttributes( display, DefaultRootWindow( display ), &attribs );

    fprintf( stderr, "going to create window.\n" );
    // window = XCreateSimpleWindow( display, DefaultRootWindow( display ), 0, 0, 720, 480, 4, fg, bg );

    xswa.override_redirect = False;
    xswa.backing_store = NotUseful;
    xswa.save_under = False;
    xswa.background_pixel = BlackPixel( display, screen );

    mask = (CWBackPixel | CWSaveUnder | CWBackingStore | CWOverrideRedirect);

    window = XCreateWindow( display, RootWindow( display, screen ), 0, 0,
                            output_width, output_height, 0,
                            CopyFromParent, InputOutput, CopyFromParent, mask, &xswa);


    gc = DefaultGC(display, screen);

    hint.x = 0;
    hint.y = 0;
    hint.width = input_width;
    hint.height = input_height;
    hint.flags = PPosition | PSize;

    XSetStandardProperties( display, window, hello, hello, None, 0, 0, &hint );

    XSelectInput( display, window, ButtonPressMask | StructureNotifyMask | KeyPressMask );

    XMapWindow( display, window );

    /* Wait for map. */
    XMaskEvent( display, StructureNotifyMask, &xev );

    /* Create a 1 pixel cursor to use in full screen mode */
    curs_pix = XCreatePixmap( display, window, 1, 1, 1 );
    curs_col.pixel = 0;
    curs_col.red = 0;
    curs_col.green = 0;
    curs_col.blue = 0;
    curs_col.flags = 0;
    curs_col.pad = 0;
    nocursor = XCreatePixmapCursor( display, curs_pix, curs_pix, &curs_col, &curs_col, 1, 1 );
    return 1;
}

static void *create_shm( int size )
{
    int error = 1;

    shminfo.shmid = shmget( IPC_PRIVATE, size, IPC_CREAT | 0777 );
    if( shminfo.shmid != -1 ) {

        shminfo.shmaddr = (char *) shmat( shminfo.shmid, 0, 0 );
        if( shminfo.shmaddr != (char *)-1 ) {

            /* On linux the IPC_RMID only kicks off once everyone detaches the shm */
            /* doing this early avoids shm leaks when we are interrupted. */
            /* this would break the solaris port though :-/ */
            /* shmctl (instance->shminfo.shmid, IPC_RMID, 0); */

            /* XShmAttach fails on remote displays, so we have to catch this event */

            XSync( display, False );
            x11_InstallXErrorHandler();

            shminfo.readOnly = True;
            if( XShmAttach( display, &shminfo ) ) {
                error = 0;
            }

            XSync( display, False );
            XSetErrorHandler( 0 );
        }
    }

    if( error ) {
        fprintf( stderr, "cannot create shared memory\n" );
        return 0;
    } else {
        return shminfo.shmaddr;
    }
}

static void xv_alloc_frame( void )
{
    int size;
    unsigned char *alloc;

    size = input_width * input_height * 2;
    alloc = (unsigned char *) create_shm( size);
    if( alloc ) {
        fprintf( stderr, "creating image.\n" );
        image = XvShmCreateImage( display, xv_port, FOURCC_YUY2, (char *) alloc,
                                  input_width, input_height, &shminfo );
        image_data = alloc;
    }
}

int xv_init( int inputwidth, int inputheight, int outputwidth, int aspect )
{
    output_aspect = aspect;
    input_width = inputwidth;
    input_height = inputheight;
    output_width = inputwidth;
    output_height = inputheight;
    open_display();
    xv_check_extension();
    xv_alloc_frame();
    return 1;
}

void sizehint( int x, int y, int width, int height, int max )
{
    XSizeHints hints;
    hints.flags = PPosition | PSize | PWinGravity | PBaseSize;
    hints.x = x; hints.y = y; hints.width = width; hints.height = height;
    if( max ) {
        hints.max_width = width; hints.max_height=height;
        hints.flags |= PMaxSize;
    } else {
        hints.max_width = 0; hints.max_height = 0; 
    }
    hints.base_width = width; hints.base_height = height;
    hints.win_gravity = StaticGravity;
    XSetWMNormalHints( display, window, &hints );
}

int xv_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
/*
    output_fullscreen = !output_fullscreen;
    if( output_fullscreen ) {
        window_width = output_width;
        window_height = output_height;
        window_x = output_x;
        window_y = output_y;

        output_width = fullscreen_width;
        output_height = fullscreen_height;

        sizehint( 0, 0, output_width, output_height, 0 );

        XMoveResizeWindow( display, window, 0, 0, output_width, output_height );
        XGrabPointer( display, window, True, 0, GrabModeAsync, GrabModeAsync,
                      window, None, CurrentTime );
        XGrabKeyboard( display, window, True, GrabModeAsync, GrabModeAsync, CurrentTime );
        XDefineCursor( display, window, nocursor );
        XRaiseWindow( display, window );
        XSync( display, True );
    } else {
        output_width = window_width;
        output_height = window_height;
        output_x = window_x;
        output_y = window_y;

        sizehint( output_x, output_y, output_width, output_height, 0 );

        XUngrabPointer( display, CurrentTime );
        XUngrabKeyboard( display, CurrentTime );
        XUndefineCursor( display, window );
        XMoveResizeWindow( display, window, window_x, window_y, output_width, output_height );
    }
*/

    return output_fullscreen;
}

int xv_toggle_aspect( void )
{
    output_aspect = !output_aspect;
    return output_aspect;
}

void xv_show_frame( void )
{
    int x, y, w, h;
    int curwidth, curheight;
    int widthratio = output_aspect ? 16 : 4;
    int heightratio = output_aspect ? 9 : 3;

    curwidth = output_width;
    curheight = ( output_width / widthratio ) * heightratio;
    if( curheight > output_height ) {
        curheight = output_height;
        curwidth = ( output_height / heightratio ) * widthratio;
    }

    x = ( output_width - curwidth ) / 2;
    y = ( output_height - curheight ) / 2;
    w = curwidth;
    h = curheight;

    XvShmPutImage( display, xv_port, window, gc, image,
                   0, 0, input_width, input_height,
                   x, y, w, h, False );
    XFlush( display );
}

void xv_poll_events( input_t *in )
{
    Window junk;
    XEvent event;
    KeySym mykey;
    int arg = 0;

    while (XPending(display)) {
        XNextEvent(display, &event);
        switch( event.type ) {
        case ConfigureNotify:
	    XTranslateCoordinates( display, window, DefaultRootWindow( display ), 0, 0,
			           &output_x, &output_y, &junk );

            if( event.xconfigure.width != output_width || event.xconfigure.height != output_height ) {
                output_width = event.xconfigure.width;
                output_height = event.xconfigure.height;
                XClearWindow( display, window );
            }
            break;
        case KeyPress:
	    mykey = XKeycodeToKeysym( display, event.xkey.keycode, 0 );
            if( event.xkey.state & ShiftMask ) arg |= I_SHIFT;
            if( event.xkey.state & ControlMask ) arg |= I_CTRL;
            if( event.xkey.state & Mod1Mask ) arg |= I_META;

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

            default: arg |= mykey - XK_a + 'a'; break;
            }
            input_callback( in, I_KEYDOWN, arg );
        default: break;
        }
    }
}

void xv_quit( void )
{
    XShmDetach( display, &shminfo );
    shmdt( shminfo.shmaddr );
    XDestroyWindow( display, window );
    XCloseDisplay( display );
}

int xv_get_stride( void )
{
    return image->pitches[ 0 ];
}

int xv_is_interlaced( void )
{
    return 0;
}

void xv_wait_for_sync( int field )
{
}

void xv_lock_output( void )
{
}

void xv_unlock_output( void )
{
}

void xv_set_window_caption( const char *caption )
{
}

unsigned char *xv_get_output( void )
{
    return image_data;
}

static output_api_t xvoutput =
{
    xv_init,

    xv_lock_output,
    xv_get_output,
    xv_get_stride,
    xv_unlock_output,

    xv_is_interlaced,
    xv_wait_for_sync,
    xv_show_frame,

    xv_toggle_aspect,
    xv_toggle_fullscreen,
    xv_set_window_caption,

    xv_poll_events,
    xv_quit
};

output_api_t *get_xv_output( void )
{
    return &xvoutput;
}

