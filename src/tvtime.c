/**
 * Copyright (c) 2001, 2002 Billy Biggs <vektor@dumbterm.net>.
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
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>
#include "pngoutput.h"
#include "pnginput.h"
#include "videoinput.h"
#include "rtctimer.h"
#include "videotools.h"
#include "mixer.h"
#include "tvtimeosd.h"
#include "tvtimeconf.h"
#include "input.h"
#include "speedy.h"
#include "deinterlace.h"
#include "menu.h"
#include "videocorrection.h"
#include "plugins.h"
#include "performance.h"
#include "taglines.h"
#include "xvoutput.h"
#include "console.h"
#include "vbidata.h"
#include "vbiscreen.h"

/**
 * This is ridiculous, but apparently I need to give my own
 * prototype for this function because of a glibc bug. -Billy
 */
int setresuid( uid_t ruid, uid_t euid, uid_t suid );


/* Number of frames to pause after channel change. */
#define CHANNEL_ACTIVE_DELAY 2

/**
 * Current deinterlacing method.
 */
static deinterlace_method_t *curmethod;
static int curmethodid;

static int fadepos = 0;
static double fadespeed = 65.0;

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

static void build_blue_frame( unsigned char *output, int width, int height )
{
    unsigned char bluergb[ 3 ];
    unsigned char blueycbcr[ 3 ];

    bluergb[ 0 ] = 0;
    bluergb[ 1 ] = 0;
    bluergb[ 2 ] = 0xff;
    rgb24_to_packed444_rec601_scanline( blueycbcr, bluergb, 1 );
    blit_colour_packed422_scanline( output, width * height, blueycbcr[ 0 ],
                                    blueycbcr[ 1 ], blueycbcr[ 2 ] );
}

static void save_last_frame( unsigned char *saveframe, unsigned char *curframe,
                             int width, int height, int savestride, int curstride )
{
    height /= 2;
    height--;
    while( height-- ) {
        blit_packed422_scanline( saveframe, curframe, width );
        saveframe += savestride;
        interpolate_packed422_scanline( saveframe, curframe, curframe + (curstride*2), width );
        saveframe += savestride;
        curframe += (curstride*2);
    }
    blit_packed422_scanline( saveframe, curframe, width );
    saveframe += savestride;
    blit_packed422_scanline( saveframe, curframe, width );
    saveframe += savestride;
}

static void crossfade_frame( unsigned char *output,
                             unsigned char *src1,
                             unsigned char *src2,
                             int width, int height, int outstride,
                             int src1stride, int src2stride, int pos )
{
    while( height-- ) {
        blend_packed422_scanline( output, src1, src2, width, pos );
        output += outstride;
        src1 += src1stride;
        src2 += src2stride;
    }
}

