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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/videodev.h>
#include <signal.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "videodev2.h"
#include "videoinput.h"
#include "mixer.h"


/**
 * Default values below for bttv from dScaler.
 * See http://deinterlace.sf.net/
 */

/* 10/19/2000 Mark Rejhon
 * Better NTSC defaults
 */

/* range -128,127 */
#define DEFAULT_HUE_NTSC 0

/* range -128,127 */
#define DEFAULT_BRIGHTNESS_NTSC 20

/* range 0,511 */
#define DEFAULT_CONTRAST_NTSC 207

/* range 0,511 */
#define DEFAULT_SAT_U_NTSC 254
#define DEFAULT_SAT_V_NTSC 219

/* PAL defaults these work for OTA PAL signals */
#define DEFAULT_HUE_PAL 0
#define DEFAULT_BRIGHTNESS_PAL 0
#define DEFAULT_CONTRAST_PAL 219
#define DEFAULT_SAT_U_PAL 254
#define DEFAULT_SAT_V_PAL 219

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

static int videoinput_get_v4l1_norm( int norm )
{
    if( norm == VIDEOINPUT_NTSC ) {
        return VIDEO_MODE_NTSC;
    } else if( norm == VIDEOINPUT_PAL ) {
        return VIDEO_MODE_PAL;
    } else if( norm == VIDEOINPUT_SECAM ) {
        return VIDEO_MODE_SECAM;
    } else if( norm == VIDEOINPUT_PAL_NC ) {
        return 3;
    } else if( norm == VIDEOINPUT_PAL_M ) {
        return 4;
    } else if( norm == VIDEOINPUT_PAL_N ) {
        return 5;
    } else if( norm == VIDEOINPUT_NTSC_JP ) {
        return 6;
    } else {
        return 0;
    }
}

static int videoinput_get_audmode_v4l2( int mode )
{
    if( mode == VIDEOINPUT_MONO ) {
        return V4L2_TUNER_MODE_MONO;
    } else if( mode == VIDEOINPUT_STEREO ) {
        return V4L2_TUNER_MODE_STEREO;
    } else if( mode == VIDEOINPUT_LANG1 ) {
        return V4L2_TUNER_MODE_LANG1;
    } else if( mode == VIDEOINPUT_LANG2 ) {
        return V4L2_TUNER_MODE_LANG2;
    }
    return V4L2_TUNER_MODE_MONO;
}

