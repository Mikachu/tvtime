/**
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * Uses hacky bttv detection code from xawtv:
 *   (c) 1997-2001 Gerd Knorr <kraxel@bytesex.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
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
 * How long to wait when we lose a signal, or aquire a signal.
 */
#define SIGNAL_RECOVER_DELAY 2
#define SIGNAL_AQUIRE_DELAY  2

/**
 * How many seconds to wait before deciding it's a driver problem.
 */
#define SYNC_TIMEOUT 3

struct videoinput_s
{
    int grab_fd;
    int grab_size;
    int numframes;
    int have_mmap;
    int numinputs;

    int width;
    int height;

    int norm;
    int isbttv;

    unsigned char *grab_data;
    unsigned char *map;
    struct video_mmap *grab_buf;
    struct video_window grab_win;
    struct video_audio audio;
    struct video_channel grab_chan;
    int curframe;

    int has_audio;
    int audiomode;

    int tuner_number;
    struct video_tuner tuner;

    struct video_mbuf gb_buffers;

    int verbose;

    int cur_tuner_state;
    int signal_recover_wait;
    int signal_aquire_wait;

    int muted;
    int user_muted;

    struct video_picture temp_pict;
};

static int alarms;

static void sigalarm( int signal )
{
    alarms++;
    fprintf( stderr, "videoinput: Frame capture timed out (got SIGALRM), hardware/driver problems?\n" );
    fprintf( stderr, "videoinput: Please report this on our bugs page: http://tvtime.sourceforge.net/\n" );
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
    if( ioctl( vidin->grab_fd, VIDIOCMCAPTURE, vidin->grab_buf + frameid ) < 0 ) {
        fprintf( stderr, "videoinput: Can't free frame %d: %s\n", frameid, strerror( errno ) );
    }
}

static void wait_for_frame( videoinput_t *vidin, int frameid )
{
    alarms = 0;
    alarm( SYNC_TIMEOUT );
    if( ioctl( vidin->grab_fd, VIDIOCSYNC, vidin->grab_buf + frameid ) < 0 ) {
        fprintf( stderr, "videoinput: Can't wait for frame %d: %s\n", frameid, strerror( errno ) );
    }
    alarm( 0 );
}

