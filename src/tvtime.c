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
#include <stdint.h>
#include "pngoutput.h"
#include "pnginput.h"
#include "frequencies.h"
#include "videoinput.h"
#include "rtctimer.h"
#include "sdloutput.h"
#include "videotools.h"
#include "mixer.h"
#include "osd.h"
#include "tvtimeosd.h"
#include "tvtimeconf.h"
#include "input.h"
#include "speedy.h"
#include "deinterlace.h"
#include "menu.h"
#include "videocorrection.h"
#include "plugins.h"
#include "dfboutput.h"

/**
 * Warning tolerance, just for debugging.
 */
static int tolerance = 2000;

/**
 * Current deinterlacing method.
 */
static deinterlace_method_t *curmethod;
static int curmethodid;

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}

static void build_colourbars( unsigned char *output, int width, int height )
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

static void pngscreenshot( const char *filename, unsigned char *frame422,
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
        // chroma422_to_chroma444_rec601_scanline( tempscanline,
        //                                         input422, width );
        cheap_packed422_to_packed444_scanline( tempscanline, input422, width );
        packed444_to_rgb24_rec601_scanline( tempscanline, tempscanline, width );
        pngoutput_scanline( pngout, tempscanline );
    }

    pngoutput_delete( pngout );
}

/**
 * Explination of the loop:
 *
 * We want to build frames so that they look like this:
 *  Top field:      Bot field:
 *     Copy            Double
 *     Interp          Copy
 *     Copy            Interp
 *     Interp          Copy
 *     Copy            --
 *     --              --
 *     --              --
 *     Copy            Interp
 *     Interp          Copy
 *     Copy            Interp
 *     Double          Copy
 *
 *  So, say a frame is n high.
 *  For the bottom field, the first scanline is blank (special case).
 *  For the top field, the final scanline is blank (special case).
 *  For the rest of the scanlines, we alternate between Copy then Interpolate.
 *
 *  To do the loop, I go 'Interp then Copy', and handle the first copy
 *  outside the loop for both top and bottom.
 *  The top field therefore handles n-2 scanlines in the loop.
 *  The bot field handles n-2 scanlines in the loop.
 *
 * What we pass to the deinterlacing routines:
 *
 * Each deinterlacing routine can require data from up to four fields.
 * The current field is being output is Field 4:
 *
 * | Field 1 | Field 2 | Field 3 | Field 4 |
 * |         |   T0    |         |   T1    |
 * |   M0    |         |    M1   |         |
 * |         |   B0    |         |   B1    |
 * |  NX0    |         |   NX1   |         |
 *
 * So, since we currently get frames not individual fields from V4L, there
 * are two possibilities for where these come from:
 *
 * CASE 1: Deinterlacing the top field:
 * | Field 0 | Field 1 | Field 2 | Field 3 | Field 4 |
 * |   T-1   |         |   T0    |         |   T1    |
 * |         |   M0    |         |    M1   |         |
 * |   B-1   |         |   B0    |         |   B1    |
 *  [--  secondlast --] [--  lastframe  --] [--  curframe   --]
 *
 * CASE 2: Deinterlacing the bottom field:
 * | Field 0 | Field 1 | Field 2 | Field 3 | Field 4 |
 * |   T-1   |         |   T0    |         |   T1    |
 * |         |   M0    |         |    M1   |         |
 * |   B-1   |         |   B0    |         |   B1    |
 * ndlast --] [--  lastframe  --] [--  curframe   --]
 *
 * So, in case 1, we need the previous 2 frames as well as the current
 * frame, and in case 2, we only need the previous frame, since the
 * current frame contains both Field 3 and Field 4.
 */
