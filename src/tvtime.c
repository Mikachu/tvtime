/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <termio.h>
#include <stdint.h>
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
#include "plugins.h"
#include "performance.h"
#include "taglines.h"
#include "xvoutput.h"
#include "console.h"
#include "vbidata.h"
#include "vbiscreen.h"
#include "videofilter.h"
#include "fifo.h"
#include "commands.h"
#include "station.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_DIRECTFB
#include "dfboutput.h"
#endif

#include "rvrreader.h"
#include "pulldown.h"

/**
 * Michael Niedermayer's magic FIRs
 *
 * [-1 14 3]
 * [-9 111 29 -3]
 */

/**
 * Set this to 1 to enable startup time profiling.
 */
const unsigned int profile_startup = 0;

/**
 * Function for profiling.
 */
static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}


/**
 * Which pulldown algorithm we're using.
 */
enum {
    PULLDOWN_NONE = 0,
    PULLDOWN_VEKTOR = 1,
    PULLDOWN_DALIAS = 2,
    PULLDOWN_MAX = 3,
};
static unsigned int pulldown_alg = 0;

/**
 * Which output driver we're using.
 */
enum {
    OUTPUT_XV = 0,
    OUTPUT_DIRECTFB = 1,
};
static unsigned int output_driver = 0;

/**
 * Current deinterlacing method.
 */
static deinterlace_method_t *curmethod;
static int curmethodid;

/**
 * Speed at which to fade to blue on channel changes.
 */
const double fadespeed = 65.0;

/**
 * Number of frames to wait before detecting a signal.
 */
const int scan_delay = 10;

static void build_colourbars( uint8_t *output, int width, int height )
{
    uint8_t *cb444 = malloc( width * height * 3 );
    int i;
    if( !cb444 ) { memset( output, 255, width * height * 2 ); return; }

    create_colourbars_packed444( cb444, width, height, width*3 );
    for( i = 0; i < height; i++ ) {
        uint8_t *curout = output + (i * width * 2);
        uint8_t *curin = cb444 + (i * width * 3);
        packed444_to_packed422_scanline_c( curout, curin, width );
    }

    free( cb444 );
}

static void build_blue_frame( uint8_t *output, int width, int height )
{
    uint8_t bluergb[ 3 ];
    uint8_t blueycbcr[ 3 ];

    bluergb[ 0 ] = 0;
    bluergb[ 1 ] = 0;
    bluergb[ 2 ] = 0xff;
    rgb24_to_packed444_rec601_scanline( blueycbcr, bluergb, 1 );
    blit_colour_packed422_scanline( output, width * height, blueycbcr[ 0 ],
                                    blueycbcr[ 1 ], blueycbcr[ 2 ] );
}

static void save_last_frame( uint8_t *saveframe, uint8_t *curframe,
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

static void pngscreenshot( const char *filename, uint8_t *frame422,
                           int width, int height, int stride )
{
    pngoutput_t *pngout = pngoutput_new( filename, width, height, 0, 0.45, 0 );
    uint8_t *tempscanline = malloc( width * 3 );
    int i;

    if( !tempscanline ) {
        if( pngout ) pngoutput_delete( pngout );
        return;
    }

    if( !pngout ) {
        free( tempscanline );
        return;
    }

    for( i = 0; i < height; i++ ) {
        uint8_t *input422 = frame422 + (i * stride);
        packed422_to_packed444_rec601_scanline( tempscanline, input422, width );
        packed444_to_rgb24_rec601_scanline( tempscanline, tempscanline, width );
        pngoutput_scanline( pngout, tempscanline );
    }

    pngoutput_delete( pngout );
    free( tempscanline );
}

/**
 * This is how many frames to wait until deciding if the pulldown phase
 * has changed or if we've really found a pulldown sequence.  This is
 * currently set to about 1 second, that is, we won't go into film mode
 * until we've seen a pulldown sequence successfully for 1 second.
 */
#define PULLDOWN_ERROR_WAIT     60

/**
 * This is how many predictions have to be incorrect before we fall back to
 * video mode.  Right now, if we mess up, we jump to video mode immediately.
 */
#define PULLDOWN_ERROR_THRESHOLD 2


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
 * | Field 3 | Field 2 | Field 1 | Field 0 |
 * |         |   T2    |         |   T0    |
 * |   M3    |         |    M1   |         |
 * |         |   B2    |         |   B0    |
 * |  NX3    |         |   NX1   |         |
 *
 * So, since we currently get frames not individual fields from V4L, there
 * are two possibilities for where these come from:
 *
 * CASE 1: Deinterlacing the top field:
 * | Field 4 | Field 3 | Field 2 | Field 1 | Field 0 |
 * |   T4    |         |   T2    |         |   T0    |
 * |         |   M3    |         |    M1   |         |
 * |   B4    |         |   B2    |         |   B0    |
 *  [--  secondlast --] [--  lastframe  --] [--  curframe   --]
 *
 * CASE 2: Deinterlacing the bottom field:
 * | Field 4 | Field 3 | Field 2 | Field 1 | Field 0 |
 * |   T4    |         |   T2    |         |   T0    |
 * |         |   M3    |         |    M1   |         |
 * |   B4    |         |   B2    |         |   B0    |
 * ndlast --] [--  lastframe  --] [--  curframe   --]
 *
 * So, in case 1, we need the previous 2 frames as well as the current
 * frame, and in case 2, we only need the previous frame, since the
 * current frame contains both Field 3 and Field 4.
 */
static int pdoffset = PULLDOWN_OFFSET_1;
static int pderror = PULLDOWN_ERROR_WAIT;
static int pdlastbusted = 0;
static int filmmode = 0;

static int last_topdiff = 0;
static int last_botdiff = 0;

static videofilter_t *filter = 0;
static int filtered_cur = 0;

static int pulldown_merge = 0;
static int pulldown_copy = 0;
static int did_copy_top = 0;
static int last_fieldcount = 0;

static void tvtime_do_pulldown_action( uint8_t *output,
                                       uint8_t *curframe,
                                       uint8_t *lastframe,
                                       uint8_t *secondlastframe,
                                       tvtime_osd_t *osd,
                                       console_t *con,
                                       vbiscreen_t *vs,
                                       int bottom_field,
                                       int width,
                                       int frame_height,
                                       int instride,
                                       int outstride,
                                       int action )
{
    unsigned int topdiff = 0;
    unsigned int botdiff = 0;
    int i;

    for( i = 0; i < frame_height; i++ ) {
        uint8_t *curoutput = output + (i * outstride);

        if( filter && !filtered_cur && videofilter_active_on_scanline( filter, i ) ) {
            videofilter_packed422_scanline( filter, curframe + (i*instride), width, 0, i );
        }

        if( pulldown_alg == PULLDOWN_VEKTOR ) {
            if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
                topdiff += diff_factor_packed422_scanline( curframe + (i*instride),
                                                           lastframe + (i*instride), width );
                botdiff += diff_factor_packed422_scanline( curframe + (i*instride) + instride,
                                                           lastframe + (i*instride) + instride,
                                                           width );
            }
        }

        if( action != PULLDOWN_ACTION_DROP2 ) {
            if( action == PULLDOWN_ACTION_MRGE3 && (i & 1) ) {
                blit_packed422_scanline( curoutput, lastframe + (i*instride), width );
            } else {
                blit_packed422_scanline( curoutput, curframe + (i*instride), width );
            }
            if( vs ) vbiscreen_composite_packed422_scanline( vs, curoutput, width, 0, i );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, curoutput, width, 0, i );
            if( con ) console_composite_packed422_scanline( con, curoutput, width, 0, i );
        }
    }

    last_topdiff = topdiff;
    last_botdiff = botdiff;
    filtered_cur = 1;
}