unsigned char *videoinput_next_frame( videoinput_t *vidin, int *frameid )
{
    if( vidin->have_mmap ) {
        unsigned char *cur;
        wait_for_frame( vidin, vidin->curframe );
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

void videoinput_free_frame( videoinput_t *vidin, int frameid )
{
    if( vidin->have_mmap ) {
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

const char *videoinput_norm_name( int norm )
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
    } else {
        return "ERROR";
    }
}

const char *videoinput_audio_mode_name( int mode )
{
    if( mode == VIDEO_SOUND_MONO ) {
        return "Mono";
    } else if( mode == VIDEO_SOUND_STEREO ) {
        return "Stereo";
    } else if( mode == VIDEO_SOUND_LANG1 ) {
        return "Language 1";
    } else if( mode == VIDEO_SOUND_LANG2 ) {
        return "Language 2";
    } else if( mode == 0 ) {
        return "Unset";
    } else {
        fprintf( stderr, "error: %d\n", mode );
        return "ERROR";
    }
}

videoinput_t *videoinput_new( const char *v4l_device, int capwidth,
                              int norm, int verbose )
{
    videoinput_t *vidin = (videoinput_t *) malloc( sizeof( videoinput_t ) );
    struct video_capability grab_cap;
    struct video_picture grab_pict;
    /* struct video_buffer fbuf; */
    int i;

    if( !vidin ) {
        fprintf( stderr, "videoinput: Can't allocate memory.\n" );
        return 0;
    }

    siginit();

    vidin->curframe = 0;
    vidin->verbose = verbose;
    vidin->norm = norm;
    vidin->height = ( vidin->norm == VIDEOINPUT_NTSC || vidin->norm == VIDEOINPUT_NTSC_JP || vidin->norm == VIDEOINPUT_PAL_M ) ? 480 : 576;
    vidin->cur_tuner_state = TUNER_STATE_NO_SIGNAL;
    vidin->signal_recover_wait = 0;
    vidin->signal_aquire_wait = 0;
    vidin->muted = 1;
    vidin->user_muted = 0;
    vidin->has_audio = 1;

    /* First, open the device. */
    vidin->grab_fd = open( v4l_device, O_RDWR );
    if( vidin->grab_fd < 0 ) {
        fprintf( stderr, "videoinput: Can't open '%s': %s\n",
                 v4l_device, strerror( errno ) );
        free( vidin );
        return 0;
    }

    /* Next, ask for its capabilities.  This will also confirm it's a V4L device. */
    if( ioctl( vidin->grab_fd, VIDIOCGCAP, &grab_cap ) < 0 ) {
        fprintf( stderr, "videoinput: video4linux device '%s' refuses "
                 "to provide capability information, giving up.\n"
                 "videoinput: Is '%s' a video4linux device?\n", v4l_device, v4l_device );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }

    if( vidin->verbose ) {
        fprintf( stderr, "videoinput: Using video4linux driver '%s'.\n"
                         "videoinput: Card type is %x, audio %d.\n",
                 grab_cap.name, grab_cap.type, grab_cap.audios );
    }

    /* The capabilities should tell us how many inputs this card has. */
    vidin->numinputs = grab_cap.channels;
    if( vidin->numinputs == 0 ) {
        fprintf( stderr, "videoinput: No inputs available on "
                 "video4linux device '%s'.\n", v4l_device );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }

    /* Check if this is a bttv-based card.  Code taken from xawtv. */
#define BTTV_VERSION            _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
    /* dirty hack time / v4l design flaw -- works with bttv only
     * this adds support for a few less common PAL versions */
    vidin->isbttv = 0;
    if( !(ioctl( vidin->grab_fd, BTTV_VERSION, &i ) < 0) ) {
        vidin->isbttv = 1;
    } else if( norm > VIDEOINPUT_SECAM ) {
        fprintf( stderr, "videoinput: Capture card '%s' does not seem to use the bttv driver.\n"
                 "videoinput: The norm you requested, %s, is only supported for bttv-based cards.\n",
                 v4l_device, videoinput_norm_name( norm ) );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }

    /* On initialization, set to input 0.  This is just to start things up. */
    videoinput_set_input_num( vidin, 0 );

    /* Test for audio support. */
    memset( &(vidin->audio), 0, sizeof( struct video_audio ) );
    if( ( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &(vidin->audio) ) < 0 ) && vidin->verbose ) {
        vidin->has_audio = 0;
        fprintf( stderr, "videoinput: No audio capability detected (asked for audio, got '%s').\n",
                 strerror( errno ) );
    }

    /* Set to stereo by default. */
    videoinput_set_audio_mode( vidin, VIDEO_SOUND_STEREO );

    /**
     * Once we've done that, we've set the hardware norm.  Now confirm that
     * our width is acceptable by again asking what the capabilities are.
     */
    if( ioctl( vidin->grab_fd, VIDIOCGCAP, &grab_cap ) < 0 ) {
        fprintf( stderr, "videoinput: video4linux device '%s' refuses "
                 "to provide set capability information, giving up.\n", v4l_device );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }
    if( capwidth > grab_cap.maxwidth ) {
        fprintf( stderr, "videoinput: Requested input width %d too large: "
                 "maximum supported by card is %d.  Using %d.\n",
                 capwidth, grab_cap.maxwidth, grab_cap.maxwidth );
        capwidth = grab_cap.maxwidth;
    }
    if( capwidth < grab_cap.minwidth ) {
        fprintf( stderr, "videoinput: Requested input width %d too small: "
                 "minimum supported by card is %d.  Using %d.\n",
                 capwidth, grab_cap.minwidth, grab_cap.minwidth );
        capwidth = grab_cap.minwidth;
    }
    vidin->width = capwidth;

    /* Set the format using the SPICT ioctl. */
    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        fprintf( stderr, "videoinput: Can't get image information from the card, unable to"
                 "process the output: %s.\n", strerror( errno ) );
        fprintf( stderr, "videoinput: Please post a bug report to http://sourceforge.net/projects/tvtime/"
                 "indicating your capture card, driver, and the error message above.\n" );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }

    grab_pict.depth = 16;
    grab_pict.palette = VIDEO_PALETTE_YUV422;
    if( ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
        fprintf( stderr, "videoinput: Can't get Y'CbCr 4:2:2 packed images from the card, unable to"
                 "process the output: %s.\n", strerror( errno ) );
        fprintf( stderr, "videoinput: Please post a bug report to http://sourceforge.net/projects/tvtime/"
                 "indicating your capture card, driver, and the error message above.\n" );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }
    if( vidin->verbose ) {
        fprintf( stderr, "videoinput: Brightness %d, hue %d, colour %d, contrast %d\n"
                         "videoinput: Whiteness %d, depth %d, palette %d.\n",
                 grab_pict.brightness, grab_pict.hue, grab_pict.colour, grab_pict.contrast,
                 grab_pict.whiteness, grab_pict.depth, grab_pict.palette );
    }

    /* Set and get the window settings. */
    memset( &(vidin->grab_win), 0, sizeof( struct video_window ) );
    vidin->grab_win.width = vidin->width;
    vidin->grab_win.height = vidin->height;
    if( ioctl( vidin->grab_fd, VIDIOCSWIN, &(vidin->grab_win) ) < 0 ) {
        fprintf( stderr, "videoinput: Failed to set the V4L window size (capture width and height): %s\n",
                 strerror( errno ) );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }
    if( ioctl( vidin->grab_fd, VIDIOCGWIN, &(vidin->grab_win) ) < 0 ) {
        fprintf( stderr, "videoinput: Failed to get the V4L window size (capture width and height): %s\n",
                 strerror( errno ) );
        fprintf( stderr, "videoinput: Not important, but please send a bug report including what driver "
                         "you're using and output of 'tvtime -v' to our bug report page off of "
                         "http://tvtime.sourceforge.net/\n" );
    }

    if( vidin->verbose ) {
        fprintf( stderr, "videoinput: V4LWIN set to (%d,%d/%dx%d), chromakey %d, flags %d, clips %d.\n",
             vidin->grab_win.x, vidin->grab_win.y, vidin->grab_win.width,
             vidin->grab_win.height, vidin->grab_win.chromakey,
             vidin->grab_win.flags, vidin->grab_win.clipcount );
    }

    /* Try to set up mmap-based capture. */
    if( ioctl( vidin->grab_fd, VIDIOCGMBUF, &(vidin->gb_buffers) ) < 0 ) {
        fprintf( stderr, "videoinput: Can't get capture buffer properties.  No mmap support?\n"
                 "Please post a bug report about this to http://www.sourceforge.net/projects/tvtime/\n"
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

        vidin->grab_buf = (struct video_mmap *) malloc( sizeof( struct video_mmap ) * vidin->numframes );
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

        vidin->map = (unsigned char *) mmap( 0, vidin->gb_buffers.size, PROT_READ|PROT_WRITE,
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
    vidin->grab_size = (vidin->grab_win.width * vidin->grab_win.height * 2);
    vidin->grab_data = (unsigned char *) malloc( vidin->grab_size );
    vidin->numframes = 2;

    return vidin;
}

int videoinput_get_hue( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    } else {
        return (int) ((((double) grab_pict.hue / 65535.0) * 100.0) + 0.5);
    }
}

void videoinput_set_hue( videoinput_t *vidin, int newhue )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
        if( newhue > 100 ) newhue = 100;
        if( newhue <   0 ) newhue = 0;
        grab_pict.hue = (int) (((((double) newhue) / 100.0) * 65535.0) + 0.5);
        ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
    }
}