static void tvtime_build_deinterlaced_frame( unsigned char *output,
                                             unsigned char *curframe,
                                             unsigned char *lastframe,
                                             unsigned char *secondlastframe,
                                             video_correction_t *vc,
                                             tvtime_osd_t *osd,
                                             menu_t *menu,
                                             int bottom_field,
                                             int correct_input,
                                             int width,
                                             int frame_height,
                                             int instride,
                                             int outstride )
{
    int scanline = 0;
    unsigned char *out = output;
    int i;

    if( bottom_field ) {
        /* Advance frame pointers to the next input line. */
        curframe += instride;
        lastframe += instride;
        secondlastframe += instride;

        /* Double the top scanline a scanline. */
        blit_packed422_scanline( output, curframe, width );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        output += outstride;
        scanline++;
    }

    /* Copy a scanline. */
    blit_packed422_scanline( output, curframe, width );

    if( correct_input ) {
        video_correction_correct_packed422_scanline( vc, output, output, width );
    }

    if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
    output += outstride;
    scanline++;

    for( i = ((frame_height - 2) / 2); i; --i ) {
        unsigned char *top1 = curframe;
        unsigned char *mid1 = curframe + instride;
        unsigned char *bot1 = curframe + (instride*2);
        unsigned char *nxt1 = curframe + (instride*3);

        unsigned char *top0 = lastframe;
        unsigned char *mid0 = lastframe + instride;
        unsigned char *bot0 = lastframe + (instride*2);
        unsigned char *nxt0 = lastframe + (instride*3);

        if( i == 1 ) {
            nxt1 = mid1;
            nxt0 = mid0;
        }

        if( !bottom_field ) {
            mid1 = mid0;
            nxt1 = nxt0;
            mid0 = secondlastframe + instride;
            nxt0 = secondlastframe + (instride*3);
        }

        curmethod->interpolate_scanline( output, top1, mid1, bot1, top0, mid0, bot0, width );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        output += outstride;
        scanline++;

        /* Copy a scanline. */
        curmethod->copy_scanline( output, bot1, mid1, bot0, nxt1, mid0, nxt0, width );
        curframe += instride * 2;
        lastframe += instride * 2;
        secondlastframe += instride * 2;

        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        output += outstride;
        scanline++;
    }

    if( !bottom_field ) {
        /* Double the bottom scanline. */
        blit_packed422_scanline( output, curframe, width );

        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        output += outstride;
        scanline++;
    }

    if( menu ) menu_composite_packed422( menu, out, width, frame_height, outstride );
}


static void tvtime_build_interlaced_frame( unsigned char *output,
                                           unsigned char *curframe,
                                           video_correction_t *vc,
                                           tvtime_osd_t *osd,
                                           menu_t *menu,
                                           int bottom_field,
                                           int correct_input,
                                           int width,
                                           int frame_height,
                                           int instride,
                                           int outstride )
{
    unsigned char tempscanline[ 768*2 ];
    int scanline = 0;
    int i;

    if( bottom_field ) {
        /* Advance frame pointers to the next input line. */
        curframe += instride;

        /* Skip the top scanline. */
        output += outstride;
        scanline++;
    }

    /* Copy a scanline. */
    blit_packed422_scanline( output, curframe, width );

/*
    if( correct_input ) {
        video_correction_correct_packed422_scanline( vc, output, output, width );
    }
    if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
*/
    output += outstride;
    scanline++;

    for( i = ((frame_height - 2) / 2); i; --i ) {
        /* Skip a scanline. */
        output += outstride;
        scanline++;

        /* Copy a scanline. */
        curframe += instride * 2;
        blit_packed422_scanline( output, curframe, width );

/*
        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
*/
        output += outstride;
        scanline++;
    }
}