static int videoinput_is_audmode_supported_v4l2( uint32_t rxchans, int mode )
{
    if( mode == VIDEOINPUT_MONO ) {
        return (rxchans & V4L2_TUNER_SUB_MONO) ? 1 : 0;
    } else if( mode == VIDEOINPUT_STEREO ) {
        return (rxchans & V4L2_TUNER_SUB_STEREO) ? 1 : 0;
    } else if( mode == VIDEOINPUT_LANG1 ) {
        return (rxchans & V4L2_TUNER_SUB_LANG1) ? 1 : 0;
    } else if( mode == VIDEOINPUT_LANG2 ) {
        return (rxchans & V4L2_TUNER_SUB_LANG2) ? 1 : 0;
    }
    return 1;
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

static int videoinput_next_compatible_norm( int norm, int isbttv )
{
    int height = videoinput_get_norm_height( norm );

    do {
        norm = norm + 1;
        if( isbttv ) {
            norm %= VIDEOINPUT_NTSC_JP + 1;
        } else {
            norm %= VIDEOINPUT_PAL_NC;
        }
    } while( videoinput_get_norm_height( norm ) != height );

    return norm;
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

    int numinputs;
    int curinput;
    char inputname[ 32 ];

    int width;
    int height;
    int norm;

    int isbttv;
    int isv4l2;
    int isuyvy;

    int curframe;
    int numframes;

    int hasaudio;
    int audiomode;
    int change_muted;
    int user_muted;

    int hastuner;
    int numtuners;
    int tunerid;
    int tunerlow;

    int cur_tuner_state;
    int signal_recover_wait;
    int signal_acquire_wait;

    /* V4L2 capture state. */
    capture_buffer_t capbuffers[ MAX_CAPTURE_BUFFERS ];
    int is_streaming;

    /* V4L1 mmap-mode state. */
    int have_mmap;
    uint8_t *map;
    struct video_mmap *grab_buf;
    struct video_mbuf gb_buffers;

    /* V4L1 read-mode state. */
    int grab_size;
    uint8_t *grab_data;
};

const char *videoinput_get_audio_mode_name( videoinput_t *vidin, int mode )
{
    if( mode == VIDEO_SOUND_MONO ) {
        return "Mono";
    } else if( mode == VIDEO_SOUND_STEREO ) {
        return "Stereo";
    } else if( vidin->norm == VIDEOINPUT_NTSC ) {
        if( mode == VIDEO_SOUND_LANG2 ) {
            return "SAP";
        }
    } else {
        if( mode == VIDEO_SOUND_LANG1 ) {
            return "Language 1";
        } else if( mode == VIDEO_SOUND_LANG2 ) {
            return "Language 2";
        }
    }

    return "ERROR";
}

static void videoinput_start_capture_v4l2( videoinput_t *vidin )
{
    if( !vidin->is_streaming ) {
        if( ioctl( vidin->grab_fd, VIDIOC_STREAMON, &vidin->capbuffers[ 0 ].vidbuf.type ) < 0 ) {
            fprintf( stderr, "videoinput: Driver refuses to start streaming: %s.\n",
                     strerror( errno ) );
        }
        vidin->is_streaming = 1;
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
    fprintf( stderr, "videoinput: Frame capture timed out, hardware/driver problems?\n" );
    fprintf( stderr, "videoinput: Please report this on our bugs page: " PACKAGE_BUGREPORT "\n" );
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
    if( vidin->isv4l2 ) {
        if( ioctl( vidin->grab_fd, VIDIOC_QBUF, &vidin->capbuffers[ frameid ].vidbuf ) < 0 ) {
            fprintf( stderr, "videoinput: Can't free frame %d: %s\n",
                     frameid, strerror( errno ) );
        }
        vidin->capbuffers[ frameid ].free = 1;
    } else {
        if( ioctl( vidin->grab_fd, VIDIOCMCAPTURE, vidin->grab_buf + frameid ) < 0 ) {
            fprintf( stderr, "videoinput: Can't free frame %d: %s\n",
                     frameid, strerror( errno ) );
        }
    }
}

void videoinput_free_frame( videoinput_t *vidin, int frameid )
{
    if( vidin->isv4l2 || vidin->have_mmap ) {
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

static void wait_for_frame_v4l1( videoinput_t *vidin, int frameid )
{
    alarms = 0;
    alarm( SYNC_TIMEOUT );
    if( ioctl( vidin->grab_fd, VIDIOCSYNC, vidin->grab_buf + frameid ) < 0 ) {
        fprintf( stderr, "videoinput: Can't wait for frame %d: %s\n",
                 frameid, strerror( errno ) );
    }
    alarm( 0 );
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
    if( vidin->isv4l2 ) {
        struct v4l2_buffer cur_buf;
 
        wait_for_frame_v4l2( vidin );
 
        cur_buf.type = vidin->capbuffers[ 0 ].vidbuf.type;
        if( ioctl( vidin->grab_fd, VIDIOC_DQBUF, &cur_buf ) < 0 ) {
            fprintf( stderr, "videoinput: Can't read frame. Error was: %s.\n",
                     strerror( errno ) );
            /* We must now restart capture. */
            videoinput_stop_capture_v4l2( vidin );
            videoinput_free_all_frames( vidin );
            videoinput_start_capture_v4l2( vidin );
            *frameid = -1;
            return 0;
        }
        vidin->capbuffers[ cur_buf.index ].free = 0;
        *frameid = cur_buf.index;
        return vidin->capbuffers[ cur_buf.index ].data;
    } else {
        if( vidin->have_mmap ) {
            uint8_t *cur;
            wait_for_frame_v4l1( vidin, vidin->curframe );
            cur = vidin->map + vidin->gb_buffers.offsets[ vidin->curframe ];
            *frameid = vidin->curframe;
            vidin->curframe = ( vidin->curframe + 1 ) % vidin->numframes;
            return cur;
        } else {
            int rc = read( vidin->grab_fd, vidin->grab_data, vidin->grab_size );
            if( vidin->grab_size != rc ) {
                fprintf( stderr, "videoinput: Can't read frame. Returned %d, error: %s.\n",
                         rc, strerror( errno ) );
                return 0;
            } else {
                return vidin->grab_data;
            }
        }
    }
}

int videoinput_buffer_invalid( videoinput_t *vidin, int frameid )
{
    if( !vidin->isv4l2 ) {
        return 0;
    } else {
        return vidin->capbuffers[ frameid ].free;
    }
}

videoinput_t *videoinput_new( const char *v4l_device, int capwidth,
                              int norm, int verbose )
{
    videoinput_t *vidin = malloc( sizeof( videoinput_t ) );
    struct video_capability caps_v4l1;
    struct v4l2_capability caps_v4l2;
    struct video_picture grab_pict;
    struct video_window grab_win;
    int i;

    if( capwidth & 1 ) {
        capwidth -= 1;
        fprintf( stderr, "videoinput: Odd values for input width not allowed, "
                         "using %d instead.\n", capwidth );
    }

    if( !vidin ) {
        fprintf( stderr, "videoinput: Can't allocate memory.\n" );
        return 0;
    }

    siginit();

    vidin->curframe = 0;
    vidin->verbose = verbose;
    vidin->norm = norm;
    vidin->height = videoinput_get_norm_height( norm );
    vidin->cur_tuner_state = TUNER_STATE_NO_SIGNAL;

    vidin->hastuner = 0;
    vidin->numtuners = 0;
    vidin->tunerid = 0;
    vidin->tunerlow = 0;

    vidin->isv4l2 = 0;
    vidin->isbttv = 0;
    vidin->isuyvy = 0;

    vidin->signal_recover_wait = 0;
    vidin->signal_acquire_wait = 0;
    vidin->change_muted = 1;
    vidin->user_muted = 0;
    vidin->hasaudio = 1;
    vidin->audiomode = 0;
    vidin->curinput = 0;
    vidin->have_mmap = 0;
    vidin->is_streaming = 0;

    memset( vidin->inputname, 0, sizeof( vidin->inputname ) );

    /* First, open the device. */
    vidin->grab_fd = open( v4l_device, O_RDWR );
    if( vidin->grab_fd < 0 ) {
        fprintf( stderr, "videoinput: Can't open '%s': %s\n",
                 v4l_device, strerror( errno ) );
        free( vidin );
        return 0;
    }

    /**
     * Next, ask for its capabilities.  This will also confirm it's a V4L2 
     * device. 
     */
    if( ioctl( vidin->grab_fd, VIDIOC_QUERYCAP, &caps_v4l2 ) < 0 ) {
        /* Can't get V4L2 capabilities, maybe this is a V4L1 device? */
        if( ioctl( vidin->grab_fd, VIDIOCGCAP, &caps_v4l1 ) < 0 ) {
            fprintf( stderr, "videoinput: video4linux device '%s' refuses "
                     "to provide capability information, giving up.\n"
                     "videoinput: Is '%s' a video4linux device?\n", v4l_device, v4l_device );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        } else if( vidin->verbose ) {
            fprintf( stderr, "videoinput: Using video4linux driver '%s'.\n"
                             "videoinput: Card type is %x, audio %d.\n",
                     caps_v4l1.name, caps_v4l1.type, caps_v4l1.audios );
        }
    } else {
        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: Using video4linux2 driver '%s', card '%s' (bus %s).\n"
                             "videoinput: Version is %u, capabilities %x.\n",
                     caps_v4l2.driver, caps_v4l2.card, caps_v4l2.bus_info,
                     caps_v4l2.version, caps_v4l2.capabilities );
        }
        vidin->isv4l2 = 1;
    }

    if( vidin->isv4l2 ) {
        struct v4l2_input in;

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
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
    } else {
        /* The capabilities should tell us how many inputs this card has. */
        vidin->numinputs = caps_v4l1.channels;
        if( vidin->numinputs == 0 ) {
            fprintf( stderr, "videoinput: No inputs available on "
                     "video4linux device '%s'.\n", v4l_device );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
    }

    if( !vidin->isv4l2 ) {
        /* Check if this is a bttv-based card.  Code taken from xawtv. */
#define BTTV_VERSION            _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
        /* dirty hack time / v4l design flaw -- works with bttv only
         * this adds support for a few less common PAL versions */
        if( !(ioctl( vidin->grab_fd, BTTV_VERSION, &i ) < 0) ) {
            vidin->isbttv = 1;
        } else if( norm > VIDEOINPUT_SECAM ) {
            fprintf( stderr, "videoinput: Capture card '%s' does not seem to use the bttv driver.\n"
                             "videoinput: The norm %s is only supported in V4L1 for bttv-supported cards.\n",
                     v4l_device, videoinput_get_norm_name( norm ) );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
#undef BTTV_VERSION

        if( norm > VIDEOINPUT_NTSC_JP ) {
            fprintf( stderr, "videoinput: Detected only a V4L1 driver.  The PAL-60 norm\n"
                             "videoinput: is only available if driver supports V4L2.\n" );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
    }

    /* On initialization, set to input 0.  This is just to start things up. */
    videoinput_set_input_num( vidin, 0 );

    /* Test for audio support. */
    if( !vidin->isv4l2 ) {
        struct video_audio audio;

        if( ( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &audio ) < 0 ) && vidin->verbose ) {
            vidin->hasaudio = 0;
            fprintf( stderr, "videoinput: No audio capability detected (asked for audio, got '%s').\n",
                     strerror( errno ) );
        } else if( verbose ) {
            fprintf( stderr, "videoinput: Audio supports " );
            if( audio.flags & VIDEO_AUDIO_MUTE ) {
                fprintf( stderr, "Mute " );
            } else if( audio.flags & VIDEO_AUDIO_MUTABLE ) {
                fprintf( stderr, "Mutable " );
            } else if( audio.flags & VIDEO_AUDIO_VOLUME ) {
                fprintf( stderr, "Volume " );
            } else if( audio.flags & VIDEO_AUDIO_BASS ) {
                fprintf( stderr, "Bass " );
            } else if( audio.flags & VIDEO_AUDIO_TREBLE ) {
                fprintf( stderr, "Treble " );
            }
            fprintf( stderr, "\n" );
        }
    }

    /* Set to stereo by default. */
    videoinput_set_audio_mode( vidin, VIDEO_SOUND_STEREO );

    /**
     * Once we're here, we've set the hardware norm.  Now confirm that
     * our width is acceptable by again asking what the capabilities
     * are.
     */
    if( vidin->isv4l2 ) {
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
                fprintf( stderr, "videoinput: video4linux device '%s' refuses "
                         "to provide YUYV or UYVY format video: %s.\n",
                         v4l_device, strerror( errno ) );
                fprintf( stderr, "videoinput: Please post a bug report to " PACKAGE_BUGREPORT "\n"
                                 "videoinput: indicating your capture card, driver, and the\n"
                                 "videoinput: error message above.\n" );
                close( vidin->grab_fd );
                free( vidin );
                return 0;
            }
            vidin->isuyvy = 1;
        }

        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: Field %d, colorspace %d.\n",
                     imgformat.fmt.pix.field, imgformat.fmt.pix.colorspace );
        }

        if( vidin->height != imgformat.fmt.pix.height ) {
            fprintf( stderr, "videoinput: Card refuses to give full-height "
                     "frames, gives %d instead.\n"
                     "videoinput: Giving up, sorry!\n",
                     imgformat.fmt.pix.height );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }

        if( capwidth != imgformat.fmt.pix.width ) {
            fprintf( stderr, "videoinput: Requested input width %d unavailable: "
                     "driver offers %d.  Using %d.\n",
                     capwidth, imgformat.fmt.pix.width,
                     imgformat.fmt.pix.width );
        }
        vidin->width = imgformat.fmt.pix.width;

    } else {
        if( ioctl( vidin->grab_fd, VIDIOCGCAP, &caps_v4l1 ) < 0 ) {
            fprintf( stderr, "videoinput: video4linux device '%s' refuses "
                     "to provide set capability information, giving up.\n", v4l_device );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
        if( capwidth > caps_v4l1.maxwidth ) {
            fprintf( stderr, "videoinput: Requested input width %d too large: "
                     "maximum supported by card is %d.  Using %d.\n",
                     capwidth, caps_v4l1.maxwidth, caps_v4l1.maxwidth );
            capwidth = caps_v4l1.maxwidth;
        }
        if( capwidth < caps_v4l1.minwidth ) {
            fprintf( stderr, "videoinput: Requested input width %d too small: "
                     "minimum supported by card is %d.  Using %d.\n",
                     capwidth, caps_v4l1.minwidth, caps_v4l1.minwidth );
            capwidth = caps_v4l1.minwidth;
        }
        vidin->width = capwidth;


        /* Set the format using the SPICT ioctl. */
        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
            fprintf( stderr, "videoinput: Can't get image information from the card, unable to"
                     "process the output: %s.\n", strerror( errno ) );
            fprintf( stderr, "videoinput: Please post a bug report to " PACKAGE_BUGREPORT
                     " indicating your capture card, driver, and the error message above.\n" );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }

        grab_pict.depth = 16;
        grab_pict.palette = VIDEO_PALETTE_YUV422;
        if( ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
            /* Try for UYVY instead. */
            grab_pict.palette = VIDEO_PALETTE_UYVY;
            if( ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
                fprintf( stderr, "videoinput: Can't get YUVY or UYVY images from the card, unable to"
                         "process the output: %s.\n", strerror( errno ) );
                fprintf( stderr, "videoinput: Please post a bug report to " PACKAGE_BUGREPORT
                         " indicating your capture card, driver, and the error message above.\n" );
                close( vidin->grab_fd );
                free( vidin );
                return 0;
            }
            vidin->isuyvy = 1;
        }
        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: Brightness %d, hue %d, colour %d, contrast %d\n"
                             "videoinput: Whiteness %d, depth %d, palette %d.\n",
                     grab_pict.brightness, grab_pict.hue, grab_pict.colour, grab_pict.contrast,
                     grab_pict.whiteness, grab_pict.depth, grab_pict.palette );
        }

        /* Set and get the window settings. */
        memset( &grab_win, 0, sizeof( struct video_window ) );
        grab_win.width = vidin->width;
        grab_win.height = vidin->height;
        if( ioctl( vidin->grab_fd, VIDIOCSWIN, &grab_win ) < 0 ) {
            fprintf( stderr, "videoinput: Failed to set the V4L window size (capture width and height): %s\n",
                     strerror( errno ) );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
        if( ioctl( vidin->grab_fd, VIDIOCGWIN, &grab_win ) < 0 ) {
            fprintf( stderr, "videoinput: Failed to get the V4L window size (capture width and height): %s\n",
                     strerror( errno ) );
            fprintf( stderr, "videoinput: Not important, but please send a bug report including what driver "
                             "you're using and output of 'tvtime -v' to our bug report page off of "
                             PACKAGE_BUGREPORT "\n" );
        }

        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: Window set to (%d,%d/%dx%d), chromakey %d, flags %d, clips %d.\n",
                 grab_win.x, grab_win.y, grab_win.width,
                 grab_win.height, grab_win.chromakey,
                 grab_win.flags, grab_win.clipcount );
        }
    }

    if( vidin->isv4l2 ) {
        struct v4l2_requestbuffers req;

        /**
         * Ask Video Device for Buffers 
         */
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if( ioctl( vidin->grab_fd, VIDIOC_REQBUFS, &req ) < 0 ) {
            fprintf( stderr, "videoinput: Card failed to allocate capture buffers: %s\n",
                     strerror( errno ) );
            close( vidin->grab_fd );
            free( vidin );
            return 0;
        }
        vidin->numframes = req.count;

        if( vidin->numframes < 1 ) {
            fprintf( stderr, "videoinput: No capture buffers available from card.\n" );
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
            if( ioctl( vidin->grab_fd, VIDIOC_QUERYBUF, vidbuf ) < 0 ) {
                fprintf( stderr, "videoinput: Can't get information about buffer %d: %s.\n",
                         i, strerror( errno ) );
                close( vidin->grab_fd );
                free( vidin );
                return 0;
            }

            vidin->capbuffers[ i ].data = mmap( 0, vidbuf->length, PROT_READ | PROT_WRITE,
                                                MAP_SHARED, vidin->grab_fd, vidbuf->m.offset );
            if( ((int) vidin->capbuffers[ i ].data) == -1 ) {
                fprintf( stderr, "videoinput: Can't map buffer %d: %s.\n",
                         i, strerror( errno ) );
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
    } else {
        /* Try to set up mmap-based capture. */
        if( ioctl( vidin->grab_fd, VIDIOCGMBUF, &(vidin->gb_buffers) ) < 0 ) {
            fprintf( stderr, "videoinput: Can't get capture buffer properties.  No mmap support?\n"
                     "Please post a bug report about this to " PACKAGE_BUGREPORT "\n"
                     "with your card, driver and this error message: %s.\n", strerror( errno ) );
            fprintf( stderr, "videoinput: Will try to fall back to (untested) read()-based capture..\n" );
        } else {
            /* If we got more frames than we asked for, limit to 4 for now. */
            if( vidin->gb_buffers.frames > 4 ) {
                if( vidin->verbose ) {
                    fprintf( stderr, "videoinput: Capture card provides %d buffers, but we only need 4.\n",
                             vidin->gb_buffers.frames );
                }
                vidin->numframes = 4;
            } else {
                vidin->numframes = vidin->gb_buffers.frames;
            }

            vidin->grab_buf = malloc( sizeof( struct video_mmap ) * vidin->numframes );
            if( !vidin->grab_buf ) {
                fprintf( stderr, "videoinput: Can't allocate grab_buf memory.\n" );
                close( vidin->grab_fd );
                free( vidin );
                return 0;
            }

            /* Setup mmap capture buffers. */
            for( i = 0; i < vidin->numframes; i++ ) {
                vidin->grab_buf[ i ].format = VIDEO_PALETTE_YUV422; /* Y'CbCr 4:2:2 Packed. */
                vidin->grab_buf[ i ].frame = i;
                vidin->grab_buf[ i ].width = vidin->width;
                vidin->grab_buf[ i ].height = vidin->height;
            }

            vidin->map = (uint8_t *) mmap( 0, vidin->gb_buffers.size, PROT_READ|PROT_WRITE,
                                           MAP_SHARED, vidin->grab_fd, 0 );
            if( (int) (vidin->map) != -1 ) {
                vidin->have_mmap = 1;
                videoinput_free_all_frames( vidin );
                return vidin;
            } else {
                fprintf( stderr, "videoinput: mmap() failed.  Will try to fall back to "
                        "(untested) read()-based capture..\n" );
            }
        }

        /* Fallback to read().  THIS CODE IS UNTESTED.  IS THIS EVEN FINISHED? */
        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: No mmap support available, using read().\n" );
        }

        vidin->have_mmap = 0;
        vidin->grab_size = (grab_win.width * grab_win.height * 2);
        vidin->grab_data = malloc( vidin->grab_size );
        vidin->numframes = 2;

        return vidin;
    }
}