void videoinput_set_hue_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_hue( vidin, videoinput_get_hue( vidin ) + offset );
}

int videoinput_get_brightness( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    } else {
        return (int) ((((double) grab_pict.brightness / 65535.0) * 100.0) + 0.5);
    }
}

void videoinput_set_brightness( videoinput_t *vidin, int newbright )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
        if( newbright > 100 ) newbright = 100;
        if( newbright <   0 ) newbright = 0;
        grab_pict.brightness = (int) (((((double) newbright) / 100.0) * 65535.0) + 0.5);
        ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
    }
}

void videoinput_set_brightness_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_brightness( vidin, videoinput_get_brightness( vidin ) + offset );
}

int videoinput_get_contrast( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    } else {
        return (int) ((((double) grab_pict.contrast / 65535.0) * 100.0) + 0.5);
    }
}

void videoinput_set_contrast( videoinput_t *vidin, int newcont )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
        if( newcont > 100 ) newcont = 100;
        if( newcont <   0 ) newcont = 0;
        grab_pict.contrast = (int) (((((double) newcont) / 100.0) * 65535.0) + 0.5);
        ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
    }
}

void videoinput_set_contrast_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_contrast( vidin, videoinput_get_contrast( vidin ) + offset );
}

int videoinput_get_colour( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    } else {
        return (int) ((((double) grab_pict.colour / 65535.0) * 100.0) + 0.5);
    }
}

