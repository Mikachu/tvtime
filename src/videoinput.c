/**
 * Copyright (C) 2001, 2002, 2003  Billy Biggs <vektor@dumbterm.net>.
 *
 * Uses hacky bttv detection code from xawtv:
 *   (c) 1997-2001 Gerd Knorr <kraxel@bytesex.org>
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
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <linux/videodev2.h>
#include "videoinput.h"
#include "mixer.h"

/**
 * How long to wait when we lose a signal, or acquire a signal.
 */
#define SIGNAL_RECOVER_DELAY 2
#define SIGNAL_ACQUIRE_DELAY 2

/**
 * How many seconds to wait before deciding it's a driver problem.
 */
#define SYNC_TIMEOUT 3

/**
 * Maximum number of capture buffers we allow.
 */
#define MAX_CAPTURE_BUFFERS 16

const char *videoinput_get_norm_name( int norm )
{
    if( norm == VIDEOINPUT_NTSC ) {
        return "NTSC";
    } else if( norm == VIDEOINPUT_PAL ) {
        return "PAL";
    } else if( norm == VIDEOINPUT_SECAM ) {
        return "SECAM";
    } else if( norm == VIDEOINPUT_PAL_NC ) {
        return "PAL-NC";
    } else if( norm == VIDEOINPUT_PAL_M ) {
        return "PAL-M";
    } else if( norm == VIDEOINPUT_PAL_N ) {
        return "PAL-N";
    } else if( norm == VIDEOINPUT_NTSC_JP ) {
        return "NTSC-JP";
    } else if( norm == VIDEOINPUT_PAL_60 ) {
        return "PAL-60";
    } else {
        return "ERROR";
    }
}

static v4l2_std_id videoinput_get_v4l2_norm( int norm )
{
    if( norm == VIDEOINPUT_NTSC ) {
        return V4L2_STD_NTSC_M;
    } else if( norm == VIDEOINPUT_PAL ) {
        return V4L2_STD_PAL;
    } else if( norm == VIDEOINPUT_SECAM ) {
        return V4L2_STD_SECAM;
    } else if( norm == VIDEOINPUT_PAL_NC ) {
        return V4L2_STD_PAL_Nc;
    } else if( norm == VIDEOINPUT_PAL_M ) {
        return V4L2_STD_PAL_M;
    } else if( norm == VIDEOINPUT_PAL_N ) {
        return V4L2_STD_PAL_N;
    } else if( norm == VIDEOINPUT_NTSC_JP ) {
        return V4L2_STD_NTSC_M_JP;
    } else if( norm == VIDEOINPUT_PAL_60 ) {
        return V4L2_STD_PAL_60;
    } else {
        return 0;
    }
}

int videoinput_get_norm_number( const char *name )
{
    int i;

    for( i = 0; i < VIDEOINPUT_PAL_60 + 1; i++ ) {
        if( !strcasecmp( name, videoinput_get_norm_name( i ) ) ) {
            return i;
        }
    }

    return -1;
}

static int videoinput_get_norm_height( int norm )
{
    if( norm == VIDEOINPUT_NTSC || norm == VIDEOINPUT_NTSC_JP || norm == VIDEOINPUT_PAL_M || norm == VIDEOINPUT_PAL_60 ) {
        return 480;
    } else {
        return 576;
    }
}

int videoinput_get_time_per_field( int norm )
{
    if( norm == VIDEOINPUT_NTSC || norm == VIDEOINPUT_NTSC_JP || norm == VIDEOINPUT_PAL_M || norm == VIDEOINPUT_PAL_60 ) {
        return 16683;
    } else {
        return 20000;
    }
}

typedef struct capture_buffer_s
{
    struct v4l2_buffer vidbuf;
    uint8_t *data;
    int length;
    int free;
} capture_buffer_t;

struct videoinput_s
{
    int verbose;
    int grab_fd;
    char drivername[ 64 ];
    char shortdriver[ 64 ];

    int numinputs;
    int curinput;
    char inputname[ 32 ];

    int width;
    int height;
    int maxwidth;
    int norm;
    int volume;
    int amode;

    int isv4l2;
    int isuyvy;

    int curframe;
    int numframes;

    int hasaudio;
    int audiomode;
    int change_muted;
    int user_muted;
    int hw_muted;

    int hastuner;
    int numtuners;
    int tunerid;
    int tunerlow;

    int cur_tuner_state;
    int signal_recover_wait;
    int signal_acquire_wait;

