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
#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
#include <math.h>
#include <time.h>
#include <errno.h>
#include <libxml/parser.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
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
#include "dfboutput.h"
#include "mgaoutput.h"
#include "xmgaoutput.h"
#include "rvrreader.h"
#include "sdloutput.h"
#include "pulldown.h"
#include "utils.h"
#include "cpuinfo.h"
#include "outputfilter.h"
#include "mm_accel.h"
#include "menu.h"

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
 * Michael Niedermayer's magic FIRs
 *
 * [-1 14 3]
 * [-9 111 29 -3]
 */

/**
 * Which pulldown algorithm we're using.
 */
enum {
    PULLDOWN_NONE = 0,
    PULLDOWN_VEKTOR = 1,
    PULLDOWN_MAX = 2
};

/**
 * Which output driver we're using.
 */
enum {
    OUTPUT_XV,
    OUTPUT_DIRECTFB,
    OUTPUT_MGA,
    OUTPUT_XMGA,
    OUTPUT_SDL
};

/**
 * Speed at which to fade to blue on channel changes.
 */
const int fadespeed = 65;

/**
 * Number of frames to wait before detecting a signal.
 */
const int scan_delay = 10;

typedef struct tvtime_deinterlacer_s
{
    int start;
    int end;
    int cur;
} tvtime_deinterlacer_t;

typedef struct tvtime_s
{
    deinterlace_method_t *curmethod;
    videofilter_t *inputfilter;
    outputfilter_t *outputfilter;
    int filtered_curframe;
    int last_topdiff;
    int last_botdiff;

    /* 2-3 pulldown detection. */
    int pulldown_alg;
    int pdoffset;
    int pderror;
    int pdlastbusted;
    int filmmode;
} tvtime_t;

tvtime_t *tvtime_new( void )
{
    tvtime_t *tvtime = malloc( sizeof( tvtime_t ) );
    if( !tvtime ) return 0;

    tvtime->curmethod = 0;
    tvtime->inputfilter = videofilter_new();
    tvtime->outputfilter = outputfilter_new();
    tvtime->filtered_curframe = 0;
    tvtime->last_topdiff = 0;
    tvtime->last_botdiff = 0;

    tvtime->pulldown_alg = PULLDOWN_NONE;
    tvtime->pdoffset = PULLDOWN_SEQ_AA;
    tvtime->pderror = PULLDOWN_ERROR_WAIT;
    tvtime->pdlastbusted = 0;
    tvtime->filmmode = 0;

    return tvtime;
}

void tvtime_delete( tvtime_t *tvtime )
{
    if( tvtime->inputfilter ) {
        videofilter_delete( tvtime->inputfilter );
    }
    if( tvtime->outputfilter ) {
        outputfilter_delete( tvtime->outputfilter );
    }
    free( tvtime );
}

void tvtime_set_deinterlacer( tvtime_t *tvtime, deinterlace_method_t *method )
{
    tvtime->curmethod = method;
}

