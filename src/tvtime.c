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
#include <string.h>
#include <stdlib.h>
#include <limits.h>
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
#include "videocorrection.h"
#include "plugins.h"
#include "performance.h"
#include "taglines.h"
#include "xvoutput.h"
#include "console.h"
#include "vbidata.h"
#include "vbiscreen.h"
#include "fifo.h"
#include "commands.h"
#include "station.h"
#include "configsave.h"
#include "config.h"
#include "vgasync.h"

/**
 * Set this to 1 to enable the experimental pulldown detection code.
 */
static unsigned int detect_pulldown = 0;

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
    pngoutput_t *pngout = pngoutput_new( filename, width, height, 0, 0.45 );
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
 * This is how many frames to wait until deciding if the pulldown phase
 * has changed or if we've really found a pulldown sequence.  This is
 * currently set to about 1 second, that is, we won't go into film mode
 * until we've seen a pulldown sequence successfully for 1 second.
 */
#define PULLDOWN_ERROR_WAIT     24

/**
 * This is how many predictions have to be incorrect before we fall back to
 * video mode.  Right now, if we mess up, we jump to video mode immediately.
 */
#define PULLDOWN_ERROR_THRESHOLD 4

/**
 * Possible pulldown offsets.
 */
#define PULLDOWN_OFFSET_1 (1<<0)
#define PULLDOWN_OFFSET_2 (1<<1)
#define PULLDOWN_OFFSET_3 (1<<2)
#define PULLDOWN_OFFSET_4 (1<<3)
#define PULLDOWN_OFFSET_5 (1<<4)

/* Offset                  1     2     3      4      5   */
/* Field Pattern          [T B  T][B  T][B   T B]  [T B] */
/* Action                 Copy  Save  Merge  Copy  Copy  */
/*                              Bot   Top                */
int tff_top_pattern[] = { 0,    1,    0,     0,    0     };
int tff_bot_pattern[] = { 0,    0,    0,     1,    0     };

#define HISTORY_SIZE 5

static int tophistory[ 5 ];
static int bothistory[ 5 ];
static int histpos = 0;

void fill_history( int tff )
{
    if( tff ) {
        tophistory[ 0 ] = INT_MAX; bothistory[ 0 ] = INT_MAX;
        tophistory[ 1 ] =       0; bothistory[ 1 ] = INT_MAX;
        tophistory[ 2 ] = INT_MAX; bothistory[ 2 ] = INT_MAX;
        tophistory[ 3 ] = INT_MAX; bothistory[ 3 ] =       0;
        tophistory[ 4 ] = INT_MAX; bothistory[ 3 ] = INT_MAX;
    } else {
        tophistory[ 0 ] = INT_MAX; bothistory[ 0 ] = INT_MAX;
        tophistory[ 1 ] = INT_MAX; bothistory[ 1 ] =       0;
        tophistory[ 2 ] = INT_MAX; bothistory[ 2 ] = INT_MAX;
        tophistory[ 3 ] =       0; bothistory[ 3 ] = INT_MAX;
        tophistory[ 4 ] = INT_MAX; bothistory[ 3 ] = INT_MAX;
    }

    histpos = 0;
}

int determine_pulldown_offset( int top_repeat, int bot_repeat, int tff, int *realbest )
{
    int avgbot = 0;
    int avgtop = 0;
    int best = 0;
    int min = -1;
    int minpos = 0;
    int minbot = 0;
    int j;
    int ret;
    int mintopval = -1;
    int mintoppos = -1;
    int minbotval = -1;
    int minbotpos = -1;

    tophistory[ histpos ] = top_repeat;
    bothistory[ histpos ] = bot_repeat;

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        avgtop += tophistory[ j ];
        avgbot += bothistory[ j ];
    }
    avgtop /= 5;
    avgbot /= 5;

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        // int cur = (tophistory[ j ] - avgtop);
        int cur = tophistory[ j ];
        if( cur < min || min < 0 ) {
            min = cur;
            minpos = j;
        }
        if( cur < mintopval || mintopval < 0 ) {
            mintopval = cur;
            mintoppos = j;
        }
    }

    for( j = 0; j < HISTORY_SIZE; j++ ) {
        // int cur = (bothistory[ j ] - avgbot);
        int cur = bothistory[ j ];
        if( cur < min || min < 0 ) {
            min = cur;
            minpos = j;
            minbot = 1;
        }
        if( cur < minbotval || minbotval < 0 ) {
            minbotval = cur;
            minbotpos = j;
        }
    }

    if( minbot ) {
        best = tff ? ( minpos + 2 ) : ( minpos + 4 );
    } else {
        best = tff ? ( minpos + 4 ) : ( minpos + 2 );
    }
    best = best % HISTORY_SIZE;
    *realbest = 1 << ( ( histpos + (2*HISTORY_SIZE) - best ) % HISTORY_SIZE );

    best = (minbotpos + 2) % 5;
    ret  = 1 << ( ( histpos + (2*HISTORY_SIZE) - best ) % HISTORY_SIZE );
    best = (mintoppos + 4) % 5;
    ret |= 1 << ( ( histpos + (2*HISTORY_SIZE) - best ) % HISTORY_SIZE );

    histpos = (histpos + 1) % HISTORY_SIZE;
    return ret;
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