    int frames_since_start;

    /* V4L2 capture state. */
    capture_buffer_t capbuffers[ MAX_CAPTURE_BUFFERS ];
    int is_streaming;
};

static void videoinput_start_capture_v4l2( videoinput_t *vidin )
{
    if( !vidin->is_streaming ) {
        if( ioctl( vidin->grab_fd, VIDIOC_STREAMON, &vidin->capbuffers[ 0 ].vidbuf.type ) < 0 ) {
            fprintf( stderr, "videoinput: Driver refuses to start streaming: %s.\n",
                     strerror( errno ) );
        }
        vidin->is_streaming = 1;
        vidin->frames_since_start = 0;
    }
}

static void videoinput_stop_capture_v4l2( videoinput_t *vidin )
{
    if( vidin->is_streaming ) {
        if( ioctl( vidin->grab_fd, VIDIOC_STREAMOFF, &vidin->capbuffers[ 0 ].vidbuf.type ) < 0 ) {
            fprintf( stderr, "videoinput: Driver refuses to stop streaming: %s.\n",
                     strerror( errno ) );
        }
        vidin->is_streaming = 0;
    }
}

static int alarms;

static void sigalarm( int signal )
{
    alarms++;
}

static void siginit( void )
{
    struct sigaction act, old;

    memset( &act, 0, sizeof( act ) );
    act.sa_handler = sigalarm;
    sigemptyset( &act.sa_mask );
    sigaction( SIGALRM, &act, &old );
}

static void free_frame( videoinput_t *vidin, int frameid )
{
    if( ioctl( vidin->grab_fd, VIDIOC_QBUF, &vidin->capbuffers[ frameid ].vidbuf ) < 0 ) {
	fprintf( stderr, "videoinput: Can't free frame %d: %s\n",
		 frameid, strerror( errno ) );
    }
    vidin->capbuffers[ frameid ].free = 1;
}

void videoinput_free_frame( videoinput_t *vidin, int frameid )
{
    if( vidin->isv4l2 ) {
        free_frame( vidin, frameid );
    }
}

static void videoinput_free_all_frames( videoinput_t *vidin )
{
    int i;

    for( i = 0; i < vidin->numframes; i++ ) {
        free_frame( vidin, i );
    }
    vidin->curframe = 0;
}

static void wait_for_frame_v4l2( videoinput_t * vidin )
{
    struct timeval timeout;
    fd_set rdset;
    int n;

    FD_ZERO( &rdset );
    FD_SET( vidin->grab_fd, &rdset );

    timeout.tv_sec = SYNC_TIMEOUT;
    timeout.tv_usec = 0;

    n = select( vidin->grab_fd + 1, &rdset, 0, 0, &timeout );
    if( n == -1 ) {
        fprintf( stderr, "videoinput: Error waiting for frame: %s\n",
                 strerror( errno ) );
    } else if( n == 0 ) {
        sigalarm( 0 );
    }
}

uint8_t *videoinput_next_frame( videoinput_t *vidin, int *frameid )
{
    struct v4l2_buffer cur_buf;
 
    wait_for_frame_v4l2( vidin );
 
    cur_buf.type = vidin->capbuffers[ 0 ].vidbuf.type;
    cur_buf.memory = vidin->capbuffers[ 0 ].vidbuf.memory;

    if( ioctl( vidin->grab_fd, VIDIOC_DQBUF, &cur_buf ) < 0 ) {
	/* some drivers return EIO when there is no signal */
	if( errno != EIO ) {
	    fprintf( stderr, "videoinput: Can't read frame. Error was: %s (%d).\n",
		     strerror( errno ), vidin->frames_since_start );
	}
	/* We must now restart capture. */
	videoinput_stop_capture_v4l2( vidin );
	videoinput_free_all_frames( vidin );
	videoinput_start_capture_v4l2( vidin );
	*frameid = -1;
	return 0;
    }
    vidin->frames_since_start++;
    vidin->capbuffers[ cur_buf.index ].free = 0;
    *frameid = cur_buf.index;
    return vidin->capbuffers[ cur_buf.index ].data;
}

int videoinput_buffer_invalid( videoinput_t *vidin, int frameid )
{
    return vidin->capbuffers[ frameid ].free;
}