void videoinput_set_colour( videoinput_t *vidin, int newcolour )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) >= 0 ) {
        if( newcolour > 100 ) newcolour = 100;
        if( newcolour <   0 ) newcolour = 0;
        grab_pict.colour = (int) (((((double) newcolour) / 100.0) * 65535.0) + 0.5);
        ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
    }
}

void videoinput_set_colour_relative( videoinput_t *vidin, int offset )
{
    videoinput_set_colour( vidin, videoinput_get_colour( vidin ) + offset );
}

int videoinput_has_tuner( videoinput_t *vidin )
{
    return (vidin->tuner_number > -1);
}

int videoinput_get_audio_mode( videoinput_t *vidin )
{
    return vidin->audiomode;
}

void videoinput_set_audio_mode( videoinput_t *vidin, int mode )
{
    vidin->audio.mode = mode;
    if( ioctl( vidin->grab_fd, VIDIOCSAUDIO, &(vidin->audio) ) < 0 ) {
        fprintf( stderr, "videoinput: Can't set audio mode setting.  I have no idea what "
                 "might cause this.  Post a bug report with your driver info to "
                 "http://www.sourceforge.net/projects/tvtime/\n" );
        fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
    }
    if( ( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &(vidin->audio) ) < 0 ) && vidin->verbose ) {
        vidin->has_audio = 0;
        fprintf( stderr, "videoinput: No audio capability detected (asked for audio, got '%s').\n",
                 strerror( errno ) );
    }
    if( vidin->audio.mode & mode ) {
        vidin->audiomode = mode;
    } else if( vidin->audio.mode > mode ) {
        while( !(vidin->audio.mode & mode) && vidin->audio.mode > mode ) mode <<= 1;
        videoinput_set_audio_mode( vidin, mode );
    } else {
        if( mode != VIDEO_SOUND_MONO ) {
            videoinput_set_audio_mode( vidin, VIDEO_SOUND_MONO );
        } else {
            vidin->audiomode = VIDEO_SOUND_MONO;
        }
    }
}

int videoinput_is_muted( videoinput_t *vidin )
{
    return ( ( vidin->audio.flags & VIDEO_AUDIO_MUTE ) == VIDEO_AUDIO_MUTE );
}

static void videoinput_do_mute( videoinput_t *vidin, int mute )
{
    if( vidin->has_audio && mute != videoinput_is_muted( vidin ) ) {

        if( mute ) {
            vidin->audio.flags |= VIDEO_AUDIO_MUTE;
        } else {
            vidin->audio.flags &= ~VIDEO_AUDIO_MUTE;
        }

        if( ioctl( vidin->grab_fd, VIDIOCSAUDIO, &(vidin->audio) ) < 0 ) {
            fprintf( stderr, "videoinput: Can't set audio settings.  I have no idea what "
                     "might cause this.  Post a bug report with your driver info to "
                     "http://www.sourceforge.net/projects/tvtime/\n" );
            fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
        }
    }
}

void videoinput_mute( videoinput_t *vidin, int mute )
{
    vidin->user_muted = mute;
    videoinput_do_mute( vidin, vidin->user_muted || vidin->muted );
}

int videoinput_get_muted( videoinput_t *vidin )
{
    return vidin->user_muted;
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

        if ( !(vidin->tuner.flags & VIDEO_TUNER_LOW) ) {
            frequency /= 1000; /* switch to MHz */
        }

        vidin->muted = 1;
        videoinput_do_mute( vidin, vidin->user_muted || vidin->muted );
        vidin->cur_tuner_state = TUNER_STATE_SIGNAL_DETECTED;
        vidin->signal_aquire_wait = SIGNAL_AQUIRE_DELAY;
        vidin->signal_recover_wait = 0;

        if( ioctl( vidin->grab_fd, VIDIOCSFREQ, &frequency ) < 0 ) {
            fprintf( stderr, "videoinput: Tuner present, but our request to change "
                             "to frequency %d failed with this error: %s.\n", freqKHz, strerror( errno ) );
            fprintf( stderr, "videoinput: Please file a bug report at http://tvtime.sourceforge.net/\n" );
        }
    } else if( vidin->verbose ) {
        fprintf( stderr, "videoinput: Cannot set tuner freq on a channel without a tuner.\n" );
    }
}

