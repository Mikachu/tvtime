/**
 * Copyright (c) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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
#include <dirent.h>
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
#include "config.h"
#include "vgasync.h"
#include "rvrreader.h"
#include "pulldown.h"

/**
 * 0 == PULLDOWN_NONE
 * 1 == PULLDOWN_VEKTOR
 * 2 == PULLDOWN_DALIAS
 */
enum {
    PULLDOWN_NONE = 0,
    PULLDOWN_VEKTOR = 1,
    PULLDOWN_DALIAS = 2,
    PULLDOWN_MAX = 3,
};

static unsigned int pulldown_alg = 0;

/**
 * scratch paper:
 *
 *  A A  A  B  B  C  C C  D D
 * [T B  T][B  T][B  T B][T B]
 * [1 1][2  2][3  3][4 4][5 5]
 * [C C]      [M  M][C C][C C]
 *  D A  A  A  B  B  C C  C D
 *
 * Top 1 : Drop
 * Bot 1 : Show
 * Top 2 : Drop
 * Bot 2 : Drop
 * Top 3 : Merge
 * Bot 3 : Drop
 * Top 4 : Show 
 * Bot 4 : Drop
 * Top 5 : Drop
 * Bot 5 : Show
 */

/**
 * This is ridiculous, but apparently I need to give my own
 * prototype for this function because of a glibc bug. -Billy
 */
int setresuid( uid_t ruid, uid_t euid, uid_t suid );

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

/**
 * These are for the ogle code.
 */
char *program_name = "tvtime";
int dlevel = 3;

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
    pngoutput_t *pngout = pngoutput_new( filename, width, height, 0, 0.45, 0 );
    unsigned char *tempscanline = (unsigned char *) malloc( width * 3 );
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
        unsigned char *input422 = frame422 + (i * stride);
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

