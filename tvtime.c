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
#include "osd.h"

/* Number of frames to wait for next channel digit */
#define CHANNEL_DELAY 100


/**
 * Warning tolerance, just for debugging.
 */
static int tolerance = 2000;

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-vas] [-w <width>] [-o <mode>] "
                     "[-d <device> [-i <input>] [-n <norm>] "
                     "[-f <frequencies>] [-t <tuner>]\n"
                     "\t-v\tShow verbose messages.\n"
                     "\t-a\t16:9 mode.\n"
                     "\t-s\tPrint frame skip information (for debugging).\n"
                     "\t-w\tOutput window width, defaults to 800.\n"
                     "\t-o\tOutput mode: '422' (default) or '420'.\n"
                     "\t-d\tvideo4linux device (defaults to /dev/video0).\n"
                     "\t-i\tvideo4linux input number (defaults to 0).\n"
                     "\t-c\tApply luma correction.\n"
                     "\t-l\tLuma correction value (defaults to 1.0, use of this implies -c).\n"
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
    struct timeval lastfieldtime;
    struct timeval lastframetime;
    struct timeval checkpoint[ 7 ];
    double luma_correction = 1.0;
    int apply_luma_correction = 0;
    video_correction_t *vc = 0;
    videoinput_t *vidin;
    rtctimer_t *rtctimer;
    char v4ldev[ 256 ];
    char norm[ 7 ];
    char freq[ 20 ];
    int width, height;
    int output420 = 0;
    int palmode = 0;
    int secam = 0;
    int inputnum = 0;
    int aspect = 0;
    int outputwidth = 800;
    int fieldtime;
    int tuner_number = 0;
    int blittime = 0;
    int skipped = 0;
    int verbose = 0;
    int volume;
    int debug = 0;
    osd_string_t *channel_number, *volume_bar, *muted_osd;
    int c, i, frame_counter = 0, digit_counter = 0;
    char next_chan_buffer[5];


    /* Default device. */
    strcpy( v4ldev, "/dev/video0" );
    
    /* Default norm */
    strcpy( norm, "ntsc" );

    /* Default freq */
    strcpy( freq, "us-cable" );
    
    memset( next_chan_buffer, 0, 5 );

    while( (c = getopt( argc, argv, "hw:avcso:d:i:l:n:f:t:" )) != -1 ) {
        switch( c ) {
        case 'w': outputwidth = atoi( optarg ); break;
        case 'v': verbose = 1; break;
        case 'a': aspect = 1; break;
        case 's': debug = 1; break;
        case 'c': apply_luma_correction = 1; break;
        case 'o': if( strcmp( "420", optarg ) == 0 ) output420 = 1; break;
        case 'd': strncpy( v4ldev, optarg, 250 ); break;
        case 'i': inputnum = atoi( optarg ); break;
        case 'l': luma_correction = atof( optarg ); apply_luma_correction = 1; break;
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
    width = videoinput_get_width( vidin );
    height = videoinput_get_height( vidin );

    /* Setup OSD stuff. */
    channel_number = osd_string_new( "helr.ttf", 80, width, height );
    volume_bar = osd_string_new( "helr.ttf", 80, width, height );
    muted_osd = osd_string_new( "helr.ttf", 80, width, height );
    osd_string_set_colour( channel_number, 200, 128, 128 );
    osd_string_set_colour( volume_bar, 200, 128, 128 );
    osd_string_set_colour( muted_osd, 200, 128, 128 );


    /* Setup the tuner if available. */
    if( videoinput_has_tuner( vidin ) ) {
        if( palmode ) {
            if( secam ) {
                videoinput_set_tuner( vidin, tuner_number, VIDEO_MODE_SECAM );
            } else {
                videoinput_set_tuner( vidin, tuner_number, VIDEO_MODE_PAL );
            }
        } else {
            videoinput_set_tuner( vidin, tuner_number, VIDEO_MODE_NTSC );
        }
        /* Set to the current channel, or the first channel in our frequency list. */
        if( !frequencies_set_chanlist( freq ? freq : "us-cable" ) ) {
            fprintf( stderr, "tvtime: Invalid channel/frequency region: %s.", freq ? freq : "us-cable" );
            /* XXX: Print valid frequency regions here. */
            return 1;
        } else {
            if( frequencies_find_current_index( vidin ) == -1 ) {
                videoinput_set_tuner_freq( vidin, chanlist[ chanindex ].freq );
            }
            if( verbose ) fprintf( stderr, "tvtime: Changing to channel %s.\n", chanlist[ chanindex ].name );
            osd_string_show_text( channel_number, chanlist[ chanindex ].name, 80 );
        }
    }

    /* Setup the video correction tables. */
    if( apply_luma_correction ) {
        vc = video_correction_new();
        if( !vc ) {
            fprintf( stderr, "tvtime: Can't initialize luma and chroma correction tables.\n" );
            return 1;
        }
        if( luma_correction < 0.0 || luma_correction > 10.0 ) {
            fprintf( stderr, "tvtime: Luma correction value out of range. Using 1.0.\n" );
            luma_correction = 1.0;
        }
        if( verbose ) fprintf( stderr, "tvtime: Luma correction value: %.1f\n", luma_correction );
        video_correction_set_luma_power( vc, luma_correction );
    } else {
        if( verbose ) fprintf( stderr, "tvtime: Luma correction disabled.\n" );
    }

    /* Steal system resources in the name of performance. */
    if( verbose ) fprintf( stderr, "tvtime: Attempting to aquire performance-enhancing features.\n" );
    if( setpriority( 0, 0, -19 ) < 0 ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't renice to -19.\n" );
    }
    if( mlockall( MCL_CURRENT | MCL_FUTURE ) ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't use mlockall() to lock memory.\n" );
    }
    if( !set_realtime_priority( 0 ) ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't set realtime priority (need root).\n" );
    }
    rtctimer = rtctimer_new();
    if( !rtctimer ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't open /dev/rtc (for my wakeup call).\n" );
    } else {
        if( !rtctimer_set_interval( rtctimer, 1024 ) ) {
            if( verbose ) fprintf( stderr, "tvtime: Can't set 1024hz from /dev/rtc (need root).\n" );
            rtctimer_delete( rtctimer );
            rtctimer = 0;
        } else {
            rtctimer_start_clock( rtctimer );
        }
    }
    if( verbose ) fprintf( stderr, "tvtime: Done stealing resources.\n" );

    /* Setup the output. */
    if( !sdl_init( width, height, output420, outputwidth, aspect ) ) {
        fprintf( stderr, "tvtime: SDL failed to initialize: no video output available.\n" );
        return 1;
    }
    if( verbose ) fprintf( stderr, "tvtime: Output is Y'CbCr at %s sampling.\n", output420 ? "4:2:0" : "4:2:2" );

    /* Set the mixer volume. */
    mixer_set_volume( mixer_get_volume() );

    /* Begin capturing frames. */
    videoinput_free_all_frames( vidin );

    /* Initialize our timestamps. */
    for( i = 0; i < 7; i++ ) gettimeofday( &(checkpoint[ i ]), 0 );
    gettimeofday( &lastframetime, 0 );
    gettimeofday( &lastfieldtime, 0 );
    for(;;) {
        unsigned char *curluma;
        unsigned char *curcb422;
        unsigned char *curcr422;
        struct timeval curframetime;
        struct timeval curfieldtime;
        struct timeval blitstart;
        struct timeval blitend;
        int printdebug = 0;
        int commands;

        commands = sdl_poll_events();
        if( commands & TVTIME_CHANNEL_CHAR ) {
            next_chan_buffer[ digit_counter ] = (char)(commands & 0x000000FF);
            digit_counter++;
            digit_counter %= 4;
            frame_counter = CHANNEL_DELAY;
        } else {
            if( commands & TVTIME_QUIT ) {
                break;
            }
            if( commands & TVTIME_DEBUG ) {
                printdebug = 1;
            }
            if( commands & TVTIME_LUMA_UP || commands & TVTIME_LUMA_DOWN ) {
                if( !apply_luma_correction ) {
                    fprintf( stderr, "tvtime: Luma correction disabled.  Run with -c to use it.\n" );
                } else {
                    luma_correction += ( (commands & TVTIME_LUMA_UP) ? 0.1 : -0.1 );
                    if( luma_correction > 10.0 ) luma_correction = 10.0;
                    if( luma_correction <  0.0 ) luma_correction =  0.0;
                    if( verbose ) fprintf( stderr, "tvtime: Luma correction value: %.1f\n", luma_correction );
                    video_correction_set_luma_power( vc, luma_correction );
                }
            }
            if( commands & TVTIME_CHANNEL_UP || commands & TVTIME_CHANNEL_DOWN ) {
                if( !videoinput_has_tuner( vidin ) ) {
                    fprintf( stderr, "tvtime: Can't change channel, no tuner present!\n" );
                } else {
                    chanindex = (chanindex + ( (commands & TVTIME_CHANNEL_UP) ? 1 : -1) + chancount) % chancount;
                    videoinput_set_tuner_freq( vidin, chanlist[ chanindex ].freq );
                    if( verbose ) fprintf( stderr, "tvtime: Changing to channel %s\n", chanlist[ chanindex ].name );
                    osd_string_show_text( channel_number, chanlist[ chanindex ].name, 80 );
                }
            }
            if( commands & TVTIME_MIXER_UP || commands & TVTIME_MIXER_DOWN ) {
                char bar[19];
                volume = mixer_set_volume( ( (commands & TVTIME_MIXER_UP) ? 3 : -3 ) );
                if( verbose )
                    fprintf( stderr, "tvtime: volume %d\n", 
                             (volume & 0x000000FF) );
                memset( bar, 0, 19 );
                strcpy( bar, "Vol " );
                memset( bar+4, '|', (volume & 0x000000FF)/10 );
                osd_string_show_text( volume_bar, bar, 80 );
            }
            if( commands & TVTIME_MIXER_MUTE ) {
                mixer_toggle_mute();
            }
            if( commands & TVTIME_DIGIT ) {
                char digit = '0';

                if( commands & TVTIME_KP0 ) { digit = '0'; }
                if( commands & TVTIME_KP1 ) { digit = '1'; }
                if( commands & TVTIME_KP2 ) { digit = '2'; }
                if( commands & TVTIME_KP3 ) { digit = '3'; }
                if( commands & TVTIME_KP4 ) { digit = '4'; }
                if( commands & TVTIME_KP5 ) { digit = '5'; }
                if( commands & TVTIME_KP6 ) { digit = '6'; }
                if( commands & TVTIME_KP7 ) { digit = '7'; }
                if( commands & TVTIME_KP8 ) { digit = '8'; }
                if( commands & TVTIME_KP9 ) { digit = '9'; }

                next_chan_buffer[ digit_counter ] = digit;
                digit_counter++;
                digit_counter %= 4;
                frame_counter = CHANNEL_DELAY;
            }
            if( commands & TVTIME_KP_ENTER ) {
                if( frame_counter ) {
                    if( *next_chan_buffer ) {
                        int found;

                        /* this sets chanindex accordingly */
                        found = frequencies_find_named_channel( next_chan_buffer );
                        if( found > -1 ) {
                            videoinput_set_tuner_freq( vidin, 
                                                       chanlist[ chanindex ].freq );

                            if( verbose ) 
                                fprintf( stderr, 
                                         "tvtime: Changing to channel %s\n", 
                                         chanlist[ chanindex ].name );

                            osd_string_show_text( channel_number, 
                                                  chanlist[ chanindex ].name, 80 );
                            frame_counter = 0;
                        } else {
                            /* no valid channel */
                            frame_counter = 0;
                        }
                    }
                }
            }
        } /* DON'T PROCESS commands PAST HERE */

        /* Increment the frame counter if user is typing digits */
        if( frame_counter > 0 ) frame_counter--;
        if( frame_counter == 0 ) {
            memset( (void*)next_chan_buffer, 0, 5 );
            digit_counter = 0;
        }

        if( frame_counter > 0 && !(frame_counter % 5)) {
            char input_text[6];

            strcpy( input_text, next_chan_buffer );
            if( !(frame_counter % 10) )
                strcat( input_text, "_" );
            osd_string_show_text( channel_number, 
                                  input_text, CHANNEL_DELAY );
        }

        /* CHECKPOINT1 : Blit the second field */
        gettimeofday( &(checkpoint[ 0 ]), 0 );

        /* Aquire the next frame. */
        curluma  = videoinput_next_image( vidin );
        curcb422 = curluma  + ( width   * height );
        curcr422 = curcb422 + ( width/2 * height );

        /* CHECKPOINT2 : Got the frame */
        gettimeofday( &(checkpoint[ 1 ]), 0 );

        /* Print statistics and check for missed frames. */
        gettimeofday( &curframetime, 0 );
        if( printdebug ) {
            fprintf( stderr, "tvtime: aquire %d, build top %d "
                             "blit top %d built bot %d free input %d "
                             "wait bot %d blit bot %d\n",
                     timediff( &(checkpoint[ 1 ]), &(checkpoint[ 0 ]) ),
                     timediff( &(checkpoint[ 2 ]), &lastframetime ),
                     timediff( &(checkpoint[ 3 ]), &(checkpoint[ 2 ]) ),
                     timediff( &(checkpoint[ 4 ]), &(checkpoint[ 3 ]) ),
                     timediff( &(checkpoint[ 5 ]), &(checkpoint[ 4 ]) ),
                     timediff( &(checkpoint[ 6 ]), &(checkpoint[ 5 ]) ),
                     timediff( &(checkpoint[ 0 ]), &(checkpoint[ 6 ]) ) );
        }
        if( debug && ((timediff( &curframetime, &lastframetime ) - tolerance) > (fieldtime*2)) ) {
            fprintf( stderr, "tvtime: Skip %3d: diff %dus, frametime %dus\n",
                     skipped++,
                     timediff( &curframetime, &lastframetime ), (fieldtime*2) );
        }
        lastframetime = curframetime;


        /* Build the output from the top field. */
        if( output420 ) {
            luma_plane_field_to_frame( sdl_get_output(), curluma,
                                       width, height, width*2, 0 );
            chroma_plane_field_to_frame( sdl_get_cb(), curcb422,
                                         width/2, height/2, width*2, 0 );
            chroma_plane_field_to_frame( sdl_get_cr(), curcr422,
                                         width/2, height/2, width*2, 0 );
        } else {
            if( apply_luma_correction ) {
                video_correction_planar422_field_to_packed422_frame( vc,
                                                                     sdl_get_output(),
                                                                     curluma,
                                                                     curcb422,
                                                                     curcr422,
                                                                     0, width * 2, width, width, height );
            } else {
                planar422_field_to_packed422_frame( sdl_get_output(), curluma,
                                                    curcb422, curcr422, 0, width * 2,
                                                    width, width, height );
            }
            osd_string_composite_packed422( channel_number, sdl_get_output(), width, height, width*2, 50, 50, 0 );
            if( mixer_ismute() ) {
                osd_string_show_text( muted_osd, "Mute", 100 );
                osd_string_composite_packed422( muted_osd, sdl_get_output(), width, height, width*2, 50, height-105, 0 );
            } else {
                osd_string_composite_packed422( volume_bar, sdl_get_output(), width, height, width*2, 50, height-105, 0 );
            }
        }


        /* CHECKPOINT3 : Constructed the top field. */
        gettimeofday( &(checkpoint[ 2 ]), 0 );


        /* Wait for the next field time and display. */
        gettimeofday( &curfieldtime, 0 );
        /* I'm commenting this out for now, tvtime takes too much CPU here -Billy
        while( timediff( &curfieldtime, &lastfieldtime ) < (fieldtime-blittime) ) {
            if( rtctimer ) {
                rtctimer_next_tick( rtctimer );
            } else {
                usleep( 20 );
            }
            gettimeofday( &curfieldtime, 0 );
        }
        */
        gettimeofday( &blitstart, 0 );
        sdl_show_frame();
        gettimeofday( &blitend, 0 );
        lastfieldtime = blitend;
        blittime = timediff( &blitend, &blitstart );


        /* CHECKPOINT4 : Blit the first field */
        gettimeofday( &(checkpoint[ 3 ]), 0 );


        /* Build the output from the bottom field. */
        if( output420 ) {
            luma_plane_field_to_frame( sdl_get_output(), curluma + width,
                                       width, height, width*2, 1 );
            chroma_plane_field_to_frame( sdl_get_cb(), curcb422,
                                         width/2, height/2, width*2, 1 );
            chroma_plane_field_to_frame( sdl_get_cr(), curcr422,
                                         width/2, height/2, width*2, 1 );
        } else {
            if( apply_luma_correction ) {
                video_correction_planar422_field_to_packed422_frame( vc,
                                                                     sdl_get_output(),
                                                                     curluma + width,
                                                                     curcb422 + (width / 2),
                                                                     curcr422 + (width / 2),
                                                                     1, width * 2, width, width, height );
            } else {
                planar422_field_to_packed422_frame( sdl_get_output(), curluma + width,
                                                    curcb422 + (width / 2),
                                                    curcr422 + (width / 2),
                                                    1, width * 2, width, width, height );
            }
            osd_string_composite_packed422( channel_number, sdl_get_output(), width, height, width*2, 50, 50, 0 );
            if( mixer_ismute() ) {
                osd_string_show_text( muted_osd, "Mute", 51 );
                osd_string_composite_packed422( muted_osd, sdl_get_output(), width, height, width*2, 50, height-105, 0 );
            } else {
                osd_string_composite_packed422( volume_bar, sdl_get_output(), width, height, width*2, 50, height-105, 0 );
            }
        }


        /* CHECKPOINT5 : Built the second field */
        gettimeofday( &(checkpoint[ 4 ]), 0 );


        /* We're done with the input now. */
        videoinput_free_last_frame( vidin );


        /* CHECKPOINT6 : Released a frame to V4L. */
        gettimeofday( &(checkpoint[ 5 ]), 0 );


        /* Wait for the next field time. */
        gettimeofday( &curfieldtime, 0 );
        while( timediff( &curfieldtime, &lastfieldtime ) < (fieldtime-blittime) ) {
            if( rtctimer ) {
                rtctimer_next_tick( rtctimer );
            } else {
                usleep( 20 );
            }
            gettimeofday( &curfieldtime, 0 );
        }

        /* CHECKPOINT7 : Done waiting to blit the bottom field. */
        gettimeofday( &(checkpoint[ 6 ]), 0 );


        /* Display the bottom field. */
        gettimeofday( &blitstart, 0 );
        sdl_show_frame();
        gettimeofday( &blitend, 0 );
        lastfieldtime = blitend;
        blittime = timediff( &blitend, &blitstart );

        osd_string_advance_frame( channel_number );
        osd_string_advance_frame( muted_osd );
        osd_string_advance_frame( volume_bar );
    }

    if( verbose ) fprintf( stderr, "tvtime: Cleaning up.\n" );
    videoinput_delete( vidin );
    if( vc ) {
        video_correction_delete( vc );
    }
    if( rtctimer ) {
        rtctimer_delete( rtctimer );
    }
    return 0;
}