int videoinput_get_tuner_freq( videoinput_t *vidin ) 
{
    unsigned long frequency;

    if (vidin->tuner.tuner > -1) {

        if( ioctl( vidin->grab_fd, VIDIOCGFREQ, &frequency ) < 0 ) {
            fprintf( stderr, "videoinput: Tuner refuses to tell us the current frequency: %s\n",
                     strerror( errno ) );
            fprintf( stderr, "videoinput: Please file a bug report at http://tvtime.sourceforge.net/\n" );
            return 0;
        }

        if ( !(vidin->tuner.flags & VIDEO_TUNER_LOW) ) {
            frequency *= 1000; /* switch from MHz to KHz */
        }

        return frequency/16;
    } else if( vidin->verbose ) {
        fprintf( stderr, "videoinput: cannot get tuner freq on a channel without a tuner\n" );
    }
    return 0;
}

int videoinput_freq_present( videoinput_t *vidin )
{
    if( videoinput_has_tuner( vidin ) ) {
        if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &(vidin->tuner) ) < 0 ) {
            if( vidin->verbose ) {
                fprintf( stderr, "videoinput: Can't detect signal from tuner, ioctl failed: %s\n",
                         strerror( errno ) );
            }
            return 1;
        }
        if( vidin->tuner.signal ) {
            return 1;
        } else {
            return 0;
        }
    }
    return 1;
}

int videoinput_get_input_num( videoinput_t *vidin )
{
    return vidin->grab_chan.channel;
}

const char *videoinput_get_input_name( videoinput_t *vidin )
{
    return vidin->grab_chan.name;
}

/**
 * Finds an appropriate tuner.
 */
static void videoinput_find_and_set_tuner( videoinput_t *vidin )
{
    int found = 0;
    int i;

    vidin->tuner_number = -1;
    for( i = 0; i < vidin->grab_chan.tuners; i++ ) {
        vidin->tuner.tuner = i;

        if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &(vidin->tuner) ) >= 0 ) {
            if( ((vidin->norm == VIDEOINPUT_PAL || vidin->norm == VIDEOINPUT_PAL_NC || vidin->norm == VIDEOINPUT_PAL_N) && vidin->tuner.flags & VIDEO_TUNER_PAL) ||
                (vidin->norm == VIDEOINPUT_SECAM && vidin->tuner.flags & VIDEO_TUNER_SECAM) ||
                ((vidin->norm == VIDEOINPUT_NTSC || vidin->norm == VIDEOINPUT_NTSC_JP || vidin->norm == VIDEOINPUT_PAL_M) && vidin->tuner.flags & VIDEO_TUNER_NTSC) ) {
                found = 1;
                vidin->tuner_number = i;
                break;
            }
        }
    }

    if( found ) {
        int mustchange = 0;

        vidin->tuner.tuner = vidin->tuner_number;

        if( vidin->norm == VIDEOINPUT_PAL || vidin->norm == VIDEOINPUT_PAL_NC || vidin->norm == VIDEOINPUT_PAL_N ) {
            if( vidin->tuner.mode != VIDEO_MODE_PAL ) {
                mustchange = 1;
                vidin->tuner.mode = VIDEO_MODE_PAL;
            }
        } else if( vidin->norm == VIDEOINPUT_SECAM ) {
            if( vidin->tuner.mode != VIDEO_MODE_SECAM ) {
                mustchange = 1;
                vidin->tuner.mode = VIDEO_MODE_SECAM;
            }
        } else {
            if( vidin->tuner.mode != VIDEO_MODE_NTSC ) {
                mustchange = 1;
                vidin->tuner.mode = VIDEO_MODE_NTSC;
            }
        }

        if( mustchange && ( ioctl( vidin->grab_fd, VIDIOCSTUNER, &(vidin->tuner) ) < 0 ) ) {
            fprintf( stderr, "videoinput: Tuner is not in the correct mode, and we can't set it.\n"
                     "            Please file a bug report at http://www.sourceforge.net/projects/tvtime/\n"
                     "            indicating your card, driver and this error message: %s.\n",
                     strerror( errno ) );
        }
    }

    if( vidin->grab_chan.tuners > 0 ) {
        vidin->tuner.tuner = vidin->tuner_number;

        if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &(vidin->tuner) ) < 0 ) {
            fprintf( stderr, "videoinput: Input indicates that tuners are available, but "
                     "driver refuses to give information about them.\n"
                     "videoinput: Please file a bug report at http://www.sourceforge.net/projects/tvtime/ "
                     "and indicate that card and driver you have.\n" );
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
                     vidin->tuner.tuner, vidin->tuner.name, vidin->tuner.rangelow,
                     vidin->tuner.rangehigh, vidin->tuner.signal );

            if( vidin->tuner.flags & VIDEO_TUNER_PAL ) fprintf( stderr, "PAL " );
            if( vidin->tuner.flags & VIDEO_TUNER_NTSC ) fprintf( stderr, "NTSC " );
            if( vidin->tuner.flags & VIDEO_TUNER_SECAM ) fprintf( stderr, "SECAM " );
            if( vidin->tuner.flags & VIDEO_TUNER_LOW ) fprintf( stderr, "LOW " );
            if( vidin->tuner.flags & VIDEO_TUNER_NORM ) fprintf( stderr, "NORM " );
            if( vidin->tuner.flags & VIDEO_TUNER_STEREO_ON ) fprintf( stderr, "STEREO_ON " );
            if( vidin->tuner.flags & VIDEO_TUNER_RDS_ON ) fprintf( stderr, "RDS_ON " );
            if( vidin->tuner.flags & VIDEO_TUNER_MBS_ON ) fprintf( stderr, "MBS_ON" );

            fprintf( stderr, "\nvideoinput: tuner.mode = " );
            switch (vidin->tuner.mode) {
            case VIDEO_MODE_PAL: fprintf( stderr, "PAL" ); break;
            case VIDEO_MODE_NTSC: fprintf( stderr, "NTSC" ); break;
            case VIDEO_MODE_SECAM: fprintf( stderr, "SECAM" ); break;
            case VIDEO_MODE_AUTO: fprintf( stderr, "AUTO" ); break;
            default: fprintf( stderr, "UNDEFINED" ); break;
            }
            fprintf( stderr, "\n");
        }
    }
}