static void tvtime_do_pulldown_action( unsigned char *output,
                                       unsigned char *curframe,
                                       unsigned char *lastframe,
                                       unsigned char *secondlastframe,
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
        unsigned char *curoutput = output + (i * outstride);

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

static void tvtime_build_deinterlaced_frame( unsigned char *output,
                                             unsigned char *curframe,
                                             unsigned char *lastframe,
                                             unsigned char *secondlastframe,
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
                if( osd ) tvtime_osd_show_message( osd, "Film mode enabled." );
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
                if( osd ) tvtime_osd_show_message( osd, "Film mode disabled." );
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
            unsigned char *curoutput = output + (i * outstride);
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

        loop_size = ((frame_height - 2) / 2);
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

static void tvtime_build_interlaced_frame( unsigned char *output,
                                           unsigned char *curframe,
                                           tvtime_osd_t *osd,
                                           console_t *con,
                                           vbiscreen_t *vs,
                                           int bottom_field,
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
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        */
        output += outstride;
        scanline++;
    }
}


static void tvtime_build_copied_field( unsigned char *output,
                                       unsigned char *curframe,
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
    blit_packed422_scanline( output, curframe, width );

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
            interpolate_packed422_scanline( output, curframe, curframe - (instride*2), width );
        } else {
            if( filter && !filtered_cur && videofilter_active_on_scanline( filter, scanline ) ) {
                videofilter_packed422_scanline( filter, curframe, width, 0, scanline );
            }
            blit_packed422_scanline( output, curframe, width );
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
    tvtime_osd_t *osd = 0;
    unsigned char *colourbars;
    unsigned char *lastframe = 0;
    unsigned char *secondlastframe = 0;
    unsigned char *saveframe = 0;
    unsigned char *fadeframe = 0;
    unsigned char *blueframe = 0;
    unsigned char *curframe = 0;
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
    char number[4];
    int scanwait = scan_delay;
    int scanning = 0;
    int firstscan = 0;
    int use_vgasync = 0;
    int framerate_mode = -1;
    int preset_mode = -1;
    int i;

    fprintf( stderr, "tvtime: Running %s.\n", PACKAGE_STRING );

    /* Disable this code for a release. */
    fprintf( stderr, "\n*** WARNING: you are running a DEVELOPMENT version of tvtime.\n" );
    fprintf( stderr,   "*** We often break stuff during development.  Please submit bug reports\n"
                       "*** based on released versions only!!\n\n" );

    ct = config_new( argc, argv );
    if( !ct ) {
        fprintf( stderr, "tvtime: Can't set configuration options, exiting.\n" );
        return 1;
    }
    verbose = config_get_verbose( ct );
    if( verbose ) {
        dlevel = 5;
    }

    setup_speedy_calls( verbose );

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

    stationmgr = station_new( videoinput_norm_name( norm ), config_get_v4l_freq( ct ),
                              config_get_ntsc_cable_mode( ct ), verbose );
    if( !stationmgr ) {
        fprintf( stderr, "tvtime: Can't create station manager (no memory?), exiting.\n" );
        return 1;
    }
    station_set( stationmgr, config_get_prev_channel( ct ) );
    station_set( stationmgr, config_get_start_channel( ct ) );

    /* Default to a width specified on the command line. */
    width = config_get_inputwidth( ct );

    if( config_get_rvr_filename( ct ) ) {
        rvrreader = rvrreader_new( config_get_rvr_filename( ct ) );
        if( !rvrreader ) {
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
            fprintf( stderr, "tvtime: Can't open video input, "
                             "maybe try a different device?\n" );
        } else {
            videoinput_set_input_num( vidin, config_get_inputnum( ct ) );
            width = videoinput_get_width( vidin );
            height = videoinput_get_height( vidin );
            if( verbose ) {
                fprintf( stderr, "tvtime: V4L sampling %d pixels per scanline.\n", width );
            }
        }
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
        fieldsavailable = 1;
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
        fprintf( stderr, "tvtime: Invalid preferred deinterlace method, using first method found.\n" );
        curmethodid = 0;
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
    osd = tvtime_osd_new( width, height, config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0),
                          config_get_channel_text_rgb( ct ), config_get_other_text_rgb( ct ) );
    if( !osd ) {
        fprintf( stderr, "tvtime: OSD initialization failed, OSD disabled.\n" );
    } else {
        tvtime_osd_set_timeformat( osd, config_get_timeformat( ct ) );
        tvtime_osd_set_deinterlace_method( osd, curmethod->name );
        if( rvrreader ) {
            tvtime_osd_set_input( osd, "RVR" );
            tvtime_osd_set_norm( osd, height == 480 ? "NTSC" : "PAL" );
        } else if( vidin ) {
            tvtime_osd_set_input( osd, videoinput_get_input_name( vidin ) );
            tvtime_osd_set_norm( osd, videoinput_norm_name( videoinput_get_norm( vidin ) ) );
        } else {
            tvtime_osd_set_input( osd, "No video source" );
            tvtime_osd_set_norm( osd, "" );
        }
    }

    commands = commands_new( ct, vidin, stationmgr, osd );
    if( !commands ) {
        fprintf( stderr, "tvtime: Can't create command handler.\n" );
        return 1;
    }


    /* Setup the video correction tables. */
    if( verbose ) {
        if( config_get_apply_luma_correction( ct ) ) {
            fprintf( stderr, "tvtime: Luma correction enabled.\n" );
        } else {
            fprintf( stderr, "tvtime: Luma correction disabled.\n" );
        }
    }

    filter = videofilter_new();
    if( !filter ) {
        fprintf( stderr, "tvtime: Can't initialize filters.\n" );
        return 1;
    } else {
        videofilter_set_bt8x8_correction( filter, commands_apply_luma_correction( commands ) );
        videofilter_set_luma_power( filter, commands_get_luma_power( commands ) );
    }

    /*
    menu = menu_new( commands, ct, vidin, osd, width, height, 
                     config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0) );
    if( !menu ) {
        fprintf( stderr, "tvtime: Can't create menu.\n" );
    }
    commands_set_menu( commands, menu );
    */

    /* Steal system resources in the name of performance. */
    if( verbose ) {
        fprintf( stderr, "tvtime: Attempting to acquire "
                         "performance-enhancing features.\n" );
    }
    if( getenv( "TVTIME_USE_VGASYNC" ) && vgasync_init( verbose ) && verbose ) {
        fprintf( stderr, "tvtime: Enabling VGA port polling.\n" );
        use_vgasync = 1;
    } else if( verbose ) {
        fprintf( stderr, "tvtime: Disabling VGA port polling.\n" );
    }
    if( setpriority( PRIO_PROCESS, 0, config_get_priority( ct ) ) < 0 && verbose ) {
        fprintf( stderr, "tvtime: Can't renice to %d.\n", config_get_priority( ct ) );
    }

    if( !set_realtime_priority( 0 ) && verbose ) {
        fprintf( stderr, "tvtime: Can't set realtime priority (need root).\n" );
    }
    rtctimer = rtctimer_new( verbose );
    if( !rtctimer ) {
        fprintf( stderr, "\n*** /dev/rtc support is needed for smooth video.  We STRONGLY recommend\n"
                         "*** that you load the 'rtc' kernel module before starting tvtime.\n"
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
    if( verbose ) {
        fprintf( stderr, "tvtime: Done stealing resources.\n" );
    }

    /* We've now stolen all our root-requiring resources, drop to a user. */
    if( setresuid( getuid(), getuid(), getuid() ) == -1 ) {
        fprintf( stderr, "tvtime: Unknown problems dropping root access: %s\n", strerror( errno ) );
        return 1;
    }

    /* Ensure the FIFO directory exists */
    fifodir = opendir( FIFODIR );
    if( !fifodir ) {
        fprintf( stderr, "tvtime: Not reading input from fifo.  "
                         "Directory " FIFODIR " does not exist.\n" );
    }
    else {
        closedir( fifodir );
        /* Setup the FIFO */
        fifo = fifo_new( config_get_command_pipe( ct ) );
        if( !fifo ) {
            fprintf( stderr, "tvtime: Not reading input from FIFO. Creating "
                             "FIFO object failed.\n" );
        }
    }

    /* Setup the console */
    con = console_new( (width*10)/100, height - (height*20)/100, (width*80)/100, (height*20)/100, 10,
                       width, height, config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0),
                       config_get_other_text_rgb( ct ) );
    if( !con ) {
        fprintf( stderr, "tvtime: Could not setup console.\n" );
    } else {
        if( fifo ) {
            console_setup_pipe( con, fifo_get_filename( fifo ) );
        }
        commands_set_console( commands, con );
    }

    in = input_new( ct, commands, con, 0 );
    if( !in ) {
        fprintf( stderr, "tvtime: Can't create input handler.\n" );
        return 1;
    }

    usevbi = config_get_usevbi( ct );
    if( usevbi ) {
        vs = vbiscreen_new( width, height, 
                            config_get_aspect( ct ) ? (16.0 / 9.0) : (4.0 / 3.0), 
                            verbose );
        if( !vs ) {
            fprintf( stderr, "tvtime: Could not create vbiscreen.\n" );
            return 1;
        }
    }

    /* Open the VBI device. */
    if( usevbi ) {
        vbidata = vbidata_new( config_get_vbidev( ct ), vs, osd, verbose );
        if( !vbidata ) {
            fprintf( stderr, "tvtime: Could not create vbidata.\n" );
        } else {
            vbidata_capture_mode( vbidata, CAPTURE_OFF );
        }
        commands_set_vbidata( commands, vbidata );
    }

    /* Setup the output. */
    output = get_xv_output();
    if( !output->init( width, height, config_get_outputheight( ct ), 
                       config_get_aspect( ct ), verbose ) ) {
        fprintf( stderr, "tvtime: XVideo output failed to initialize: "
                         "no video output available.\n" );
        /* FIXME: Delete everything here! */
        if( vidin ) videoinput_delete( vidin );
        return 1;
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

    /* Begin capturing frames. */
    if( vidin ) {
        if( fieldsavailable == 3 ) {
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        } else if( fieldsavailable == 5 ) {
            secondlastframe = videoinput_next_frame( vidin, &secondlastframeid );
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        }
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

        output_x = (int) ((((double) width) * commands_get_horizontal_overscan( commands )) + 0.5);
        output_w = (int) ((((double) width) - (((double) width) * commands_get_horizontal_overscan( commands ) * 2.0)) + 0.5);
        output_y = (int) ((((double) height) * commands_get_vertical_overscan( commands )) + 0.5);
        output_h = (int) ((((double) height) - (((double) height) * commands_get_vertical_overscan( commands ) * 2.0)) + 0.5);

        if( fifo ) {
            int cmd;
            cmd = fifo_get_next_command( fifo );
            fifo_args = fifo_get_arguments( fifo );
            if( cmd != TVTIME_NOCOMMAND ) commands_handle( commands, cmd, 0 );
        }

        output->poll_events( in );

        if( commands_quit( commands ) ) break;
        printdebug = commands_print_debug( commands );
        showbars = commands_show_bars( commands );
        screenshot = commands_take_screenshot( commands );
        paused = commands_pause( commands );
        if( commands_toggle_mode( commands ) && config_get_num_modes( ct ) ) {
            tvtime_mode_settings_t *cur;
            preset_mode = (preset_mode + 1) % config_get_num_modes( ct );
            cur = config_get_mode_info( ct, preset_mode );
            if( cur ) {
                int firstmethod = curmethodid;
                while( strcasecmp( cur->deinterlacer, curmethod->short_name ) ) {
                    curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
                    curmethod = get_deinterlace_method( curmethodid );
                    if( curmethodid == firstmethod ) break;
                }
                if( osd ) {
                    tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                }
                if( cur->fullscreen && !output->is_fullscreen() ) {
                    output->toggle_fullscreen( cur->fullscreen_width, cur->fullscreen_height );
                } else if( cur->window_height ) {
                    output->set_window_height( cur->window_height );
                }
                if( framerate_mode != cur->framerate_mode ) {
                    commands_set_framerate( commands, cur->framerate_mode );
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
                if( config_get_configsave( ct ) ) {
                    configsave( config_get_configsave( ct ), "StartupFullScreen", "1" );
                }
                if( osd ) tvtime_osd_show_message( osd, "Fullscreen mode active." );
            } else {
                if( config_get_configsave( ct ) ) {
                    configsave( config_get_configsave( ct ), "StartupFullScreen", "0" );
                }
                if( osd ) tvtime_osd_show_message( osd, "Windowed mode active." );
            }
        }
        if( commands_toggle_aspect( commands ) ) {
            if( output->toggle_aspect() ) {
                if( osd ) tvtime_osd_show_message( osd, "16:9 display mode active." );
                if( config_get_configsave( ct ) ) {
                    configsave( config_get_configsave( ct ), "StartupWideScreen", "1" );
                }
            } else {
                if( osd ) tvtime_osd_show_message( osd, "4:3 display mode active." );
                if( config_get_configsave( ct ) ) {
                    configsave( config_get_configsave( ct ), "StartupWideScreen", "0" );
                }
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
        if( commands_toggle_deinterlacing_mode( commands ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            if( osd ) {
                tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                tvtime_osd_show_info( osd );
            }
            if( config_get_configsave( ct ) ) {
                snprintf( number, 3, "%d", curmethodid );
                number[3] = '\0';
                configsave( config_get_configsave( ct ), "StartupDeinterlaceMethod", number );
            }
        }
        if( commands_update_luma_power( commands ) ) {
            videofilter_set_luma_power( filter, commands_get_luma_power( commands ) );
        }
        commands_next_frame( commands );
        input_next_frame( in );


        /* Notice this because it's cheap. */
        videofilter_set_bt8x8_correction( filter, commands_apply_luma_correction( commands ) );

        /* Aquire the next frame. */
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
            firstscan = station_get_current_id( stationmgr );
            scanwait = scan_delay;
        }

        if( scanning ) {
            scanwait--;

            if( !scanwait ) {
                scanwait = scan_delay;
                if( tuner_state == TUNER_STATE_HAS_SIGNAL ) {
                    station_set_current_active( stationmgr, 1 );
                } else {
                    station_set_current_active( stationmgr, 0 );
                }
                commands_handle( commands, TVTIME_CHANNEL_UP, 0 );

                /* Stop when we loop around. */
                if( station_get_current_id( stationmgr ) == firstscan ) {
                    commands_handle( commands, TVTIME_CHANNEL_SCAN, 0 );
                }
            }
        }

        performance_checkpoint_acquired_input_frame( perf );

        /* Print statistics and check for missed frames. */
        if( printdebug ) {
            fprintf( stderr, "tvtime: Stats using '%s' at %dx%d.\n", curmethod->name, width, height );
            performance_print_last_frame_stats( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
            fprintf( stderr, "tvtime: Speedy time last frame: %d cycles.\n", speedy_get_cycles() );
        }
        if( config_get_debug( ct ) ) {
            performance_print_frame_drops( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
        }
        speedy_reset_timer();

        if( output->is_exposed() && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_TFF) ) {
            if( output->is_interlaced() ) {
                /* Wait until we can draw the even field. */
                output->wait_for_sync( 0 );

                output->lock_output_buffer();
                tvtime_build_interlaced_frame( output->get_output_buffer(),
                           curframe, osd, con, vs, 0,
                           width, height, width * 2, output->get_output_stride() );
                output->unlock_output_buffer();
                performance_checkpoint_constructed_top_field( perf );
                performance_checkpoint_delayed_blit_top_field( perf );
                performance_checkpoint_blit_top_field_start( perf );
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
                        sprintf( filename, "tvtime-output-%s.png", timestamp );
                        sprintf( pathfilename, "%s/%s", config_get_screenshot_dir( ct ), filename );
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
                    sprintf( message, "Screenshot saved: %s", basename );
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
                if( use_vgasync ) vgasync_spin_until_out_of_refresh();
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

        if( output->is_exposed() && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_BFF) ) {
            if( output->is_interlaced() ) {
                /* Wait until we can draw the odd field. */
                output->wait_for_sync( 1 );

                output->lock_output_buffer();
                tvtime_build_interlaced_frame( output->get_output_buffer(),
                           curframe, osd, con, vs, 1,
                           width, height, width * 2, output->get_output_stride() );
                output->unlock_output_buffer();
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
                        sprintf( filename, "tvtime-output-%s.png", timestamp );
                        sprintf( pathfilename, "%s/%s", config_get_screenshot_dir( ct ), filename );
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
                    sprintf( message, "Screenshot saved: %s", basename );
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

        if( (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_BFF)  && !output->is_interlaced() ) {
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
            if( use_vgasync ) vgasync_spin_until_out_of_refresh();
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

    if( config_get_configsave( ct ) ) {
        snprintf( number, 3, "%d", station_get_prev_id( stationmgr ) );
        configsave( config_get_configsave( ct ), "StartupPrevChannel", number );

        snprintf( number, 3, "%d", station_get_current_id( stationmgr ) );
        configsave( config_get_configsave( ct ), "StartupChannel", number );

        snprintf( number, 3, "%d", framerate_mode );
        configsave( config_get_configsave( ct ), "StartupFramerateMode", number );

        if( vidin ) {
            snprintf( number, 3, "%d", videoinput_get_input_num( vidin ) );
            configsave( config_get_configsave( ct ), "V4LInput", number );
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