videoinput_t *videoinput_new( const char *v4l_device, int capwidth,
                              int volume, int norm, int verbose, char *error_string )
{
    videoinput_t *vidin = malloc( sizeof( videoinput_t ) );
    struct v4l2_capability caps_v4l2;
    struct v4l2_input in;
    int i;

    if( capwidth & 1 ) {
        capwidth -= 1;
        if( verbose ) {
            fprintf( stderr, "videoinput: Odd values for input width not allowed, "
                             "using %d instead.\n", capwidth );
        }
    }

    if( !vidin ) {
        fprintf( stderr, "videoinput: Cannot allocate memory.\n" );
        sprintf( error_string, "%s", strerror( ENOMEM ) );
        return 0;
    }

    siginit();

    vidin->curframe = 0;
    vidin->verbose = verbose;
    vidin->norm = norm;
    vidin->volume = volume;
    vidin->amode = 0;
    vidin->height = videoinput_get_norm_height( norm );
    vidin->cur_tuner_state = TUNER_STATE_NO_SIGNAL;

    vidin->hastuner = 0;
    vidin->numtuners = 0;
    vidin->tunerid = 0;
    vidin->tunerlow = 0;

    vidin->isv4l2 = 0;
    vidin->isuyvy = 0;

    vidin->signal_recover_wait = 0;
    vidin->signal_acquire_wait = 0;
    vidin->change_muted = 1;
    vidin->user_muted = 0;
    vidin->hw_muted = 1;
    vidin->hasaudio = 1;
    vidin->audiomode = 0;
    vidin->curinput = 0;
    vidin->is_streaming = 0;

    memset( vidin->inputname, 0, sizeof( vidin->inputname ) );
    memset( vidin->drivername, 0, sizeof( vidin->drivername ) );
    memset( vidin->shortdriver, 0, sizeof( vidin->shortdriver ) );

    /* First, open the device. */
    vidin->grab_fd = open( v4l_device, O_RDWR );
    if( vidin->grab_fd < 0 ) {
        fprintf( stderr, "videoinput: Cannot open capture device %s: %s\n",
                 v4l_device, strerror( errno ) );
        sprintf( error_string, "%s", strerror( errno ) );
        free( vidin );
        return 0;
    }

    /**
     * Next, ask for its capabilities.  This will also confirm it's a V4L2 
     * device. 
     */
    if( ioctl( vidin->grab_fd, VIDIOC_QUERYCAP, &caps_v4l2 ) < 0 ) {
        /* Can't get V4L2 capabilities, maybe this is a V4L1 device? */
	fprintf(stderr, "V4L1 devices not supported\n");
	sprintf( error_string, "V4L1 devices not supported" );
	close( vidin->grab_fd );
	free( vidin );
	return 0;
    } else {
        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: Using video4linux2 driver '%s', card '%s' (bus %s).\n"
                             "videoinput: Version is %u, capabilities %x.\n",
                     caps_v4l2.driver, caps_v4l2.card, caps_v4l2.bus_info,
                     caps_v4l2.version, caps_v4l2.capabilities );
        }
        vidin->isv4l2 = 1;
        snprintf( vidin->drivername, sizeof( vidin->drivername ),
                  "%s [%s/%s/%u]",
                  caps_v4l2.driver, caps_v4l2.card,
                  caps_v4l2.bus_info, caps_v4l2.version );
        snprintf( vidin->shortdriver, sizeof( vidin->shortdriver ),
                  "%s", caps_v4l2.driver );
    }


    vidin->numinputs = 0;
    in.index = vidin->numinputs;

    /**
     * We have to enumerate all of the inputs just to know how many
     * there are.
     */
    while( ioctl( vidin->grab_fd, VIDIOC_ENUMINPUT, &in ) >= 0 ) {
	vidin->numinputs++;
	in.index = vidin->numinputs;
    }

    if( !vidin->numinputs ) {
	fprintf( stderr, "videoinput: No inputs available on "
		 "video4linux2 device '%s'.\n", v4l_device );
	sprintf( error_string, "No inputs available" );
	close( vidin->grab_fd );
	free( vidin );
	return 0;
    }

    /* On initialization, set to input 0.  This is just to start things up. */
    videoinput_set_input_num( vidin, 0 );

    /**
     * Once we're here, we've set the hardware norm.  Now confirm that
     * our width is acceptable by again asking what the capabilities
     * are.
     */
    struct v4l2_format imgformat;

    imgformat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    memset( &(imgformat.fmt.pix), 0, sizeof( struct v4l2_pix_format ) );
    imgformat.fmt.pix.width = capwidth;
    imgformat.fmt.pix.height = vidin->height;
    imgformat.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    imgformat.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if( ioctl( vidin->grab_fd, VIDIOC_S_FMT, &imgformat ) < 0 ) {
	/* Try for UYVY instead. */
	imgformat.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
	if( ioctl( vidin->grab_fd, VIDIOC_S_FMT, &imgformat ) < 0 ) {

	    fprintf( stderr, "\n"
     "    Your capture card driver: %s\n"
     "    does not support studio-quality colour images required by tvtime.\n"
     "    This is a hardware limitation of some cards including many\n"
     "    low-quality webcams.  Please select a different video device to use\n"
     "    with the command line option --device.\n"
     "\n"
     "    Message from the card was: %s\n\n",
		     vidin->drivername, strerror( errno ) );

	    sprintf( error_string, "%s: %s", vidin->shortdriver, strerror( errno ) );
	    close( vidin->grab_fd );
	    free( vidin );
	    return 0;
	}
	vidin->isuyvy = 1;
    }

    if( vidin->height != imgformat.fmt.pix.height ) {
	fprintf( stderr, "\n"
    "    Your capture card driver: %s\n"
    "    does not support full size studio-quality images required by tvtime.\n"
    "    This is true for many low-quality webcams.  Please select a\n"
    "    different video device for tvtime to use with the command line\n"
    "    option --device.\n\n", vidin->drivername );
	sprintf( error_string, "Frames too short from %s", vidin->shortdriver );
	close( vidin->grab_fd );
	free( vidin );
	return 0;
    }

    if( capwidth != imgformat.fmt.pix.width && verbose ) {
	fprintf( stderr, "videoinput: Width %d too high, using %d "
		 "instead as suggested by the driver.\n",
		 capwidth, imgformat.fmt.pix.width );
    }
    vidin->width = imgformat.fmt.pix.width;

    /* Query the maximum input width - I wish there was a nicer way */
    imgformat.fmt.pix.width = 10000;
    if( ioctl( vidin->grab_fd, VIDIOC_TRY_FMT, &imgformat ) < 0 ) {
	vidin->maxwidth = 768;
	if( verbose ) {
	    fprintf( stderr, "videoinput: Your capture card driver does "
		     "not implement the TRY_FMT query.\n"
		     "Setting maximum input width to 768.\n" );
	}
    } else {
	vidin->maxwidth = imgformat.fmt.pix.width;
	if( verbose ) {
	    fprintf( stderr, "videoinput: Maximum input width: %d "
		     "pixels.\n", vidin->maxwidth );
	}
    }

    struct v4l2_requestbuffers req;

    /**
     * Ask Video Device for Buffers 
     */
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if( ioctl( vidin->grab_fd, VIDIOC_REQBUFS, &req ) < 0 ) {
	fprintf( stderr, "videoinput: Card failed to allocate capture buffers: %s\n",
		 strerror( errno ) );
	sprintf( error_string, "%s: %s", vidin->shortdriver, strerror( errno ) );
	close( vidin->grab_fd );
	free( vidin );
	return 0;
    }
    vidin->numframes = req.count;

    if( vidin->numframes < 1 ) {
	fprintf( stderr, "videoinput: No capture buffers available from card.\n" );
	sprintf( error_string, "%s: No capture buffers", vidin->shortdriver );
	close( vidin->grab_fd );
	free( vidin );
	return 0;
    } else if( vidin->numframes > 4 ) {
	/* If we got more frames than we asked for, limit to 4 for now. */
	if( vidin->verbose ) {
	    fprintf( stderr, "videoinput: Capture card provides %d buffers, but we only need 4.\n",
		     vidin->numframes );
	}
	vidin->numframes = 4;
    }

    /**
     * Query each buffer and map it to the video device
     */
    for( i = 0; i < vidin->numframes; i++ ) {
	struct v4l2_buffer *vidbuf = &(vidin->capbuffers[ i ].vidbuf);

	vidbuf->index = i;
	vidbuf->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vidbuf->memory = V4L2_MEMORY_MMAP;
	if( ioctl( vidin->grab_fd, VIDIOC_QUERYBUF, vidbuf ) < 0 ) {
	    fprintf( stderr, "videoinput: Can't get information about buffer %d: %s.\n",
		     i, strerror( errno ) );
	    sprintf( error_string, "%s: %s", vidin->shortdriver, strerror( errno ) );
	    close( vidin->grab_fd );
	    free( vidin );
	    return 0;
	}

	vidin->capbuffers[ i ].data = mmap( 0, vidbuf->length,
					    PROT_READ | PROT_WRITE,
					    MAP_SHARED, vidin->grab_fd,
					    vidbuf->m.offset );
	if( vidin->capbuffers[ i ].data == MAP_FAILED ) {
	    fprintf( stderr, "videoinput: Can't map buffer %d: %s.\n",
		     i, strerror( errno ) );
	    sprintf( error_string, "%s: %s", vidin->shortdriver, strerror( errno ) );
	    close( vidin->grab_fd );
	    free( vidin );
	    return 0;
	}
	vidin->capbuffers[ i ].length = vidbuf->length;
    }
    videoinput_free_all_frames( vidin );

    /**
     * Tell video stream to begin capture.
     */
    videoinput_start_capture_v4l2( vidin );

    return vidin;
}