void videoinput_set_input_num( videoinput_t *vidin, int inputnum )
{
    if( inputnum >= vidin->numinputs ) {
        fprintf( stderr, "videoinput: Requested input number %d not valid, "
                 "max is %d.\n", inputnum, vidin->numinputs );
    } else {
        vidin->grab_chan.channel = inputnum;
        if( ioctl( vidin->grab_fd, VIDIOCGCHAN, &(vidin->grab_chan) ) < 0 ) {
            fprintf( stderr, "videoinput: Card refuses to give information on its current input.\n"
                     "Please post a bug report to http://www.sourceforge.net/projects/tvtime/\n"
                     "indicating your card, driver, and this error message: %s.\n", strerror( errno ) );
        } else {
            vidin->grab_chan.channel = inputnum;
            if( vidin->norm == VIDEOINPUT_NTSC ) {
                vidin->grab_chan.norm = VIDEO_MODE_NTSC;
            } else if( vidin->norm == VIDEOINPUT_PAL ) {
                vidin->grab_chan.norm = VIDEO_MODE_PAL;
            } else if( vidin->norm == VIDEOINPUT_SECAM ) {
                vidin->grab_chan.norm = VIDEO_MODE_SECAM;
            } else if( vidin->norm == VIDEOINPUT_PAL_NC ) {
                vidin->grab_chan.norm = 3;
            } else if( vidin->norm == VIDEOINPUT_PAL_M ) {
                vidin->grab_chan.norm = 4;
            } else if( vidin->norm == VIDEOINPUT_PAL_N ) {
                vidin->grab_chan.norm = 5;
            } else if( vidin->norm == VIDEOINPUT_NTSC_JP ) {
                vidin->grab_chan.norm = 6;
            }

            if( ioctl( vidin->grab_fd, VIDIOCSCHAN, &(vidin->grab_chan) ) < 0 ) {
                fprintf( stderr, "videoinput: Card refuses to set the channel.\n"
                         "Please post a bug report to http://www.sourceforge.net/projects/tvtime/\n"
                         "indicating your card, driver, and this error message: %s.\n", strerror( errno ) );
            }

            vidin->grab_chan.channel = inputnum;
            ioctl( vidin->grab_fd, VIDIOCGCHAN, &(vidin->grab_chan) );
        }
    }

    /* Once we've set the input, go look for a tuner. */
    videoinput_find_and_set_tuner( vidin );
}

int videoinput_is_bttv( videoinput_t *vidin )
{
    return vidin->isbttv;
}