static void tvtime_build_deinterlaced_frame( uint8_t *output,
                                             uint8_t *curframe,
                                             uint8_t *lastframe,
                                             uint8_t *secondlastframe,
                                             tvtime_osd_t *osd,
                                             console_t *con,
                                             vbiscreen_t *vs,
                                             int bottom_field,
                                             int width,
                                             int frame_height,
                                             int instride,
                                             int outstride )
{
    unsigned int topdiff = 0;
    unsigned int botdiff = 0;
    int i;

    if( pulldown_alg == PULLDOWN_NONE ) {
        if( osd ) tvtime_osd_set_film_mode( osd, -1 );
    }

    if( pulldown_alg != PULLDOWN_VEKTOR ) {
        /* If we leave vektor pulldown mode, lose our state. */
        filmmode = 0;
    }

    /* Make pulldown decisions every top field. */
    if( (pulldown_alg == PULLDOWN_VEKTOR) && !bottom_field ) {
        int predicted;

        predicted = pdoffset << 1;
        if( predicted > PULLDOWN_OFFSET_5 ) predicted = PULLDOWN_OFFSET_1;

        /**
         * Old algorithm:
        pdoffset = determine_pulldown_offset_history( last_topdiff, last_botdiff, 1, &realbest );
        if( pdoffset & predicted ) { pdoffset = predicted; } else { pdoffset = realbest; }
         */

        pdoffset = determine_pulldown_offset_short_history_new( last_topdiff, last_botdiff, 1, predicted );
        //pdoffset = determine_pulldown_offset_history_new( last_topdiff, last_botdiff, 1, predicted );

        /* 3:2 pulldown state machine. */
        if( !pdoffset ) {
            /* No pulldown offset applies, drop out of pulldown immediately. */
            pdlastbusted = 0;
            pderror = PULLDOWN_ERROR_WAIT;
        } else if( pdoffset != predicted ) {
            if( pdlastbusted ) {
                pdlastbusted--;
                pdoffset = predicted;
            } else {
                pderror = PULLDOWN_ERROR_WAIT;
            }
        } else {
            if( pderror ) {
                pderror--;
            }

            if( !pderror ) {
                pdlastbusted = PULLDOWN_ERROR_THRESHOLD;
            }
        }


        if( !pderror ) {
            int curoffset = pdoffset << 1;
            if( curoffset > PULLDOWN_OFFSET_5 ) curoffset = PULLDOWN_OFFSET_1;

            // We're in pulldown, reverse it.
            if( !filmmode ) {
                fprintf( stderr, "Film mode enabled.\n" );
                if( osd ) tvtime_osd_set_film_mode( osd, curoffset );
                filmmode = 1;
            }
            if( curoffset == PULLDOWN_OFFSET_2 ) {
                // Drop.
                tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                           osd, con, vs, bottom_field, width,
                                           frame_height, instride, outstride,
                                           PULLDOWN_ACTION_DROP2 );
            } else if( curoffset == PULLDOWN_OFFSET_3 ) {
                // Merge.
                tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                           osd, con, vs, bottom_field, width,
                                           frame_height, instride, outstride,
                                           PULLDOWN_ACTION_MRGE3 );
            } else if( curoffset == PULLDOWN_OFFSET_4 ) {
                // Copy.
                tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                           osd, con, vs, bottom_field, width,
                                           frame_height, instride, outstride,
                                           PULLDOWN_ACTION_COPY4 );
            }
            return;
        } else {
            if( filmmode ) {
                fprintf( stderr, "Film mode disabled.\n" );
                if( osd ) tvtime_osd_set_film_mode( osd, -1 );
                filmmode = 0;
            }
        }
    } else if( (pulldown_alg == PULLDOWN_VEKTOR) && !pderror ) {
        int curoffset = pdoffset << 1;
        if( curoffset > PULLDOWN_OFFSET_5 ) curoffset = PULLDOWN_OFFSET_1;

        if( curoffset == PULLDOWN_OFFSET_1 || curoffset == PULLDOWN_OFFSET_5 ) {
            // Copy.
            tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                       osd, con, vs, bottom_field, width,
                                       frame_height, instride, outstride,
                                       PULLDOWN_ACTION_COPY1 );
        } else if( curoffset == PULLDOWN_OFFSET_5 ) {
            // Copy.
            tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                       osd, con, vs, bottom_field, width,
                                       frame_height, instride, outstride,
                                       PULLDOWN_ACTION_COPY5 );
        }
        return;
    }

    if( pulldown_alg == PULLDOWN_DALIAS ) {
        if( osd ) tvtime_osd_set_film_mode( osd, 0 );

        last_fieldcount++;

        if( !bottom_field ) {
            static pulldown_metrics_t old_peak;
            static pulldown_metrics_t old_rel;
            static pulldown_metrics_t old_mean;
            static int drop_next = 0;
            pulldown_metrics_t new_peak, new_rel, new_mean;
            int drop_cur = 0;

            diff_factor_packed422_frame( &new_peak, &new_rel, &new_mean, lastframe, curframe,
                                         width, frame_height, width*2, width*2 );
            /*
            fprintf( stderr, "stats: pd=%d re=%d ro=%d rt=%d rs=%d rp=%d rd=%d\n",
                    new_peak.d, new_rel.e, new_rel.o, new_rel.t, new_rel.s, new_rel.p, new_rel.d );
            */

            if( drop_next ) {
                tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                           osd, con, vs, bottom_field, width,
                                           frame_height, instride, outstride,
                                           PULLDOWN_ACTION_DROP2 );
                drop_next = 0;
                drop_cur = 1;
                pulldown_copy = 2;
            } else {
                switch(determine_pulldown_offset_dalias( &old_peak, &old_rel, &old_mean,
                                                         &new_peak, &new_rel, &new_mean )) {
                    case PULLDOWN_ACTION_MRGE3: drop_next = 1; pulldown_merge = 1; break;
                    default:
                    case PULLDOWN_ACTION_COPY1: break;
                    case PULLDOWN_ACTION_DROP2: break;
                }
            }
            old_peak = new_peak;
            old_rel = new_rel;
            old_mean = new_mean;
            if( !drop_next && !drop_cur && pulldown_copy ) {
                // Copy.
                tvtime_do_pulldown_action( output, lastframe, curframe, secondlastframe,
                                           osd, con, vs, bottom_field, width,
                                           frame_height, instride, outstride,
                                           PULLDOWN_ACTION_COPY1 );
                pulldown_copy--;
                did_copy_top = 1;
                // fprintf( stderr, ": %d\n", last_fieldcount ); last_fieldcount = 0;
            }
        } else if( pulldown_merge ) {
            // Merge.
            tvtime_do_pulldown_action( output, curframe, lastframe, secondlastframe,
                                       osd, con, vs, bottom_field, width,
                                       frame_height, instride, outstride,
                                       PULLDOWN_ACTION_MRGE3 );
            pulldown_merge = 0;
            // fprintf( stderr, ": %d\n", last_fieldcount ); last_fieldcount = 0;
        } else if( !pulldown_copy ) {
            // Copy.
            if( did_copy_top ) {
                did_copy_top = 0;
            } else {
                tvtime_do_pulldown_action( output, lastframe, curframe, secondlastframe,
                                           osd, con, vs, bottom_field, width,
                                           frame_height, instride, outstride,
                                           PULLDOWN_ACTION_COPY4 );
                // fprintf( stderr, ": %d\n", last_fieldcount ); last_fieldcount = 0;
            }
        }
        return;
    }


    if( !curmethod->scanlinemode ) {
        deinterlace_frame_data_t data;

        if( ((pulldown_alg == PULLDOWN_VEKTOR) && !bottom_field) || (filter && !filtered_cur) ) {
            for( i = 0; i < frame_height; i++ ) {
                if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
                    topdiff += diff_factor_packed422_scanline( curframe + (i*instride),
                                                               lastframe + (i*instride), width );
                    botdiff += diff_factor_packed422_scanline( curframe + (i*instride) + instride,
                                                               lastframe + (i*instride) + instride, width );
                }
                if( videofilter_active_on_scanline( filter, i ) ) {
                    videofilter_packed422_scanline( filter, curframe + (i*instride), width, 0, i );
                }
            }
        }
        last_topdiff = topdiff;
        last_botdiff = botdiff;

        data.f0 = curframe;
        data.f1 = lastframe;
        data.f2 = secondlastframe;

        curmethod->deinterlace_frame( output, &data, bottom_field, width, frame_height );

        for( i = 0; i < frame_height; i++ ) {
            uint8_t *curoutput = output + (i * outstride);
            if( vs ) vbiscreen_composite_packed422_scanline( vs, curoutput, width, 0, i );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, curoutput, width, 0, i );
            if( con ) console_composite_packed422_scanline( con, curoutput, width, 0, i );
        }
    } else {
        int loop_size;
        int scanline = 0;

        if( bottom_field ) {
            /* Advance frame pointers to the next input line. */
            curframe += instride;
            lastframe += instride;
            secondlastframe += instride;

            /* Double the top scanline a scanline. */
            blit_packed422_scanline( output, curframe, width );
            if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

            output += outstride;
            scanline++;
        }

        /* Copy a scanline. */
        if( filter && !filtered_cur && videofilter_active_on_scanline( filter, scanline ) ) {
            videofilter_packed422_scanline( filter, curframe, width, 0, scanline );
        }
        blit_packed422_scanline( output, curframe, width );
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

        output += outstride;
        scanline++;

        /* Something is wrong here. -Billy */
        loop_size = ((frame_height - 2) / 2) - bottom_field;
        for( i = loop_size; i; --i ) {
            deinterlace_scanline_data_t data;

            data.bottom_field = bottom_field;

            data.t0 = curframe;
            data.b0 = curframe + (instride*2);

            if( filter && !filtered_cur && videofilter_active_on_scanline( filter, scanline + 1 ) ) {
                videofilter_packed422_scanline( filter, curframe + instride, width, 0, scanline );
                videofilter_packed422_scanline( filter, curframe + (instride*2), width, 0, scanline + 1 );
            }

            if( bottom_field ) {
                data.tt1 = (i < loop_size) ? (curframe - instride) : (curframe + instride);
                data.m1  = curframe + instride;
                data.bb1 = i ? (curframe + (instride*3)) : (curframe + instride);
            } else {
                data.tt1 = (i < loop_size) ? (lastframe - instride) : (lastframe + instride);
                data.m1  = lastframe + instride;
                data.bb1 = i ? (lastframe + (instride*3)) : (lastframe + instride);
            }

            data.t2 = lastframe;
            data.b2 = lastframe + (instride*2);

            if( bottom_field ) {
                data.tt3 = (i < loop_size) ? (lastframe - instride) : (lastframe + instride);
                data.m3  = lastframe + instride;
                data.bb3 = i ? (lastframe + (instride*3)) : (lastframe + instride);
            } else {
                data.tt3 = (i < loop_size) ? (secondlastframe - instride) : (secondlastframe + instride);
                data.m3  = secondlastframe + instride;
                data.bb3 = i ? (secondlastframe + (instride*3)) : (secondlastframe + instride);
            }

            curmethod->interpolate_scanline( output, &data, width );
            if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

            output += outstride;
            scanline++;

            data.tt0 = curframe;
            data.m0  = curframe + (instride*2);
            data.bb0 = i ? (curframe + (instride*4)) : (curframe + (instride*2));

            if( bottom_field ) {
                data.t1 = curframe + instride;
                data.b1 = curframe + (instride*3);
            } else {
                data.t1 = lastframe + instride;
                data.b1 = lastframe + (instride*3);
            }

            data.tt2 = lastframe;
            data.m2  = lastframe + (instride*2);
            data.bb2 = i ? (lastframe + (instride*4)) : (lastframe + (instride*2));

            if( bottom_field ) {
                data.t2 = lastframe + instride;
                data.b2 = lastframe + (instride*3);
            } else {
                data.t2 = secondlastframe + instride;
                data.b2 = secondlastframe + (instride*3);
            }

            /* Copy a scanline. */
            if( (pulldown_alg == PULLDOWN_VEKTOR) && !bottom_field && scanline > 40 && (scanline & 3) == 0 && scanline < frame_height - 40 ) {
                topdiff += diff_factor_packed422_scanline( curframe, lastframe, width );
                botdiff += diff_factor_packed422_scanline( curframe + instride, lastframe + instride, width );
            }
            curmethod->copy_scanline( output, &data, width );
            curframe += instride * 2;
            lastframe += instride * 2;
            secondlastframe += instride * 2;

            if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

            output += outstride;
            scanline++;
        }

        if( !bottom_field ) {
            last_topdiff = topdiff;
            last_botdiff = botdiff;

            /* Double the bottom scanline. */
            blit_packed422_scanline( output, curframe, width );

            if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );


            output += outstride;
            scanline++;
        }
    }

    filtered_cur = 1;
}