void videoinput_delete( videoinput_t *vidin )
{
    /* Mute audio on exit. */
    videoinput_mute( vidin, 1 );

    if( vidin->isv4l2 ) {
    } else {
        if( vidin->have_mmap ) {
            munmap( vidin->map, vidin->gb_buffers.size );
            free( vidin->grab_buf );
        } else {
            free( vidin->grab_data );
        }
    }

    close( vidin->grab_fd );
    free( vidin );
}

static double videoinput_get_control_v4l2( videoinput_t *vidin, uint32_t id )
{
    struct v4l2_queryctrl query;

    query.id = id;
    if( ioctl( vidin->grab_fd, VIDIOC_QUERYCTRL, &query ) >= 0 ) {
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
    if( ioctl( vidin->grab_fd, VIDIOC_QUERYCTRL, &query ) >= 0 ) {
        struct v4l2_control control;
        control.id = id;
        control.value = query.minimum + ((int) ((val * ((double) (query.maximum - query.minimum))) + 0.5));
        ioctl( vidin->grab_fd, VIDIOC_S_CTRL, &control );
    }
}

int videoinput_get_hue( videoinput_t *vidin )
{
    if( vidin->isv4l2 ) {
        return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_HUE ) * 100.0) + 0.5);
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            return (int) ((((double) grab_pict.hue / 65535.0) * 100.0) + 0.5);
        }
    }

    return 0;
}

