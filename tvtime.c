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
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "frequencies.h"
#include "videoinput.h"
#include "rtctimer.h"
#include "sdloutput.h"
#include "videotools.h"
#include "mixer.h"

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-aps] [-w <width>] [-o <mode>] "
                     "[-d <device> [-i <input>] [-n <norm>] "
                     "[-f <frequencies>] [-t <tuner>]\n"
                     "\t-a\t16:9 mode.\n"
                     "\t-w\tOutput window width, defaults to 800.\n"
                     "\t-o\tOutput mode: '422' (default) or '420'.\n"
                     "\t-d\tvideo4linux device (defaults to /dev/video0).\n"
                     "\t-i\tvideo4linux input number (defaults to 0).\n"
                     "\t-l\tLuma correction value (defaults to 1.0).\n"
                     "\t-n\tThe mode to set the tuner to: PAL, NTSC or SECAM.\n"
                     "\t  \t(defaults to NTSC)\n"
                     "\t-f\tThe channels you are receiving with the tuner\n"
                     "\t  \t(defaults to us-cable).\n"
                     "\t  \tValid values are:\n"
                     "\t  \t\tus-bcast\n"
                     "\t  \t\tus-cable\n"
                     "\t  \t\tus-cable-hrc\n"
                     "\t  \t\tjapan-bcast\n"
                     "\t  \t\tjapan-cable\n"
                     "\t  \t\teurope-west\n"
                     "\t  \t\teurope-east\n"
                     "\t  \t\titaly\n"
                     "\t  \t\tnewzealand\n"
                     "\t  \t\taustralia\n"
                     "\t  \t\tireland\n"
                     "\t  \t\tfrance\n"
                     "\t  \t\tchina-bcast\n"
                     "\t  \t\tsouthafrica\n"
                     "\t  \t\targentina\n"
                     "\t  \t\tcanada-cable\n"
                     "\t  \t\taustralia-optus\n"
                     "\t-t\tThe tuner number for this input (defaults to 0).\n"
                     "\n\tSee the README for more details.\n",
                     argv[ 0 ] );
}