void videoinput_delete( videoinput_t *vidin )
{
    int i;

    /* Mute audio on exit. */
    videoinput_mute( vidin, 1 );

    videoinput_stop_capture_v4l2( vidin );
    videoinput_free_all_frames( vidin );

    for( i = 0; i < vidin->numframes; i++ ) {
	munmap( vidin->capbuffers[ i ].data, vidin->capbuffers[ i ].length );
    }

    close( vidin->grab_fd );
    free( vidin );
}

static double videoinput_get_control_v4l2( videoinput_t *vidin, uint32_t id )
{
    struct v4l2_queryctrl query;

    query.id = id;
    if( ioctl( vidin->grab_fd, VIDIOC_QUERYCTRL, &query ) >= 0 && !(query.flags & V4L2_CTRL_FLAG_DISABLED) ) {
        struct v4l2_control control;
        control.id = id;
        if( ioctl( vidin->grab_fd, VIDIOC_G_CTRL, &control ) >= 0 ) {
            return (((double) (control.value - query.minimum)) / ((double) (query.maximum - query.minimum)));
        }
    }
    return 0.0;
}

static void videoinput_set_control_v4l2( videoinput_t *vidin, uint32_t id, double val )
{
    struct v4l2_queryctrl query;

    query.id = id;
    if( ioctl( vidin->grab_fd, VIDIOC_QUERYCTRL, &query ) >= 0 && !(query.flags & V4L2_CTRL_FLAG_DISABLED) ) {
        struct v4l2_control control;
        control.id = id;
        control.value = query.minimum + ((int) ((val * ((double) (query.maximum - query.minimum))) + 0.5));
        ioctl( vidin->grab_fd, VIDIOC_S_CTRL, &control );
    }
}