static void build_colourbars( uint8_t *output, int width, int height )
{
    uint8_t *cb444 = malloc( width * height * 3 );
    int i;
    if( !cb444 ) { memset( output, 255, width * height * 2 ); return; }

    create_colourbars_packed444( cb444, width, height, width*3 );
    for( i = 0; i < height; i++ ) {
        uint8_t *curout = output + (i * width * 2);
        uint8_t *curin = cb444 + (i * width * 3);
        packed444_to_packed422_scanline( curout, curin, width );
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

static int curpos = 0;
static void save_channel( uint8_t *previewframe, uint8_t *curframe,
                          int width, int height, int savestride, int curstride )
{
    int startx, starty;

    startx = ((curpos % 3) * (width/3)) & ~1;
    starty = (curpos / 3) * (height/3);
    pointsample_packed422_image( previewframe + (startx*2) + (starty*savestride), width/3, height/3, savestride,
                                 curframe, width, height, curstride );
    curpos = (curpos + 1) % 9;
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
static void pulldown_merge_fields( uint8_t *output,
                                   uint8_t *topfield,
                                   uint8_t *botfield,
                                   outputfilter_t *of,
                                   int width,
                                   int frame_height,
                                   int fieldstride,
                                   int outstride )
{
    int i;

    for( i = 0; i < frame_height; i++ ) {
        uint8_t *curoutput = output + (i * outstride);

        if( i & 1 ) {
            blit_packed422_scanline( curoutput, botfield + ((i / 2) * fieldstride), width );
        } else {
            blit_packed422_scanline( curoutput, topfield + ((i / 2) * fieldstride), width );
        }

        if( of ) outputfilter_composite_packed422_scanline( of, curoutput, width, 0, i );
    }
}

static void calculate_pulldown_score_vektor( tvtime_t *tvtime,
                                             uint8_t *curframe,
                                             uint8_t *lastframe,
                                             int instride,
                                             int frame_height,
                                             int width )
{
    int i;

    tvtime->last_topdiff = 0;
    tvtime->last_botdiff = 0;

    for( i = 0; i < frame_height; i++ ) {
        if( tvtime->inputfilter && !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, i ) ) {
            videofilter_packed422_scanline( tvtime->inputfilter, curframe + (i*instride), width, 0, i );
        }

        if( i > 40 && (i & 3) == 0 && i < frame_height - 40 ) {
            tvtime->last_topdiff += diff_factor_packed422_scanline( curframe + (i*instride),
                                                                    lastframe + (i*instride), width );
            tvtime->last_botdiff += diff_factor_packed422_scanline( curframe + (i*instride) + instride,
                                                                    lastframe + (i*instride) + instride,
                                                                    width );
        }
    }

    tvtime->filtered_curframe = 1;
}

static void tvtime_build_deinterlaced_frame( tvtime_t *tvtime,
                                             uint8_t *output,
                                             uint8_t *curframe,
                                             uint8_t *lastframe,
                                             uint8_t *secondlastframe,
                                             int bottom_field,
                                             int width,
                                             int frame_height,
                                             int instride,
                                             int outstride )
{
    int i;

    if( tvtime->pulldown_alg != PULLDOWN_VEKTOR ) {
        /* If we leave vektor pulldown mode, lose our state. */
        tvtime->filmmode = 0;
    }

    if( tvtime->pulldown_alg == PULLDOWN_VEKTOR ) {
        /* Make pulldown phase decisions every top field. */
        if( !bottom_field ) {
            int predicted;

            predicted = tvtime->pdoffset << 1;
            if( predicted > PULLDOWN_SEQ_DD ) predicted = PULLDOWN_SEQ_AA;

            calculate_pulldown_score_vektor( tvtime, curframe, lastframe,
                                             instride, frame_height, width );
            tvtime->pdoffset = determine_pulldown_offset_short_history_new( tvtime->last_topdiff,
                                                                            tvtime->last_botdiff,
                                                                            1, predicted );

            /* 3:2 pulldown state machine. */
            if( !tvtime->pdoffset ) {
                /* No pulldown offset applies, drop out of pulldown immediately. */
                tvtime->pdlastbusted = 0;
                tvtime->pderror = PULLDOWN_ERROR_WAIT;
            } else if( tvtime->pdoffset != predicted ) {
                if( tvtime->pdlastbusted ) {
                    tvtime->pdlastbusted--;
                    tvtime->pdoffset = predicted;
                } else {
                    tvtime->pderror = PULLDOWN_ERROR_WAIT;
                }
            } else {
                if( tvtime->pderror ) {
                    tvtime->pderror--;
                }

                if( !tvtime->pderror ) {
                    tvtime->pdlastbusted = PULLDOWN_ERROR_THRESHOLD;
                }
            }


            if( !tvtime->pderror ) {
                /* We're in pulldown, reverse it. */
                if( !tvtime->filmmode ) {
                    fprintf( stderr, "Film mode enabled.\n" );
                    tvtime->filmmode = 1;
                }

                if( pulldown_source( tvtime->pdoffset, 0 ) ) {
                    pulldown_merge_fields( output, lastframe, lastframe + instride,
                                           tvtime->outputfilter, width, frame_height, instride*2, outstride );
                } else {
                    pulldown_merge_fields( output, curframe, lastframe + instride,
                                           tvtime->outputfilter, width, frame_height, instride*2, outstride );
                }

                return;
            } else {
                if( tvtime->filmmode ) {
                    fprintf( stderr, "Film mode disabled.\n" );
                    tvtime->filmmode = 0;
                }
            }
        } else if( !tvtime->pderror ) {
            if( pulldown_source( tvtime->pdoffset, 1 ) ) {
                pulldown_merge_fields( output, curframe, lastframe + instride,
                                       tvtime->outputfilter, width, frame_height, instride*2, outstride );
            } else {
                pulldown_merge_fields( output, curframe, curframe + instride,
                                       tvtime->outputfilter, width, frame_height, instride*2, outstride );
            }

            return;
        }
    }

    if( !tvtime->curmethod->scanlinemode ) {
        deinterlace_frame_data_t data;

        if( tvtime->inputfilter && !tvtime->filtered_curframe ) {
            for( i = 0; i < frame_height; i++ ) {
                if( videofilter_active_on_scanline( tvtime->inputfilter, i ) ) {
                    videofilter_packed422_scanline( tvtime->inputfilter, curframe + (i*instride), width, 0, i );
                }
            }
        }

        data.f0 = curframe;
        data.f1 = lastframe;
        data.f2 = secondlastframe;

        tvtime->curmethod->deinterlace_frame( output, outstride, &data, bottom_field, width, frame_height );

        if( tvtime->outputfilter ) {
            for( i = 0; i < frame_height; i++ ) {
                uint8_t *curoutput = output + (i * outstride);
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, curoutput, width, 0, i );
            }
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
            if( tvtime->outputfilter ) {
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
            }

            output += outstride;
            scanline++;
        }

        /* Copy a scanline. */
        if( tvtime->inputfilter && !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, scanline ) ) {
            videofilter_packed422_scanline( tvtime->inputfilter, curframe, width, 0, scanline );
        }
        blit_packed422_scanline( output, curframe, width );
        if( tvtime->outputfilter ) {
            outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
        }

        output += outstride;
        scanline++;

        /* Something is wrong here. -Billy */
        loop_size = ((frame_height - 2) / 2) - bottom_field;
        for( i = loop_size; i; --i ) {
            deinterlace_scanline_data_t data;

            data.bottom_field = bottom_field;

            data.t0 = curframe;
            data.b0 = curframe + (instride*2);

            if( tvtime->inputfilter && !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, scanline + 1 ) ) {
                videofilter_packed422_scanline( tvtime->inputfilter, curframe + instride, width, 0, scanline );
                videofilter_packed422_scanline( tvtime->inputfilter, curframe + (instride*2), width, 0, scanline + 1 );
            }

            if( bottom_field ) {
                data.tt1 = (i < loop_size) ? (curframe - instride) : (curframe + instride);
                data.m1  = curframe + instride;
                data.bb1 = (i > 1) ? (curframe + (instride*3)) : (curframe + instride);
            } else {
                data.tt1 = (i < loop_size) ? (lastframe - instride) : (lastframe + instride);
                data.m1  = lastframe + instride;
                data.bb1 = (i > 1) ? (lastframe + (instride*3)) : (lastframe + instride);
            }

            data.t2 = lastframe;
            data.b2 = lastframe + (instride*2);

            if( bottom_field ) {
                data.tt3 = (i < loop_size) ? (lastframe - instride) : (lastframe + instride);
                data.m3  = lastframe + instride;
                data.bb3 = (i > 1) ? (lastframe + (instride*3)) : (lastframe + instride);
            } else {
                data.tt3 = (i < loop_size) ? (secondlastframe - instride) : (secondlastframe + instride);
                data.m3  = secondlastframe + instride;
                data.bb3 = (i > 1) ? (secondlastframe + (instride*3)) : (secondlastframe + instride);
            }

            tvtime->curmethod->interpolate_scanline( output, &data, width );
            if( tvtime->outputfilter ) {
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
            }

            output += outstride;
            scanline++;

            data.tt0 = curframe;
            data.m0  = curframe + (instride*2);
            data.bb0 = (i > 1) ? (curframe + (instride*4)) : (curframe + (instride*2));

            if( bottom_field ) {
                data.t1 = curframe + instride;
                data.b1 = (i > 1) ? (curframe + (instride*3)) : (curframe + instride);
            } else {
                data.t1 = lastframe + instride;
                data.b1 = (i > 1) ? (lastframe + (instride*3)) : (lastframe + instride);
            }

            data.tt2 = lastframe;
            data.m2  = lastframe + (instride*2);
            data.bb2 = (i > 1) ? (lastframe + (instride*4)) : (lastframe + (instride*2));

            if( bottom_field ) {
                data.t2 = lastframe + instride;
                data.b2 = lastframe + (instride*3);
            } else {
                data.t2 = secondlastframe + instride;
                data.b2 = secondlastframe + (instride*3);
            }

            /* Copy a scanline. */
            tvtime->curmethod->copy_scanline( output, &data, width );
            curframe += instride * 2;
            lastframe += instride * 2;
            secondlastframe += instride * 2;

            if( tvtime->outputfilter ) {
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
            }

            output += outstride;
            scanline++;
        }

        if( !bottom_field ) {
            /* Double the bottom scanline. */
            blit_packed422_scanline( output, curframe, width );

            if( tvtime->outputfilter ) {
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
            }

            output += outstride;
            scanline++;
        }
    }

    tvtime->filtered_curframe = 1;
}

