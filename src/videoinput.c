/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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
#include "videoinput.h"
#include "frequencies.h"
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

    unsigned char *grab_data;
    unsigned char *map;
    struct video_mmap *grab_buf;
    struct video_window grab_win;
    struct video_audio audio;
    struct video_channel grab_chan;
    struct video_picture grab_pict;
    int curframe;

    int tuner_number;
    struct video_tuner tuner;

    struct video_mbuf gb_buffers;

    int verbose;
};


static void free_frame( videoinput_t *vidin, int frameid )
{
    if( ioctl( vidin->grab_fd, VIDIOCMCAPTURE, vidin->grab_buf + frameid ) < 0 ) {
        fprintf( stderr, "videoinput: Can't free frame %d: %s\n", frameid, strerror( errno ) );
    }
}

static void wait_for_frame( videoinput_t *vidin, int frameid )
{
    if( ioctl( vidin->grab_fd, VIDIOCSYNC, vidin->grab_buf + frameid ) < 0 ) {
        fprintf( stderr, "videoinput: Can't wait for frame %d: %s\n", frameid, strerror( errno ) );
    }
}

unsigned char *videoinput_next_frame( videoinput_t *vidin, int *frameid )
{
    int rc;

    if( vidin->have_mmap ) {
        unsigned char *cur;
        wait_for_frame( vidin, vidin->curframe );
        cur = vidin->map + vidin->gb_buffers.offsets[ vidin->curframe ];
        *frameid = vidin->curframe;
        vidin->curframe = ( vidin->curframe + 1 ) % vidin->numframes;
        return cur;
    } else {
        rc = read( vidin->grab_fd, vidin->grab_data, vidin->grab_size );
        if( vidin->grab_size != rc ) {
            fprintf( stderr, "videoinput: grabber read error (rc=%d)\n", rc );
            return 0;
        } else {
            return vidin->grab_data;
        }
    }
}