int videoinput_get_hue( videoinput_t *vidin )
{
    return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_HUE ) * 100.0) + 0.5);

    return 0;
}

void videoinput_set_hue( videoinput_t *vidin, int newhue )
{
    if( newhue > 100 ) newhue = 100;
    if( newhue <   0 ) newhue = 50;

    videoinput_set_control_v4l2( vidin, V4L2_CID_HUE, ((double) newhue) / 100.0 );
}

void videoinput_set_hue_relative( videoinput_t *vidin, int offset )
{
    int hue = videoinput_get_hue( vidin );
    if( hue + offset >= 0 ) {
        videoinput_set_hue( vidin, hue + offset );
    }
}

int videoinput_get_brightness( videoinput_t *vidin )
{
    return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_BRIGHTNESS ) * 100.0) + 0.5);
}

void videoinput_set_brightness( videoinput_t *vidin, int newbright )
{
    if( newbright > 100 ) newbright = 100;
    if( newbright <   0 ) newbright = 50;

    videoinput_set_control_v4l2( vidin, V4L2_CID_BRIGHTNESS,
				 ((double) newbright) / 100.0 );
}

void videoinput_set_brightness_relative( videoinput_t *vidin, int offset )
{
    int brightness = videoinput_get_brightness( vidin );
    if( brightness + offset >= 0 ) {
        videoinput_set_brightness( vidin, brightness + offset );
    }
}

int videoinput_get_contrast( videoinput_t *vidin )
{
    return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_CONTRAST ) * 100.0) + 0.5);
}

void videoinput_set_contrast( videoinput_t *vidin, int newcont )
{
    if( newcont > 100 ) newcont = 100;
    if( newcont <   0 ) newcont = 50;

    videoinput_set_control_v4l2( vidin, V4L2_CID_CONTRAST,
				 ((double) newcont) / 100.0 );
}

