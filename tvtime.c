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
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "pngoutput.h"
#include "frequencies.h"
#include "videoinput.h"
#include "rtctimer.h"
#include "sdloutput.h"
#include "videotools.h"
#include "mixer.h"
#include "osd.h"
#include "tvtimeosd.h"
#include "config.h"
#include "input.h"

/* Number of frames to wait for next channel digit. */
#define CHANNEL_DELAY 100

/* Number of frames to pause during channel change. */
#define CHANNEL_HOLD 2

/**
 * Warning tolerance, just for debugging.
 */
static int tolerance = 2000;

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

#if 0

static void build_test_frames( unsigned char *oddframe, unsigned char *evenframe, int width, int height )
{
    osd_string_t *test_string;

    test_string = osd_string_new( "helr.ttf", 80, width, height, 4.0 / 3.0 );
    blit_colour_packed422( oddframe, width, height, width*2, 16, 128, 128 );
    blit_colour_packed422( evenframe, width, height, width*2, 80, 128, 128 );

    osd_string_set_colour( test_string, 235, 128, 128 );
    osd_string_show_text( test_string, "Odd Field", 80 );
    osd_string_composite_packed422( test_string, oddframe, width, height, width*2, 50, 50, 0 );

    osd_string_set_colour( test_string, 235, 128, 128 );
    osd_string_show_text( test_string, "Even Field", 80 );
    osd_string_composite_packed422( test_string, evenframe, width, height, width*2, 50, 150, 0 );
    osd_string_delete( test_string );
}

void build_colourbars( unsigned char *output, int width, int height )
{
    unsigned char *cb444 = (unsigned char *) malloc( width * height * 3 );
    int i;
    if( !cb444 ) { memset( output, 255, width * height * 2 ); return; }

    create_colourbars_packed444( cb444, width, height, width*3 );
    for( i = 0; i < height; i++ ) {
        unsigned char *curout = output + (i * width * 2);
        unsigned char *curin = cb444 + (i * width * 3);
        cheap_packed444_to_packed422_scanline( curout, curin, width );
    }

    free( cb444 );
}

static void pngscreenshot( unsigned char *filename, unsigned char *frame422,
                           int width, int height, int stride )
{
    pngoutput_t *pngout = pngoutput_new( filename, width, height, 0.45 );
    unsigned char *tempscanline = (unsigned char *) malloc( width * 3 );
    int i;

    if( !tempscanline ) {
        pngoutput_delete( pngout );
        return;
    }

    for( i = 0; i < height; i++ ) {
        unsigned char *input422 = frame422 + (i * stride);
        //chroma422_to_chroma444_rec601_scanline( tempscanline, input422, width );
        cheap_packed422_to_packed444_scanline( tempscanline, input422, width );
        packed444_to_rgb24_rec601_scanline( tempscanline, tempscanline, width );
        pngoutput_scanline( pngout, tempscanline );
    }

    pngoutput_delete( pngout );
}

#endif