void videoinput_free_frame( videoinput_t *vidin, int frameid )
{
    free_frame( vidin, frameid );
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

int videoinput_get_numframes( videoinput_t *vidin )
{
    return vidin->numframes;
}

int videoinput_get_num_inputs( videoinput_t *vidin )
{
    return vidin->numinputs;
}

static void videoinput_get_and_print_tuner_info( videoinput_t *vidin )
{
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
            fprintf( stderr, "tuner.tuner = %d\n"
                             "tuner.name = %s\n"
                             "tuner.rangelow = %ld\n"
                             "tuner.rangehigh = %ld\n"
                             "tuner.signal = %d\n"
                             "tuner.flags = ",
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

            fprintf( stderr, "\ntuner.mode = " );
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

/**
 * Finds an appropriate tuner.
 */
void videoinput_find_and_set_tuner( videoinput_t *vidin )
{
    int found = 0;
    int i;

    vidin->tuner_number = -1;
    for( i = 0; i < vidin->grab_chan.tuners; i++ ) {
        vidin->tuner.tuner = i;

        if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &(vidin->tuner) ) >= 0 ) {
            if( ((vidin->norm == VIDEOINPUT_PAL || vidin->norm == VIDEOINPUT_PAL_NC || vidin->norm == VIDEOINPUT_PAL_M || vidin->norm == VIDEOINPUT_PAL_N) && vidin->tuner.flags & VIDEO_TUNER_PAL) ||
                (vidin->norm == VIDEOINPUT_SECAM && vidin->tuner.flags & VIDEO_TUNER_SECAM) ||
                ((vidin->norm == VIDEOINPUT_NTSC || vidin->norm == VIDEOINPUT_NTSC_JP) && vidin->tuner.flags & VIDEO_TUNER_NTSC) ) {
                found = 1;
                vidin->tuner_number = i;
                break;
            }
        }
    }

    if( found ) {
        vidin->tuner.tuner = vidin->tuner_number;

        if( vidin->norm == VIDEOINPUT_PAL || vidin->norm == VIDEOINPUT_PAL_NC || vidin->norm == VIDEOINPUT_PAL_M || vidin->norm == VIDEOINPUT_PAL_N ) {
            vidin->tuner.mode = VIDEO_MODE_PAL;
        } else if( vidin->norm == VIDEOINPUT_SECAM ) {
            vidin->tuner.mode = VIDEO_MODE_SECAM;
        } else {
            vidin->tuner.mode = VIDEO_MODE_NTSC;
        }

        if( ioctl( vidin->grab_fd, VIDIOCSTUNER, &(vidin->tuner) ) < 0 ) {
            fprintf( stderr, "videoinput: Can't set tuner mode.  Please file a bug report "
                     "at http://www.sourceforge.net/projects/tvtime/ indicating your card, "
                     "driver and this error message: %s.\n", strerror( errno ) );
        }
    }

    videoinput_get_and_print_tuner_info( vidin );
}


videoinput_t *videoinput_new( const char *v4l_device, int capwidth,
                              int norm, int verbose )
{
    videoinput_t *vidin = (videoinput_t *) malloc( sizeof( videoinput_t ) );
    struct video_capability grab_cap;
    struct video_picture grab_pict;
    int i;

    if( !vidin ) {
        fprintf( stderr, "videoinput: Can't allocate memory.\n" );
        return 0;
    }

    vidin->curframe = 0;
    vidin->verbose = verbose;
    vidin->norm = norm;
    vidin->height = ( vidin->norm == VIDEOINPUT_NTSC || vidin->norm == VIDEOINPUT_NTSC_JP ) ? 480 : 576;

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

    /* The capabilities should tell us how many inputs this card has. */
    vidin->numinputs = grab_cap.channels;
    if( vidin->numinputs == 0 ) {
        fprintf( stderr, "videoinput: No input channels available on "
                 "video4linux device '%s'.\n", v4l_device );
        close( vidin->grab_fd );
        free( vidin );
        return 0;
    }


    /* On initialization, set to input 0.  This is just to start things up. */
    videoinput_set_input_num( vidin, 0 );


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



    /* Upon creation, unmute the audio from the card. */
    if( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &(vidin->audio) ) < 0 ) {
        fprintf( stderr, "videoinput: Can't get audio settings, no audio on this card?\n" );
        fprintf( stderr, "videoinput: Please post a bug report on "
                 "http://www.sourceforge.net/projects/tvtime/ and indicate which card/driver you have.\n" );
        fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
    } else {
        vidin->audio.flags &= ~VIDEO_AUDIO_MUTE;
        if( ioctl( vidin->grab_fd, VIDIOCSAUDIO, &(vidin->audio) ) < 0 ) {
            fprintf( stderr, "videoinput: Can't set audio settings.  I have no idea what "
                     "might cause this.  Post a bug report with your driver info to "
                     "http://www.sourceforge.net/projects/tvtime/\n" );
            fprintf( stderr, "videoinput: Include this error: '%s'\n", strerror( errno ) );
        }
    }



    /* This code should all be cleaned up.... */
    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        perror( "ioctl VIDIOCGPICT" );
        return 0;
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
        perror( "ioctl VIDIOCSPICT" );
        return 0;
    }

    if( vidin->verbose ) {
       fprintf( stderr, "videoinput: Set to brightness %d, hue %d, "
                        "colour %d, contrast %d.\n",
                grab_pict.brightness, grab_pict.hue, grab_pict.colour,
                grab_pict.contrast );
    }


    /* Try to set up mmap-based capture. */
    if( ioctl( vidin->grab_fd, VIDIOCGMBUF, &(vidin->gb_buffers) ) < 0 ) {
        fprintf( stderr, "videoinput: Can't get capture buffer properties.  No mmap support?\n"
                 "Please post a bug report about this to http://www.sourceforge.net/projects/tvtime/\n"
                 "with your card, driver and this error message: %s.\n", strerror( errno ) );
        fprintf( stderr, "videoinput: Will try to fall back to (untested) read()-based capture..\n" );
    } else {
        /* If we got more frames than we asked for, well, let's ignore it for now. */
        vidin->numframes = vidin->gb_buffers.frames;

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

    /* Set the format using the SPICT ioctl. */
    grab_pict.depth = 16;
    grab_pict.palette = VIDEO_PALETTE_YUV422;
    if( ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
        perror( "ioctl VIDIOCSPICT" );
        return 0;
    }

    /* Set and get the window settings. */
    memset( &(vidin->grab_win), 0, sizeof( struct video_window ) );
    vidin->grab_win.width = vidin->width;
    vidin->grab_win.height = vidin->height;
    if( ioctl( vidin->grab_fd, VIDIOCSWIN, &(vidin->grab_win) ) < 0 ) {
        perror( "ioctl VIDIOCSWIN" );
        return 0;
    }
    if( ioctl( vidin->grab_fd, VIDIOCGWIN, &(vidin->grab_win) ) < 0 ) {
        perror( "ioctl VIDIOCGWIN" );
        return 0;
    }
    vidin->grab_size = vidin->grab_win.width * vidin->grab_win.height * 3;
    vidin->grab_data = (unsigned char *) malloc( vidin->grab_size );

    return vidin;
}

int videoinput_get_hue( videoinput_t *vidin )
{
    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &vidin->grab_pict ) < 0 ) {
        return 0;
    }
    return (int) ((((double) vidin->grab_pict.hue / 65535.0) * 100.0) + 0.5);
}

void videoinput_set_hue( videoinput_t *vidin, int newhue )
{
    vidin->grab_pict.hue = (int) (((((double) newhue) / 100.0) * 65535.0) + 0.5);
    ioctl( vidin->grab_fd, VIDIOCSPICT, &vidin->grab_pict );
}

void videoinput_set_hue_relative( videoinput_t *vidin, int offset )
{
    struct video_picture grab_pict;
    int newhue;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return;
    }

    newhue = (int) ((((double) grab_pict.hue / 65535.0) * 100.0) + 0.5);
    newhue += offset;
    if( newhue > 100 ) newhue = 100;
    if( newhue <   0 ) newhue = 0;
    grab_pict.hue = (int) (((((double) newhue) / 100.0) * 65535.0) + 0.5);
    ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
}

int videoinput_get_brightness( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    }
    return (int) ((((double) grab_pict.brightness / 65535.0) * 100.0) + 0.5);
}