void videoinput_set_contrast_relative( videoinput_t *vidin, int offset )
{
    int contrast = videoinput_get_contrast( vidin );
    if( contrast + offset >= 0 ) {
        videoinput_set_contrast( vidin, contrast + offset );
    }
}

int videoinput_get_saturation( videoinput_t *vidin )
{
    return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_SATURATION ) * 100.0) + 0.5);
}

void videoinput_set_saturation( videoinput_t *vidin, int newsaturation )
{
    if( newsaturation > 100 ) newsaturation = 100;
    if( newsaturation <   0 ) newsaturation = 50;

    videoinput_set_control_v4l2( vidin, V4L2_CID_SATURATION,
				 ((double) newsaturation) / 100.0 );
}

void videoinput_set_saturation_relative( videoinput_t *vidin, int offset )
{
    int saturation = videoinput_get_saturation( vidin );
    if( saturation + offset >= 0 ) {
        videoinput_set_saturation( vidin, saturation + offset );
    }
}

static void videoinput_do_mute( videoinput_t *vidin, int mute )
{
    if( vidin->hasaudio && mute != vidin->hw_muted ) {
	struct v4l2_control control;

	control.id = V4L2_CID_AUDIO_MUTE;
	control.value = mute ? 1 : 0;

	if( ioctl( vidin->grab_fd, VIDIOC_S_CTRL, &control ) < 0 ) {
	    fprintf( stderr, "videoinput: Can't mute card.  Post a bug report with your\n"
		     "videoinput: driver info to " PACKAGE_BUGREPORT "\n" );
	    fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
	}

	if( !mute && vidin->volume > 0 ) {
	    videoinput_set_control_v4l2( vidin, V4L2_CID_AUDIO_VOLUME, ((double) vidin->volume) / 100.0 );
	}
        vidin->hw_muted = mute;
    }
}

/* freqKHz is in KHz (duh) */
void videoinput_set_tuner_freq( videoinput_t *vidin, int freqKHz )
{
    unsigned long frequency = freqKHz;
    struct v4l2_frequency freqinfo;
    int wasstreaming;

    if( videoinput_has_tuner( vidin ) ) {
        if( frequency < 0 ) {
            /* Ignore bogus frequencies. */
            return;
        }

        frequency *= 16;

        if( !vidin->tunerlow ) {
            frequency /= 1000; /* switch to MHz */
        }

        vidin->change_muted = 1;
        mixer_mute( 1 );
        videoinput_do_mute( vidin, vidin->user_muted || vidin->change_muted );
        vidin->cur_tuner_state = TUNER_STATE_SIGNAL_DETECTED;
        vidin->signal_acquire_wait = SIGNAL_ACQUIRE_DELAY;
        vidin->signal_recover_wait = 0;


	wasstreaming = vidin->is_streaming;

	memset( &freqinfo, 0, sizeof( struct v4l2_frequency ) );
	freqinfo.tuner = vidin->tunerid;
	freqinfo.type = V4L2_TUNER_ANALOG_TV;
	freqinfo.frequency = frequency;

	videoinput_stop_capture_v4l2( vidin );
	if( ioctl( vidin->grab_fd, VIDIOC_S_FREQUENCY, &freqinfo ) < 0 ) {
	    fprintf( stderr, "videoinput: Tuner present, but our request to change to\n"
		     "videoinput: frequency %d failed with this error: %s.\n", freqKHz, strerror( errno ) );
	    fprintf( stderr, "videoinput: Please file a bug report at " PACKAGE_BUGREPORT "\n" );
	}
	if( wasstreaming ) {
	    videoinput_free_all_frames( vidin );
	    videoinput_start_capture_v4l2( vidin );
	}
    }
}

int videoinput_get_tuner_freq( videoinput_t *vidin ) 
{
    if( vidin->hastuner ) {
        unsigned long frequency;
	struct v4l2_frequency freqinfo;

	freqinfo.tuner = vidin->tunerid;
	freqinfo.type = V4L2_TUNER_ANALOG_TV;

	if( ioctl( vidin->grab_fd, VIDIOC_G_FREQUENCY, &freqinfo ) < 0 ) {
	    fprintf( stderr, "videoinput: Tuner refuses to tell us the current frequency: %s\n",
		     strerror( errno ) );
	    fprintf( stderr, "videoinput: Please file a bug report at " PACKAGE_BUGREPORT "\n" );
	    return 0;
	}
	frequency = freqinfo.frequency;

        if( !vidin->tunerlow ) {
            frequency *= 1000; /* switch from MHz to KHz */
        }

        return frequency / 16;
    }
    return 0;
}