int main( int argc, char **argv )
{
    double luma_correction = 1.0;
    video_correction_t *vc;
    videoinput_t *vidin;
    rtctimer_t *rtctimer;
    char v4ldev[ 256 ];
    char norm[ 7 ];
    char freq[ 20 ];
    int width, height;
    int output420 = 0;
    int palmode = 0;
    int inputnum = 0;
    int aspect = 0;
    int outputwidth = 800;
    int fieldtime;
    int tuner_number = 0;
    int secam = 0;
    int c;
    long last_chan_time = 0;
    int volume;
    int muted = 0;

    /* Default device. */
    strcpy( v4ldev, "/dev/video0" );
    
    /* Default norm */
    strcpy( norm, "ntsc" );

    /* Default freq */
    strcpy( freq, "us-cable" );

    while( (c = getopt( argc, argv, "hw:ao:d:i:l:n:f:t:" )) != -1 ) {
        switch( c ) {
        case 'w': outputwidth = atoi( optarg ); break;
        case 'a': aspect = 1; break;
        case 'o': if( strcmp( "420", optarg ) == 0 ) output420 = 1; break;
        case 'd': strncpy( v4ldev, optarg, 250 ); break;
        case 'i': inputnum = atoi( optarg ); break;
        case 'l': luma_correction = atof( optarg ); break;
        case 'n': strncpy( norm, optarg, 6 ); break;
        case 'f': strncpy( freq, optarg, 17 ); break;
        case 't': tuner_number = atoi( optarg ); break;
        default:
            print_usage( argv );
            return 1;
        }
    }

    if( norm && !strcasecmp( norm, "pal" ) ) {
        palmode = 1;
    } else if( norm && !strcasecmp( norm, "secam" ) ) {
        palmode = 1;
        secam = 1;
    } else {
        /* Only allow NTSC otherwise. */
        palmode = 0;
    }

    /* Field display in microseconds. */
    if( palmode ) {
        fieldtime = 20000;
    } else {
        fieldtime = 16683;
    }

    vidin = videoinput_new( v4ldev, inputnum, 720, palmode ? 576 : 480, palmode );
    if( !vidin ) {
        fprintf( stderr, "tvtime: Can't open video input, "
                         "maybe try a different device?\n" );
        return 1;
    }

    /* Setup the tuner now */
    if( palmode ) {
        if( secam ) {
            videoinput_set_tuner( tuner_number, VIDEO_MODE_SECAM );
        } else {
            videoinput_set_tuner( tuner_number, VIDEO_MODE_PAL );
        }
    } else {
        videoinput_set_tuner( tuner_number, VIDEO_MODE_NTSC );
    }

    /* Set to the first channel in our frequency list */
    if( !frequencies_set_chanlist( freq ? freq : "us-cable" ) ) {
        fprintf( stderr, "tvtime: Invalid channel/frequency region." );

        return 1;
    } else {
        if( frequencies_find_current_index() == -1 ) {
            videoinput_set_tuner_freq( chanlist[ chanindex ].freq );
        }
        fprintf( stderr, "tvtime: Changing to channel %s\n", 
                 chanlist[ chanindex ].name );
    }

    if( setpriority( 0, 0, -19 ) < 0 ) {
        fprintf( stderr, "tvtime: can't renice to -19\n" );
    }
    if( mlockall( MCL_CURRENT | MCL_FUTURE ) ) {
        fprintf( stderr, "tvtime: Warning, mlockall() failed.\n" );
    }

    rtctimer = rtctimer_new();
    if( !rtctimer ) {
        fprintf( stderr, "tvtime: Can't open /dev/rtc "
                         "(for my wakeup call).\n" );
        return 1;
    }

    if( !rtctimer_set_interval( rtctimer, 1024 ) ) {
        fprintf( stderr, "tvtime: Can't set 1024hz from /dev/rtc (need root).\n" );
        return 1;
    }
    rtctimer_start_clock( rtctimer );

    if( !set_realtime_priority( 0 ) ) {
        fprintf( stderr, "tvtime: Can't set realtime priority (need root).\n" );
        return 1;
    }

    width = videoinput_get_width( vidin );
    height = videoinput_get_height( vidin );

    /* Announce our resolution. */
    fprintf( stderr, "tvtime: Input is %dx%d.\n", width, height );

    /* Initialize output. */
    if( !sdl_init( width, height, output420, outputwidth, aspect ) ) {
        fprintf( stderr, "tvtime: Can't open video output.\n" );
        return 1;
    }
    fprintf( stderr, "tvtime: Output to Y'CbCr at %s sampling.\n",
             output420 ? "4:2:0" : "4:2:2" );

    vc = video_correction_new();
    if( !vc ) {
        fprintf( stderr, "tvtime: Can't initialize luma and chroma "
                         "correction tables.\n" );
        return 1;
    }

    if( luma_correction < 0.0 || luma_correction > 10.0 ) {
        fprintf( stderr, "tvtime: Luma correction value out of range. "
                         "Using 1.0.\n" );
        luma_correction = 1.0;
    }
    fprintf( stderr, "tvtime: Luma correction value: %.1f\n",
             luma_correction );
    video_correction_set_luma_power( vc, luma_correction );

    /* Setup the video input. */
    videoinput_free_all_frames( vidin );

    for(;;) {
        unsigned char *curluma;
        unsigned char *curcb422;
        unsigned char *curcr422;
        struct timeval firstfield;
        int commands;

        commands = sdl_poll_events();
        if( commands & TVTIME_QUIT ) {
            break;
        }
        if( commands & TVTIME_LUMA_UP ) {
            if( luma_correction < 10.0 ) {
                luma_correction += 0.1;
                fprintf( stderr, "tvtime: Luma correction value: %.1f\n",
                         luma_correction );
                video_correction_set_luma_power( vc, luma_correction );
            }
        }
        if( commands & TVTIME_LUMA_DOWN ) {
            if( luma_correction > 0.0 ) {
                luma_correction -= 0.1;
                fprintf( stderr, "tvtime: Luma correction value: %.1f\n",
                         luma_correction );
                video_correction_set_luma_power( vc, luma_correction );
            }
        }
        if( commands & TVTIME_CHANNEL_UP ) {
            chanindex++;
            chanindex %= chancount;
            last_chan_time = 0;
            videoinput_set_tuner_freq( chanlist[ chanindex ].freq );
            fprintf( stderr, "tvtime: Changing to channel %s\n", 
                     chanlist[ chanindex ].name );
        }
        if( commands & TVTIME_CHANNEL_DOWN ) {
            chanindex--;
            chanindex %= chancount;
            last_chan_time = 0;
            videoinput_set_tuner_freq( chanlist[ chanindex ].freq );

            fprintf( stderr, "tvtime: Changing to channel %s\n", 
                     chanlist[ chanindex ].name );
        }
        if( commands & TVTIME_MIXER_UP ) {
            mixer_set_volume(1);
            volume = mixer_get_volume();
        }
        if( commands & TVTIME_MIXER_DOWN ) {
            mixer_set_volume(-1);
            volume = mixer_get_volume();
        }
        if( commands & TVTIME_MIXER_MUTE ) {
            muted = ~muted;
            videoinput_mute(muted);
        }
        if( commands & TVTIME_DIGIT ) {
            int digit = 0;
            if( commands & TVTIME_KP0 ) {
                digit = 0;
            }
            if( commands & TVTIME_KP1 ) {
                digit = 1;
            }
            if( commands & TVTIME_KP2 ) {
                digit = 2;
            }
            if( commands & TVTIME_KP3 ) {
                digit = 3;
            }
            if( commands & TVTIME_KP4 ) {
                digit = 4;
            }
            if( commands & TVTIME_KP5 ) {
                digit = 5;
            }
            if( commands & TVTIME_KP6 ) {
                digit = 6;
            }
            if( commands & TVTIME_KP7 ) {
                digit = 7;
            }
            if( commands & TVTIME_KP8 ) {
                digit = 8;
            }
            if( commands & TVTIME_KP9 ) {
                digit = 9;
            }

            if( (long)time(NULL) - last_chan_time > 500 ) {
                if( digit == 0 ) {
                    last_chan_time = 0;
                    chanindex = 0;
                } else {
                    last_chan_time = time(NULL);
                    chanindex = digit - 1;

                    videoinput_set_tuner_freq( chanlist[ chanindex ].freq);

                    fprintf( stderr, "tvtime: Changing to channel %s\n",
                             chanlist[ chanindex ].name );

                }
            } else {
                last_chan_time = time(NULL);
                if( ((chanindex+1)*10 + digit - 1) >= chancount ) {
                    chanindex = digit - 1;
                } else {
                    chanindex = (chanindex+1)*10 + digit - 1;
                }

                videoinput_set_tuner_freq( chanlist[ chanindex ].freq);

                fprintf( stderr, "tvtime: Changing to channel %s\n", 
                         chanlist[ chanindex ].name );                    
            }
        }


        /* Aquire the next frame. */
        curluma  = videoinput_next_image( vidin );
        curcb422 = curluma  + ( width   * height );
        curcr422 = curcb422 + ( width/2 * height );

        /* Build our frame, pivot on the top field. */
        if( output420 ) {
            luma_plane_field_to_frame( sdl_get_output(), curluma,
                                       width, height, width*2, 0 );
            chroma_plane_field_to_frame( sdl_get_cb(), curcb422,
                                         width/2, height/2, width*2, 0 );
            chroma_plane_field_to_frame( sdl_get_cr(), curcr422,
                                         width/2, height/2, width*2, 0 );
        } else {
            video_correction_planar422_field_to_packed422_frame( vc,
                                                                 sdl_get_output(),
                                                                 curluma,
                                                                 curcb422,
                                                                 curcr422,
                                                                 0, width * 2, width, width, height );
        }

        /* Display. */
        gettimeofday( &firstfield, 0 );
        sdl_show_frame();

        /* Build our frame, pivot on the top field. */
        if( output420 ) {
            luma_plane_field_to_frame( sdl_get_output(), curluma + width,
                                       width, height, width*2, 1 );
            chroma_plane_field_to_frame( sdl_get_cb(), curcb422,
                                         width/2, height/2, width*2, 1 );
            chroma_plane_field_to_frame( sdl_get_cr(), curcr422,
                                         width/2, height/2, width*2, 1 );
        } else {
            video_correction_planar422_field_to_packed422_frame( vc,
                                                                 sdl_get_output(),
                                                                 curluma + width,
                                                                 curcb422 + (width / 2),
                                                                 curcr422 + (width / 2),
                                                                 1, width * 2, width, width, height );
        }

        /* We're done with the input now. */
        videoinput_free_last_frame( vidin );

        /* Wait for next field time. */
        for(;;) {
            struct timeval secondfield;
            gettimeofday( &secondfield, 0 );

            /**
             * Hack: Give 6ms for the blit time.
             */
            if( timediff( &secondfield, &firstfield ) > fieldtime - 6000 ) {
                /* Display. */
                sdl_show_frame();
                break;
            }

            /**
             * We spin while we're waiting, and use /dev/rtc to throttle.
             */
            rtctimer_next_tick( rtctimer );
        }
    }

    fprintf( stderr, "tvtime: Cleaning up.\n" );
    videoinput_delete( vidin );
    rtctimer_delete( rtctimer );
    return 0;
}