int main( int argc, char **argv )
{
    struct timeval lastfieldtime;
    struct timeval lastframetime;
    struct timeval checkpoint[ 7 ];
    video_correction_t *vc = 0;
    videoinput_t *vidin;
    rtctimer_t *rtctimer;
    int width, height;
    int palmode = 0;
    int secam = 0;
    int fieldtime;
    int blittime = 0;
    int skipped = 0;
    int verbose;
    tvtime_osd_t *osd;
    int videohold = CHANNEL_HOLD;
    int i;
/*
    unsigned char *testframe_odd;
    unsigned char *testframe_even;
    unsigned char *colourbars;
*/
    config_t *ct;
    input_t *in;

    ct = config_new( argc, argv );
    if( !ct ) {
        fprintf( stderr, "tvtime: Can't set configuration options.\n" );
        return 0;
    }
    verbose = config_get_verbose( ct );

    if( !strcasecmp( config_get_v4l_norm( ct ), "pal" ) ) {
        palmode = 1;
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "secam" ) ) {
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

    if( config_get_inputwidth( ct ) != 720 && verbose ) {
        fprintf( stderr, "tvtime: V4L sampling %d pixels per scanline.\n", 
                 config_get_inputwidth( ct ) );
    }
    vidin = videoinput_new( config_get_v4l_device( ct ), 
                            config_get_inputnum( ct ), 
                            config_get_inputwidth( ct ), 
                            palmode ? 576 : 480, palmode, 
                            verbose );
    if( !vidin ) {
        fprintf( stderr, "tvtime: Can't open video input, "
                         "maybe try a different device?\n" );
        return 1;
    }

    width = videoinput_get_width( vidin );
    height = videoinput_get_height( vidin );

/*  Screw this for now -- doug
    testframe_odd = (unsigned char *) malloc( width * height * 2 );
    testframe_even = (unsigned char *) malloc( width * height * 2 );
    colourbars = (unsigned char *) malloc( width * height * 2 );
    if( !testframe_odd || !testframe_even || !colourbars ) {
        fprintf( stderr, "tvtime: Can't allocate test memory.\n" );
        return 1;
    }
    build_test_frames( testframe_odd, testframe_even, width, height );
    build_colourbars( colourbars, width, height );
*/

    /* Setup OSD stuff. */
    osd = tvtime_osd_new( width, height, 4.0 / 3.0 );

    /* Setup the tuner if available. */
    if( videoinput_has_tuner( vidin ) ) {
        if( palmode ) {
            if( secam ) {
                videoinput_set_tuner( vidin, config_get_tuner_number( ct ), 
                                      VIDEOINPUT_SECAM );
            } else {
                videoinput_set_tuner( vidin, config_get_tuner_number( ct ),
                                      VIDEOINPUT_PAL );
            }
        } else {
            videoinput_set_tuner( vidin, config_get_tuner_number( ct ),
                                  VIDEOINPUT_NTSC );
        }
        /* Set to the current channel, or the first channel in our frequency list. */
        if( !frequencies_set_chanlist( (char*)config_get_v4l_freq( ct ) ) ) {
            fprintf( stderr, "tvtime: Invalid channel/frequency region: %s.", 
                     config_get_v4l_freq( ct ) );
            /* XXX: Print valid frequency regions here. */
            return 1;
        } else {
            int rc = frequencies_find_current_index( vidin );
            if( rc == -1 ) {
                /* set to a known frequency */
                videoinput_set_tuner_freq( vidin, chanlist[ chanindex ].freq );
                videohold = CHANNEL_HOLD;

                if( verbose ) fprintf( stderr, 
                                       "tvtime: Changing to channel %s.\n", 
                                       chanlist[ chanindex ].name );
/*
                osd_string_show_text( channel_number, 
                                      chanlist[ chanindex ].name, 80 );
*/
            } else if( rc > 0 ) {
                if( verbose ) fprintf( stderr, 
                                       "tvtime: Changing to channel %s.\n",
                                       chanlist[ chanindex ].name );
/*
                osd_string_show_text( channel_number, 
                                      chanlist[ chanindex ].name, 80 );
*/
            }
            tvtime_osd_show_channel_number( osd, chanlist[ chanindex ].name );
        }
    }

    /* Setup the video correction tables. */
    if( config_get_apply_luma_correction( ct ) ) {
        vc = video_correction_new();
        if( !vc ) {
            fprintf( stderr, "tvtime: Can't initialize luma "
                             "and chroma correction tables.\n" );
            return 1;
        }
        if( config_get_luma_correction( ct ) < 0.0 || 
            config_get_luma_correction( ct ) > 10.0 ) {
            fprintf( stderr, "tvtime: Luma correction value out of range. "
                             "Using 1.0.\n" );
            config_set_luma_correction( ct, 1.0 );
        }
        if( verbose ) fprintf( stderr, "tvtime: Luma correction value: %.1f\n",
                               config_get_luma_correction( ct ) );
        video_correction_set_luma_power( vc, 
                                         config_get_luma_correction( ct ) );
    } else {
        if( verbose ) fprintf( stderr, "tvtime: Luma correction disabled.\n" );
    }

    in = input_new( ct, vidin, osd, vc );
    if( !in ) {
        fprintf( stderr, "tvtime: Can't create input handler.\n" );
        return 1;
    }


    /* Steal system resources in the name of performance. */
    if( verbose ) fprintf( stderr, "tvtime: Attempting to aquire "
                                   "performance-enhancing features.\n" );
    if( setpriority( 0, 0, -19 ) < 0 ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't renice to -19.\n" );
    }
    if( mlockall( MCL_CURRENT | MCL_FUTURE ) ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't use mlockall() to "
                                       "lock memory.\n" );
    }
    if( !set_realtime_priority( 0 ) ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't set realtime "
                                       "priority (need root).\n" );
    }
    rtctimer = rtctimer_new();
    if( !rtctimer ) {
        if( verbose ) fprintf( stderr, "tvtime: Can't open /dev/rtc "
                                       "(for my wakeup call).\n" );
    } else {
        if( !rtctimer_set_interval( rtctimer, 1024 ) ) {
            if( verbose ) fprintf( stderr, "tvtime: Can't set 1024hz "
                                           "from /dev/rtc (need root).\n" );
            rtctimer_delete( rtctimer );
            rtctimer = 0;
        } else {
            rtctimer_start_clock( rtctimer );
        }
    }
    if( verbose ) fprintf( stderr, "tvtime: Done stealing resources.\n" );

    /* Setup the output. */
    if( !sdl_init( width, height, config_get_outputwidth( ct ), 
                   config_get_aspect( ct ) ) ) {
        fprintf( stderr, "tvtime: SDL failed to initialize: "
                         "no video output available.\n" );
        return 1;
    }

    /* Set the mixer volume. */
    mixer_set_volume( mixer_get_volume() );

    /* Begin capturing frames. */
    videoinput_free_all_frames( vidin );

    /* Initialize our timestamps. */
    for( i = 0; i < 7; i++ ) gettimeofday( &(checkpoint[ i ]), 0 );
    gettimeofday( &lastframetime, 0 );
    gettimeofday( &lastfieldtime, 0 );
    for(;;) {
        unsigned char *curframe;
        struct timeval curframetime;
        struct timeval curfieldtime;
        struct timeval blitstart;
        struct timeval blitend;
        int printdebug = 0;

        sdl_poll_events( in );
        if ( !input_next_frame( in ) ) break;

        /* CHECKPOINT1 : Blit the second field */
        gettimeofday( &(checkpoint[ 0 ]), 0 );

        /* Aquire the next frame. */
        curframe = videoinput_next_image( vidin );


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
        if( config_get_debug( ct ) && ((timediff( &curframetime, &lastframetime ) - tolerance) > (fieldtime*2)) ) {
            fprintf( stderr, "tvtime: Skip %3d: diff %dus, frametime %dus\n",
                     skipped++,
                     timediff( &curframetime, &lastframetime ), (fieldtime*2) );
        }
        lastframetime = curframetime;


        /* Build the output from the top field. */
        if( config_get_apply_luma_correction( ct ) ) {
            video_correction_packed422_field_to_frame_top( vc, 
                                                           sdl_get_output(),
                                                           width*2, 
                                                           curframe,
                                                           width, height/2,
                                                           width*4 );
        } else {
            packed422_field_to_frame_top( sdl_get_output(), width*2, 
                                          curframe,
                                          width, height/2, width*4 );
        }

        tvtime_osd_volume_muted( osd, mixer_ismute() );
        tvtime_osd_composite_packed422( osd, sdl_get_output(), width,
                                        height, width*2 );


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
        if( !videohold && !input_get_videohold( in ) ) sdl_show_frame();
        gettimeofday( &blitend, 0 );
        lastfieldtime = blitend;
        blittime = timediff( &blitend, &blitstart );


        /* CHECKPOINT4 : Blit the first field */
        gettimeofday( &(checkpoint[ 3 ]), 0 );


        /* Build the output from the bottom field. */
        if( config_get_apply_luma_correction( ct ) ) {
            video_correction_packed422_field_to_frame_bot( vc, 
                                                           sdl_get_output(),
                                                           width*2,
                                                           curframe + (width*2),
                                                           width, height/2,
                                                           width*4 );
        } else {
            packed422_field_to_frame_bot( sdl_get_output(), width*2, 
                                          curframe + (width*2),
                                          width, height/2, width*4 );
        }
        tvtime_osd_volume_muted( osd, mixer_ismute() );
        tvtime_osd_composite_packed422( osd, sdl_get_output(), width, height, width*2 );


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
        if( !videohold && !input_get_videohold( in ) ) sdl_show_frame();
        gettimeofday( &blitend, 0 );
        lastfieldtime = blitend;
        blittime = timediff( &blitend, &blitstart );

        tvtime_osd_advance_frame( osd );

        if( videohold ) videohold--;
        if( input_get_videohold( in ) ) input_dec_videohold( in );
    }

    sdl_quit();

    if( verbose ) fprintf( stderr, "tvtime: Cleaning up.\n" );
    videoinput_delete( vidin );
    config_delete( ct );
    input_delete( in );
    if( vc ) {
        video_correction_delete( vc );
    }
    if( rtctimer ) {
        rtctimer_delete( rtctimer );
    }
    return 0;
}