int videoinput_freq_present( videoinput_t *vidin )
{
    if( videoinput_has_tuner( vidin ) ) {
	struct v4l2_tuner tuner;

	tuner.index = vidin->tunerid;
	if( ioctl( vidin->grab_fd, VIDIOC_G_TUNER, &tuner ) < 0 ) {
	    if( vidin->verbose ) {
		fprintf( stderr, "videoinput: Cannot detect signal from tuner: %s\n",
			 strerror( errno ) );
	    }
	} else if( !tuner.signal ) {
	    return 0;
	}
    }
    return 1;
}

/**
 * Finds an appropriate tuner.
 */
static void videoinput_find_and_set_tuner( videoinput_t *vidin )
{
    struct v4l2_tuner tuner;

    tuner.index = vidin->tunerid;
    if( ioctl( vidin->grab_fd, VIDIOC_G_TUNER, &tuner ) < 0 ) {
	fprintf( stderr, "videoinput: Can't get tuner info: %s\n",
		 strerror( errno ) );
    } else {
	vidin->tunerlow = (tuner.capability & V4L2_TUNER_CAP_LOW) ? 1 : 0;
    }
}


void videoinput_set_input_num( videoinput_t *vidin, int inputnum )
{
    v4l2_std_id std;
    int index = inputnum;
    int wasstreaming = vidin->is_streaming;

    videoinput_stop_capture_v4l2( vidin );

    if( ioctl( vidin->grab_fd, VIDIOC_S_INPUT, &index ) < 0 ) {
	fprintf( stderr, "videoinput: Card refuses to set its input.\n"
		 "Please post a bug report to " PACKAGE_BUGREPORT "\n"
		 "indicating your card, driver, and this error message: %s.\n",
		 strerror( errno ) );
    } else {
	struct v4l2_input input;

	if( ioctl( vidin->grab_fd, VIDIOC_G_INPUT, &input.index ) < 0 ) {
	    fprintf( stderr, "videoinput: Driver won't tell us its input: %s\n",
		     strerror( errno ) );
	} else if( input.index != inputnum ) {
	    fprintf( stderr, "videoinput: Driver refuses to switch to input %d.\n",
		     inputnum );
	    inputnum = input.index;
	}
	vidin->curinput = inputnum;

	if( ioctl( vidin->grab_fd, VIDIOC_ENUMINPUT, &input ) < 0 ) {
	    fprintf( stderr, "videoinput: Driver won't tell us input info: %s\n",
		     strerror( errno ) );
	} else {
	    snprintf( vidin->inputname, sizeof( vidin->inputname ), "%s", input.name );
	    vidin->hastuner = input.type == V4L2_INPUT_TYPE_TUNER;
	    vidin->tunerid = input.tuner;
	}
    }

    if( ioctl( vidin->grab_fd, VIDIOC_G_STD, &std ) < 0 ) {
	fprintf( stderr, "videoinput: Driver won't tell us its norm: %s\n",
		 strerror( errno ) );
    } else {
	std = videoinput_get_v4l2_norm( vidin->norm );
	if( std == V4L2_STD_PAL ) {
	    if( vidin->amode == VIDEOINPUT_PAL_DK_AUDIO ) {
		std = V4L2_STD_PAL_DK;
	    } else if( vidin->amode == VIDEOINPUT_PAL_I_AUDIO ) {
		std = V4L2_STD_PAL_I;
	    }
	} 
	if( ioctl( vidin->grab_fd, VIDIOC_S_STD, &std ) < 0 ) {
	    fprintf( stderr, "videoinput: Driver refuses to set norm: %s\n",
		     strerror( errno ) );
	}
    }

    if( wasstreaming ) {
	videoinput_free_all_frames( vidin );
	videoinput_start_capture_v4l2( vidin );
    }

    /* Once we've set the input, go look for a tuner. */
    videoinput_find_and_set_tuner( vidin );
}