static void tvtime_build_interlaced_frame( tvtime_t *tvtime,
                                           uint8_t *output,
                                           uint8_t *curframe,
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
        if( tvtime->outputfilter ) {
            outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
        }

        output += outstride;
        scanline++;

        for( i = ((frame_height - 2) / 2); i; --i ) {
            /* Skip a scanline. */
            output += outstride;
            scanline++;

            /* Copy a scanline. */
            curframe += instride * 2;
            blit_packed422_scanline( output, curframe, width );

            if( tvtime->outputfilter ) {
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
            }
  
            output += outstride;
            scanline++;
        }
    } else {
        for( i = frame_height; i; --i ) {

            /* Copy a scanline. */
            blit_packed422_scanline( output, curframe, width );

            if( tvtime->outputfilter ) {
                outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
            }
  
            curframe += instride;         
            output += outstride;
            scanline++;
        }
    }
}

static void tvtime_build_copied_field( tvtime_t *tvtime,
                                       uint8_t *output,
                                       uint8_t *curframe,
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

    /* Interpolate the top scanline as a special case. */
    if( tvtime->inputfilter && !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, scanline + bottom_field ) ) {
        videofilter_packed422_scanline( tvtime->inputfilter, curframe, width, 0, 0 );
        videofilter_packed422_scanline( tvtime->inputfilter, curframe + instride, width, 0, 1 );
        videofilter_packed422_scanline( tvtime->inputfilter, curframe + (instride*2), width, 0, 2 );
    }
    quarter_blit_vertical_packed422_scanline( output, curframe + (instride*2), curframe, width );

    if( tvtime->outputfilter ) {
        outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
    }
  
    curframe += instride * 2;
    output += outstride;
    scanline += 2;

    for( i = ((frame_height - 2) / 2); i; --i ) {

        if( tvtime->inputfilter && !tvtime->filtered_curframe && 
                (videofilter_active_on_scanline( tvtime->inputfilter, scanline + 1 ) ||
                 videofilter_active_on_scanline( tvtime->inputfilter, scanline + 2 )) ) {
            videofilter_packed422_scanline( tvtime->inputfilter, curframe + instride, width, 0, scanline + 1 );
            if( i > 1 ) {
                videofilter_packed422_scanline( tvtime->inputfilter, curframe + (instride*2), width, 0, scanline + 2 );
            }
        }

        /* Interpolate scanline. */
        if( bottom_field ) {
            quarter_blit_vertical_packed422_scanline( output, curframe - (instride*2), curframe, width );
        } else {
            if( i > 1 ) {
                quarter_blit_vertical_packed422_scanline( output, curframe + (instride*2), curframe, width );
            } else {
                blit_packed422_scanline( output, curframe, width );
            }
        }
        curframe += instride * 2;

        if( tvtime->outputfilter ) {
            outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
        }

        output += outstride;
        scanline += 2;
    }

    tvtime->filtered_curframe = 1;
}

static void osd_list_deinterlacer_info( tvtime_osd_t *osd, int curmethod )
{
    int i;

    tvtime_osd_list_set_lines( osd, 11 );
    tvtime_osd_list_set_text( osd, 0, get_deinterlace_method( curmethod )->name );
    for( i = 0; i < 10; i++ ) {
        tvtime_osd_list_set_text( osd, i + 1, get_deinterlace_method( curmethod )->description[ i ] );
    }
    tvtime_osd_list_set_hilight( osd, -1 );
    tvtime_osd_show_list( osd, 1 );
}

static void osd_list_deinterlacers( tvtime_osd_t *osd, int curmethod )
{
    int nummethods = get_num_deinterlace_methods();
    int i;

    tvtime_osd_list_set_lines( osd, get_num_deinterlace_methods() + 1 );
    tvtime_osd_list_set_text( osd, 0, "Deinterlacer mode" );
    for( i = 0; i < nummethods; i++ ) {
        tvtime_osd_list_set_text( osd, i + 1, get_deinterlace_method( i )->name );
    }
    tvtime_osd_list_set_hilight( osd, curmethod + 1 );
    tvtime_osd_show_list( osd, 1 );
}

static void build_deinterlacer_menu( menu_t *menu, int curmethod )
{
    int nummethods = get_num_deinterlace_methods();
    char string[ 128 ];
    int i;

    for( i = 0; i < nummethods; i++ ) {
        if( i == curmethod ) {
            snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa5,
                      get_deinterlace_method( i )->name );
        } else {
            snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa4,
                      get_deinterlace_method( i )->name );
        }
        menu_set_text( menu, i + 1, string );
        menu_set_enter_command( menu, i + 1, TVTIME_SET_DEINTERLACER,
                                get_deinterlace_method( i )->short_name );
        menu_set_right_command( menu, i + 1, TVTIME_SET_DEINTERLACER,
                                get_deinterlace_method( i )->short_name );
        menu_set_left_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    }

    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, nummethods + 1, string );
    menu_set_enter_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
}

static void build_deinterlacer_description_menu( menu_t *menu, int curmethod )
{
    char string[ 128 ];
    int i;

    menu_set_text( menu, 0, get_deinterlace_method( curmethod )->name );
    for( i = 0; i < 10; i++ ) {
        menu_set_text( menu, i + 1, get_deinterlace_method( curmethod )->description[ i ] );
        menu_set_enter_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
        menu_set_right_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
        menu_set_left_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    }
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_default_cursor( menu, i );
    menu_set_text( menu, i + 1, string );
    menu_set_enter_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
}

static void build_framerate_menu( menu_t *menu, double maxrate, int mode )
{
    char string[ 128 ];
    char text[ 128 ];

    sprintf( text, "Full rate: %.2ffps", maxrate );
    if( mode == 0 ) {
        snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa5, text );
    } else {
        snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa4, text );
    }
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SET_FRAMERATE, "full" );
    menu_set_right_command( menu, 1, TVTIME_SET_FRAMERATE, "full" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "processing" );

    sprintf( text, "Half rate, deinterlace top fields: %.2ffps", maxrate / 2 );
    if( mode == 1 ) {
        snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa5, text );
    } else {
        snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa4, text );
    }
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SET_FRAMERATE, "top" );
    menu_set_right_command( menu, 2, TVTIME_SET_FRAMERATE, "top" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "processing" );

    sprintf( text, "Half rate, deinterlace bottom fields: %.2ffps", maxrate / 2 );
    if( mode == 2 ) {
        snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa5, text );
    } else {
        snprintf( string, sizeof( string ), "%c%c%c  %s", 0xee, 0x80, 0xa4, text );
    }
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SET_FRAMERATE, "bottom" );
    menu_set_right_command( menu, 3, TVTIME_SET_FRAMERATE, "bottom" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "processing" );

    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
}