static void tvtime_build_deinterlaced_frame( unsigned char *output,
                                             unsigned char *curframe,
                                             unsigned char *lastframe,
                                             unsigned char *secondlastframe,
                                             video_correction_t *vc,
                                             tvtime_osd_t *osd,
                                             console_t *con,
                                             vbiscreen_t *vs,
                                             int bottom_field,
                                             int correct_input,
                                             int width,
                                             int frame_height,
                                             int instride,
                                             int outstride )
{
    unsigned int topdiff = 0;
    unsigned int botdiff = 0;
    int i;

    /* Make pulldown decisions every top field. */
    if( detect_pulldown && !bottom_field ) {
        int realbest;
        int predicted;

        predicted = pdoffset << 1;
        if( predicted > PULLDOWN_OFFSET_5 ) predicted = PULLDOWN_OFFSET_1;
        pdoffset = determine_pulldown_offset( last_topdiff, last_botdiff, 1, &realbest );
        if( pdoffset & predicted ) {
            pdoffset = predicted;
            // fprintf( stderr, "   LUCK %d:%d\n", last_topdiff, last_botdiff );
        } else {
            pdoffset = realbest;
            // fprintf( stderr, "NO LUCK %d:%d\n", last_topdiff, last_botdiff );
        }

        /* 3:2 pulldown state machine. */
        if( pdoffset != predicted ) {
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
                // if( osd ) tvtime_osd_show_message( osd, "Film mode enabled." );
                filmmode = 1;
            }
            if( curoffset == PULLDOWN_OFFSET_2 ) {
                // Drop.
                for( i = 0; i < frame_height; i++ ) {
                    if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
                        topdiff += diff_factor_packed422_scanline( curframe + (i*instride*2),
                                                                   lastframe + (i*instride*2), width );
                        botdiff += diff_factor_packed422_scanline( curframe + (i*instride*2) + instride,
                                                                   lastframe + (i*instride*2) + instride,
                                                                   width );
                    }
                }
                last_topdiff = topdiff;
                last_botdiff = botdiff;
            } else if( curoffset == PULLDOWN_OFFSET_3 ) {
                // Merge.
                for( i = 0; i < frame_height; i++ ) {
                    unsigned char *curoutput = output + (i * outstride);

                    if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
                        topdiff += diff_factor_packed422_scanline( curframe + (i*instride*2),
                                                                   lastframe + (i*instride*2), width );
                        botdiff += diff_factor_packed422_scanline( curframe + (i*instride*2) + instride,
                                                                   lastframe + (i*instride*2) + instride,
                                                                   width );
                    }

                    if( i & 1 ) {
                        blit_packed422_scanline( curoutput, lastframe + (i * instride), width );
                    } else {
                        blit_packed422_scanline( curoutput, curframe + (i * instride), width );
                    }
                    if( correct_input ) {
                        video_correction_correct_packed422_scanline( vc, curoutput, curoutput, width );
                    }
                    if( vs ) vbiscreen_composite_packed422_scanline( vs, curoutput, width, 0, i );
                    if( osd ) tvtime_osd_composite_packed422_scanline( osd, curoutput, width, 0, i );
                    if( con ) console_composite_packed422_scanline( con, curoutput, width, 0, i );
                }
                last_topdiff = topdiff;
                last_botdiff = botdiff;
            } else if( curoffset == PULLDOWN_OFFSET_4 ) {
                // Copy.
                for( i = 0; i < frame_height; i++ ) {
                    unsigned char *curoutput = output + (i * outstride);

                    if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
                        topdiff += diff_factor_packed422_scanline( curframe + (i*instride*2),
                                                                   lastframe + (i*instride*2), width );
                        botdiff += diff_factor_packed422_scanline( curframe + (i*instride*2) + instride,
                                                                   lastframe + (i*instride*2) + instride,
                                                                   width );
                    }

                    blit_packed422_scanline( curoutput, curframe + (i * instride), width );
                    if( correct_input ) {
                        video_correction_correct_packed422_scanline( vc, curoutput, curoutput, width );
                    }
                    if( vs ) vbiscreen_composite_packed422_scanline( vs, curoutput, width, 0, i );
                    if( osd ) tvtime_osd_composite_packed422_scanline( osd, curoutput, width, 0, i );
                    if( con ) console_composite_packed422_scanline( con, curoutput, width, 0, i );
                }
                last_topdiff = topdiff;
                last_botdiff = botdiff;
            }
            return;
        } else {
            if( filmmode ) {
                fprintf( stderr, "Film mode disabled.\n" );
                // if( osd ) tvtime_osd_show_message( osd, "Film mode disabled." );
                filmmode = 0;
            }
        }
    } else if( detect_pulldown && !pderror ) {
        int curoffset = pdoffset << 1;
        if( curoffset > PULLDOWN_OFFSET_5 ) curoffset = PULLDOWN_OFFSET_1;

        if( curoffset == PULLDOWN_OFFSET_1 || curoffset == PULLDOWN_OFFSET_5 ) {
            // Copy.
            for( i = 0; i < frame_height; i++ ) {
                unsigned char *curoutput = output + (i * outstride);

                if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
                    topdiff += diff_factor_packed422_scanline( curframe + (i*instride*2),
                                                               lastframe + (i*instride*2), width );
                    botdiff += diff_factor_packed422_scanline( curframe + (i*instride*2) + instride,
                                                               lastframe + (i*instride*2) + instride,
                                                               width );
                }

                blit_packed422_scanline( curoutput, curframe + (i * instride), width );
                if( correct_input ) {
                    video_correction_correct_packed422_scanline( vc, curoutput, curoutput, width );
                }
                if( vs ) vbiscreen_composite_packed422_scanline( vs, curoutput, width, 0, i );
                if( osd ) tvtime_osd_composite_packed422_scanline( osd, curoutput, width, 0, i );
                if( con ) console_composite_packed422_scanline( con, curoutput, width, 0, i );
            }
            last_topdiff = topdiff;
            last_botdiff = botdiff;
        }
        return;
    }


    if( !curmethod->scanlinemode ) {
        deinterlace_frame_data_t data;

        if( detect_pulldown && !bottom_field ) {
            for( i = 40; i < frame_height/2 - 40; i += 2 ) {
                topdiff += diff_factor_packed422_scanline( curframe + (i*instride*2),
                                                           lastframe + (i*instride*2), width );
            }
            for( i = 40; i < frame_height/2 - 40; i += 2 ) {
                botdiff += diff_factor_packed422_scanline( curframe + (i*instride*2) + instride,
                                                           lastframe + (i*instride*2) + instride, width );
            }
            last_topdiff = topdiff;
            last_botdiff = botdiff;
        }

        data.f0 = curframe;
        data.f1 = lastframe;
        data.f2 = secondlastframe;

        curmethod->deinterlace_frame( output, &data, bottom_field, width, frame_height );

        for( i = 0; i < frame_height; i++ ) {
            unsigned char *curoutput = output + (i * outstride);
            if( correct_input ) {
                video_correction_correct_packed422_scanline( vc, curoutput, curoutput, width );
            }
            if( vs ) vbiscreen_composite_packed422_scanline( vs, curoutput, width, 0, i );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, curoutput, width, 0, i );
            if( con ) console_composite_packed422_scanline( con, curoutput, width, 0, i );
        }
    } else {
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
        blit_packed422_scanline( output, curframe, width );

        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

        output += outstride;
        scanline++;

        for( i = ((frame_height - 2) / 2); i; --i ) {
            deinterlace_scanline_data_t data;

            data.t0 = curframe;
            data.b0 = curframe + (instride*2);

            if( bottom_field ) {
                data.tt1 = 0;
                data.m1  = curframe + instride;
                data.bb1 = 0;
            } else {
                data.tt1 = 0;
                data.m1  = lastframe + instride;
                data.bb1 = 0;
            }

            data.t2 = lastframe;
            data.b2 = lastframe + (instride*2);

            if( bottom_field ) {
                data.tt3 = 0;
                data.m3  = lastframe + instride;
                data.bb3 = 0;
            } else {
                data.tt3 = 0;
                data.m3  = secondlastframe + instride;
                data.bb3 = 0;
            }

            curmethod->interpolate_scanline( output, &data, width );
            if( correct_input ) {
                video_correction_correct_packed422_scanline( vc, output, output, width );
            }
            if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

            output += outstride;
            scanline++;

            data.tt0 = 0;
            data.m0  = curframe + (instride*2);
            data.bb0 = 0;

            if( bottom_field ) {
                data.t1 = curframe + instride;
                data.b1 = curframe + (instride*3);
            } else {
                data.t1 = lastframe + instride;
                data.b1 = lastframe + (instride*3);
            }

            data.tt2 = 0;
            data.m2  = lastframe + (instride*2);
            data.bb2 = 0;

            if( bottom_field ) {
                data.t2 = lastframe + instride;
                data.b2 = lastframe + (instride*3);
            } else {
                data.t2 = secondlastframe + instride;
                data.b2 = secondlastframe + (instride*3);
            }

            /* Copy a scanline. */
            if( detect_pulldown && !bottom_field && scanline > 40 && (scanline & 3) == 0 && scanline < frame_height - 40 ) {
                topdiff += diff_factor_packed422_scanline( curframe, lastframe, width );
                botdiff += diff_factor_packed422_scanline( curframe + instride, lastframe + instride, width );
            }
            curmethod->copy_scanline( output, &data, width );
            curframe += instride * 2;
            lastframe += instride * 2;
            secondlastframe += instride * 2;

            if( correct_input ) {
                video_correction_correct_packed422_scanline( vc, output, output, width );
            }
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

            if( correct_input ) {
                video_correction_correct_packed422_scanline( vc, output, output, width );
            }
            if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
            if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
            if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );


            output += outstride;
            scanline++;
        }
    }
}