void videoinput_reset_default_settings( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        fprintf( stderr, "videoinput: Driver won't tell us picture settings: %s\n",
                 strerror( errno ) );
        fprintf( stderr, "videoinput: Please file a bug report at http://tvtime.sourceforge.net/\n" );
        return;
    }

    if( vidin->verbose ) {
        fprintf( stderr, "videoinput: Current brightness %d, hue %d, "
                         "colour %d, contrast %d.\n",
                 grab_pict.brightness, grab_pict.hue, grab_pict.colour,
                 grab_pict.contrast );
    }

    if( vidin->norm != VIDEOINPUT_NTSC && vidin->norm != VIDEOINPUT_NTSC_JP ) {
        grab_pict.hue = (int) (((((double) DEFAULT_HUE_PAL) + 128.0) / 255.0) * 65535.0);
        grab_pict.brightness = (int) (((((double) DEFAULT_BRIGHTNESS_PAL) + 128.0) / 255.0) * 65535.0);
        grab_pict.contrast = (int) ((((double) DEFAULT_CONTRAST_PAL) / 511.0) * 65535.0);
        grab_pict.colour = (int) ((((double) (DEFAULT_SAT_U_PAL + DEFAULT_SAT_V_PAL)/2) / 511.0) * 65535.0);
    } else {
        grab_pict.hue = (int) (((((double) DEFAULT_HUE_NTSC) + 128.0) / 255.0) * 65535.0);
        grab_pict.brightness = (int) (((((double) DEFAULT_BRIGHTNESS_NTSC) + 128.0) / 255.0) * 65535.0);
        grab_pict.contrast = (int) ((((double) DEFAULT_CONTRAST_NTSC) / 511.0) * 65535.0);
        grab_pict.colour = (int) ((((double) (DEFAULT_SAT_U_NTSC + DEFAULT_SAT_V_NTSC)/2) / 511.0) * 65535.0);
    }
    if( ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
        fprintf( stderr, "videoinput: Driver won't let us set picture settings: %s\n",
                 strerror( errno ) );
        fprintf( stderr, "videoinput: Please file a bug report at http://tvtime.sourceforge.net/\n" );
        return;
    }

    if( vidin->verbose ) {
       fprintf( stderr, "videoinput: Set to brightness %d, hue %d, "
                        "colour %d, contrast %d.\n",
                grab_pict.brightness, grab_pict.hue, grab_pict.colour,
                grab_pict.contrast );
    }

}

void videoinput_delete( videoinput_t *vidin )
{
    /* Mute audio on exit. */
    videoinput_do_mute( vidin, 1 );

    if( vidin->have_mmap ) {
        munmap( vidin->map, vidin->gb_buffers.size );
    }

    close( vidin->grab_fd );

    if( !vidin->have_mmap ) {
        free( vidin->grab_data );
    }

    free( vidin->grab_buf );
    free( vidin );
}

int videoinput_check_for_signal( videoinput_t *vidin, int check_freq_present )
{
    if( videoinput_freq_present( vidin ) || !check_freq_present ) {
        switch( vidin->cur_tuner_state ) {
        case TUNER_STATE_NO_SIGNAL:
        case TUNER_STATE_SIGNAL_LOST:
            vidin->cur_tuner_state = TUNER_STATE_SIGNAL_DETECTED;
            vidin->signal_aquire_wait = SIGNAL_AQUIRE_DELAY;
            vidin->signal_recover_wait = 0;
        case TUNER_STATE_SIGNAL_DETECTED:
            if( vidin->signal_aquire_wait ) {
                vidin->signal_aquire_wait--;
                break;
            } else {
                vidin->cur_tuner_state = TUNER_STATE_HAS_SIGNAL;
            }
        default:
            if( vidin->muted ) {
                vidin->muted = 0;
                videoinput_do_mute( vidin, vidin->user_muted || vidin->muted );
            }
            break;
        }
    } else {
        switch( vidin->cur_tuner_state ) {
        case TUNER_STATE_HAS_SIGNAL:
        case TUNER_STATE_SIGNAL_DETECTED:
            vidin->cur_tuner_state = TUNER_STATE_SIGNAL_LOST;
            vidin->signal_recover_wait = SIGNAL_RECOVER_DELAY;
            vidin->muted = 1;
            videoinput_do_mute( vidin, vidin->user_muted || vidin->muted );
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