static void pngscreenshot( const char *filename, unsigned char *frame422,
                           int width, int height, int stride )
{
    pngoutput_t *pngout = pngoutput_new( filename, width, height, 0, 0.45 );
    unsigned char *tempscanline = (unsigned char *) malloc( width * 3 );
    int i;

    if( !tempscanline ) {
        pngoutput_delete( pngout );
        return;
    }

    for( i = 0; i < height; i++ ) {
        unsigned char *input422 = frame422 + (i * stride);
        /**
         * This function clearly has bugs:
         * chroma422_to_chroma444_rec601_scanline( tempscanline,
         *                                         input422, width );
         */
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
                                             console_t *con,
                                             vbiscreen_t *vs,
                                             int bottom_field,
                                             int correct_input,
                                             int width,
                                             int frame_height,
                                             int instride,
                                             int outstride )
{
    int scanline = 0;
    int i;

    if( bottom_field ) {
        /* Advance frame pointers to the next input line. */
        curframe += instride;
        lastframe += instride;
        secondlastframe += instride;

        /* Double the top scanline a scanline. */
        blit_packed422_scanline( output, curframe, width );
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( menu ) menu_composite_packed422_scanline( menu, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

        output += outstride;
        scanline++;
    }

    /* Copy a scanline. */
    blit_packed422_scanline( output, curframe, width );

    if( correct_input ) {
        video_correction_correct_packed422_scanline( vc, output, output, width );
    }
    if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
    if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
    if( menu ) menu_composite_packed422_scanline( menu, output, width, 0, scanline );
    if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

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
        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( menu ) menu_composite_packed422_scanline( menu, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );


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
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( menu ) menu_composite_packed422_scanline( menu, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

        output += outstride;
        scanline++;
    }

    if( !bottom_field ) {
        /* Double the bottom scanline. */
        blit_packed422_scanline( output, curframe, width );

        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( menu ) menu_composite_packed422_scanline( menu, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );


        output += outstride;
        scanline++;
    }
}


static void tvtime_build_interlaced_frame( unsigned char *output,
                                           unsigned char *curframe,
                                           video_correction_t *vc,
                                           tvtime_osd_t *osd,
                                           menu_t *menu,
                                           console_t *con,
                                           vbiscreen_t *vs,
                                           int bottom_field,
                                           int correct_input,
                                           int width,
                                           int frame_height,
                                           int instride,
                                           int outstride )
{
    /* unsigned char tempscanline[ 768*2 ]; */
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
    video_correction_t *vc = 0;
    videoinput_t *vidin;
    rtctimer_t *rtctimer;
    int width, height;
    int norm = 0;
    int fieldtime;
    int safetytime;
    int fieldsavailable = 0;
    int verbose;
    tvtime_osd_t *osd;
    unsigned char *colourbars;
    unsigned char *lastframe = 0;
    unsigned char *secondlastframe = 0;
    unsigned char *saveframe = 0;
    unsigned char *fadeframe = 0;
    unsigned char *blueframe = 0;
    const char *tagline;
    int lastframeid;
    int secondlastframeid;
    config_t *ct;
    input_t *in;
    menu_t *menu;
    output_api_t *output;
    performance_t *perf;
    console_t *con;
    int has_signal = 0;
    vbidata_t *vbidata;
    vbiscreen_t *vs;

    setup_speedy_calls();

    greedy_plugin_init();
    videobob_plugin_init();
    greedy2frame_plugin_init();
    twoframe_plugin_init();
    linear_plugin_init();
    weave_plugin_init();
    double_plugin_init();
    linearblend_plugin_init();

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
    safetytime = fieldtime - ((fieldtime*3)/4);
    perf = performance_new( fieldtime );
    if( !perf ) {
        fprintf( stderr, "tvtime: Can't initialize performance monitor.\n" );
        return 1;
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
    if( verbose ) {
        fprintf( stderr, "tvtime: V4L sampling %d pixels per scanline.\n", 
                 width );
    }

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
        fprintf( stderr, "tvtime: No deinterlacing methods "
                         "available, exiting.\n" );
        return 1;
    }
    curmethodid = config_get_preferred_deinterlace_method( ct );
	if( curmethodid >= get_num_deinterlace_methods() ||
		curmethodid < 0) {
		fprintf( stderr, "tvtime: Invalid preferred deinterlace method, "
				 "exiting.\n" );
		return 1;
	}
    curmethod = get_deinterlace_method( curmethodid );

    /* Build colourbars. */
    colourbars = (unsigned char *) malloc( width * height * 2 );
    saveframe = (unsigned char *) malloc( width * height * 2 );
    fadeframe = (unsigned char *) malloc( width * height * 2 );
    blueframe = (unsigned char *) malloc( width * height * 2 );
    if( !colourbars || !saveframe || !fadeframe || !blueframe ) {
        fprintf( stderr, "tvtime: Can't allocate extra frame storage memory.\n" );
        return 1;
    }
    build_colourbars( colourbars, width, height );
    build_blue_frame( blueframe, width, height );
    blit_packed422_scanline( saveframe, blueframe, width * height );


    /* Setup OSD stuff. */
    osd = tvtime_osd_new( ct, width, height, config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0) );
    if( !osd ) {
        fprintf( stderr, "Can't initialize OSD object.\n" );
    } else {
        tvtime_osd_set_timeformat( osd, config_get_timeformat( ct ) );
        tvtime_osd_set_input( osd, videoinput_get_input_name( vidin ) );
        tvtime_osd_set_norm( osd, videoinput_norm_name( videoinput_get_norm( vidin ) ) );
        tvtime_osd_set_deinterlace_method( osd, curmethod->name );
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

    menu = menu_new( in, ct, vidin, osd, width, height, 
                     config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0) );
    if( !menu ) {
        fprintf( stderr, "tvtime: Can't create menu.\n" );
    }
    if( menu ) input_set_menu( in, menu );

    /* Steal system resources in the name of performance. */
    if( verbose ) {
        fprintf( stderr, "tvtime: Attempting to aquire "
                         "performance-enhancing features.\n" );
    }
    if( setpriority( PRIO_PROCESS, 0, config_get_priority( ct ) ) < 0 && verbose ) {
        fprintf( stderr, "tvtime: Can't renice to %d.\n", config_get_priority( ct ) );
    }
/* This is bad now. --Doug
    if( mlockall( MCL_CURRENT | MCL_FUTURE ) && verbose ) {
        fprintf( stderr, "tvtime: Can't use mlockall() to lock memory.\n" );
    }
*/
    if( !set_realtime_priority( 0 ) && verbose ) {
        fprintf( stderr, "tvtime: Can't set realtime priority (need root).\n" );
    }
    rtctimer = rtctimer_new( verbose );
    if( !rtctimer ) {
        if( verbose ) {
            fprintf( stderr, "tvtime: Can't open /dev/rtc (needed for accurate blit scheduling).\n" );
        }
    } else {
        if( !rtctimer_set_interval( rtctimer, 1024 ) && !rtctimer_set_interval( rtctimer, 64 ) ) {
            rtctimer_delete( rtctimer );
            rtctimer = 0;
        } else {
            rtctimer_start_clock( rtctimer );

            if( rtctimer_get_resolution( rtctimer ) < 1024 ) {
                fprintf( stderr, "tvtime: Can't get 1024hz resolution from /dev/rtc.\n" );
            }
        }
    }
    if( verbose ) {
        fprintf( stderr, "tvtime: Done stealing resources.\n" );
    }

    /* We've now stolen all our root-requiring resources, drop to a user. */
    if( setresuid( getuid(), getuid(), getuid() ) == -1 ) {
        fprintf( stderr, "tvtime: Unknown problems dropping root access: %s\n", strerror( errno ) );
        return 1;
    }

    /* Setup the console */
    con = console_new( ct, (width*10)/100, height - (height*20)/100, (width*80)/100, (height*20)/100, 10,
                       width, height, 
                       config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0) );
    if( !con ) {
        fprintf( stderr, "tvtime: Could not setup console.\n" );
    }


    if( con ) {
        console_setup_pipe( con, config_get_command_pipe( ct ) );
        input_set_console( in, con );
    }

    vs = vbiscreen_new( width, height, 
                        config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0), 
                        verbose );
    if( !vs ) {
        fprintf( stderr, "tvtime: Could not create vbiscreen.\n" );
        return 1;
    }

    /* Open the VBI device. */
    vbidata = vbidata_new( "/dev/vbi0", vs, verbose );
    if( !vbidata ) {
        fprintf( stderr, "tvtime: Could not create vbidata.\n" );
    } else {
        vbidata_capture_mode( vbidata, CAPTURE_CC1 );
    }

    /* Setup the output. */
    output = get_xv_output();
    if( !output->init( width, height, config_get_outputwidth( ct ), 
                       config_get_aspect( ct ) ) ) {
        fprintf( stderr, "tvtime: XVideo output failed to initialize: "
                         "no video output available.\n" );
        return 1;
    }

    input_set_vbidata( in, vbidata );

    srand( time( 0 ) );
    tagline = taglines[ rand() % numtaglines ];
    output->set_window_caption( tagline );

    if( config_get_fullscreen( ct ) ) {
        output->toggle_fullscreen( 0, 0 );
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
    for(;;) {
        unsigned char *curframe = 0;
        int curframeid;
        int printdebug = 0;
        int showbars, screenshot;
        int aquired = 0;
        int tuner_state;
        int we_were_late = 0;

        output->poll_events( in );

        if( input_quit( in ) ) break;
        printdebug = input_print_debug( in );
        showbars = input_show_bars( in );
        screenshot = input_take_screenshot( in );
        if( input_toggle_fullscreen( in ) ) {
            output->toggle_fullscreen( 0, 0 );
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
                tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                tvtime_osd_show_info( osd );
            }
        }
        input_next_frame( in );

        /* Aquire the next frame. */
        tuner_state = videoinput_check_for_signal( vidin );

        if( has_signal && tuner_state != TUNER_STATE_HAS_SIGNAL ) {
            has_signal = 0;
            save_last_frame( saveframe, lastframe, width, height, width*2, width*2 );
            fadepos = 0;
        }

        if( tuner_state == TUNER_STATE_HAS_SIGNAL ) {
            has_signal = 1;
            if( osd ) tvtime_osd_signal_present( osd, 1 );
        } else if( tuner_state == TUNER_STATE_NO_SIGNAL ) {
            if( osd ) tvtime_osd_signal_present( osd, 0 );
            if( fadepos < 256 ) {
                crossfade_frame( fadeframe, saveframe, blueframe, width,
                                 height, width*2, width*2, width*2, fadepos );
                fadepos += fadespeed;
            }
        }

        if( tuner_state == TUNER_STATE_HAS_SIGNAL || tuner_state == TUNER_STATE_SIGNAL_DETECTED ) {
            curframe = videoinput_next_frame( vidin, &curframeid );
            aquired = 1;
        }

        if( tuner_state != TUNER_STATE_HAS_SIGNAL ) {
            if( fadepos == 0 ) {
                secondlastframe = lastframe = curframe = saveframe;
            } else if( fadepos >= 256 ) {
                secondlastframe = lastframe = curframe = blueframe;
            } else {
                secondlastframe = lastframe = curframe = fadeframe;
            }
        }
        performance_checkpoint_aquired_input_frame( perf );

        /* Print statistics and check for missed frames. */
        if( printdebug ) {
            performance_print_last_frame_stats( perf );
            fprintf( stderr, "Speedy time last frame: %dus\n", speedy_get_usecs() );
        }
        if( config_get_debug( ct ) ) {
            performance_print_frame_drops( perf );
        }
        speedy_reset_timer();

        if( output->is_interlaced() ) {
            /* Wait until we can draw the even field. */
            output->wait_for_sync( 0 );

            output->lock_output_buffer();
            tvtime_build_interlaced_frame( output->get_output_buffer(),
                       curframe, vc, osd, menu, con, vs, 0,
                       vc && config_get_apply_luma_correction( ct ),
                       width, height, width * 2, output->get_output_stride() );
            output->unlock_output_buffer();
        } else {
            /* Build the output from the top field. */
            output->lock_output_buffer();
            if( showbars ) {
                blit_packed422_scanline( output->get_output_buffer(),
                                         colourbars, width*height );
            } else {
                tvtime_build_deinterlaced_frame( output->get_output_buffer(),
                                   curframe, lastframe, secondlastframe,
                                   vc, osd, menu, con, vs, 0,
                                   vc && config_get_apply_luma_correction( ct ),
                                   width, height, width * 2, output->get_output_stride() );
            }

            /* For the screenshot, use the output after we build the top field. */
            if( screenshot ) {
                char filename[ 256 ];
                char timestamp[ 50 ];
                time_t tm = time( 0 );
                strftime( timestamp, sizeof( timestamp ),
                          config_get_timeformat( ct ), localtime( &tm ) );
                sprintf( filename, "tvtime-output-%s.png", timestamp );
                pngscreenshot( filename, output->get_output_buffer(),
                               width, height, width * 2 );
            }
            output->unlock_output_buffer();
            performance_checkpoint_constructed_top_field( perf );


            /* Wait until it's time to blit the first field. */
            if( rtctimer ) {

                we_were_late = 1;
                while( performance_get_usecs_since_frame_aquired( perf )
                       < ( fieldtime - safetytime
                           - performance_get_usecs_of_last_blit( perf )
                           - ( rtctimer_get_usecs( rtctimer ) / 2 ) ) ) {
                    rtctimer_next_tick( rtctimer );
                    we_were_late = 0;
                }

            }
            performance_checkpoint_delayed_blit_top_field( perf );

            performance_checkpoint_blit_top_field_start( perf );
            output->show_frame();
            performance_checkpoint_blit_top_field_end( perf );
        }

        if( output->is_interlaced() ) {
            /* Wait until we can draw the odd field. */
            output->wait_for_sync( 1 );

            output->lock_output_buffer();
            tvtime_build_interlaced_frame( output->get_output_buffer(),
                       curframe, vc, osd, menu, con, vs, 1,
                       vc && config_get_apply_luma_correction( ct ),
                       width, height, width * 2, output->get_output_stride() );
            output->unlock_output_buffer();
        } else {
            /* Build the output from the bottom field. */
            output->lock_output_buffer();
            if( showbars ) {
                blit_packed422_scanline( output->get_output_buffer(),
                                         colourbars, width*height );
            } else {
                tvtime_build_deinterlaced_frame( output->get_output_buffer(),
                                  curframe, lastframe,
                                  secondlastframe, vc, osd, menu, con, vs, 1,
                                  vc && config_get_apply_luma_correction( ct ),
                                  width, height, width * 2, output->get_output_stride() );
            }
            output->unlock_output_buffer();
            performance_checkpoint_constructed_bot_field( perf );
        }

        /* We're done with the input now. */
        if( aquired ) {
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

            if( vbidata ) vbidata_process_frame( vbidata, printdebug );

        }


        if( !output->is_interlaced() ) {
            /* Wait for the next field time. */
            if( rtctimer && !we_were_late ) {

                while( performance_get_usecs_since_last_field( perf )
                       < ( fieldtime
                           - performance_get_usecs_of_last_blit( perf )
                           - ( rtctimer_get_usecs( rtctimer ) / 2 ) ) ) {
                    rtctimer_next_tick( rtctimer );
                }

            }
            performance_checkpoint_delayed_blit_bot_field( perf );

            /* Display the bottom field. */
            performance_checkpoint_blit_bot_field_start( perf );
            output->show_frame();
            performance_checkpoint_blit_bot_field_end( perf );
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
    if( vbidata ) {
        vbidata_delete( vbidata );
    }
    if( vc ) {
        video_correction_delete( vc );
    }
    if( rtctimer ) {
        rtctimer_delete( rtctimer );
    }
    if( menu ) {
        menu_delete( menu );
    }
    if( con ) {
        console_delete( con );
    }
    if( vs ) {
        vbiscreen_delete( vs );
    }
    return 0;
}