void videoinput_set_hue( videoinput_t *vidin, int newhue )
{
    if( newhue > 100 ) newhue = 100;
    if( newhue <   0 ) newhue = 0;

    if( vidin->isv4l2 ) {
        videoinput_set_control_v4l2( vidin, V4L2_CID_HUE, ((double) newhue) / 100.0 );
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            grab_pict.hue = (int) (((((double) newhue) / 100.0) * 65535.0) + 0.5);
            ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
        }
    }
}

void videoinput_set_hue_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_hue( vidin, videoinput_get_hue( vidin ) + offset );
}

int videoinput_get_brightness( videoinput_t *vidin )
{
    if( vidin->isv4l2 ) {
        return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_BRIGHTNESS ) * 100.0) + 0.5);
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            return (int) ((((double) grab_pict.brightness / 65535.0) * 100.0) + 0.5);
        }
    }

    return 0;
}

void videoinput_set_brightness( videoinput_t *vidin, int newbright )
{
    if( newbright > 100 ) newbright = 100;
    if( newbright <   0 ) newbright = 0;

    if( vidin->isv4l2 ) {
        videoinput_set_control_v4l2( vidin, V4L2_CID_BRIGHTNESS, ((double) newbright) / 100.0 );
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            grab_pict.brightness = (int) (((((double) newbright) / 100.0) * 65535.0) + 0.5);
            ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
        }
    }
}

