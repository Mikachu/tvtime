/**
 * Copyright (C) 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * Helped by XTest code from xine, a free video player,
 * Copyright (C) 2000-2003 the xine project
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "xvoutput.h"

#ifdef HAVE_XV

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
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#ifdef HAVE_XTESTEXTENSION
#include <X11/extensions/XTest.h>
#endif

#include "display.h"
#include "speedy.h"
#include "xcommon.h"

#define FOURCC_YUY2 0x32595559

static Display *display;
static Window output_window;

static XvImage *image;
static uint8_t *image_data;
static XShmSegmentInfo shminfo;
static XvPortID xv_port;

static int input_width, input_height;
static int xvoutput_verbose = 0;
static int xvoutput_error = 0;

static int HandleXError( Display *display, XErrorEvent *xevent )
{
    char str[ 1024 ];

    XGetErrorText( display, xevent->error_code, str, 1024 );
    fprintf( stderr, "xvoutput: Received X error event: %s\n", str );
    xvoutput_error = 1;
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
    int has_yuy2 = 0;
    XvAdaptorInfo *adaptorInfo;

    if( ( XvQueryExtension( display, &version, &release,
                            &dummy, &dummy, &dummy) != Success ) ||
         ( version < 2 ) || ( ( version == 2 ) && ( release < 2 ) ) ) {

        fprintf( stderr, "xvoutput: XVIDEO extension not found: X too old? didn't load extmod?\n" );
        return 0;
    }

    XvQueryAdaptors( display, output_window, &adaptors, &adaptorInfo );

    for( i = 0; i < adaptors; i++ ) {
        if( adaptorInfo[ i ].type & XvImageMask ) {
            for( j = 0; j < adaptorInfo[ i ].num_ports; j++ ) {
                if( xv_port_has_yuy2( adaptorInfo[ i ].base_id + j ) ) {
                    if( XvGrabPort( display, adaptorInfo[ i ].base_id + j, 0 ) == Success ) {
                        xv_port = adaptorInfo[ i ].base_id + j;
                        if( xvoutput_verbose ) {
                            fprintf( stderr, "xvoutput: Using XVIDEO adaptor %lu: %s.\n",
                                     adaptorInfo[ i ].base_id + j, adaptorInfo[ i ].name );
                        }
                        XvFreeAdaptorInfo( adaptorInfo );
                        return 1;
                    }
                    has_yuy2 = 1;
                }
            }
        }
    }

    XvFreeAdaptorInfo( adaptorInfo );

    if( has_yuy2 ) {
        fprintf( stderr, "xvoutput: No YUY2 XVIDEO port available.\n" );

        fprintf( stderr, "\n*** tvtime requires a hardware YUY2 overlay.  One is supported\n"
                           "*** by your driver, but we could not grab it.  It is likely\n"
                           "*** being used by another application, either another tvtime\n"
                           "*** instance or a media player.  Please shut down this other\n"
                           "*** application and try tvtime again.\n\n" );
    } else {
        fprintf( stderr, "xvoutput: No XVIDEO port found which supports YUY2 images.\n" );

        fprintf( stderr, "\n*** tvtime requires hardware YUY2 overlay support from your video card\n"
                           "*** driver.  If you are using an older NVIDIA card (TNT2), then\n"
                           "*** this capability is only available with their binary drivers.\n"
                           "*** For some ATI cards, this feature may be found in the experimental\n"
                           "*** GATOS drivers: http://gatos.souceforge.net/\n"
                           "*** If unsure, please check with your distribution to see if your\n"
                           "*** X driver supports hardware overlay surfaces.\n\n" );
    }
    return 0;
}

static void *create_shm( int size )
{
    int error = 1;
    int major;
    int minor;
    Bool pixmaps;

    if( ( XShmQueryVersion( display, &major, &minor, &pixmaps) == 0 ) ||
         (major < 1) || ((major == 1) && (minor < 1))) {
        fprintf( stderr, "xvoutput: No xshm extension available.\n" );
        return 0;
    }

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

            /**
             * We immediately delete the shared memory segment, protecting us for
             * if we crash or whatever, to make sure we still clean up.
             */
            shmctl( shminfo.shmid, IPC_RMID, 0 );
        }
    }

    if( error ) {
        fprintf( stderr, "xvoutput: Cannot create shared memory.\n" );
        return 0;
    } else {
        return shminfo.shmaddr;
    }
}

