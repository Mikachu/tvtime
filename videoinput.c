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
#include "videoinput.h"
#include "frequencies.h"
#include "mixer.h"

struct videoinput_s
{
    int foo;
};

static int grab_fd;
static int grab_size;
static int numframes;
static int have_mmap;

static int width;
static int height;

/* Wouldn't it be nice if ... */
static int maxbufs = 16;

static unsigned char *grab_data;
static unsigned char *map;
static struct video_mmap *grab_buf;
static struct video_window grab_win;
static struct video_audio audio;
static struct video_channel grab_chan;
static struct video_tuner tuner;

static int curframe = 0;

static struct video_mbuf gb_buffers = { 2*0x151000, 0, {0,0x151000 }};

int videoinput_get_num_frames( videoinput_t *vidin )
{
    return numframes;
}

static int grab_next( void )
{
    int frame = curframe++ % numframes;

    if( ioctl( grab_fd, VIDIOCMCAPTURE, grab_buf + frame ) < 0 ) {
        perror( "VIDIOCMCAPTURE" );
    }

    return frame;
}

static int grab_wait( void )
{
    int frame = curframe % numframes;

    if( ioctl( grab_fd, VIDIOCSYNC, grab_buf + frame ) < 0 ) {
        perror( "VIDIOCSYNC" );
    }

    return frame;
}

unsigned char *videoinput_next_image( videoinput_t *vidin )
{
    int rc;

    for(;;) {
        if( have_mmap ) {
            return map + gb_buffers.offsets[ grab_wait() ];
        } else {
            rc = read( grab_fd, grab_data, grab_size );
            if( grab_size != rc ) {
                fprintf( stderr, "videoinput: grabber read error (rc=%d)\n", rc );
                return NULL;
            } else {
                return grab_data;
            }
        }

        sleep( 1 );
    }
}

void videoinput_free_last_frame( videoinput_t *vidin )
{
    grab_next();
}

void videoinput_free_all_frames( videoinput_t *vidin )
{
    int i;

    for( i = 0; i < numframes; i++ ) grab_next();
}

int videoinput_get_width( videoinput_t *vidin )
{
    return width;
}

int videoinput_get_height( videoinput_t *vidin )
{
    return height;
}

/**
 * Reasonable defaults:
 *
 * v4l_device  : /dev/video0
 * inputnum    : 1
 * max_buffers : 16
 */