void videoinput_set_brightness_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_brightness( vidin, videoinput_get_brightness( vidin ) + offset );
}

int videoinput_get_contrast( videoinput_t *vidin )
{
    if( vidin->isv4l2 ) {
        return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_CONTRAST ) * 100.0) + 0.5);
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            return (int) ((((double) grab_pict.contrast / 65535.0) * 100.0) + 0.5);
        }
    }

    return 0;
}

void videoinput_set_contrast( videoinput_t *vidin, int newcont )
{
    if( newcont > 100 ) newcont = 100;
    if( newcont <   0 ) newcont = 0;

    if( vidin->isv4l2 ) {
        videoinput_set_control_v4l2( vidin, V4L2_CID_CONTRAST, ((double) newcont) / 100.0 );
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            grab_pict.contrast = (int) (((((double) newcont) / 100.0) * 65535.0) + 0.5);
            ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
        }
    }
}

void videoinput_set_contrast_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_contrast( vidin, videoinput_get_contrast( vidin ) + offset );
}

int videoinput_get_colour( videoinput_t *vidin )
{
    if( vidin->isv4l2 ) {
        return (int) ((videoinput_get_control_v4l2( vidin, V4L2_CID_SATURATION ) * 100.0) + 0.5);
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            return (int) ((((double) grab_pict.colour / 65535.0) * 100.0) + 0.5);
        }
    }

    return 0;
}

void videoinput_set_colour( videoinput_t *vidin, int newcolour )
{
    if( newcolour > 100 ) newcolour = 100;
    if( newcolour <   0 ) newcolour = 0;

    if( vidin->isv4l2 ) {
        videoinput_set_control_v4l2( vidin, V4L2_CID_SATURATION, ((double) newcolour) / 100.0 );
    } else {
        struct video_picture grab_pict;

        if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
            grab_pict.colour = (int) (((((double) newcolour) / 100.0) * 65535.0) + 0.5);
            ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
        }
    }
}

void videoinput_set_colour_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_colour( vidin, videoinput_get_colour( vidin ) + offset );
}