int videoinput_check_for_signal( videoinput_t *vidin, int check_freq_present )
{
    if( videoinput_freq_present( vidin ) || !check_freq_present ) {
        switch( vidin->cur_tuner_state ) {
        case TUNER_STATE_NO_SIGNAL:
        case TUNER_STATE_SIGNAL_LOST:
            vidin->cur_tuner_state = TUNER_STATE_SIGNAL_DETECTED;
            vidin->signal_acquire_wait = SIGNAL_ACQUIRE_DELAY;
            vidin->signal_recover_wait = 0;
        case TUNER_STATE_SIGNAL_DETECTED:
            if( vidin->signal_acquire_wait ) {
                vidin->signal_acquire_wait--;
                break;
            } else {
                vidin->cur_tuner_state = TUNER_STATE_HAS_SIGNAL;
            }
        default:
            if( vidin->change_muted ) {
                vidin->change_muted = 0;
                videoinput_do_mute( vidin, vidin->user_muted || vidin->change_muted );
                mixer_mute( 0 );
            }
            break;
        }
    } else {
        switch( vidin->cur_tuner_state ) {
        case TUNER_STATE_HAS_SIGNAL:
        case TUNER_STATE_SIGNAL_DETECTED:
            vidin->cur_tuner_state = TUNER_STATE_SIGNAL_LOST;
            vidin->signal_recover_wait = SIGNAL_RECOVER_DELAY;
            vidin->change_muted = 1;
            mixer_mute( 1 );
            videoinput_do_mute( vidin, vidin->user_muted || vidin->change_muted );
        case TUNER_STATE_SIGNAL_LOST:
            if( vidin->signal_recover_wait ) {
                vidin->signal_recover_wait--;
                break;
            } else {
                vidin->cur_tuner_state = TUNER_STATE_NO_SIGNAL;
            }
        default: break;
        }
    }

    return vidin->cur_tuner_state;
}

void videoinput_toggle_pal_secam( videoinput_t *vidin )
{
    if( vidin->norm == VIDEOINPUT_PAL ) {
        vidin->norm = VIDEOINPUT_SECAM;
    } else {
        vidin->norm = VIDEOINPUT_PAL;
    }
    videoinput_set_input_num( vidin, videoinput_get_input_num( vidin ) );
}

void videoinput_switch_pal_secam( videoinput_t *vidin, int norm )
{
    if( norm != vidin->norm && (norm == VIDEOINPUT_PAL || norm == VIDEOINPUT_SECAM) ) {
        vidin->norm = norm;
        videoinput_set_input_num( vidin, videoinput_get_input_num( vidin ) );
    }
}

void videoinput_mute( videoinput_t *vidin, int mute )
{
    vidin->user_muted = mute;
    videoinput_do_mute( vidin, vidin->user_muted || vidin->change_muted );
}

int videoinput_get_muted( videoinput_t *vidin )
{
    return vidin->user_muted;
}

void videoinput_set_pal_audio_mode( videoinput_t *vidin, int amode )
{
    if( amode != vidin->amode && vidin->norm == VIDEOINPUT_PAL ) {
        vidin->amode = amode;
        videoinput_set_input_num( vidin, videoinput_get_input_num( vidin ) );
    }
}

int videoinput_get_pal_audio_mode( videoinput_t *vidin )
{
    if( !vidin ) return 0;
    return vidin->amode;
}

int videoinput_has_tuner( videoinput_t *vidin )
{
    return vidin->hastuner;
}

int videoinput_get_width( videoinput_t *vidin )
{
    return vidin->width;
}

int videoinput_get_height( videoinput_t *vidin )
{
    return vidin->height;
}

int videoinput_get_maxwidth( videoinput_t *vidin )
{
    return vidin->maxwidth;
}

int videoinput_get_norm( videoinput_t *vidin )
{
    return vidin->norm;
}

int videoinput_get_numframes( videoinput_t *vidin )
{
    return vidin->numframes;
}

int videoinput_get_num_inputs( videoinput_t *vidin )
{
    return vidin->numinputs;
}

int videoinput_is_uyvy( videoinput_t *vidin )
{
    return vidin->isuyvy;
}

int videoinput_is_v4l2( videoinput_t *vidin )
{
    return vidin->isv4l2;
}

int videoinput_get_input_num( videoinput_t *vidin )
{
    return vidin->curinput;
}

const char *videoinput_get_input_name( videoinput_t *vidin )
{
    return vidin->inputname;
}

const char *videoinput_get_driver_name( videoinput_t *vidin )
{
    return vidin->drivername;
}

void videoinput_set_capture_volume( videoinput_t *vidin, int volume )
{
    vidin->volume = volume;
    if( vidin->volume >= 0 ) {
	videoinput_set_control_v4l2( vidin, V4L2_CID_AUDIO_VOLUME,
				     ((double) vidin->volume) / 100.0 );
    }
}