void videoinput_set_brightness_relative( videoinput_t *vidin, int offset )
{
    struct video_picture grab_pict;
    int newbright;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return;
    }

    newbright = (int) ((((double) grab_pict.brightness / 65535.0) * 100.0) + 0.5);
    newbright += offset;
    if( newbright > 100 ) newbright = 100;
    if( newbright < 0 ) newbright = 0;
    grab_pict.brightness = (int) (((((double) newbright) / 100.0) * 65535.0) + 0.5);
    ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
}

int videoinput_get_contrast( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    }
    return (int) ((((double) grab_pict.contrast / 65535.0) * 100.0) + 0.5);
}

void videoinput_set_contrast_relative( videoinput_t *vidin, int offset )
{
    struct video_picture grab_pict;
    int newcont;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return;
    }

    newcont = (int) ((((double) grab_pict.contrast / 65535.0) * 100.0) + 0.5);
    newcont += offset;
    if( newcont > 100 ) newcont = 100;
    if( newcont < 0 ) newcont = 0;
    grab_pict.contrast = (int) (((((double) newcont) / 100.0) * 65535.0) + 0.5);
    ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
}

int videoinput_get_colour( videoinput_t *vidin )
{
    struct video_picture grab_pict;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return 0;
    }
    return (int) ((((double) grab_pict.colour / 65535.0) * 100.0) + 0.5);
}

void videoinput_set_colour_relative( videoinput_t *vidin, int offset )
{
    struct video_picture grab_pict;
    int newcolour;

    if( ioctl( vidin->grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        return;
    }

    newcolour = (int) ((((double) grab_pict.colour / 65535.0) * 100.0) + 0.5);
    newcolour += offset;
    if( newcolour > 100 ) newcolour = 100;
    if( newcolour < 0 ) newcolour = 0;
    grab_pict.colour = (int) (((((double) newcolour) / 100.0) * 65535.0) + 0.5);
    ioctl( vidin->grab_fd, VIDIOCSPICT, &grab_pict );
}


int videoinput_has_tuner( videoinput_t *vidin )
{
    return (vidin->tuner_number > -1);
}

/* freqKHz is in KHz (duh) */
void videoinput_set_tuner_freq( videoinput_t *vidin, int freqKHz )
{
    unsigned long frequency = freqKHz;
    int mute;

    if (vidin->tuner.tuner > -1) {
        if (frequency < 0) return;

        if ( !(vidin->tuner.flags & VIDEO_TUNER_LOW) ) {
            frequency /= 1000; /* switch to MHz */
        }

        frequency *= 16;

        mute = mixer_conditional_mute();

        if( ioctl( vidin->grab_fd, VIDIOCSFREQ, &frequency ) < 0 ) {
            perror( "ioctl VIDIOCSFREQ" );
        }

        usleep( 20000 );

        if( mute ) {
            mixer_mute( 0 );
        }

    } else if( vidin->verbose ) {
        fprintf( stderr, "videoinput: cannot set tuner freq on a channel without a tuner\n" );
    }
}

int videoinput_get_tuner_freq( videoinput_t *vidin ) 
{
    unsigned long frequency;

    if (vidin->tuner.tuner > -1) {

        if( ioctl( vidin->grab_fd, VIDIOCGFREQ, &frequency ) < 0 ) {
            perror( "ioctl VIDIOCSFREQ" );
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
    if (vidin->tuner.tuner > -1) {

        usleep( 100000 );

        if( ioctl( vidin->grab_fd, VIDIOCGTUNER, &(vidin->tuner) ) < 0 ) {
            perror( "ioctl VIDIOCGTUNER" );
            return 0;
        }

        if( vidin->verbose ) {
            fprintf( stderr, "videoinput: strength %d\n", vidin->tuner.signal );
        }

        return( vidin->tuner.signal );
    }

    return 0;
}

int videoinput_get_input_num( videoinput_t *vidin )
{
    return vidin->grab_chan.channel;
}

const char *videoinput_get_input_name( videoinput_t *vidin )
{
    return vidin->grab_chan.name;
}

void videoinput_set_input_num( videoinput_t *vidin, int inputnum )
{
    if( inputnum >= vidin->numinputs ) {
        fprintf( stderr, "videoinput: Requested input number %d not valid, "
                 "max is %d.\n", inputnum, vidin->numinputs );
    } else {
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

            ioctl( vidin->grab_fd, VIDIOCGCHAN, &(vidin->grab_chan) );
        }
    }

    /* Once we've set the input, go look for a tuner. */
    videoinput_find_and_set_tuner( vidin );
}

void videoinput_delete( videoinput_t *vidin )
{

    if( ioctl( vidin->grab_fd, VIDIOCGAUDIO, &(vidin->audio) ) < 0 ) {
        perror( "VIDIOCGAUDIO" );
        return;
    }

    vidin->audio.flags |= VIDEO_AUDIO_MUTE;

    if( ioctl( vidin->grab_fd, VIDIOCSAUDIO, &(vidin->audio) ) < 0 ) {
        perror( "VIDIOCSAUDIO" );
        return;
    }

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