static int xv_alloc_frame( void )
{
    int size;
    uint8_t *alloc;

    size = input_width * input_height * 2;
    alloc = create_shm( size );
    if( alloc ) {
        blit_colour_packed422_scanline( alloc, input_width * input_height, 0, 128, 128 );
        image = XvShmCreateImage( display, xv_port, FOURCC_YUY2, (char *) alloc,
                                  input_width, input_height, &shminfo );
        image_data = alloc;
        return 1;
    }

    return 0;
}

static int get_colourkey( void )
{
    Atom atom;
    XvAttribute *attr;
    int value;
    int nattr;

    attr = XvQueryPortAttributes( display, xv_port, &nattr );
    if( attr ) {
        if( nattr ) {
            int k;

            for( k = 0; k < nattr; k++ ) {
                if( (attr[ k ].flags & XvSettable) && (attr[ k ].flags & XvGettable)) {
                    if( !strcmp( attr[ k ].name, "XV_COLORKEY" ) ) {
                        atom = XInternAtom( display, "XV_COLORKEY", False );
                        if( atom != None ) {
                            XvGetPortAttribute( display, xv_port, atom, &value );
                            XFree( attr );
                            return value;
                        }
                    }
                }
            }
        }
        XFree( attr );
    }
    return 0;
}

static int xv_init( int outputheight, int aspect, int verbose )
{
    xvoutput_verbose = verbose;

    if( !xcommon_open_display( aspect, outputheight, verbose ) ) {
        return 0;
    }
    display = xcommon_get_display();
    output_window = xcommon_get_output_window();

    if( !xv_check_extension() ) return 0;
    xcommon_set_colourkey( get_colourkey() );
    return 1;
}

static int xv_show_frame( int x, int y, int width, int height )
{
    area_t video_area = xcommon_get_video_area();

    XLockDisplay( display );
    xcommon_ping_screensaver();
    XvShmPutImage( display, xv_port, output_window, xcommon_get_gc(), image,
                   x, y, width, height,
                   video_area.x, video_area.y,
                   video_area.width, video_area.height, False );
    XFlush( display );
    XUnlockDisplay( display );
    xcommon_frame_drawn();
    XSync( display, False );
    if( xvoutput_error ) return 0;
    return 1;
}

static int xv_set_input_size( int inputwidth, int inputheight )
{
    input_width = inputwidth;
    input_height = inputheight;

    if( !xv_alloc_frame() ) {
        return 0;
    }

    xv_show_frame( 0, 0, input_width, input_height );
    xcommon_clear_screen();
    return 1;
}

static void xv_quit( void )
{
    XShmDetach( display, &shminfo );
    shmdt( shminfo.shmaddr );
    xcommon_close_display();
}

static int xv_get_stride( void )
{
    return image->pitches[ 0 ];
}

static int xv_is_interlaced( void )
{
    return 0;
}

static void xv_wait_for_sync( int field )
{
}

static void xv_lock_output( void )
{
}

static void xv_unlock_output( void )
{
}

static uint8_t *xv_get_output( void )
{
    return image_data;
}

static int xv_can_read_from_buffer( void )
{
    return 1;
}

static output_api_t xvoutput =
{
    xv_init,

    xv_set_input_size,

    xv_lock_output,
    xv_get_output,
    xv_get_stride,
    xv_can_read_from_buffer,
    xv_unlock_output,

    xcommon_is_exposed,
    xcommon_get_visible_width,
    xcommon_get_visible_height,
    xcommon_is_fullscreen,

    xv_is_interlaced,
    xv_wait_for_sync,
    xv_show_frame,

    xcommon_toggle_aspect,
    xcommon_toggle_fullscreen,
    xcommon_set_window_caption,

    xcommon_set_window_height,

    xcommon_poll_events,
    xv_quit
};

output_api_t *get_xv_output( void )
{
    return &xvoutput;
}

#else

output_api_t *get_xv_output( void )
{
    return 0;
}

#endif