static void tvtime_build_interlaced_frame( uint8_t *output,
                                           uint8_t *curframe,
                                           tvtime_osd_t *osd,
                                           console_t *con,
                                           vbiscreen_t *vs,
                                           int bottom_field,
                                           int width,
                                           int frame_height,
                                           int instride,
                                           int outstride,
                                           int interleaved )
{
    /* uint8_t tempscanline[ 768*2 ]; */
    int scanline = 0;
    int i;

    if( !interleaved ) {
        if( bottom_field ) {
            /* Advance frame pointers to the next input line. */
            curframe += instride;
  
            /* Skip the top scanline. */
            output += outstride;
            scanline++;
        }

        /* Copy a scanline. */
        blit_packed422_scanline( output, curframe, width );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );

        output += outstride;
        scanline++;

        for( i = ((frame_height - 2) / 2); i; --i ) {
            /* Skip a scanline. */
            output += outstride;
            scanline++;

            /* Copy a scanline. */
            curframe += instride * 2;
            blit_packed422_scanline( output, curframe, width );
  
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );

            output += outstride;
            scanline++;
        }
    } else {
        for( i = frame_height; i; --i ) {

            /* Copy a scanline. */
            blit_packed422_scanline( output, curframe, width );
  
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            curframe += instride;         
            output += outstride;
            scanline++;
        }
    }

}


static void tvtime_build_copied_field( uint8_t *output,
                                       uint8_t *curframe,
                                       tvtime_osd_t *osd,
                                       console_t *con,
                                       vbiscreen_t *vs,
                                       int bottom_field,
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
    }

    /* Copy a scanline. */
    if( filter && !filtered_cur && videofilter_active_on_scanline( filter, scanline + bottom_field ) ) {
        videofilter_packed422_scanline( filter, curframe, width, 0, scanline + bottom_field );
    }
    // blit_packed422_scanline( output, curframe, width );
    quarter_blit_vertical_packed422_scanline( output, curframe + (instride*2), curframe, width );

    if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
    if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
    if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

    curframe += instride * 2;
    output += outstride;
    scanline += 2;

    for( i = ((frame_height - 2) / 2); i; --i ) {
        /* Copy/interpolate a scanline. */
        if( bottom_field ) {
            if( filter && !filtered_cur && videofilter_active_on_scanline( filter, scanline + 1 ) ) {
                videofilter_packed422_scanline( filter, curframe, width, 0, scanline + 1 );
            }
            // interpolate_packed422_scanline( output, curframe, curframe - (instride*2), width );
            quarter_blit_vertical_packed422_scanline( output, curframe - (instride*2), curframe, width );
        } else {
            if( filter && !filtered_cur && videofilter_active_on_scanline( filter, scanline ) ) {
                videofilter_packed422_scanline( filter, curframe, width, 0, scanline );
            }
            // blit_packed422_scanline( output, curframe, width );
            if( i > 1 ) {
                quarter_blit_vertical_packed422_scanline( output, curframe + (instride*2), curframe, width );
            } else {
                blit_packed422_scanline( output, curframe, width );
            }
        }
        curframe += instride * 2;

        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

        output += outstride;
        scanline += 2;
    }
}