static void videoinput_do_mute( videoinput_t *vidin, int mute )
{
    struct video_audio audio;
    int is_muted = 1;

    if( vidin->isv4l2 ) {
        struct v4l2_control control;

        control.id = V4L2_CID_AUDIO_MUTE;
        if( ioctl( vidin->grab_fd, VIDIOC_G_CTRL, &control ) < 0 ) {
            fprintf( stderr, "videoinput: Can't get mute info.  Post a bug report with your\n"
                             "videoinput: driver info to " PACKAGE_BUGREPORT "\n" );
            fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
        } else {
            is_muted = control.value;
        }
    } else {
        if( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &audio ) < 0 ) {
            fprintf( stderr, "videoinput: Audio state query failed (got '%s').\n",
                     strerror( errno ) );
        } else {
            is_muted = ((audio.flags & VIDEO_AUDIO_MUTE) == VIDEO_AUDIO_MUTE);
        }
    }

    if( vidin->hasaudio && mute != is_muted ) {
        if( vidin->isv4l2 ) {
            struct v4l2_control control;

            control.id = V4L2_CID_AUDIO_MUTE;
            control.value = mute ? 1 : 0;

            if( ioctl( vidin->grab_fd, VIDIOC_S_CTRL, &control ) < 0 ) {
                fprintf( stderr, "videoinput: Can't mute card.  Post a bug report with your\n"
                                 "videoinput: driver info to " PACKAGE_BUGREPORT "\n" );
                fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
            }

            if( !mute ) {
                videoinput_set_control_v4l2( vidin, V4L2_CID_AUDIO_VOLUME, 1.0 );
            }
        } else {
            if( mute ) {
                audio.flags |= VIDEO_AUDIO_MUTE;
            } else {
                audio.flags &= ~VIDEO_AUDIO_MUTE;
            }
            audio.volume = 65535;

            if( ioctl( vidin->grab_fd, VIDIOCSAUDIO, &audio ) < 0 ) {
                fprintf( stderr, "videoinput: Can't set audio settings.  I have no idea what "
                         "might cause this.  Post a bug report with your driver info to "
                         PACKAGE_BUGREPORT "\n" );
                fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
            }
        }
    }

    if( vidin->isv4l2 ) {
        struct v4l2_control control;
        control.id = V4L2_CID_AUDIO_MUTE;
        if( ioctl( vidin->grab_fd, VIDIOC_G_CTRL, &control ) < 0 ) {
            fprintf( stderr, "videoinput: Can't get mute info.  Post a bug report with your\n"
                             "videoinput: driver info to " PACKAGE_BUGREPORT "\n" );
            fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
        } else {
            is_muted = control.value;
        }
    }
}

void videoinput_set_audio_mode( videoinput_t *vidin, int mode )
{
    if( mode == VIDEOINPUT_LANG1 && vidin->norm == VIDEOINPUT_NTSC ) {
        mode = VIDEOINPUT_LANG2;
    }

    if( mode > VIDEOINPUT_LANG2 ) {
        mode = VIDEOINPUT_MONO;
    }

    if( vidin->audiomode != mode ) {
        if( vidin->isv4l2 ) {
            if( vidin->hastuner ) {
                struct v4l2_tuner tuner;

                memset( &tuner, 0, sizeof( struct v4l2_tuner ) );
                tuner.index = vidin->tunerid;
                tuner.audmode = videoinput_get_audmode_v4l2( mode );
                if( ioctl( vidin->grab_fd, VIDIOC_S_TUNER, &tuner ) < 0 ) {
                    fprintf( stderr, "videoinput: Can't set tuner audio mode: %s\n",
                             strerror( errno ) );
                } else {
                    vidin->audiomode = mode;
                }
            }
        } else {
            struct video_audio audio;

            if( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &audio ) < 0 ) {
                if( vidin->verbose ) {
                    fprintf( stderr, "videoinput: Audio state query failed (got '%s').\n",
                             strerror( errno ) );
                }
            } else {
                int was_muted = (audio.flags & VIDEO_AUDIO_MUTE);

                /* Set the mode. */
                audio.mode = mode;
                if( ioctl( vidin->grab_fd, VIDIOCSAUDIO, &audio ) < 0 ) {
                    fprintf( stderr, "videoinput: Can't set audio mode setting.  I have no idea what "
                             "might cause this.  Post a bug report with your driver info to "
                             PACKAGE_BUGREPORT "\n" );
                    fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
                } else {
                    vidin->audiomode = mode;
                }

                if( was_muted & !(audio.flags & VIDEO_AUDIO_MUTE) ) {
                    /* Stupid card dropped the mute state, have to go back to being muted. */
                    videoinput_do_mute( vidin, 1 );
                }
            }
        }
    }
}

/* freqKHz is in KHz (duh) */
void videoinput_set_tuner_freq( videoinput_t *vidin, int freqKHz )
{
    unsigned long frequency = freqKHz;

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
        videoinput_do_mute( vidin, vidin->user_muted || vidin->change_muted );
        vidin->cur_tuner_state = TUNER_STATE_SIGNAL_DETECTED;
        vidin->signal_acquire_wait = SIGNAL_ACQUIRE_DELAY;
        vidin->signal_recover_wait = 0;

        if( vidin->isv4l2 ) {
            struct v4l2_frequency freqinfo;
            int wasstreaming = vidin->is_streaming;

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
        } else {
            if( ioctl( vidin->grab_fd, VIDIOCSFREQ, &frequency ) < 0 ) {
                fprintf( stderr, "videoinput: Tuner present, but our request to change "
                                 "to frequency %d failed with this error: %s.\n", freqKHz, strerror( errno ) );
                fprintf( stderr, "videoinput: Please file a bug report at " PACKAGE_BUGREPORT "\n" );
            }
        }
    }
}