static void osd_list_framerates( tvtime_osd_t *osd, double maxrate, int mode )
{
    char text[ 200 ];

    tvtime_osd_list_set_lines( osd, 4 );
    tvtime_osd_list_set_text( osd, 0, "Frame drop setting" );

    sprintf( text, "Full rate: %.2ffps", maxrate );
    tvtime_osd_list_set_text( osd, 1, text );

    sprintf( text, "Half rate, deinterlace top fields: %.2ffps", maxrate / 2 );
    tvtime_osd_list_set_text( osd, 2, text );

    sprintf( text, "Half rate, deinterlace bottom fields: %.2ffps", maxrate / 2 );
    tvtime_osd_list_set_text( osd, 3, text );

    tvtime_osd_list_set_hilight( osd, mode + 1 );
    tvtime_osd_show_list( osd, 1 );
}

static void osd_list_statistics( tvtime_osd_t *osd, performance_t *perf,
                                 const char *deinterlacer, int width,
                                 int height, int framesize, int fieldtime,
                                 int frameratemode, int norm )
{
    double blit_time = performance_get_estimated_blit_time( perf );
    char text[ 200 ];

    tvtime_osd_list_set_lines( osd, frameratemode ? 7 : 8 );
    tvtime_osd_list_set_text( osd, 0, "Performance estimates" );

    sprintf( text, "Deinterlacer: %s", deinterlacer );
    tvtime_osd_list_set_text( osd, 1, text );

    sprintf( text, "Input: %s at %dx%d", videoinput_get_norm_name( norm ), width, height );
    tvtime_osd_list_set_text( osd, 2, text );

    sprintf( text, "Attempted framerate: %.2ffps",
             frameratemode ? 1000000.0 / ((double) (fieldtime*2)) : 1000000.0 / ((double) fieldtime) );
    tvtime_osd_list_set_text( osd, 3, text );

    sprintf( text, "Average blit time: %.2fms (%.0fMB/sec)",
             blit_time, (((double) framesize) * 0.00095367431640625000) / blit_time );
    tvtime_osd_list_set_text( osd, 4, text );

    sprintf( text, "Average render time: %5.2fms",
             performance_get_estimated_rendering_time( perf ) );
    tvtime_osd_list_set_text( osd, 5, text );

    sprintf( text, "Dropped frames: %d",
             performance_get_dropped_frames( perf ) );
    tvtime_osd_list_set_text( osd, 6, text );

    if( !frameratemode ) {
        sprintf( text, "Blit spacing: %4.1f/%4.1fms (want %4.1fms)",
                 performance_get_time_top_to_bot( perf ),
                 performance_get_time_bot_to_top( perf ),
                 ((double) fieldtime) * 0.001 );
        tvtime_osd_list_set_text( osd, 7, text );
    }

    tvtime_osd_list_set_hilight( osd, -1 );
    tvtime_osd_show_list( osd, 1 );
}

static void osd_list_matte( tvtime_osd_t *osd, int mode, int sixteennine )
{
    tvtime_osd_list_set_lines( osd, 5 );
    if( sixteennine ) {
        tvtime_osd_list_set_text( osd, 0, "Matte setting (Anamorphic input)" );
        tvtime_osd_list_set_text( osd, 1, "16:9 + Overscan" );
        tvtime_osd_list_set_text( osd, 2, "1.85:1" );
        tvtime_osd_list_set_text( osd, 3, "2.35:1" );
        tvtime_osd_list_set_text( osd, 4, "4:3 centre" );
    } else {
        tvtime_osd_list_set_text( osd, 0, "Matte setting (4:3 input)" );
        tvtime_osd_list_set_text( osd, 1, "4:3 + Overscan" );
        tvtime_osd_list_set_text( osd, 2, "16:9" );
        tvtime_osd_list_set_text( osd, 3, "1.85:1" );
        tvtime_osd_list_set_text( osd, 4, "2.35:1" );
    }
    tvtime_osd_list_set_hilight( osd, mode + 1 );
    tvtime_osd_show_list( osd, 1 );
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
    int sixteennine = 0;
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
    char number[ 4 ];
    tvtime_t *tvtime;
    unsigned int output_driver = 0;
    deinterlace_method_t *curmethod;
    int curmethodid;
    int matte_x = 0;
    int matte_w = 0;
    int matte_y = 0;
    int matte_h = 0;
    int matte_mode = 0;
    int restarttvtime = 0;
    int i;

    gettimeofday( &startup_time, 0 );

    fprintf( stderr, "tvtime: Running %s.\n", PACKAGE_STRING );

    /* Disable this code for a release. */
    fprintf( stderr, "\n*** WARNING: you are running a DEVELOPMENT version of tvtime.\n" );
    fprintf( stderr,   "*** We often break stuff during development.  Please submit bug reports\n"
                       "*** based on released versions only!!\n\n" );

    /* Steal system resources in the name of performance. */
    setpriority( PRIO_PROCESS, 0, -19 );
    if( !set_realtime_priority( 0 ) ) {
        fprintf( stderr, "tvtime: Can't get realtime priority for better performance, need root access.\n" );
    }

    rtctimer = rtctimer_new( 1 );
    if( !rtctimer ) {
        fprintf( stderr, "\n*** /dev/rtc support is needed for smooth video.  We STRONGLY recommend\n"
                         "*** that you load the 'rtc' kernel module before starting tvtime,\n"
                         "*** and make sure that your user has access to the device file.\n"
                         "*** See our support page at " PACKAGE_BUGREPORT " for more information\n\n" );
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
                         "*** See our support page at " PACKAGE_BUGREPORT " for more information\n\n" );
            }
        }
    }


    /* We've now stolen all our root-requiring resources, drop to a user. */
