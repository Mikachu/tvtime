/**
 * Copyright (c) 2004 Billy Biggs <vektor@dumbterm.net>.
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

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
#else
# define _(string) string
#endif
#include "videodev.h"
#include "videodev2.h"


void probe_device( const char *device )
{
    struct video_capability caps_v4l1;
    struct v4l2_capability caps_v4l2;
    char *drivername;
    int isv4l2 = 0;
    int isbttv = 0;
    int numinputs = 0;
    int fd;
    int i;

    /* First, open the device. */
    fd = open( device, O_RDWR );
    if( fd < 0 ) {
        fprintf( stderr, "videoinput: Cannot open capture device %s: %s\n",
                 device, strerror( errno ) );
        return;
    }

    /**
     * Next, ask for its capabilities.  This will also confirm it's a V4L2 
     * device. 
     */
    if( ioctl( fd, VIDIOC_QUERYCAP, &caps_v4l2 ) < 0 ) {
        /* Can't get V4L2 capabilities, maybe this is a V4L1 device? */
        if( ioctl( fd, VIDIOCGCAP, &caps_v4l1 ) < 0 ) {
            fprintf( stderr, "videoinput: %s is not a video4linux device.\n",
                     device );
            close( fd );
            return;
        } else {
            fprintf( stderr, "videoinput: Using video4linux driver '%s'.\n"
                             "videoinput: Card type is %x, audio %d.\n",
                     caps_v4l1.name, caps_v4l1.type, caps_v4l1.audios );
        }
        //snprintf( drivername, sizeof( vidin->drivername ), "%s", caps_v4l1.name );
    } else {
        fprintf( stderr, "videoinput: Using video4linux2 driver '%s', card '%s' (bus %s).\n"
                         "videoinput: Version is %u, capabilities %x.\n",
                 caps_v4l2.driver, caps_v4l2.card, caps_v4l2.bus_info,
                 caps_v4l2.version, caps_v4l2.capabilities );
        isv4l2 = 1;
        /*
        snprintf( vidin->drivername, sizeof( vidin->drivername ),
                  "%s (card %s, bus %s) - %u",
                  caps_v4l2.driver, caps_v4l2.card,
                  caps_v4l2.bus_info, caps_v4l2.version );
        */
    }

    if( isv4l2 ) {
        struct v4l2_input in;

        numinputs = 0;
        in.index = numinputs;

        /**
         * We have to enumerate all of the inputs just to know how many
         * there are.
         */
        while( ioctl( fd, VIDIOC_ENUMINPUT, &in ) >= 0 ) {
            numinputs++;
            in.index = numinputs;
        }

        if( !numinputs ) {
            fprintf( stderr, "videoinput: No inputs available on "
                     "video4linux2 device '%s'.\n", device );
            close( fd );
            return;
        }
    } else {
        /* The capabilities should tell us how many inputs this card has. */
        numinputs = caps_v4l1.channels;
        if( numinputs == 0 ) {
            fprintf( stderr, "videoinput: No inputs available on "
                     "video4linux device '%s'.\n", device );
            close( fd );
            return;
        }
    }
    fprintf( stderr, "%d inputs\n", numinputs );

    /* Check if this is a bttv-based card.  Code taken from xawtv. */
#define BTTV_VERSION            _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
    /* dirty hack time / v4l design flaw -- works with bttv only
     * this adds support for a few less common PAL versions */
    if( !(ioctl( fd, BTTV_VERSION, &i ) < 0) ) {
        isbttv = 1;
        fprintf( stderr, "bttv\n" );
    }
#undef BTTV_VERSION
}


int main( int argc, char **argv )
{
    GtkWidget *dialog;

    gtk_init_check( &argc, &argv );


    probe_device( "/dev/video0" );
    probe_device( "/dev/video1" );
    probe_device( "/dev/video2" );
    probe_device( "/dev/video3" );

    /* Here is how we will talk to the user. */
    dialog = gtk_message_dialog_new( 0,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "Error you suck" );
    gtk_dialog_run( GTK_DIALOG( dialog ) );
    gtk_widget_destroy( dialog );

    return 0;
}