int videoinput_get_tuner_freq( videoinput_t *vidin ) 
{
    if( vidin->hastuner ) {
        unsigned long frequency;

        if( vidin->isv4l2 ) {
            struct v4l2_frequency freqinfo;

            if( ioctl( vidin->grab_fd, VIDIOC_G_FREQUENCY, &freqinfo ) < 0 ) {
                fprintf( stderr, "videoinput: Tuner refuses to tell us the current frequency: %s\n",
                         strerror( errno ) );
                fprintf( stderr, "videoinput: Please file a bug report at " PACKAGE_BUGREPORT "\n" );
                return 0;
            }
            frequency = freqinfo.frequency;
        } else {
            if( ioctl( vidin->grab_fd, VIDIOCGFREQ, &frequency ) < 0 ) {
                fprintf( stderr, "videoinput: Tuner refuses to tell us the current frequency: %s\n",
                         strerror( errno ) );
                fprintf( stderr, "videoinput: Please file a bug report at " PACKAGE_BUGREPORT "\n" );
                return 0;
            }
        }

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
        if( vidin->isv4l2 ) {
            struct v4l2_tuner tuner;

            tuner.index = vidin->tunerid;
            if( ioctl( vidin->grab_fd, VIDIOC_G_TUNER, &tuner ) < 0 ) {
                if( vidin->verbose ) {
                    fprintf( stderr, "videoinput: Can't detect signal from tuner: %s\n",
                             strerror( errno ) );
                }
            } else if( !tuner.signal ) {
                return 0;
            }
        } else {
            struct video_tuner tuner;

            if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &tuner ) < 0 ) {
                if( vidin->verbose ) {
                    fprintf( stderr, "videoinput: Can't detect signal from tuner: %s\n",
                             strerror( errno ) );
                }
            } else if( !tuner.signal ) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Finds an appropriate tuner.
 */
static void videoinput_find_and_set_tuner( videoinput_t *vidin )
{
    if( vidin->isv4l2 ) {
        struct v4l2_tuner tuner;

        tuner.index = vidin->tunerid;
        if( ioctl( vidin->grab_fd, VIDIOC_G_TUNER, &tuner ) < 0 ) {
            fprintf( stderr, "videoinput: Can't get tuner info: %s\n",
                     strerror( errno ) );
        } else {
            vidin->tunerlow = (tuner.capability & V4L2_TUNER_CAP_LOW) ? 1 : 0;
        }
    } else {
        struct video_tuner tuner;
        int tuner_number = -1;
        int found = 0;
        int i;

        for( i = 0; i < vidin->numtuners; i++ ) {
            tuner.tuner = i;

            if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &tuner ) >= 0 ) {
                if( ((vidin->norm == VIDEOINPUT_PAL || vidin->norm == VIDEOINPUT_PAL_NC || vidin->norm == VIDEOINPUT_PAL_N) && tuner.flags & VIDEO_TUNER_PAL) ||
                    (vidin->norm == VIDEOINPUT_SECAM && tuner.flags & VIDEO_TUNER_SECAM) ||
                    ((vidin->norm == VIDEOINPUT_NTSC || vidin->norm == VIDEOINPUT_NTSC_JP || vidin->norm == VIDEOINPUT_PAL_M) && tuner.flags & VIDEO_TUNER_NTSC) ) {
                    found = 1;
                    tuner_number = i;
                    break;
                }
            }
        }

        if( found ) {
            int mustchange = 0;

            tuner.tuner = tuner_number;

            if( vidin->norm == VIDEOINPUT_PAL || vidin->norm == VIDEOINPUT_PAL_NC || vidin->norm == VIDEOINPUT_PAL_N ) {
                if( tuner.mode != VIDEO_MODE_PAL ) {
                    mustchange = 1;
                    tuner.mode = VIDEO_MODE_PAL;
                }
            } else if( vidin->norm == VIDEOINPUT_SECAM ) {
                if( tuner.mode != VIDEO_MODE_SECAM ) {
                    mustchange = 1;
                    tuner.mode = VIDEO_MODE_SECAM;
                }
            } else {
                if( tuner.mode != VIDEO_MODE_NTSC ) {
                    mustchange = 1;
                    tuner.mode = VIDEO_MODE_NTSC;
                }
            }

            if( mustchange ) {
                struct video_channel grab_chan;

                if( ioctl( vidin->grab_fd, VIDIOCSTUNER, &tuner ) < 0 ) {
                    fprintf( stderr, "videoinput: Tuner is not in the correct mode, and we can't set it.\n"
                             "            Please file a bug report at " PACKAGE_BUGREPORT "\n"
                             "            indicating your card, driver and this error message: %s.\n",
                             strerror( errno ) );
                }

                /**
                 * After setting the tuner, we need to re-set the norm on our channel.
                 * Doing this fixes PAL-M, at least with the bttv driver in 2.4.20.
                 * I am not happy with this, as I haven't seen it documented anywhere.
                 * Please correct me if I'm wrong.
                 */
                grab_chan.channel = vidin->curinput;
                grab_chan.norm = videoinput_get_v4l1_norm( vidin->norm );

                if( ioctl( vidin->grab_fd, VIDIOCSCHAN, &grab_chan ) < 0 ) {
                    fprintf( stderr, "videoinput: Card refuses to re-set the channel.\n"
                             "Please post a bug report to " PACKAGE_BUGREPORT "\n"
                             "indicating your card, driver, and this error message: %s.\n", strerror( errno ) );
                } else {
                    vidin->numtuners = grab_chan.tuners;
                }
            }
        }

        if( vidin->numtuners > 0 ) {
            tuner.tuner = tuner_number;

            if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &tuner ) < 0 ) {
                fprintf( stderr, "videoinput: Input indicates that tuners are available, but "
                         "driver refuses to give information about them.\n"
                         "videoinput: Please file a bug report at " PACKAGE_BUGREPORT
                         " and indicate that card and driver you have.\n" );
                fprintf( stderr, "videoinput: Also include this error: '%s'\n", strerror( errno ) );
                return;
            }

            if( vidin->verbose ) {
                fprintf( stderr, "videoinput: tuner.tuner = %d\n"
                                 "videoinput: tuner.name = %s\n"
                                 "videoinput: tuner.rangelow = %ld\n"
                                 "videoinput: tuner.rangehigh = %ld\n"
                                 "videoinput: tuner.signal = %d\n"
                                 "videoinput: tuner.flags = ",
                         tuner.tuner, tuner.name, tuner.rangelow,
                         tuner.rangehigh, tuner.signal );

                if( tuner.flags & VIDEO_TUNER_PAL ) fprintf( stderr, "PAL " );
                if( tuner.flags & VIDEO_TUNER_NTSC ) fprintf( stderr, "NTSC " );
                if( tuner.flags & VIDEO_TUNER_SECAM ) fprintf( stderr, "SECAM " );
                if( tuner.flags & VIDEO_TUNER_LOW ) fprintf( stderr, "LOW " );
                if( tuner.flags & VIDEO_TUNER_NORM ) fprintf( stderr, "NORM " );
                if( tuner.flags & VIDEO_TUNER_STEREO_ON ) fprintf( stderr, "STEREO_ON " );
                if( tuner.flags & VIDEO_TUNER_RDS_ON ) fprintf( stderr, "RDS_ON " );
                if( tuner.flags & VIDEO_TUNER_MBS_ON ) fprintf( stderr, "MBS_ON" );

                fprintf( stderr, "\nvideoinput: tuner.mode = " );
                switch (tuner.mode) {
                case VIDEO_MODE_PAL: fprintf( stderr, "PAL" ); break;
                case VIDEO_MODE_NTSC: fprintf( stderr, "NTSC" ); break;
                case VIDEO_MODE_SECAM: fprintf( stderr, "SECAM" ); break;
                case VIDEO_MODE_AUTO: fprintf( stderr, "AUTO" ); break;
                default: fprintf( stderr, "UNDEFINED" ); break;
                }
                fprintf( stderr, "\n");
            }

            vidin->tunerlow = (tuner.flags & VIDEO_TUNER_LOW) ? 1 : 0;
            vidin->hastuner = 1;
        }
    }
}