#ifdef _POSIX_SAVED_IDS
    if( seteuid( getuid() ) == -1 ) {
#else
    if( setreuid( -1, getuid() ) == -1 ) {
#endif
        fprintf( stderr, "tvtime: Unknown problems dropping root access: %s\n", 
                 strerror( errno ) );
        return 1;
    }


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

    verbose = config_get_verbose( ct );
    if( verbose ) {
        cpuinfo_print_info();
    }

    if( setpriority( PRIO_PROCESS, 0, config_get_priority( ct ) ) < 0 && verbose ) {
        fprintf( stderr, "tvtime: Can't renice to %d.\n", config_get_priority( ct ) );
    }

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
        } else if( !strcasecmp( config_get_output_driver( ct ), "mga" ) ) {
            output_driver = OUTPUT_MGA;
        } else if( !strcasecmp( config_get_output_driver( ct ), "xmga" ) ) {
            output_driver = OUTPUT_XMGA;
        } else if( !strcasecmp( config_get_output_driver( ct ), "sdl" ) ) {
            output_driver = OUTPUT_SDL;
        } else {
            output_driver = OUTPUT_XV;
        }
    } else {
        output_driver = OUTPUT_XV;
    }

    /* Setup the output. */
    if( output_driver == OUTPUT_DIRECTFB ) {
        output = get_dfb_output();
    } else if( output_driver == OUTPUT_MGA ) {
        output = get_mga_output();
    } else if( output_driver == OUTPUT_XMGA ) {
        output = get_xmga_output();
    } else if( output_driver == OUTPUT_SDL ) {
        output = get_sdl_output();
    } else {
        output = get_xv_output();
    }

    sixteennine = config_get_aspect( ct );

    if( !output || !output->init( config_get_outputheight( ct ), sixteennine, verbose ) ) {
        fprintf( stderr, "tvtime: Output driver failed to initialize: "
                         "no video output available.\n" );
        /* FIXME: Delete everything here! */
        return 1;
    }
    if( config_get_useposition( ct ) ) {
        output->set_window_position( config_get_output_x( ct ), config_get_output_y( ct ) );
    }

    /* Setup the speedy calls. */
    setup_speedy_calls( mm_accel(), verbose );

    /* Setup the tvtime object. */
    tvtime = tvtime_new();
    if( !tvtime->inputfilter ) {
        fprintf( stderr, "tvtime: Can't initialize input filters.\n" );
    }
    if( !tvtime->outputfilter ) {
        fprintf( stderr, "tvtime: Can't initialize output filters.\n" );
    }

    if( !output->is_interlaced() ) {
        linear_plugin_init();
        scalerbob_plugin_init();

        linearblend_plugin_init();
        vfir_plugin_init();

        dscaler_tomsmocomp_plugin_init();
        dscaler_greedyh_plugin_init();
        greedy_plugin_init();

        weavetff_plugin_init();
        weavebff_plugin_init();
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
    } else if( !strcasecmp( config_get_v4l_norm( ct ), "pal-60" ) ) {
        norm = VIDEOINPUT_PAL_60;
    } else {
        /* Only allow NTSC otherwise. */
        norm = VIDEOINPUT_NTSC;
    }

    /* Field display in microseconds. */
    fieldtime = videoinput_get_time_per_field( norm );
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
    if( !output->set_input_size( width, height ) ) {
        fprintf( stderr, "tvtime: Can't display input size %dx%d.\n", width, height );
        /* FIXME: Clean up. */
        return 1;
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
        fieldsavailable = 4;
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
                     "tvtime: to those which only need 2 fields.\n",
                     videoinput_get_numframes( vidin ) );
            fieldsavailable = 2;
        } else {
            fieldsavailable = 4;
        }
        if( fieldsavailable < 4 && videoinput_is_bttv( vidin ) ) {
            fprintf( stderr, "\n*** You are using the bttv driver, but without enough gbuffers available.\n"
                               "*** See the support page at " PACKAGE_BUGREPORT " for information\n"
                               "*** on how to increase your gbuffers setting.\n\n" );
        }
    } else {
        fieldsavailable = 4;
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
    tvtime_set_deinterlacer( tvtime, curmethod );

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



    /* Setup OSD stuff. */
    pixel_aspect = ( (double) width ) / ( ( (double) height ) * ( sixteennine ? (16.0 / 9.0) : (4.0 / 3.0) ) );
    osd = tvtime_osd_new( width, height, pixel_aspect, fieldtime,
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
    if( tvtime->outputfilter ) {
        outputfilter_set_osd( tvtime->outputfilter, osd );
    }

    commands = commands_new( ct, vidin, stationmgr, osd, fieldtime );
    if( !commands ) {
        fprintf( stderr, "tvtime: Can't create command handler.\n" );
        return 1;
    }
    build_deinterlacer_menu( commands_get_menu( commands, "deinterlacer" ), curmethodid );
    build_deinterlacer_description_menu( commands_get_menu( commands, "deintdescription" ), curmethodid );

    if( tvtime->inputfilter ) {
        /* Setup the video correction tables. */
        videofilter_set_bt8x8_correction( tvtime->inputfilter, commands_apply_luma_correction( commands ) );
        videofilter_set_luma_power( tvtime->inputfilter, commands_get_luma_power( commands ) );
        videofilter_set_colour_invert( tvtime->inputfilter, commands_apply_colour_invert( commands ) );
        videofilter_set_mirror( tvtime->inputfilter, commands_apply_mirror( commands ) );
    }

    if( vidin && videoinput_is_uyvy( vidin ) ) {
        if( tvtime->inputfilter ) {
            videofilter_enable_uyvy_conversion( tvtime->inputfilter );
        } else {
            fprintf( stderr, "tvtime: Card requires conversion from UYVY, but "
                     "we failed to initialize our converter!\n" );
            return 1;
        }
    }

    /* Create the user's FIFO directory */
    if( !get_tvtime_fifodir( config_get_uid( ct ) ) ) {
        fprintf( stderr, "tvtime: Cannot find FIFO directory.  "
                         "FIFO disabled.\n" );
    } else {
        int success = 0;
        if( mkdir( get_tvtime_fifodir( config_get_uid( ct ) ), S_IRWXU ) < 0 ) {
            if( errno != EEXIST ) {
                fprintf( stderr, "tvtime: Cannot create directory %s.  "
                                 "FIFO disabled.\n", 
                         get_tvtime_fifodir( config_get_uid( ct ) ) );
            } else {
                fifodir = opendir( get_tvtime_fifodir( config_get_uid( ct ) ) );
                if( !fifodir ) {
                    fprintf( stderr, "tvtime: %s is not a directory.  "
                                     "FIFO disabled.\n", 
                             get_tvtime_fifodir( config_get_uid( ct ) ) );
                } else {
                    struct stat dirstat;
                    closedir( fifodir );
                    /* Ensure the FIFO directory is owned by the user. */
                    if( !stat( get_tvtime_fifodir( config_get_uid( ct ) ), &dirstat ) ) {
                        if( dirstat.st_uid == config_get_uid( ct ) ) {
                            success = 1;
                        } else {
                            fprintf( stderr, "tvtime: You do not own %s.  "
                                             "FIFO disabled.\n",
                                     get_tvtime_fifodir( config_get_uid( ct ) ) );
                        }
                    } else {
                        fprintf( stderr, "tvtime: Cannot stat %s.  "
                                         "FIFO disabled.\n",
                                 get_tvtime_fifodir( config_get_uid( ct ) ) );
                    }
                }
            }
        } else {
            success = 1;
        }

        if( success ) {
            /* Setup the FIFO */
            if( !get_tvtime_fifo( config_get_uid( ct ) ) ) {
                fprintf( stderr, "tvtime: Cannot find FIFO file.  "
                                 "Failed to create FIFO object.\n"  );
            } else {
                fifo = fifo_new( get_tvtime_fifo( config_get_uid( ct ) ) );
                if( !fifo && verbose ) {
                    fprintf( stderr, "tvtime: Not reading input from FIFO."
                                     "  Failed to create FIFO object.\n" );
                }
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
    if( tvtime->outputfilter ) {
        outputfilter_set_console( tvtime->outputfilter, con );
    }

    in = input_new( ct, commands, con, verbose );
    if( !in ) {
        fprintf( stderr, "tvtime: Can't create input handler.\n" );
        return 1;
    }

    usevbi = config_get_usevbi( ct );
    if( usevbi ) {
        vs = vbiscreen_new( width, height, pixel_aspect, verbose );
        if( !vs ) {
            fprintf( stderr, "tvtime: Could not create vbiscreen, closed captions unavailable.\n" );
        }
    }
    if( tvtime->outputfilter ) {
        outputfilter_set_vbiscreen( tvtime->outputfilter, vs );
    }

    /* Open the VBI device. */
    if( usevbi ) {
        if( height == 480 ) {
            vbidata = vbidata_new( config_get_vbidev( ct ), vs, verbose );
            if( !vbidata ) {
                fprintf( stderr, "tvtime: Could not create vbidata.\n" );
            } else {
                vbidata_capture_mode( vbidata, CAPTURE_OFF );
            }
            commands_set_vbidata( commands, vbidata );
        } else {
            fprintf( stderr, "tvtime: VBI decoding not available for your TV norm.\n" );
        }
    }

    /* Randomly assign a tagline as the window caption. */
    srand( time( 0 ) );
    tagline = taglines[ rand() % numtaglines ];
    output->set_window_caption( tagline );

    /* Set the fullscreen position. */
    output->set_fullscreen_position( config_get_fullscreen_position( ct ) );

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

    /* Set the mier device. */
    mixer_set_device( config_get_mixer_device( ct ) );

    /* Begin capturing frames. */
    if( vidin ) {
        if( fieldsavailable == 2 ) {
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        } else if( fieldsavailable == 4 ) {
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
        int paused = 0;
        int output_x, output_y, output_w, output_h;
        int output_success = 1;
        int exposed = output->is_exposed();

        if( matte_mode ) {
            output_x = matte_x;
            output_w = matte_w;
            output_y = matte_y;
            output_h = matte_h;
        } else {
            output_x = (int) ((((double) width) * commands_get_overscan( commands )) + 0.5);
            output_w = (int) ((((double) width) - (((double) width) * commands_get_overscan( commands ) * 2.0)) + 0.5);
            output_y = (int) ((((double) height) * commands_get_overscan( commands )) + 0.5);
            output_h = (int) ((((double) height) - (((double) height) * commands_get_overscan( commands ) * 2.0)) + 0.5);
        }

        if( fifo ) {
            int cmd;
            cmd = fifo_get_next_command( fifo );
            fifo_args = fifo_get_arguments( fifo );
            if( cmd != TVTIME_NOCOMMAND ) commands_handle( commands, cmd, fifo_args );
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

        if( osd ) {
            tvtime_osd_advance_frame( osd );
        }

        output->poll_events( in );

        if( commands_quit( commands ) ) break;
        if( commands_restart_tvtime( commands ) ) {
            restarttvtime = 1;
            break;
        }
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
                        tvtime_set_deinterlacer( tvtime, curmethod );
                        if( curmethodid == firstmethod ) break;
                    }
                    if( osd ) {
                        tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                    }
                }

                if( config_get_fullscreen( cur ) && !output->is_fullscreen() ) {
                    output->toggle_fullscreen( 0, 0 );
                } else {
                    if( config_get_outputheight( cur ) < 0 ) {
                        output->resize_window_fullscreen();
                    } else {
                        output->set_window_height( config_get_outputheight( cur ) );
                        if( config_get_useposition( cur ) ) {
                            output->set_window_position( config_get_output_x( cur ), config_get_output_y( cur ) );
                        }
                    }
                }
                if( framerate_mode != config_get_framerate_mode( cur ) ) {
                    commands_set_framerate( commands, config_get_framerate_mode( cur ) );
                } else if( osd ) {
                    tvtime_osd_show_info( osd );
                }
            }
        }
        if( framerate_mode < 0 || framerate_mode != commands_get_framerate( commands ) ) {
            int showlist = !(framerate_mode < 0 ) && !commands_menu_active( commands );
            framerate_mode = commands_get_framerate( commands );

            if( osd ) {
                build_framerate_menu( commands_get_menu( commands, "framerate" ),
                                      1000000.0 / ((double) fieldtime), framerate_mode );
                commands_refresh_menu( commands );
                if( showlist ) {
                    osd_list_framerates( osd, 1000000.0 / ((double) fieldtime), framerate_mode );
                }
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
        if( commands_toggle_alwaysontop( commands ) ) {
            if( output->toggle_alwaysontop() ) {
                if( osd ) tvtime_osd_show_message( osd, "Window set as always-on-top." );
            } else {
                if( osd ) tvtime_osd_show_message( osd, "Window not set always-on-top." );
            }
        }
        if( commands_toggle_aspect( commands ) ) {
            matte_mode = 0;
            output->set_matte( 0, 0 );
            if( output->toggle_aspect() ) {
                sixteennine = 1;
                if( osd ) tvtime_osd_show_message( osd, "16:9 display mode active." );
                config_save( ct, "WideScreen", "1" );
                pixel_aspect = ( (double) width ) / ( ( (double) height ) * (16.0 / 9.0) );
            } else {
                sixteennine = 0;
                if( osd ) tvtime_osd_show_message( osd, "4:3 display mode active." );
                config_save( ct, "WideScreen", "0" );
                pixel_aspect = ( (double) width ) / ( ( (double) height ) * (4.0 / 3.0) );
            }
            if( osd ) {
                tvtime_osd_show_list( osd, 0 );
                tvtime_osd_set_pixel_aspect( osd, pixel_aspect );
            }
        }
        if( commands_toggle_matte( commands ) ) {
            double matte = 4.0 / 3.0;
            int sqwidth = sixteennine ? ((height * 16) / 9) : ((height * 4) / 3);
            int sqheight = sixteennine ? ((width * 9) / 16) : ((width * 3) / 4);

            matte_x = 0;
            matte_w = width;
            matte_mode = (matte_mode + 1) % 4;
            if( sixteennine ) {
                if( matte_mode == 0 ) {
                    matte = 16.0 / 9.0;
                } else if( matte_mode == 1 ) {
                    matte = 1.85;
                } else if( matte_mode == 2 ) {
                    matte = 2.35;
                } else if( matte_mode == 3 ) {
                    matte = 4.0 / 3.0;
                    matte_w = (int) (((double) sqheight * matte) + 0.5);
                    matte_x = (width - matte_w) / 2;
                    matte_h = height;
                    matte_y = 0;
                    output->set_matte( (matte_h * 4) / 3, matte_h );
                }
            } else {
                if( matte_mode == 1 ) {
                    matte = 16.0 / 9.0;
                } else if( matte_mode == 2 ) {
                    matte = 1.85;
                } else if( matte_mode == 3 ) {
                    matte = 2.35;
                }
            }
            if( !matte_x ) {
                matte_h = (int) ((((double) sqwidth)/matte) + 0.5);
                matte_y = (height - matte_h) / 2;
                output->set_matte( sqwidth, matte_h );
            }
            if( osd ) osd_list_matte( osd, matte_mode, sixteennine );
        }
        if( commands_toggle_pulldown_detection( commands ) ) {
            if( height == 480 ) {
                tvtime->pulldown_alg = (tvtime->pulldown_alg + 1) % PULLDOWN_MAX;
                if( osd ) {
                    if( tvtime->pulldown_alg == PULLDOWN_NONE ) {
                        tvtime_osd_show_message( osd, "Pulldown detection disabled." );
                    } else if( tvtime->pulldown_alg == PULLDOWN_VEKTOR ) {
                        tvtime_osd_show_message( osd, "Using vektor's adaptive pulldown detection." );
                    }
                }
            } else if( osd ) {
                tvtime_osd_show_message( osd, "Pulldown detection not available for your TV norm." );
            }
        }
        if( commands_show_deinterlacer_info( commands ) ) {
            osd_list_deinterlacer_info( osd, curmethodid );
        }
        if( !output->is_interlaced() && commands_toggle_deinterlacer( commands ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            tvtime_set_deinterlacer( tvtime, curmethod );
            if( osd ) {
                build_deinterlacer_menu( commands_get_menu( commands, "deinterlacer" ), curmethodid );
                build_deinterlacer_description_menu( commands_get_menu( commands, "deintdescription" ), curmethodid );
                commands_refresh_menu( commands );
                osd_list_deinterlacers( osd, curmethodid );
                tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                tvtime_osd_show_info( osd );
            }
            config_save( ct, "DeinterlaceMethod", curmethod->short_name );
        }
        if( commands_set_deinterlacer( commands ) ) {
            curmethodid = 0;
            curmethod = get_deinterlace_method( 0 );
            while( strcasecmp( commands_get_new_deinterlacer( commands ), curmethod->short_name ) ) {
                curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
                curmethod = get_deinterlace_method( curmethodid );
                if( !curmethodid ) break;
            }
            if( osd ) {
                build_deinterlacer_menu( commands_get_menu( commands, "deinterlacer" ), curmethodid );
                build_deinterlacer_description_menu( commands_get_menu( commands, "deintdescription" ), curmethodid );
                commands_refresh_menu( commands );
            }
            tvtime_set_deinterlacer( tvtime, curmethod );
            config_save( ct, "DeinterlaceMethod", curmethod->short_name );
        }
        if( commands_update_luma_power( commands ) ) {
            videofilter_set_luma_power( tvtime->inputfilter, commands_get_luma_power( commands ) );
        }
        if( commands_resize_window( commands ) ) {
            output->set_window_height( output->get_visible_height() );
        }
        commands_next_frame( commands );

        /* Notice this because it's cheap. */
        videofilter_set_bt8x8_correction( tvtime->inputfilter, commands_apply_luma_correction( commands ) );
        videofilter_set_colour_invert( tvtime->inputfilter, commands_apply_colour_invert( commands ) );
        videofilter_set_mirror( tvtime->inputfilter, commands_apply_mirror( commands ) );

        if( osd ) {
            tvtime_osd_set_pulldown( osd, tvtime->pulldown_alg );
            if( tvtime->pulldown_alg == PULLDOWN_NONE ) {
                tvtime_osd_set_film_mode( osd, -1 );
            } else {
                tvtime_osd_set_film_mode( osd, tvtime->filmmode );
            }
        }


        /**
         * Acquire the next frame.
         */
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
            acquired = 1;
            if( rvrreader ) {
                if( !rvrreader_next_frame( rvrreader ) ) break;
                curframe = rvrreader_get_curframe( rvrreader );
                lastframe = rvrreader_get_lastframe( rvrreader );
                secondlastframe = rvrreader_get_secondlastframe( rvrreader );
            } else if( vidin ) {
                curframe = videoinput_next_frame( vidin, &curframeid );
                if( !curframe ) {
                    fprintf( stderr, "tvtime: Can't capture next frame, exiting.\n" );
                    tuner_state = TUNER_STATE_NO_SIGNAL;
                    acquired = 0;
                } else {
                    /* Make sure our buffers are valid. */
                    if( fieldsavailable == 2 ) {
                        if( videoinput_buffer_invalid( vidin, lastframeid ) ) {
                            lastframe = curframe;
                            lastframeid = -1;
                        }
                    } else if( fieldsavailable == 4 ) {
                        if( videoinput_buffer_invalid( vidin, secondlastframeid ) ) {
                            secondlastframe = curframe;
                            secondlastframeid = -1;
                        }
                        if( videoinput_buffer_invalid( vidin, lastframeid ) ) {
                            lastframe = curframe;
                            lastframeid = -1;
                        }
                    }
                }
            }
            tvtime->filtered_curframe = 0;
        } else {
            /**
             * Wait for the next field time.
             */
            if( rtctimer ) {
                while( performance_get_usecs_since_frame_acquired( perf )
                       < ( (fieldtime*2) - (rtctimer_get_usecs( rtctimer ) / 2) ) ) {
                    rtctimer_next_tick( rtctimer );
                }
            } else if( performance_get_usecs_since_frame_acquired( perf ) < fieldtime ) {
                usleep( fieldtime - performance_get_usecs_since_frame_acquired( perf ) );
            }
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
            int framesize = width * height * 2;
            fprintf( stderr, "tvtime: Stats using '%s' at %dx%d.\n", 
                     (curmethod) ? curmethod->name : "interlaced passthrough", 
                     width, height );
            if( curmethod && curmethod->doscalerbob ) {
                framesize = width * height;
            }
            performance_print_last_frame_stats( perf, framesize );
            osd_list_statistics( osd, perf, (curmethod) ? curmethod->name : "interlaced passthrough",
                                 width, height, framesize, fieldtime, framerate_mode,
                                 videoinput_get_norm( vidin ) );
        }
        if( config_get_debug( ct ) ) {
            if( curmethod )  {
                performance_print_frame_drops( perf, curmethod->doscalerbob ? (width * height) : (width * height * 2) );
            } else {
                performance_print_frame_drops( perf, width * height * 2 );
            }
        }

        /**
         * Show the bottom field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_TFF) && !output->is_interlaced() ) {
            if( curmethod->doscalerbob && !showbars ) {
                output_success = output->show_frame( output_x, output_y/2, output_w, output_h/2 );
            } else {
                output_success = output->show_frame( output_x, output_y, output_w, output_h );
            }
        }
        performance_checkpoint_show_bot_field( perf );
        if( !output_success ) break;


        /**
         * Deinterlace the top field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_BFF) ) {
            if( output->is_interlaced() ) {
                if( send_fields ) {
                    /* Wait until we can draw the even field. */
                    output->wait_for_sync( 0 );

                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( tvtime, output->get_output_buffer(),
                                                   curframe, 0, width, height, width * 2, 
                                                   output->get_output_stride(), 0);
                    output->unlock_output_buffer();
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                } else {
                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( tvtime, output->get_output_buffer(),
                                                   curframe, 0, width, height, width * 2, 
                                                   output->get_output_stride(), 1 );
                    output->unlock_output_buffer();
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                }
            } else {
                /* Build the output from the top field. */
                output->lock_output_buffer();
                if( showbars ) {
                    for( i = 0; i < height; i++ ) {
                        blit_packed422_scanline( output->get_output_buffer() + i*output->get_output_stride(),
                                                 colourbars + (i*width*2), width );
                    }
                } else {
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( tvtime, output->get_output_buffer(),
                                                   curframe, 0, width, height, width * 2,
                                                   output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( tvtime, output->get_output_buffer(),
                                                         curframe, lastframe, secondlastframe,
                                                         0, width, height, width * 2,
                                                         output->get_output_stride() );
                    }
                }

                /* For the screenshot, use the output after we build the top field. */
                if( screenshot ) {
                    const char *basename;
                    const char *outfile;
                    char *filename;
                    char *pathfilename;
                    char *message;
                    char timestamp[ 256 ];
                    time_t tm = time( 0 );

                    if( fifo_args && strlen( fifo_args ) ) {
                        basename = outfile = fifo_args;
                    } else {
                        strftime( timestamp, 
                                  sizeof( timestamp ),
                                  config_get_timeformat( ct ),
                                  localtime( &tm ) );
                        asprintf( &filename,
                                  "tvtime-output-%s.png",
                                  timestamp );
                        asprintf( &pathfilename,
                                  "%s/%s",
                                  config_get_screenshot_dir( ct ),
                                  filename );
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
                    asprintf( &message,
                              "Screenshot saved: %s",
                              basename );
                    if( osd ) tvtime_osd_show_message( osd, message );
                    screenshot = 0;
                }
                output->unlock_output_buffer();
            }
        }

        performance_checkpoint_constructed_top_field( perf );


        /**
         * We're done with the secondlastframe now.
         */
        if( vidin && acquired ) {
            if( fieldsavailable == 4 ) {
                if( secondlastframeid >= 0 ) videoinput_free_frame( vidin, secondlastframeid );
                if( vbidata ) vbidata_process_frame( vbidata, printdebug );
            } else if( fieldsavailable == 2 ) {
                if( lastframeid >= 0 ) videoinput_free_frame( vidin, lastframeid );
                if( vbidata ) vbidata_process_frame( vbidata, printdebug );
            }
        }


        /**
         * Wait for the next field time.
         */
        if( rtctimer ) {
            while( performance_get_usecs_since_frame_acquired( perf )
                   < ( fieldtime - (rtctimer_get_usecs( rtctimer ) / 2) ) ) {
                rtctimer_next_tick( rtctimer );
            }
        } else if( performance_get_usecs_since_frame_acquired( perf ) < fieldtime ) {
            usleep( fieldtime - performance_get_usecs_since_frame_acquired( perf ) );
        }
        performance_checkpoint_wait_for_bot_field( perf );


        /**
         * Show the top field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_BFF) && !output->is_interlaced() ) {
            if( curmethod->doscalerbob && !showbars ) {
                output_success = output->show_frame( output_x, output_y/2, output_w, output_h/2 );
            } else {
                output_success = output->show_frame( output_x, output_y, output_w, output_h );
            }
        }
        performance_checkpoint_show_top_field( perf );
        if( !output_success ) break;


        /**
         * Deinterlace the bottom field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL || framerate_mode == FRAMERATE_HALF_TFF) ) {
            if( output->is_interlaced() ) {
                if( send_fields ) {
                    /* Wait until we can draw the odd field. */
                    output->wait_for_sync( 1 );
                    
                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( tvtime, output->get_output_buffer(),
                                                   curframe, 1, width, height, width * 2, 
                                                   output->get_output_stride(), 0 );
                    output->unlock_output_buffer();
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                }
            } else {
                /* Build the output from the bottom field. */
                output->lock_output_buffer();
                if( showbars ) {
                    for( i = 0; i < height; i++ ) {
                        blit_packed422_scanline( output->get_output_buffer() + i*output->get_output_stride(),
                                                 colourbars + (i*width*2), width );
                    }
                } else {
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( tvtime, output->get_output_buffer(),
                                                   curframe, 1, width, height, width * 2,
                                                   output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( tvtime, output->get_output_buffer(),
                                                         curframe, lastframe, secondlastframe, 1,
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
            if( fieldsavailable == 4 ) {
                secondlastframeid = lastframeid;
                secondlastframe = lastframe;
                lastframeid = curframeid;
                lastframe = curframe;
            } else if( fieldsavailable == 2 ) {
                lastframeid = curframeid;
                lastframe = curframe;
            } else {
                if( curframeid >= 0 ) videoinput_free_frame( vidin, curframeid );
                if( vbidata ) vbidata_process_frame( vbidata, printdebug );
            }
        }
    }

    /* Return to normal scheduling. */
    set_default_priority();

    /* Remember to save our settings if we were scanning. */
    if( scanning ) {
        station_writeconfig( stationmgr );
    }

    config_save( ct, "Norm", commands_get_new_norm( commands ) );

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

    /* Free state. */
    tvtime_delete( tvtime );

    xmlCleanupParser();

    if( restarttvtime ) {
        char *newargv[ 2 ];

        /* I think I'm doing this correctly... ?? */
        newargv[ 0 ] = argv[ 0 ];
        newargv[ 1 ] = 0;

        fprintf( stderr, "tvtime: Restarting.\n" );
        execve( argv[ 0 ], newargv, environ );
    }

    fprintf( stderr, "tvtime: Thank you for using tvtime.\n" );
    return 0;
}