int main( int argc, char **argv )
{
    struct timeval lastfieldtime;
    struct timeval lastframetime;
    struct timeval checkpoint[ 7 ];
    video_correction_t *vc = 0;
    videoinput_t *vidin;
    rtctimer_t *rtctimer;
    int width, height;
    int norm = 0;
    int fieldtime;
    int blittime = 0;
    int skipped = 0;
    int fieldsavailable = 0;
    int verbose;
    tvtime_osd_t *osd;
    int i;
    unsigned char *colourbars;
    unsigned char *lastframe = 0;
    unsigned char *secondlastframe = 0;
    int lastframeid;
    int secondlastframeid;
    config_t *ct;
    input_t *in;
    menu_t *menu;
    output_api_t *output;

    setup_speedy_calls();

    greedy_plugin_init();
    videobob_plugin_init();
    greedy2frame_plugin_init();
    twoframe_plugin_init();
    linear_plugin_init();
    weave_plugin_init();
    double_plugin_init();

    ct = config_new( argc, argv );
    if( !ct ) {
        fprintf( stderr, "tvtime: Can't set configuration options.\n" );
        return 0;
    }
    verbose = config_get_verbose( ct );

    if( !strcasecmp( config_get_v4l_norm( ct ), "pal" ) ) {
        norm = VIDEOINPUT_PAL;
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "secam" ) ) {
        norm = VIDEOINPUT_SECAM;
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "pal-nc" ) ) {
        norm = VIDEOINPUT_PAL_NC;
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "pal-m" ) ) {
        norm = VIDEOINPUT_PAL_M;
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "pal-n" ) ) {
        norm = VIDEOINPUT_PAL_N;
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "ntsc-jp" ) ) {
        norm = VIDEOINPUT_NTSC_JP;
    } else {
        /* Only allow NTSC otherwise. */
        norm = VIDEOINPUT_NTSC;
    }

    /* Field display in microseconds. */
    if( norm != VIDEOINPUT_NTSC ) {
        fieldtime = 20000;
    } else {
        fieldtime = 16683;
    }

    if( config_get_inputwidth( ct ) != 720 && verbose ) {
        fprintf( stderr, "tvtime: V4L sampling %d pixels per scanline.\n", 
                 config_get_inputwidth( ct ) );
    }
    vidin = videoinput_new( config_get_v4l_device( ct ), 
                            config_get_inputwidth( ct ), 
                            norm, verbose );
    if( !vidin ) {
        fprintf( stderr, "tvtime: Can't open video input, "
                         "maybe try a different device?\n" );
        return 1;
    }

    videoinput_set_input_num( vidin, config_get_inputnum( ct ) );
    width = videoinput_get_width( vidin );
    height = videoinput_get_height( vidin );

    /**
     * 1 buffer : 0 fields available.  [t][b]
     * 2 buffers: 1 field  available.  [t][b][-][-]
     *                                 ^^^
     * 3 buffers: 3 fields available.  [t][b][t][b][-][-]
     *                                 ^^^^^^^^^
     * 4 buffers: 5 fields available.  [t][b][t][b][t][b][-][-]
     *                                 ^^^^^^^^^^^^^^^
     */
    if( videoinput_get_numframes( vidin ) < 2 ) {
        fprintf( stderr, "tvtime: Can only get %d frame buffers from V4L.  "
                 "Not enough to continue.  Exiting.\n",
                 videoinput_get_numframes( vidin ) );
        return 1;
    } else if( videoinput_get_numframes( vidin ) == 2 ) {
        fprintf( stderr, "tvtime: Can only get %d frame buffers from V4L.  "
                 "Limiting deinterlace plugins to those which only need 1 field.\n"
                 "tvtime: If you are using the bttv driver, do 'modprobe bttv "
                 "gbuffers=4' next time.\n",
                 videoinput_get_numframes( vidin ) );
        fieldsavailable = 1;
    } else if( videoinput_get_numframes( vidin ) == 3 ) {
        fprintf( stderr, "tvtime: Can only get %d frame buffers from V4L.  "
                 "Limiting deinterlace plugins to those which only need 3 fields.\n"
                 "tvtime: If you are using the bttv driver, do 'modprobe bttv "
                 "gbuffers=4' next time.\n",
                 videoinput_get_numframes( vidin ) );
        fieldsavailable = 3;
    } else {
        fieldsavailable = 5;
    }
    filter_deinterlace_methods( speedy_get_accel(), fieldsavailable );
    if( !get_num_deinterlace_methods() ) {
        fprintf( stderr, "tvtime: No deinterlacing methods available, exiting.\n" );
        return 1;
    }
    curmethodid = 0;
    curmethod = get_deinterlace_method( curmethodid );

    /* Build colourbars. */
    colourbars = (unsigned char *) malloc( width * height * 2 );
    if( !colourbars ) {
        fprintf( stderr, "tvtime: Can't allocate test memory.\n" );
        return 1;
    }
    build_colourbars( colourbars, width, height );

    /* Setup OSD stuff. */
    osd = tvtime_osd_new( width, height, 4.0 / 3.0 );
    if( !osd ) {
        fprintf( stderr, "Can't initialize OSD object.\n" );
    }

    /**
     * Set to the current channel, or the first channel in our
     * frequency list.
     */
    if( !frequencies_set_chanlist( config_get_v4l_freq( ct ) ) ) {
        fprintf( stderr, "tvtime: Invalid channel/frequency region: %s, using us-cable.\n", 
                 config_get_v4l_freq( ct ) );
        frequencies_set_chanlist( "us-cable" );
    }

    /* Setup the tuner if available. */
    if( videoinput_has_tuner( vidin ) ) {
        /**
         * Set to the current channel, or the first channel in our
         * frequency list.
         */
        char timestamp[50];
        time_t tm = time(NULL);
        int rc = frequencies_find_current_index( vidin );
        if( rc == -1 ) {
            /* set to a known frequency */
            videoinput_set_tuner_freq( vidin, chanlist[ chanindex ].freq +
                                       config_get_finetune( ct ) );

            if( verbose ) fprintf( stderr, 
                                   "tvtime: Changing to channel %s.\n", 
                                   chanlist[ chanindex ].name );
        } else if( rc > 0 ) {
            if( verbose ) fprintf( stderr, 
                                   "tvtime: Changing to channel %s.\n",
                                   chanlist[ chanindex ].name );
        }
        strftime( timestamp, 50, config_get_timeformat( ct ), 
                  localtime(&tm) );
        if( osd ) {
            tvtime_osd_show_channel_number( osd, chanlist[ chanindex ].name );
            tvtime_osd_show_channel_info( osd, timestamp );
            tvtime_osd_show_channel_logo( osd );
        }
    }

    /* Setup the video correction tables. */
    if( verbose ) {
        if( config_get_apply_luma_correction( ct ) ) {
            fprintf( stderr, "tvtime: Luma correction enabled.\n" );
        } else {
            fprintf( stderr, "tvtime: Luma correction disabled.\n" );
        }
    }

    vc = video_correction_new( videoinput_is_bttv( vidin ), 0 );
    if( !vc ) {
        fprintf( stderr, "tvtime: Can't initialize luma "
                         "and chroma correction tables.\n" );
        return 1;
    } else {
        if( config_get_luma_correction( ct ) < 0.0 || 
            config_get_luma_correction( ct ) > 10.0 ) {
            fprintf( stderr, "tvtime: Luma correction value out of range. "
                             "Using 1.0.\n" );
            config_set_luma_correction( ct, 1.0 );
        }
        if( verbose ) {
            fprintf( stderr, "tvtime: Luma correction value: %.1f\n",
                             config_get_luma_correction( ct ) );
        }
        video_correction_set_luma_power( vc, 
                                         config_get_luma_correction( ct ) );
    }


    in = input_new( ct, vidin, osd, vc );
    if( !in ) {
        fprintf( stderr, "tvtime: Can't create input handler.\n" );
        return 1;
    }

    menu = menu_new( in, ct, width, height, 
                     config_get_aspect( ct ) ? 16.0 / 9.0 : 4.0 / 3.0 );
    if( !menu ) {
        fprintf( stderr, "tvtime: Can't create menu.\n" );
    }
    if( menu ) input_set_menu( in, menu );


    /* Steal system resources in the name of performance. */
    if( verbose ) {
        fprintf( stderr, "tvtime: Attempting to aquire "
                         "performance-enhancing features.\n" );
    }
    if( setpriority( 0, 0, -19 ) < 0 && verbose ) {
        fprintf( stderr, "tvtime: Can't renice to -19.\n" );
    }
    if( mlockall( MCL_CURRENT | MCL_FUTURE ) && verbose ) {
        fprintf( stderr, "tvtime: Can't use mlockall() to lock memory.\n" );
    }
    if( !set_realtime_priority( 0 ) && verbose ) {
        fprintf( stderr, "tvtime: Can't set realtime priority (need root).\n" );
    }
    rtctimer = rtctimer_new( verbose );
    if( !rtctimer ) {
        if( verbose ) {
            fprintf( stderr, "tvtime: Can't open /dev/rtc "
                             "(for my wakeup call).\n" );
        }
    } else {
        if( !rtctimer_set_interval( rtctimer, 1024 ) ) {
            if( verbose ) {
                fprintf( stderr, "tvtime: Can't set 1024hz "
                                 "from /dev/rtc (need root).\n" );
            }
            rtctimer_delete( rtctimer );
            rtctimer = 0;
        } else {
            rtctimer_start_clock( rtctimer );
        }
    }
    if( verbose ) {
        fprintf( stderr, "tvtime: Done stealing resources.\n" );
    }

    /* Setup the output. */
    output = get_dfb_output();
    //output = get_sdl_output();
    if( !output->init( width, height, config_get_outputwidth( ct ), 
                       config_get_aspect( ct ) ) ) {
        fprintf( stderr, "tvtime: SDL failed to initialize: "
                         "no video output available.\n" );
        return 1;
    }

    /* Set the mixer volume. */
    mixer_set_volume( mixer_get_volume() );

    /* Begin capturing frames. */
    if( fieldsavailable == 3 ) {
        lastframe = videoinput_next_frame( vidin, &lastframeid );
    } else if( fieldsavailable == 5 ) {
        secondlastframe = videoinput_next_frame( vidin, &secondlastframeid );
        lastframe = videoinput_next_frame( vidin, &lastframeid );
    }

    /* Initialize our timestamps. */
    for( i = 0; i < 7; i++ ) gettimeofday( &(checkpoint[ i ]), 0 );
    gettimeofday( &lastframetime, 0 );
    gettimeofday( &lastfieldtime, 0 );
    for(;;) {
        unsigned char *curframe;
        int curframeid;
        struct timeval curframetime;
        struct timeval curfieldtime;
        struct timeval blitstart;
        struct timeval blitend;
        int printdebug = 0;
        int showbars, videohold, screenshot;

        output->poll_events( in );

        if( input_quit( in ) ) break;
        videohold = input_videohold( in );
        printdebug = input_print_debug( in );
        showbars = input_show_bars( in );
        screenshot = input_take_screenshot( in );
        if( input_toggle_fullscreen( in ) ) {
            output->toggle_fullscreen();
        }
        if( input_toggle_aspect( in ) ) {
            if( output->toggle_aspect() ) {
                tvtime_osd_show_message( osd, "16:9 display mode" );
            } else {
                tvtime_osd_show_message( osd, "4:3 display mode" );
            }
        }
        if( input_toggle_deinterlacing_mode( in ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            if( osd ) {
                tvtime_osd_show_message( osd, curmethod->name );
            }
        }
        if( osd ) {
            tvtime_osd_volume_muted( osd, mixer_ismute() );
        }
        input_next_frame( in );

        /* CHECKPOINT1 : Blit the second field */
        gettimeofday( &(checkpoint[ 0 ]), 0 );

        /* Aquire the next frame. */
        curframe = videoinput_next_frame( vidin, &curframeid );

/*
        if( screenshot ) {
            char filename[ 256 ];
            sprintf( filename, "tvtime-shot-input-%d-%d.png",
                     (int) checkpoint[ 0 ].tv_sec,
                     (int) checkpoint[ 0 ].tv_usec );
            pngscreenshot( filename, curframe, width, height, width * 2 );
        }
*/

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
        if( config_get_debug( ct ) && ((timediff( &curframetime,
                           &lastframetime ) - tolerance) > (fieldtime*2)) ) {
            fprintf( stderr, "tvtime: Skip %3d: diff %dus, frametime %dus\n",
                     skipped++,
                     timediff( &curframetime, &lastframetime ), (fieldtime*2) );
        }
        lastframetime = curframetime;


        if( output->is_interlaced ) {
            /* Wait until we can draw the even field. */
            output->wait_for_sync( 0 );

            output->lock_output_buffer();
            tvtime_build_interlaced_frame( output->get_output_buffer(), curframe, vc, osd, menu, 0,
                                           vc && config_get_apply_luma_correction( ct ),
                                           width, height, width * 2, output->get_output_stride() );
            output->unlock_output_buffer();
        } else {
            /* Build the output from the top field. */
            output->lock_output_buffer();
            if( showbars ) {
                blit_packed422_scanline( output->get_output_buffer(), colourbars, width*height );
            } else {
                tvtime_build_deinterlaced_frame( output->get_output_buffer(), curframe, lastframe,
                                    secondlastframe, vc, osd, menu, 0,
                                    vc && config_get_apply_luma_correction( ct ),
                                    width, height, width * 2, width * 2 );
            }
            if( screenshot ) {
                char filename[ 256 ];
                sprintf( filename, "tvtime-shot-top-%d-%d.png",
                         (int) checkpoint[ 0 ].tv_sec,
                         (int) checkpoint[ 0 ].tv_usec );
                pngscreenshot( filename, output->get_output_buffer(), width, height, width * 2 );
            }
            output->unlock_output_buffer();



            /* CHECKPOINT3 : Constructed the top field. */
            gettimeofday( &(checkpoint[ 2 ]), 0 );


            /* Wait for the next field time and display. */
            gettimeofday( &curfieldtime, 0 );

            /**
             * I'm commenting this out for now, tvtime takes
             * too much CPU here -Billy
            while( timediff( &curfieldtime, &lastfieldtime )
                                    < (fieldtime-blittime) ) {
                if( rtctimer ) {
                    rtctimer_next_tick( rtctimer );
                } else {
                    usleep( 20 );
                }
                gettimeofday( &curfieldtime, 0 );
            }
            */
            gettimeofday( &blitstart, 0 );
            if( !videohold ) output->show_frame();
            gettimeofday( &blitend, 0 );
            lastfieldtime = blitend;
            blittime = timediff( &blitend, &blitstart );
        }


        /* CHECKPOINT4 : Blit the first field */
        gettimeofday( &(checkpoint[ 3 ]), 0 );


        if( output->is_interlaced ) {
            /* Wait until we can draw the odd field. */
            output->wait_for_sync( 1 );

            output->lock_output_buffer();
            tvtime_build_interlaced_frame( output->get_output_buffer(), curframe, vc, osd, menu, 1,
                                           vc && config_get_apply_luma_correction( ct ),
                                           width, height, width * 2, output->get_output_stride() );
            output->unlock_output_buffer();

            /* We're done with the input now. */
            if( fieldsavailable == 3 ) {
                videoinput_free_frame( vidin, lastframeid );
                lastframeid = curframeid;
                lastframe = curframe;
            } else if( fieldsavailable == 5 ) {
                videoinput_free_frame( vidin, secondlastframeid );
                secondlastframeid = lastframeid;
                lastframeid = curframeid;
                secondlastframe = lastframe;
                lastframe = curframe;
            } else {
                videoinput_free_frame( vidin, curframeid );
            }
        } else {
            /* Build the output from the bottom field. */
            output->lock_output_buffer();
            if( showbars ) {
                blit_packed422_scanline( output->get_output_buffer(), colourbars, width*height );
            } else {
                tvtime_build_deinterlaced_frame( output->get_output_buffer(), curframe, lastframe,
                                    secondlastframe, vc, osd, menu, 1,
                                    vc && config_get_apply_luma_correction( ct ),
                                    width, height, width * 2, width * 2 );
            }
            if( screenshot ) {
                char filename[ 256 ];
                sprintf( filename, "tvtime-shot-bot-%d-%d.png",
                         (int) checkpoint[ 0 ].tv_sec,
                         (int) checkpoint[ 0 ].tv_usec );
                pngscreenshot( filename, output->get_output_buffer(), width, height, width * 2 );
            }
            output->unlock_output_buffer();


            /* CHECKPOINT5 : Built the second field */
            gettimeofday( &(checkpoint[ 4 ]), 0 );


            /* We're done with the input now. */
            if( fieldsavailable == 3 ) {
                videoinput_free_frame( vidin, lastframeid );
                lastframeid = curframeid;
                lastframe = curframe;
            } else if( fieldsavailable == 5 ) {
                videoinput_free_frame( vidin, secondlastframeid );
                secondlastframeid = lastframeid;
                lastframeid = curframeid;
                secondlastframe = lastframe;
                lastframe = curframe;
            } else {
                videoinput_free_frame( vidin, curframeid );
            }

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
            if( !videohold ) output->show_frame();
            gettimeofday( &blitend, 0 );
            lastfieldtime = blitend;
            blittime = timediff( &blitend, &blitstart );
        }


        if( osd ) {
            tvtime_osd_advance_frame( osd );
        }
    }

    output->shutdown();

    if( verbose ) {
        fprintf( stderr, "tvtime: Cleaning up.\n" );
    }
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