static void tvtime_build_interlaced_frame( unsigned char *output,
                                           unsigned char *curframe,
                                           video_correction_t *vc,
                                           tvtime_osd_t *osd,
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


static void tvtime_build_copied_field( unsigned char *output,
                                       unsigned char *curframe,
                                       video_correction_t *vc,
                                       tvtime_osd_t *osd,
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
    }

    /* Copy a scanline. */
    blit_packed422_scanline( output, curframe, width );

    if( correct_input ) {
        video_correction_correct_packed422_scanline( vc, output, output, width );
    }
    if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
    if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
    if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

    output += outstride;
    scanline += 2;

    for( i = ((frame_height - 2) / 2); i; --i ) {
        /* Copy a scanline. */
        blit_packed422_scanline( output, curframe, width );
        curframe += instride * 2;

        if( correct_input ) {
            video_correction_correct_packed422_scanline( vc, output, output, width );
        }
        if( vs ) vbiscreen_composite_packed422_scanline( vs, output, width, 0, scanline );
        if( osd ) tvtime_osd_composite_packed422_scanline( osd, output, width, 0, scanline );
        if( con ) console_composite_packed422_scanline( con, output, width, 0, scanline );

        output += outstride;
        scanline += 2;
    }
}


int main( int argc, char **argv )
{
    video_correction_t *vc = 0;
    videoinput_t *vidin = 0;
    rtctimer_t *rtctimer = 0;
    station_mgr_t *stationmgr = 0;
    int width, height;
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
    const char *tagline;
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
    fifo_t *fifo = 0;
    commands_t *commands = 0;
    int usevbi = 1;
    int fadepos = 0;
    char number[4];
    int scanwait = scan_delay;
    int scanning = 0;
    int firstscan = 0;
    int use_vgasync = 0;

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

    greedy_plugin_init();
    // videobob_plugin_init();
    // greedy2frame_plugin_init();
    // twoframe_plugin_init();
    linearblend_plugin_init();
    linear_plugin_init();
    weave_plugin_init();
    double_plugin_init();
    scalerbob_plugin_init();


    dscaler_greedy2frame_plugin_init();
    dscaler_twoframe_plugin_init();
    dscaler_greedyh_plugin_init();
    dscaler_greedy_plugin_init();
    dscaler_videobob_plugin_init();
    dscaler_videoweave_plugin_init();
    // how does this work anyway: dscaler_oldgame_plugin_init();
    dscaler_tomsmocomp_plugin_init();


    if( !configsave_open( config_get_config_filename( ct ) ) ) {
        fprintf( stderr, "tvtime: Can't open config file for runtime option saving.\n" );
    }

    stationmgr = station_new( config_get_v4l_freq( ct ), config_get_ntsc_cable_mode( ct ), verbose );
    if( !stationmgr ) {
        fprintf( stderr, "tvtime: Can't create station manager (no memory?), exiting.\n" );
        return 1;
    }
    station_set( stationmgr, config_get_start_channel( ct ) );

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
        fprintf( stderr, "tvtime: Can't initialize performance monitor, exiting.\n" );
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
        fprintf( stderr, "tvtime: V4L sampling %d pixels per scanline.\n", width );
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
        fprintf( stderr, "Can't initialize OSD object, OSD disabled.\n" );
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

    commands = commands_new( ct, vidin, stationmgr, osd, vc );
    if( !commands ) {
        fprintf( stderr, "tvtime: Can't create command handler.\n" );
        return 1;
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
        fprintf( stderr, "tvtime: Attempting to aquire "
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

    /* setup the fifo */
    fifo = fifo_new( config_get_command_pipe( ct ) );
    if( !fifo ) {
        fprintf( stderr, "tvtime: Not reading input from fifo. Creating "
                         "fifo object failed.\n" );
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
    if( !output->init( width, height, config_get_outputwidth( ct ), 
                       config_get_aspect( ct ), verbose ) ) {
        fprintf( stderr, "tvtime: XVideo output failed to initialize: "
                         "no video output available.\n" );
        videoinput_delete( vidin );
        return 1;
    }

    /* Randomly assign a tagline as the window caption. */
    srand( time( 0 ) );
    tagline = taglines[ rand() % numtaglines ];
    output->set_window_caption( tagline );

    /* If we start fullscreen, go into fullscreen mode now. */
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
        int output_x, output_y, output_w, output_h;

        output_x = (int) ((((double) width) * config_get_horizontal_overscan( ct )) + 0.5);
        output_w = (int) ((((double) width) - (((double) width) * config_get_horizontal_overscan( ct ) * 2.0)) + 0.5);
        output_y = (int) ((((double) height) * config_get_vertical_overscan( ct )) + 0.5);
        output_h = (int) ((((double) height) - (((double) height) * config_get_vertical_overscan( ct ) * 2.0)) + 0.5);

        if( fifo ) {
            int cmd;
            cmd = fifo_get_next_command( fifo );
            if( cmd != TVTIME_NOCOMMAND ) commands_handle( commands, cmd, 0 );
        }

        output->poll_events( in );

        if( commands_quit( commands ) ) break;
        printdebug = commands_print_debug( commands );
        showbars = commands_show_bars( commands );
        screenshot = commands_take_screenshot( commands );
        if( commands_toggle_fullscreen( commands ) ) {
            if( output->toggle_fullscreen( 0, 0 ) ) {
                configsave( "FullScreen", "1", 1 );
                if( osd ) tvtime_osd_show_message( osd, "Fullscreen mode active." );
            } else {
                configsave( "FullScreen", "0", 1 );
                if( osd ) tvtime_osd_show_message( osd, "Windowed mode active." );
            }
        }
        if( commands_toggle_aspect( commands ) ) {
            if( output->toggle_aspect() ) {
                if( osd ) tvtime_osd_show_message( osd, "16:9 display mode active." );
                configsave( "WideScreen", "1", 1 );
            } else {
                if( osd ) tvtime_osd_show_message( osd, "4:3 display mode active." );
                configsave( "WideScreen", "0", 1 );
            }
        }
        if( commands_toggle_pulldown_detection( commands ) ) {
            if( detect_pulldown ) {
                if( osd ) tvtime_osd_show_message( osd, "Pulldown detection disabled." );
            } else {
                if( osd ) tvtime_osd_show_message( osd, "Pulldown detection enabled." );
            }
            detect_pulldown = !detect_pulldown;
            fprintf( stderr, "Pulldown detection %s.\n", detect_pulldown ? "enabled" : "disabled" );
        }
        if( commands_toggle_deinterlacing_mode( commands ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            if( osd ) {
                tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                tvtime_osd_show_info( osd );
            }
            snprintf(number, 3, "%d", curmethodid);
            number[4] = '\0';
            configsave( "PreferredDeinterlaceMethod", number, 1 );
        }
        commands_next_frame( commands );
        input_next_frame( in );

        /* Aquire the next frame. */
        tuner_state = videoinput_check_for_signal( vidin, config_get_check_freq_present( ct ) );

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

        performance_checkpoint_aquired_input_frame( perf );

        /* Print statistics and check for missed frames. */
        if( printdebug ) {
            performance_print_last_frame_stats( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
            fprintf( stderr, "Speedy time last frame: %dus\n", speedy_get_usecs() );
        }
        if( config_get_debug( ct ) ) {
            performance_print_frame_drops( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
        }
        speedy_reset_timer();

        if( output->is_exposed() ) {
            if( output->is_interlaced() ) {
                /* Wait until we can draw the even field. */
                output->wait_for_sync( 0 );

                output->lock_output_buffer();
                tvtime_build_interlaced_frame( output->get_output_buffer(),
                           curframe, vc, osd, con, vs, 0,
                           vc && config_get_apply_luma_correction( ct ),
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
                                           curframe, vc, osd, con, vs, 0,
                                           vc && config_get_apply_luma_correction( ct ),
                                           width, height, width * 2, output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( output->get_output_buffer(),
                                           curframe, lastframe, secondlastframe,
                                           vc, osd, con, vs, 0,
                                           vc && config_get_apply_luma_correction( ct ),
                                           width, height, width * 2, output->get_output_stride() );
                    }
                }

                /* For the screenshot, use the output after we build the top field. */
                if( screenshot ) {
                    char filename[ 256 ];
                    char timestamp[ 50 ];
                    time_t tm = time( 0 );
                    strftime( timestamp, sizeof( timestamp ),
                              config_get_timeformat( ct ), localtime( &tm ) );
                    sprintf( filename, "tvtime-output-%s.png", timestamp );
                    if( curmethod->doscalerbob ) {
                        pngscreenshot( filename, output->get_output_buffer(),
                                       width, height/2, width * 2 );
                    } else {
                        pngscreenshot( filename, output->get_output_buffer(),
                                       width, height, width * 2 );
                    }
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
                if( use_vgasync ) vgasync_spin_until_out_of_refresh();
                if( curmethod->doscalerbob && !showbars ) {
                    output->show_frame( output_x, output_y/2, output_w, output_h/2 );
                } else {
                    output->show_frame( output_x, output_y, output_w, output_h );
                }
            }
        } else {
            performance_checkpoint_constructed_top_field( perf );
            performance_checkpoint_delayed_blit_top_field( perf );
            performance_checkpoint_blit_top_field_start( perf );
        }
        performance_checkpoint_blit_top_field_end( perf );

        if( output->is_exposed() && !commands_half_framerate( commands ) ) {
            if( output->is_interlaced() ) {
                /* Wait until we can draw the odd field. */
                output->wait_for_sync( 1 );

                output->lock_output_buffer();
                tvtime_build_interlaced_frame( output->get_output_buffer(),
                           curframe, vc, osd, con, vs, 1,
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
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( output->get_output_buffer(),
                                          curframe, vc, osd, con, vs, 1,
                                          vc && config_get_apply_luma_correction( ct ),
                                          width, height, width * 2, output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( output->get_output_buffer(),
                                          curframe, lastframe,
                                          secondlastframe, vc, osd, con, vs, 1,
                                          vc && config_get_apply_luma_correction( ct ),
                                          width, height, width * 2, output->get_output_stride() );
                    }
                }
                output->unlock_output_buffer();
            }
        }
        performance_checkpoint_constructed_bot_field( perf );

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


        if( !commands_half_framerate( commands ) && !output->is_interlaced() ) {
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
                output->show_frame( output_x, output_y/2, output_w, output_h/2 );
            } else {
                output->show_frame( output_x, output_y, output_w, output_h );
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
    }

    snprintf( number, 3, "%d", station_get_current_id( stationmgr ) );
    configsave( "StartChannel", number, 1 );
    snprintf( number, 3, "%d", videoinput_get_input_num( vidin ) );
    configsave( "CaptureSource", number, 1 );
    output->shutdown();

    if( verbose ) {
        fprintf( stderr, "tvtime: Cleaning up.\n" );
    }
    videoinput_delete( vidin );
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
    if( vc ) {
        video_correction_delete( vc );
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

    configsave_close();

    /* Free temporary memory. */
    free( colourbars );
    free( saveframe );
    free( fadeframe );
    free( blueframe );
    return 0;
}