videoinput_t *videoinput_new( const char *v4l_device, int inputnum,
                              int capwidth, int capheight, int palmode )
{
    videoinput_t *vidin = (videoinput_t *) malloc( sizeof( videoinput_t ) );
    struct video_capability grab_cap;
    struct video_picture grab_pict;
    int i;

    if( !vidin ) return 0;

    width = capwidth;
    height = capheight;

    grab_fd = open( v4l_device, O_RDWR );
    if( grab_fd < 0 ) {
        fprintf( stderr, "videoinput: Can't open %s: %s\n", v4l_device, strerror( errno ) );
        free( vidin );
        return 0;
    }

    if( ioctl( grab_fd, VIDIOCGCAP, &grab_cap ) < 0 ) {
        fprintf( stderr, "videoinput: No v4l device (%s).\n", v4l_device );
        /* XXX */
        return 0;
    }

    grab_chan.channel = inputnum;
    if( ioctl( grab_fd, VIDIOCGCHAN, &grab_chan ) < 0 ) {
        perror( "ioctl VIDIOCGCHAN" );
        return 0;
    }

    grab_chan.channel = inputnum;
    grab_chan.norm = palmode ? VIDEO_MODE_PAL : VIDEO_MODE_NTSC;

    if( ioctl( grab_fd, VIDIOCSCHAN, &grab_chan ) < 0 ) {
        perror( "ioctl VIDIOCSCHAN" );
        return 0;
    }

    if( ioctl( grab_fd, VIDIOCGMBUF, &gb_buffers ) < 0 ) {
        perror( "VIDIOCGMBUF" );
        return 0;
    }

    if( ioctl( grab_fd, VIDIOCGAUDIO, &audio ) < 0 ) {
        perror( "VIDIOCGAUDIO" );
        return 0;
    }

    audio.flags &= ~VIDEO_AUDIO_MUTE;

    if( ioctl( grab_fd, VIDIOCSAUDIO, &audio ) < 0 ) {
        perror( "VIDIOCGAUDIO" );
        return 0;
    }

    if( grab_chan.tuners > 0 ) {
        tuner.tuner = 0;
        if( ioctl( grab_fd, VIDIOCGTUNER, &tuner ) < 0 ) {
            perror( "ioctl VIDIOCGTUNER" );
            return 0;
        }
        fprintf( stderr, "tuner.tuner = %d\n"
                         "tuner.name = %s\n"
                         "tuner.rangelow = %ld\n"
                         "tuner.rangehigh = %ld\n"
                         "tuner.signal = %d\n"
                         "tuner.flags = ",
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

        fprintf( stderr, "\ntuner.mode = " );
        switch (tuner.mode) {
        case VIDEO_MODE_PAL: fprintf( stderr, "PAL" ); break;
        case VIDEO_MODE_NTSC: fprintf( stderr, "NTSC" ); break;
        case VIDEO_MODE_SECAM: fprintf( stderr, "SECAM" ); break;
        case VIDEO_MODE_AUTO: fprintf( stderr, "AUTO" ); break;
        default: fprintf( stderr, "UNDEFINED" ); break;
        }
        fprintf( stderr, "\n");
    } else {
        tuner.tuner = -1;
        fprintf( stderr, "videoinput: Channel %d has no tuner.\n", grab_chan.channel );
    }


    if( ioctl( grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        perror( "ioctl VIDIOCGPICT" );
        return 0;
    }
    fprintf( stderr, "videoinput: Current brightness %d, hue %d, colour %d, contrast %d.\n",
             grab_pict.brightness, grab_pict.hue, grab_pict.colour,
             grab_pict.contrast );

    if( palmode ) {
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
    if( ioctl( grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
        perror( "ioctl VIDIOCSPICT" );
        return 0;
    }
    fprintf( stderr, "videoinput: Set to brightness %d, hue %d, colour %d, contrast %d.\n",
             grab_pict.brightness, grab_pict.hue, grab_pict.colour,
             grab_pict.contrast );

    numframes = gb_buffers.frames;
    if( maxbufs < numframes ) numframes = maxbufs;
    grab_buf = (struct video_mmap *) malloc( sizeof( struct video_mmap ) * numframes );

    /* Try to setup mmap-based capture. */
    for( i = 0; i < numframes; i++ ) {
        grab_buf[ i ].format = VIDEO_PALETTE_YUV422P; /* Y'CbCr 4:2:0 Planar */
        grab_buf[ i ].frame = i;
        grab_buf[ i ].width = width;
        grab_buf[ i ].height = height;
    }

    map = (unsigned char *) mmap( 0, gb_buffers.size, PROT_READ|PROT_WRITE, MAP_SHARED, grab_fd, 0 );
    if( (int) map != -1 ) {
        have_mmap = 1;
        //for( i = 0; i < numframes - 1; i++ ) {
            //grab_wait();
        //}
        //for( i = 0; i < numframes - 2; i++ ) {
            //grab_next();
        //}

        return vidin;
    }

    /* Fallback to read(). */
    fprintf( stderr, "videoinput: No mmap support available, using read().\n" );
    have_mmap = 0;

    grab_pict.depth = 16;
    grab_pict.palette = VIDEO_PALETTE_YUV422P;
    if( ioctl( grab_fd, VIDIOCSPICT, &grab_pict ) < 0 ) {
        perror( "ioctl VIDIOCSPICT" );
        return 0;
    }
    if( ioctl( grab_fd, VIDIOCGPICT, &grab_pict ) < 0 ) {
        perror("ioctl VIDIOCGPICT");
        return 0;
    }
    fprintf( stderr, "\n\nbright %d, hue %d, colour %d, contrast %d, whiteness %d\n\n",
             grab_pict.brightness, grab_pict.hue, grab_pict.colour,
             grab_pict.contrast, grab_pict.whiteness );

    memset( &grab_win, 0, sizeof( struct video_window ) );
    grab_win.width = width;
    grab_win.height = height;

    if( ioctl( grab_fd, VIDIOCSWIN, &grab_win ) < 0 ) {
        perror( "ioctl VIDIOCSWIN" );
        return 0;
    }

    if( ioctl( grab_fd, VIDIOCGWIN, &grab_win ) < 0 ) {
        perror( "ioctl VIDIOCGWIN" );
        return 0;
    }

    grab_size = grab_win.width * grab_win.height * 3;
    grab_data = (unsigned char *) malloc( grab_size );

    return vidin;
}

void videoinput_set_tuner(int tuner_number, int mode)
{
    if (tuner.tuner > -1) {
        if (grab_chan.tuners <= tuner_number) {
            fprintf( stderr, "videoinput: tuner %d is not a valid tuner for channel %d\n",
                     tuner_number, grab_chan.channel );
            return;
        }

        if ( (mode == VIDEO_MODE_PAL && !(tuner.flags & VIDEO_TUNER_PAL)) ||
             (mode == VIDEO_MODE_NTSC && !(tuner.flags & VIDEO_TUNER_NTSC)) ||
             (mode == VIDEO_MODE_SECAM && !(tuner.flags & VIDEO_TUNER_SECAM))
            ) {
            fprintf( stderr, "videoinput: invalid tuner mode\n" );
            return;
        }

        tuner.tuner = tuner_number;
        tuner.mode = mode;
        if( ioctl( grab_fd, VIDIOCSTUNER, &tuner ) < 0 ) {
            perror( "ioctl VIDIOCSTUNER" );
            return;
        }
    } else {
        fprintf( stderr, "videoinput: cannot set tuner on a channel without a tuner\n" );
    }
}

/* freqKHz is in KHz (duh) */
void videoinput_set_tuner_freq( int freqKHz )
{
    unsigned long frequency = freqKHz;

    if (tuner.tuner > -1) {
        if (frequency < 0) return;

        if ( !(tuner.flags & VIDEO_TUNER_LOW) ) {
            frequency /= 1000; // switch to MHz
        }

        frequency *= 16;

        if( ioctl( grab_fd, VIDIOCSFREQ, &frequency ) < 0 ) {
            perror( "ioctl VIDIOCSFREQ" );
            return;
        }
    } else {
        fprintf( stderr, "videoinput: cannot set tuner freq on a channel without a tuner\n" );
    }
}

void videoinput_mute( int mute ) 
{
    if( mute ) {
        mixer_set_volume( -100 );
    } else {
        mixer_set_volume( 100 );
    }
}

int videoinput_get_tuner_freq( void ) 
{
    unsigned long frequency;

    if (tuner.tuner > -1) {

        if( ioctl( grab_fd, VIDIOCGFREQ, &frequency ) < 0 ) {
            perror( "ioctl VIDIOCSFREQ" );
            return 0;
        }

        if ( !(tuner.flags & VIDEO_TUNER_LOW) ) {
            frequency *= 1000; // switch from MHz to KHz
        }

        return frequency/16;
    } else {
        fprintf( stderr, "videoinput: cannot get tuner freq on a channel without a tuner\n" );
        return 0;
    }
}

void videoinput_delete( videoinput_t *vidin )
{

    if( ioctl( grab_fd, VIDIOCGAUDIO, &audio ) < 0 ) {
        perror( "VIDIOCGAUDIO" );
        return;
    }

    audio.flags |= VIDEO_AUDIO_MUTE;

    if( ioctl( grab_fd, VIDIOCSAUDIO, &audio ) < 0 ) {
        perror( "VIDIOCSAUDIO" );
        return;
    }

    if( have_mmap ) {
        munmap( map, gb_buffers.size );
    }

    close( grab_fd );

    if( !have_mmap ) {
        free( grab_data );
    }

    free( grab_buf );
    free( vidin );
}