void videoinput_set_input_num( videoinput_t *vidin, int inputnum )
{
    if( vidin->isv4l2 ) {
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
            if( ioctl( vidin->grab_fd, VIDIOC_S_STD, &std ) < 0 ) {
                fprintf( stderr, "videoinput: Driver refuses to set norm: %s\n",
                         strerror( errno ) );
            }
        }

        if( wasstreaming ) {
            videoinput_free_all_frames( vidin );
            videoinput_start_capture_v4l2( vidin );
        }
    } else {
        if( inputnum >= vidin->numinputs ) {
            fprintf( stderr, "videoinput: Requested input number %d not valid, "
                     "max is %d.\n", inputnum, vidin->numinputs );
        } else {
            struct video_channel grab_chan;

            grab_chan.channel = inputnum;
            if( ioctl( vidin->grab_fd, VIDIOCGCHAN, &grab_chan ) < 0 ) {
                fprintf( stderr, "videoinput: Card refuses to give information on its current input.\n"
                         "Please post a bug report to " PACKAGE_BUGREPORT "\n"
                         "indicating your card, driver, and this error message: %s.\n", strerror( errno ) );
            } else {
                grab_chan.channel = inputnum;
                grab_chan.norm = videoinput_get_v4l1_norm( vidin->norm );

                if( ioctl( vidin->grab_fd, VIDIOCSCHAN, &grab_chan ) < 0 ) {
                    fprintf( stderr, "videoinput: Card refuses to set the channel.\n"
                             "Please post a bug report to " PACKAGE_BUGREPORT "\n"
                             "indicating your card, driver, and this error message: %s.\n", strerror( errno ) );
                }

                vidin->curinput = inputnum;
                if( ioctl( vidin->grab_fd, VIDIOCGCHAN, &grab_chan ) < 0 ) {
                    fprintf( stderr, "videoinput: Card refuses to give information on its current input.\n"
                             "Please post a bug report to " PACKAGE_BUGREPORT "\n"
                             "indicating your card, driver, and this error message: %s.\n", strerror( errno ) );
                } else {
                    snprintf( vidin->inputname, sizeof( vidin->inputname ), "%s", grab_chan.name );
                    vidin->numtuners = grab_chan.tuners;
                    vidin->hastuner = (vidin->numtuners > 0);
                }
            }
        }
    }

    /* Once we've set the input, go look for a tuner. */
    videoinput_find_and_set_tuner( vidin );
}

void videoinput_reset_default_settings( videoinput_t *vidin )
{
    if( vidin->norm != VIDEOINPUT_NTSC && vidin->norm != VIDEOINPUT_NTSC_JP ) {
        videoinput_set_hue( vidin, (int) ((((((double) DEFAULT_HUE_PAL) + 128.0) / 255.0) * 100.0) + 0.5) );
        videoinput_set_brightness( vidin, (int) ((((((double) DEFAULT_BRIGHTNESS_PAL) + 128.0) / 255.0) * 100.0) + 0.5) );
        videoinput_set_contrast( vidin, (int) (((((double) DEFAULT_CONTRAST_PAL) / 511.0) * 100.0) + 0.5) );
        videoinput_set_colour( vidin, (int) (((((double) (DEFAULT_SAT_U_PAL + DEFAULT_SAT_V_PAL)/2) / 511.0) * 100.0) + 0.5) );
    } else {
        videoinput_set_hue( vidin, (int) ((((((double) DEFAULT_HUE_NTSC) + 128.0) / 255.0) * 100.0) + 0.5) );
        videoinput_set_brightness( vidin, (int) ((((((double) DEFAULT_BRIGHTNESS_NTSC) + 128.0) / 255.0) * 100.0) + 0.5) );
        videoinput_set_contrast( vidin, (int) (((((double) DEFAULT_CONTRAST_NTSC) / 511.0) * 100.0) + 0.5) );
        videoinput_set_colour( vidin, (int) (((((double) (DEFAULT_SAT_U_NTSC + DEFAULT_SAT_V_NTSC)/2) / 511.0) * 100.0) + 0.5) );
    }
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

void videoinput_switch_to_next_compatible_norm( videoinput_t *vidin )
{
    vidin->norm = videoinput_next_compatible_norm( vidin->norm, vidin->isbttv );
    videoinput_set_input_num( vidin, videoinput_get_input_num( vidin ) );
}

void videoinput_switch_to_compatible_norm( videoinput_t *vidin, int norm )
{
    if( norm != vidin->norm && (videoinput_get_norm_height( norm ) == videoinput_get_norm_height( vidin->norm )) ) {
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

int videoinput_get_audio_mode( videoinput_t *vidin )
{
    if( !vidin->audiomode || vidin->audiomode > VIDEO_SOUND_LANG2 ) {
        return VIDEO_SOUND_MONO;
    } else {
        return vidin->audiomode;
    }
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

int videoinput_is_bttv( videoinput_t *vidin )
{
    return vidin->isbttv;
}

int videoinput_is_uyvy( videoinput_t *vidin )
{
    return vidin->isuyvy;
}

int videoinput_get_input_num( videoinput_t *vidin )
{
    return vidin->curinput;
}

const char *videoinput_get_input_name( videoinput_t *vidin )
{
    return vidin->inputname;
}