int main( int argc, char **argv )
{
    struct timeval startup_time;
    videoinput_t *vidin = 0;
    rtctimer_t *rtctimer = 0;
    station_mgr_t *stationmgr = 0;
    rvrreader_t *rvrreader = 0;
    int width = 720;
    int height = 480;
    int norm = 0;
    int fieldtime;
    int safetytime;
    int fieldsavailable = 0;
    int verbose;
    int send_fields;
    tvtime_osd_t *osd = 0;
    uint8_t *colourbars;
    uint8_t *lastframe = 0;
    uint8_t *secondlastframe = 0;
    uint8_t *saveframe = 0;
    uint8_t *fadeframe = 0;
    uint8_t *blueframe = 0;
    uint8_t *curframe = 0;
    const char *tagline;
    int curframeid;
    int lastframeid;
    int secondlastframeid;
    config_t *ct = 0;
    input_t *in = 0;
    output_api_t *output = 0;
    performance_t *perf = 0;
    console_t *con = 0;
    int has_signal = 0;
    vbidata_t *vbidata = 0;
    vbiscreen_t *vs = 0;
    DIR *fifodir = 0;
    fifo_t *fifo = 0;
    commands_t *commands = 0;
    int usevbi = 1;
    int fadepos = 0;
    int scanwait = scan_delay;
    int scanning = 0;
    int numscanned = 0;
    int framerate_mode = -1;
    int preset_mode = -1;
    char kbd_cmd[ 1024 ];
    int kbd_pos = 0;
    int kbd_available;
    char *error_string = 0;
    int read_stdin = 1;
    double pixel_aspect;
    int i;

    gettimeofday( &startup_time, 0 );

    fprintf( stderr, "tvtime: Running %s.\n", PACKAGE_STRING );

    /* Disable this code for a release. */
    fprintf( stderr, "\n*** WARNING: you are running a DEVELOPMENT version of tvtime.\n" );
    fprintf( stderr,   "*** We often break stuff during development.  Please submit bug reports\n"
                       "*** based on released versions only!!\n\n" );

    /* Ditch stdin early. */
    if( isatty( STDIN_FILENO ) ) {
        read_stdin = 0;
        close( STDIN_FILENO );
    }

    ct = config_new();
    if( !ct ) {
        fprintf( stderr, "tvtime: Can't set configuration options, exiting.\n" );
        return 1;
    }

    /* Parse command line arguments. */
    if( !config_parse_tvtime_command_line( ct, argc, argv ) ) {
        config_delete( ct );
        return 1;
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Parsed config.\n", timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    verbose = config_get_verbose( ct );

    send_fields = config_get_send_fields( ct );
    if( verbose ) {
        if( send_fields ) {
            fprintf( stderr, "tvtime: Sending fields to interlaced output devices.\n" );
        } else {
            fprintf( stderr, "tvtime: Sending frames to interlaced output devices.\n" );
        }
    }

    if( config_get_output_driver( ct ) ) {
        if( !strcasecmp( config_get_output_driver( ct ), "directfb" ) ) {
            fprintf( stderr, "tvtime: Using DirectFB output driver.\n" );
            output_driver = OUTPUT_DIRECTFB;
        } else {
            output_driver = OUTPUT_XV;
        }
    } else {
        output_driver = OUTPUT_XV;
    }


    /* Steal system resources in the name of performance. */
    /* FIXME: This is currently disabled for the DirectFB output while
     *        we do some testing for some odd problems. */
    if( output_driver != OUTPUT_DIRECTFB ) {
        if( setpriority( PRIO_PROCESS, 0, config_get_priority( ct ) ) < 0 && verbose ) {
            fprintf( stderr, "tvtime: Can't renice to %d.\n", config_get_priority( ct ) );
        }

        if( !set_realtime_priority( 0 ) && verbose ) {
            fprintf( stderr, "tvtime: Can't set realtime priority (need root).\n" );
        }
    }

    rtctimer = rtctimer_new( verbose );
    if( !rtctimer ) {
        fprintf( stderr, "\n*** /dev/rtc support is needed for smooth video.  We STRONGLY recommend\n"
                         "*** that you load the 'rtc' kernel module before starting tvtime,\n"
                         "*** and make sure that your user has access to the device file.\n"
                         "*** See our support page at http://tvtime.sourceforge.net/ for more information\n\n" );
    } else {
        if( !rtctimer_set_interval( rtctimer, 1024 ) && !rtctimer_set_interval( rtctimer, 64 ) ) {
            rtctimer_delete( rtctimer );
            rtctimer = 0;
        } else {
            rtctimer_start_clock( rtctimer );

            if( rtctimer_get_resolution( rtctimer ) < 1024 ) {
                fprintf( stderr, "\n*** /dev/rtc support is needed for smooth video.  Support is available,\n"
                         "*** but tvtime cannot get 1024hz resolution!  Please run tvtime as root,\n"
                         "*** or, with linux kernel version 2.4.19 or later, run:\n"
                         "***       sysctl -w dev.rtc.max-user-freq=1024\n"
                         "*** See our support page at http://tvtime.sourceforge.net/ for more information\n\n" );
            }
        }
    }

    /* We've now stolen all our root-requiring resources, drop to a user. */
#ifdef _POSIX_SAVED_IDS
    if( seteuid( getuid() ) == -1 ) {
#else
    if( setreuid( -1, getuid() ) == -1 ) {
#endif
        fprintf( stderr, 
                 "tvtime: Unknown problems dropping root access: %s\n", 
                 strerror( errno ) );
        return 1;
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Stole resources.\n", timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }


    /* Setup the output. */
    if( output_driver == OUTPUT_DIRECTFB ) {
        output = get_dfb_output();
    } else {
        output = get_xv_output();
    }

    if( !output || !output->init( config_get_outputheight( ct ), config_get_aspect( ct ), verbose ) ) {
        fprintf( stderr, "tvtime: Output driver failed to initialize: "
                         "no video output available.\n" );
        /* FIXME: Delete everything here! */
        return 1;
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Created our output.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }


    /* Setup the speedy calls. */
    setup_speedy_calls( verbose );

    if( !output->is_interlaced() ) {
        dscaler_greedyh_plugin_init();
        greedy_plugin_init();

        linearblend_plugin_init();

        dscaler_greedy2frame_plugin_init();
        dscaler_twoframe_plugin_init();

        dscaler_videobob_plugin_init();
        dscaler_videoweave_plugin_init();
        dscaler_tomsmocomp_plugin_init();

        linear_plugin_init();
        weave_plugin_init();
        double_plugin_init();
        vfir_plugin_init();

        scalerbob_plugin_init();
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Setup deinterlacers.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

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
    if( norm == VIDEOINPUT_NTSC || norm == VIDEOINPUT_NTSC_JP || norm == VIDEOINPUT_PAL_M ) {
        fieldtime = 16683;
    } else {
        fieldtime = 20000;
    }
    safetytime = fieldtime - ((fieldtime*3)/4);
    perf = performance_new( fieldtime );
    if( !perf ) {
        fprintf( stderr, "tvtime: Can't initialize performance monitor, exiting.\n" );
        return 1;
    }

    stationmgr = station_new( videoinput_get_norm_name( norm ), config_get_v4l_freq( ct ),
                              config_get_ntsc_cable_mode( ct ), verbose );
    if( !stationmgr ) {
        fprintf( stderr, "tvtime: Can't create station manager (no memory?), exiting.\n" );
        return 1;
    }
    station_set( stationmgr, config_get_prev_channel( ct ) );
    station_set( stationmgr, config_get_start_channel( ct ) );


    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Setup stations.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }


    /* Default to a width specified on the command line. */
    width = config_get_inputwidth( ct );

    if( config_get_rvr_filename( ct ) ) {
        rvrreader = rvrreader_new( config_get_rvr_filename( ct ) );
        if( !rvrreader ) {
            if( asprintf( &error_string, "Can't open rvr file '%s'.",
                          config_get_rvr_filename( ct ) ) < 0 ) {
                error_string = 0;
            }
            fprintf( stderr, "tvtime: Can't open rvr file '%s'.\n",
                     config_get_rvr_filename( ct ) );
        } else {
            width = rvrreader_get_width( rvrreader );
            height = rvrreader_get_height( rvrreader );
            if( verbose ) {
                fprintf( stderr, "tvtime: RVR frame sampling %d pixels per scanline.\n", width );
            }
        }
    } else {
        vidin = videoinput_new( config_get_v4l_device( ct ), 
                                config_get_inputwidth( ct ), 
                                norm, verbose );
        if( !vidin ) {
            if( asprintf( &error_string, "Can't open video4linux device '%s'.",
                          config_get_v4l_device( ct ) ) < 0 ) {
                error_string = 0;
            }
        } else {
            videoinput_set_input_num( vidin, config_get_inputnum( ct ) );
            width = videoinput_get_width( vidin );
            height = videoinput_get_height( vidin );
            if( verbose ) {
                fprintf( stderr, "tvtime: V4L sampling %d pixels per scanline.\n", width );
            }
        }
    }

    /* Set input size. */
    output->set_input_size( width, height );

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Setup input.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
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
    if( rvrreader ) {
        fieldsavailable = 5;
    } else if( vidin ) {
        if( videoinput_get_numframes( vidin ) < 2 ) {
            fprintf( stderr, "tvtime: Can only get %d frame buffers from V4L.  "
                     "Not enough to continue.  Exiting.\n",
                     videoinput_get_numframes( vidin ) );
            return 1;
        } else if( videoinput_get_numframes( vidin ) == 2 ) {
            fprintf( stderr, "tvtime: Can only get %d frame buffers from V4L.  Limiting deinterlace plugins\n"
                     "tvtime: to those which only need 1 field.\n",
                     videoinput_get_numframes( vidin ) );
            fieldsavailable = 1;
        } else if( videoinput_get_numframes( vidin ) == 3 ) {
            fprintf( stderr, "tvtime: Can only get %d frame buffers from V4L.  Limiting deinterlace plugins\n"
                     "tvtime: to those which only need 3 fields.\n",
                     videoinput_get_numframes( vidin ) );
            fieldsavailable = 3;
        } else {
            fieldsavailable = 5;
        }
        if( fieldsavailable < 5 && videoinput_is_bttv( vidin ) ) {
            fprintf( stderr, "\n*** You are using the bttv driver, but without enough gbuffers available.\n"
                               "*** See the support page at http://tvtime.sourceforge.net/ for information\n"
                               "*** on how to increase your gbuffers setting.\n\n" );
        }
    } else {
        fieldsavailable = 5;
    }

    if( output->is_interlaced() ) {
        curmethodid = 0;
        curmethod = 0;
    } else {
        filter_deinterlace_methods( speedy_get_accel(), fieldsavailable );
        if( !output->is_interlaced() && !get_num_deinterlace_methods() ) {
            fprintf( stderr, "tvtime: No deinterlacing methods "
                             "available, exiting.\n" );
            return 1;
        }
        curmethodid = 0;
        curmethod = get_deinterlace_method( 0 );
        while( strcasecmp( config_get_deinterlace_method( ct ), curmethod->short_name ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            if( !curmethodid ) break;
        }
    }

    /* Build colourbars. */
    colourbars = malloc( width * height * 2 );
    saveframe = malloc( width * height * 2 );
    fadeframe = malloc( width * height * 2 );
    blueframe = malloc( width * height * 2 );
    if( !colourbars || !saveframe || !fadeframe || !blueframe ) {
        fprintf( stderr, "tvtime: Can't allocate extra frame storage memory.\n" );
        return 1;
    }
    build_colourbars( colourbars, width, height );
    build_blue_frame( blueframe, width, height );
    blit_packed422_scanline( saveframe, blueframe, width * height );
    secondlastframe = lastframe = blueframe;


    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Allocated and built temp frames.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }



    /* Setup OSD stuff. */
    pixel_aspect = ( (double) width ) / ( ( (double) height ) * ( config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0) ) );
    osd = tvtime_osd_new( width, height, pixel_aspect,
                          config_get_channel_text_rgb( ct ), config_get_other_text_rgb( ct ) );
    if( !osd ) {
        fprintf( stderr, "tvtime: OSD initialization failed, OSD disabled.\n" );
    } else {
        tvtime_osd_set_timeformat( osd, config_get_timeformat( ct ) );
        if( curmethod ) {
            tvtime_osd_set_deinterlace_method( osd, curmethod->name );
        }
        if( rvrreader ) {
            tvtime_osd_set_input( osd, "RVR" );
            tvtime_osd_set_norm( osd, height == 480 ? "NTSC" : "PAL" );
        } else if( vidin ) {
            tvtime_osd_set_input( osd, videoinput_get_input_name( vidin ) );
            tvtime_osd_set_norm( osd, videoinput_get_norm_name( videoinput_get_norm( vidin ) ) );
        } else {
            tvtime_osd_set_input( osd, "No video source" );
            tvtime_osd_set_norm( osd, "" );
        }

        if( error_string ) {
            tvtime_osd_set_hold_message( osd, error_string );
            tvtime_osd_hold( osd, 1 );
            free( error_string );
            error_string = 0;
        }
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Created the OSD.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    commands = commands_new( ct, vidin, stationmgr, osd );
    if( !commands ) {
        fprintf( stderr, "tvtime: Can't create command handler.\n" );
        return 1;
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Created the command parser.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    /* Setup the video correction tables. */
    filter = videofilter_new();
    if( !filter ) {
        fprintf( stderr, "tvtime: Can't initialize filters.\n" );
        return 1;
    } else {
        videofilter_set_bt8x8_correction( filter, commands_apply_luma_correction( commands ) );
        videofilter_set_luma_power( filter, commands_get_luma_power( commands ) );
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Setup the filter tables.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    /* Ensure the FIFO directory exists */
    fifodir = opendir( FIFODIR );
    if( !fifodir ) {
        fprintf( stderr, "tvtime: Directory %s does not exist.  "
                         "FIFO disabled.\n", FIFODIR );
    } else {
        int success = 0;

        closedir( fifodir );
        /* Create the user's FIFO directory */
        if( mkdir( config_get_command_pipe_dir( ct ), S_IRWXU ) < 0 ) {
            if( errno != EEXIST ) {
                fprintf( stderr, "tvtime: Cannot create directory %s.  "
                                 "FIFO disabled.\n", 
                        config_get_command_pipe_dir( ct ) );
            } else {
                fifodir = opendir( config_get_command_pipe_dir( ct ) );
                if( !fifodir ) {
                    fprintf( stderr, "tvtime: %s is not a directory.  "
                                     "FIFO disabled.\n", 
                            config_get_command_pipe_dir( ct ) );
                } else {
                    struct stat dirstat;
                    closedir( fifodir );
                    /* Ensure the FIFO directory is owned by the user. */
                    if( !stat( config_get_command_pipe_dir( ct ), &dirstat ) ) {
                        if( dirstat.st_uid == config_get_uid( ct ) ) {
                            success = 1;
                        } else {
                            fprintf( stderr, "tvtime: You do not own %s.  "
                                             "FIFO disabled.\n",
                                     config_get_command_pipe_dir( ct ) );
                        }
                    } else {
                        fprintf( stderr, "tvtime: Cannot stat %s.  "
                                         "FIFO disabled.\n",
                                 config_get_command_pipe_dir( ct ) );
                    }
                }
            }
        } else {
            success = 1;
        }

        if( success ) {
            /* Setup the FIFO */
            fifo = fifo_new( config_get_command_pipe( ct ) );
            if( !fifo && verbose ) {
                fprintf( stderr, "tvtime: Not reading input from FIFO.  "
                                 "Failed to create FIFO object.\n" );
            }
        }
    }

    /* Setup the console */
    con = console_new( (width*10)/100, height - (height*20)/100, (width*80)/100, (height*20)/100, 16,
                        width, height, pixel_aspect, config_get_other_text_rgb( ct ) );
    if( !con ) {
        fprintf( stderr, "tvtime: Could not setup console.\n" );
    } else {
        if( fifo ) {
            console_setup_pipe( con, fifo_get_filename( fifo ) );
        }
        commands_set_console( commands, con );
    }

    in = input_new( ct, commands, con, verbose );
    if( !in ) {
        fprintf( stderr, "tvtime: Can't create input handler.\n" );
        return 1;
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Setup the FIFO, console and input.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    usevbi = config_get_usevbi( ct );
    if( usevbi ) {
        vs = vbiscreen_new( width, height, pixel_aspect, verbose );
        if( !vs ) {
            fprintf( stderr, "tvtime: Could not create vbiscreen, closed captions unavailable.\n" );
        }
    }

    /* Open the VBI device. */
    if( usevbi ) {
        vbidata = vbidata_new( config_get_vbidev( ct ), vs, verbose );
        if( !vbidata ) {
            fprintf( stderr, "tvtime: Could not create vbidata.\n" );
        } else {
            vbidata_capture_mode( vbidata, CAPTURE_OFF );
        }
        commands_set_vbidata( commands, vbidata );
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Setup the VBI.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    /* Randomly assign a tagline as the window caption. */
    srand( time( 0 ) );
    tagline = taglines[ rand() % numtaglines ];
    output->set_window_caption( tagline );

    /* If we start fullscreen, go into fullscreen mode now. */
    if( config_get_fullscreen( ct ) ) {
        commands_handle( commands, TVTIME_TOGGLE_FULLSCREEN, 0 );
    }
    /* If we start half-framerate, toggle that now. */
    for( i = 0; i < config_get_framerate_mode( ct ); i++ ) {
        commands_handle( commands, TVTIME_TOGGLE_FRAMERATE, 0 );
    }
    /* If we are a new install, start scanning channels. */
    if( station_is_new_install( stationmgr ) ) {
        commands_handle( commands, TVTIME_CHANNEL_SCAN, 0 );
    }

    /* Set the mixer volume. */
    mixer_set_volume( mixer_get_volume() );

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Did startup work.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    /* Begin capturing frames. */
    if( vidin ) {
        if( fieldsavailable == 3 ) {
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        } else if( fieldsavailable == 5 ) {
            secondlastframe = videoinput_next_frame( vidin, &secondlastframeid );
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        }
    }

    if( profile_startup ) {
        struct timeval profiletime;
        gettimeofday( &profiletime, 0 );
        fprintf( stderr, "tvtime: [%8dms] Pre-rolled capture.\n",
                 timediff( &profiletime, &startup_time ) / 1000 );
        fflush( stderr );
    }

    /* Initialize our timestamps. */
    for(;;) {
        const char *fifo_args = 0;
        int printdebug = 0;
        int showbars, screenshot;
        int acquired = 0;
        int tuner_state;
        int we_were_late = 0;
        int paused = 0;
        int output_x, output_y, output_w, output_h;
        int output_success = 1;
        int exposed = output->is_exposed();

        output_x = (int) ((((double) width) * commands_get_overscan( commands )) + 0.5);
        output_w = (int) ((((double) width) - (((double) width) * commands_get_overscan( commands ) * 2.0)) + 0.5);
        output_y = (int) ((((double) height) * commands_get_overscan( commands )) + 0.5);
        output_h = (int) ((((double) height) - (((double) height) * commands_get_overscan( commands ) * 2.0)) + 0.5);

        if( fifo ) {
            int cmd;
            cmd = fifo_get_next_command( fifo );
            fifo_args = fifo_get_arguments( fifo );
            if( cmd != TVTIME_NOCOMMAND ) commands_handle( commands, cmd, 0 );
        }

        if( read_stdin ) {
            /* Read commands from standard input. */
            while( !ioctl( STDIN_FILENO, FIONREAD, &kbd_available ) ) {
                char c;

                if( (kbd_available > 0) && read( STDIN_FILENO, &c, 1 ) == 1 ) {
                    if( c == '\n' ) {
                        int cmd;

                        kbd_cmd[ kbd_pos ] = '\0';

                        cmd = tvtime_string_to_command( kbd_cmd );
                        if( cmd != TVTIME_NOCOMMAND ) {
                            commands_handle( commands, cmd, 0 );
                        }
                        memset( kbd_cmd, 0, sizeof( kbd_cmd ) );
                        kbd_pos = 0;
                    } else {
                        if( kbd_pos < sizeof( kbd_cmd ) - 1 ) {
                            kbd_cmd[ kbd_pos++ ] = c;
                        } else {
                            memset( kbd_cmd, 0, sizeof( kbd_cmd ) );
                            kbd_pos = 0;
                        }
                    }
                } else {
                    break;
                }
            }
        }

        output->poll_events( in );

        if( commands_quit( commands ) ) break;
        printdebug = commands_print_debug( commands );
        showbars = commands_show_bars( commands );
        screenshot = commands_take_screenshot( commands );
        if( screenshot ) {
            /**
             * If we're being asked to take a screenshot, force a draw by
             * marking us exposed (just for this frame).  This makes sure
             * that screenshots always happen regardless of whether the
             * window is or becomes obscured.
             */
            exposed = 1;
        }
        paused = commands_pause( commands );
        if( commands_toggle_mode( commands ) && config_get_num_modes( ct ) ) {
            config_t *cur;
            preset_mode = (preset_mode + 1) % config_get_num_modes( ct );
            cur = config_get_mode_info( ct, preset_mode );
            if( cur ) {
                if( !output->is_interlaced() ) {
                    int firstmethod = curmethodid;
                    while( strcasecmp( config_get_deinterlace_method( cur ), curmethod->short_name ) ) {
                        curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
                        curmethod = get_deinterlace_method( curmethodid );
                        if( curmethodid == firstmethod ) break;
                    }
                    if( osd ) {
                        tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                    }
                }

                if( config_get_fullscreen( cur ) && !output->is_fullscreen() ) {
                    output->toggle_fullscreen( 0, 0 );
                } else {
                    output->set_window_height( config_get_outputheight( cur ) );
                }
                if( framerate_mode != config_get_framerate_mode( cur ) ) {
                    commands_set_framerate( commands, config_get_framerate_mode( cur ) );
                } else if( osd ) {
                    tvtime_osd_show_info( osd );
                }
            }
        }
        if( framerate_mode < 0 || framerate_mode != commands_get_framerate( commands ) ) {
            framerate_mode = commands_get_framerate( commands );
            if( osd ) {
                if( framerate_mode ) {
                    tvtime_osd_set_framerate( osd, 1000000.0 / ((double) (fieldtime*2)), framerate_mode );
                } else {
                    tvtime_osd_set_framerate( osd, 1000000.0 / ((double) fieldtime), framerate_mode );
                }
                tvtime_osd_show_info( osd );
            }
        }
        if( commands_toggle_fullscreen( commands ) ) {
            if( output->toggle_fullscreen( 0, 0 ) ) {
                config_save( ct, "FullScreen", "1" );
                if( osd ) tvtime_osd_show_message( osd, "Fullscreen mode active." );
            } else {
                config_save( ct, "FullScreen", "0" );
                if( osd ) tvtime_osd_show_message( osd, "Windowed mode active." );
            }
        }
        if( commands_toggle_aspect( commands ) ) {
            if( output->toggle_aspect() ) {
                if( osd ) tvtime_osd_show_message( osd, "16:9 display mode active." );
                config_save( ct, "WideScreen", "1" );
            } else {
                if( osd ) tvtime_osd_show_message( osd, "4:3 display mode active." );
                config_save( ct, "WideScreen", "0" );
            }
        }
        if( commands_toggle_pulldown_detection( commands ) ) {
            pulldown_alg = (pulldown_alg + 1) % PULLDOWN_MAX;
            if( osd ) {
                if( pulldown_alg == PULLDOWN_NONE ) {
                    tvtime_osd_show_message( osd, "Pulldown detection disabled." );
                } else if( pulldown_alg == PULLDOWN_VEKTOR ) {
                    tvtime_osd_show_message( osd, "Using vektor's adaptive pulldown detection." );
                } else if( pulldown_alg == PULLDOWN_DALIAS ) {
                    tvtime_osd_show_message( osd, "Using dalias's pulldown detection." );
                }
            }
        }
        if( !output->is_interlaced() && commands_toggle_deinterlacing_mode( commands ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            if( osd ) {
                tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                tvtime_osd_show_info( osd );
            }
            config_save( ct, "DeinterlaceMethod", curmethod->short_name );
        }
        if( commands_update_luma_power( commands ) ) {
            videofilter_set_luma_power( filter, commands_get_luma_power( commands ) );
        }
        if( commands_resize_window( commands ) ) {
            output->set_window_height( output->get_visible_height() );
        }
        commands_next_frame( commands );
        input_next_frame( in );


        /* Notice this because it's cheap. */
        videofilter_set_bt8x8_correction( filter, commands_apply_luma_correction( commands ) );

        /* Acquire the next frame. */
        if( rvrreader ) {
            tuner_state = TUNER_STATE_HAS_SIGNAL;
        } else if( vidin ) {
            tuner_state = videoinput_check_for_signal( vidin, config_get_check_freq_present( ct ) );
        } else {
            tuner_state = TUNER_STATE_NO_SIGNAL;
        }

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

        if( !paused && (tuner_state == TUNER_STATE_HAS_SIGNAL || tuner_state == TUNER_STATE_SIGNAL_DETECTED) ) {
            if( rvrreader ) {
                if( !rvrreader_next_frame( rvrreader ) ) break;
                curframe = rvrreader_get_curframe( rvrreader );
                lastframe = rvrreader_get_lastframe( rvrreader );
                secondlastframe = rvrreader_get_secondlastframe( rvrreader );
            } else if( vidin ) {
                curframe = videoinput_next_frame( vidin, &curframeid );
            } else {
                usleep( 10000 );
            }
            acquired = 1;
            filtered_cur = 0;
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

        if( !commands_scan_channels( commands ) && scanning ) {
            station_writeconfig( stationmgr );
            scanning = 0;
        }

        if( commands_scan_channels( commands ) && !scanning ) {
            scanning = 1;
            numscanned = 0;
            scanwait = scan_delay;
        }

        if( scanning ) {
            scanwait--;

            if( !scanwait ) {
                int before_pos;
                int after_pos;

                scanwait = scan_delay;
                if( tuner_state == TUNER_STATE_HAS_SIGNAL ) {
                    station_set_current_active( stationmgr, 1 );
                } else {
                    station_set_current_active( stationmgr, 0 );
                }
                before_pos = station_get_current_pos( stationmgr );
                commands_handle( commands, TVTIME_CHANNEL_INC, 0 );
                after_pos = station_get_current_pos( stationmgr );

                if( station_get_num_stations( stationmgr ) ) {
                    numscanned += (after_pos + station_get_num_stations( stationmgr ) - before_pos) % station_get_num_stations( stationmgr );
                }

                /* Stop when we loop around. */
                if( numscanned > station_get_num_stations( stationmgr ) || !station_get_num_stations( stationmgr ) ) {
                    commands_handle( commands, TVTIME_CHANNEL_SCAN, 0 );
                }
            }
        }

        performance_checkpoint_acquired_input_frame( perf );

        /* Print statistics and check for missed frames. */
        if( printdebug ) {
            fprintf( stderr, "tvtime: Stats using '%s' at %dx%d.\n", 
                     (curmethod) ? curmethod->name : "interlaced passthrough", 
                     width, height );
            if( curmethod ) {
                performance_print_last_frame_stats( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
            } else {
                performance_print_last_frame_stats( perf, width * height * 2 );
            }
            fprintf( stderr, "tvtime: Speedy time last frame: %d cycles.\n", speedy_get_cycles() );
        }
        if( config_get_debug( ct ) ) {
            if( curmethod )  {
                performance_print_frame_drops( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
            } else {
                performance_print_frame_drops( perf, width * height * 2 );
            }
        }
        speedy_reset_timer();

        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_BFF) ) {
            if( output->is_interlaced() ) {
                if( send_fields ) {
                    /* Wait until we can draw the even field. */
                    output->wait_for_sync( 0 );

                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( output->get_output_buffer(),
                                                   curframe, osd, con, vs, 0,
                                                   width, height, width * 2, 
                                                   output->get_output_stride(),
                                                   0);
                    output->unlock_output_buffer();
                    performance_checkpoint_constructed_top_field( perf );
                    performance_checkpoint_delayed_blit_top_field( perf );
                    performance_checkpoint_blit_top_field_start( perf );
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                } else {
                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( output->get_output_buffer(),
                                                   curframe, osd, con, vs, 0,
                                                   width, height, width * 2, 
                                                   output->get_output_stride(),
                                                   1 );
                    output->unlock_output_buffer();
                    performance_checkpoint_constructed_top_field( perf );
                    performance_checkpoint_delayed_blit_top_field( perf );
                    performance_checkpoint_blit_top_field_start( perf );
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                }
            } else {
                /* Build the output from the top field. */
                output->lock_output_buffer();
                if( showbars ) {
                    blit_packed422_scanline( output->get_output_buffer(),
                                             colourbars, width*height );
                } else {
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( output->get_output_buffer(),
                                           curframe, osd, con, vs, 0,
                                           width, height, width * 2, output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( output->get_output_buffer(),
                                           curframe, lastframe, secondlastframe,
                                           osd, con, vs, 0,
                                           width, height, width * 2, output->get_output_stride() );
                    }
                }

                /* For the screenshot, use the output after we build the top field. */
                if( screenshot ) {
                    const char *basename;
                    const char *outfile;
                    char filename[ 256 ];
                    char message[ 512 ];
                    char pathfilename[ 256 ];
                    char timestamp[ 50 ];
                    time_t tm = time( 0 );

                    if( fifo_args && strlen( fifo_args ) ) {
                        basename = outfile = fifo_args;
                    } else {
                        strftime( timestamp, sizeof( timestamp ),
                                  config_get_timeformat( ct ), localtime( &tm ) );
                        snprintf( filename, sizeof( filename ), "tvtime-output-%s.png", timestamp );
                        snprintf( pathfilename, sizeof( pathfilename ), "%s/%s", config_get_screenshot_dir( ct ), filename );
                        outfile = pathfilename;
                        basename = filename;
                    }
                    if( curmethod->doscalerbob ) {
                        pngscreenshot( outfile, output->get_output_buffer(),
                                       width, height/2, width * 2 );
                    } else {
                        pngscreenshot( outfile, output->get_output_buffer(),
                                       width, height, width * 2 );
                    }
                    snprintf( message, sizeof( message ), "Screenshot saved: %s", basename );
                    if( osd ) tvtime_osd_show_message( osd, message );
                    screenshot = 0;
                }
                output->unlock_output_buffer();
                performance_checkpoint_constructed_top_field( perf );


                /* Wait until it's time to blit the first field. */
                if( rtctimer ) {

                    we_were_late = 1;
                    while( performance_get_usecs_since_frame_acquired( perf )
                           < ( fieldtime - safetytime
                               - performance_get_usecs_of_last_blit( perf )
                               - ( rtctimer_get_usecs( rtctimer ) / 2 ) ) ) {
                        rtctimer_next_tick( rtctimer );
                        we_were_late = 0;
                    }

                }
                performance_checkpoint_delayed_blit_top_field( perf );

                performance_checkpoint_blit_top_field_start( perf );
                if( curmethod->doscalerbob && !showbars ) {
                    output_success = output->show_frame( output_x, output_y/2, output_w, output_h/2 );
                } else {
                    output_success = output->show_frame( output_x, output_y, output_w, output_h );
                }
            }
        } else {
            performance_checkpoint_constructed_top_field( perf );
            performance_checkpoint_delayed_blit_top_field( perf );
            performance_checkpoint_blit_top_field_start( perf );
        }
        performance_checkpoint_blit_top_field_end( perf );

        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_TFF) ) {
            if( output->is_interlaced() ) {
                if( send_fields ) {
                    /* Wait until we can draw the odd field. */
                    output->wait_for_sync( 1 );
                    
                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( output->get_output_buffer(),
                                                   curframe, osd, con, vs, 1,
                                                   width, height, width * 2, 
                                                   output->get_output_stride(),
                                                   0);
                    output->unlock_output_buffer();
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                }
            } else {
                /* Build the output from the bottom field. */
                output->lock_output_buffer();
                if( showbars ) {
                    blit_packed422_scanline( output->get_output_buffer(),
                                             colourbars, width*height );
                } else {
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( output->get_output_buffer(),
                                          curframe, osd, con, vs, 1,
                                          width, height, width * 2, output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( output->get_output_buffer(),
                                          curframe, lastframe,
                                          secondlastframe, osd, con, vs, 1,
                                          width, height, width * 2, output->get_output_stride() );
                    }
                }

                /* For the screenshot, use may need to use the output after we build the bot field. */
                if( screenshot ) {
                    const char *basename;
                    const char *outfile;
                    char filename[ 256 ];
                    char message[ 512 ];
                    char pathfilename[ 256 ];
                    char timestamp[ 50 ];
                    time_t tm = time( 0 );

                    if( fifo_args && strlen( fifo_args ) ) {
                        basename = outfile = fifo_args;
                    } else {
                        strftime( timestamp, sizeof( timestamp ),
                                  config_get_timeformat( ct ), localtime( &tm ) );
                        snprintf( filename, sizeof( filename ), "tvtime-output-%s.png", timestamp );
                        snprintf( pathfilename, sizeof( pathfilename ), "%s/%s", config_get_screenshot_dir( ct ), filename );
                        outfile = pathfilename;
                        basename = filename;
                    }
                    if( curmethod->doscalerbob ) {
                        pngscreenshot( outfile, output->get_output_buffer(),
                                       width, height/2, width * 2 );
                    } else {
                        pngscreenshot( outfile, output->get_output_buffer(),
                                       width, height, width * 2 );
                    }
                    snprintf( message, sizeof( message ), "Screenshot saved: %s", basename );
                    if( osd ) tvtime_osd_show_message( osd, message );
                    screenshot = 0;
                }
                output->unlock_output_buffer();
            }
        }
        performance_checkpoint_constructed_bot_field( perf );

        /* We're done with the input now. */
        if( vidin && acquired ) {
            if( fieldsavailable == 5 ) {
                videoinput_free_frame( vidin, secondlastframeid );
                secondlastframeid = lastframeid;
                secondlastframe = lastframe;
                lastframeid = curframeid;
                lastframe = curframe;
            } else if( fieldsavailable == 3 ) {
                videoinput_free_frame( vidin, lastframeid );
                lastframeid = curframeid;
                lastframe = curframe;
            } else {
                videoinput_free_frame( vidin, curframeid );
            }

            if( vbidata ) vbidata_process_frame( vbidata, printdebug );
        }

        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_TFF) && !output->is_interlaced() ) {
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
            if( curmethod->doscalerbob && !showbars ) {
                output_success = output->show_frame( output_x, output_y/2, output_w, output_h/2 );
            } else {
                output_success = output->show_frame( output_x, output_y, output_w, output_h );
            }
            performance_checkpoint_blit_bot_field_end( perf );
        } else {
            performance_checkpoint_delayed_blit_bot_field( perf );
            performance_checkpoint_blit_bot_field_start( perf );
            performance_checkpoint_blit_bot_field_end( perf );
        }

        if( osd ) {
            tvtime_osd_advance_frame( osd );
        }

        if( !output_success ) break;
    }

    /* Remember to save our settings if we were scanning. */
    if( scanning ) {
        station_writeconfig( stationmgr );
    }

    {
        char number[ 4 ];

        snprintf( number, 4, "%d", station_get_prev_id( stationmgr ) );
        config_save( ct, "PrevChannel", number );

        snprintf( number, 4, "%d", station_get_current_id( stationmgr ) );
        config_save( ct, "Channel", number );

        snprintf( number, 4, "%d", framerate_mode );
        config_save( ct, "FramerateMode", number );

        snprintf( number, 4, "%2.1f", commands_get_overscan( commands ) * 2.0 * 100.0 );
        config_save( ct, "OverScan", number );

        if( vidin ) {
            snprintf( number, 4, "%d", videoinput_get_input_num( vidin ) );
            config_save( ct, "V4LInput", number );
        }
    }

    output->shutdown();

    if( verbose ) {
        fprintf( stderr, "tvtime: Cleaning up.\n" );
    }
    if( rvrreader ) {
        rvrreader_delete( rvrreader );
    }
    if( vidin ) {
        videoinput_delete( vidin );
    }
    config_delete( ct );
    input_delete( in );
    commands_delete( commands );
    performance_delete( perf );
    station_delete( stationmgr );
    if( fifo ) {
        fifo_delete( fifo );
    }
    if( vbidata ) {
        vbidata_delete( vbidata );
    }
    if( rtctimer ) {
        rtctimer_delete( rtctimer );
    }
    if( con ) {
        console_delete( con );
    }
    if( vs ) {
        vbiscreen_delete( vs );
    }
    if( osd ) {
        tvtime_osd_delete( osd );
    }

    /* Free temporary memory. */
    free( colourbars );
    free( saveframe );
    free( fadeframe );
    free( blueframe );
    return 0;
}

