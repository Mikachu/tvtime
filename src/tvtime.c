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
#include <locale.h>
#include <stdlib.h>
#include <termio.h>
#include <ctype.h>
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
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
#else
# define _(string) string
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
#include "copyfunctions.h"
#include "deinterlace.h"
#include "plugins.h"
#include "performance.h"
#include "taglines.h"
#include "xvoutput.h"
#include "vbidata.h"
#include "vbiscreen.h"
#include "videofilter.h"
#include "fifo.h"
#include "commands.h"
#include "station.h"
#include "pulldown.h"
#include "utils.h"
#include "cpuinfo.h"
#include "outputfilter.h"
#include "mm_accel.h"
#include "menu.h"
#include "tvtimeglyphs.h"

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
    OUTPUT_DIRECTFB_MATROXTV,
    OUTPUT_MGA,
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
    if( !tvtime->inputfilter ) {
        free( tvtime );
        return 0;
    }
    tvtime->outputfilter = outputfilter_new();
    if( !tvtime->outputfilter ) {
        videofilter_delete( tvtime->inputfilter );
        free( tvtime );
        return 0;
    }
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
    videofilter_delete( tvtime->inputfilter );
    outputfilter_delete( tvtime->outputfilter );
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
        if( !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, i ) ) {
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
                                             int bottom_field, int second_field,
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

        if( !tvtime->filtered_curframe ) {
            for( i = 0; i < frame_height; i++ ) {
                if( videofilter_active_on_scanline( tvtime->inputfilter, i ) ) {
                    videofilter_packed422_scanline( tvtime->inputfilter, curframe + (i*instride), width, 0, i );
                }
            }
        }

        data.f0 = curframe;
        data.f1 = lastframe;
        data.f2 = secondlastframe;

        tvtime->curmethod->deinterlace_frame( output, outstride, &data, bottom_field, bottom_field, width, frame_height );

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
        if( !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, scanline ) ) {
            videofilter_packed422_scanline( tvtime->inputfilter, curframe, width, 0, scanline );
        }
        blit_packed422_scanline( output, curframe, width );
        if( tvtime->outputfilter ) {
            outputfilter_composite_packed422_scanline( tvtime->outputfilter, output, width, 0, scanline );
        }

        output += outstride;
        scanline++;

        /* Something is wrong here. -Billy */
        /* xine's suggestion:
        loop_size = ((frame_height - 2) / 2);
        */
        loop_size = ((frame_height - 2) / 2) - bottom_field;
        for( i = loop_size; i; --i ) {
            deinterlace_scanline_data_t data;

            data.bottom_field = bottom_field;

            data.t0 = curframe;
            data.b0 = curframe + (instride*2);

            if( !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, scanline + 1 ) ) {
                videofilter_packed422_scanline( tvtime->inputfilter, curframe + instride, width, 0, scanline );
                videofilter_packed422_scanline( tvtime->inputfilter, curframe + (instride*2), width, 0, scanline + 1 );
            }

            if( second_field ) {
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

            if( second_field ) {
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

            if( second_field ) {
                data.t1 = curframe + instride;
                data.b1 = (i > 1) ? (curframe + (instride*3)) : (curframe + instride);
            } else {
                data.t1 = lastframe + instride;
                data.b1 = (i > 1) ? (lastframe + (instride*3)) : (lastframe + instride);
            }

            data.tt2 = lastframe;
            data.m2  = lastframe + (instride*2);
            data.bb2 = (i > 1) ? (lastframe + (instride*4)) : (lastframe + (instride*2));

            if( second_field ) {
                data.t2 = lastframe + instride;
                /* xine's suggestion:
                data.b2 = (i > 1) ? (lastframe + (instride*3)) : (lastframe + instride);
                */
                data.b2 = lastframe + (instride*3);
            } else {
                data.t2 = secondlastframe + instride;
                /* xine's suggestion:
                data.b2 = (i > 1) ? (secondlastframe + (instride*3)) : (secondlastframe + instride);
                */
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
    if( !tvtime->filtered_curframe && videofilter_active_on_scanline( tvtime->inputfilter, scanline + bottom_field ) ) {
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

        if( !tvtime->filtered_curframe && 
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

    if( get_deinterlace_method( curmethod ) ) {
        tvtime_osd_list_set_lines( osd, 11 );
        tvtime_osd_list_set_text( osd, 0, get_deinterlace_method( curmethod )->name );
        for( i = 0; i < 10; i++ ) {
            tvtime_osd_list_set_text( osd, i + 1, get_deinterlace_method( curmethod )->description[ i ] );
        }
        tvtime_osd_list_set_hilight( osd, -1 );
        tvtime_osd_show_list( osd, 1, 0 );
    }
}

static void osd_list_deinterlacers( tvtime_osd_t *osd, int curmethod )
{
    int nummethods = get_num_deinterlace_methods();
    int i;

    if( nummethods ) {
        tvtime_osd_list_set_lines( osd, get_num_deinterlace_methods() + 1 );
        tvtime_osd_list_set_text( osd, 0, _("Deinterlacer configuration") );
        for( i = 0; i < nummethods; i++ ) {
            tvtime_osd_list_set_text( osd, i + 1, get_deinterlace_method( i )->name );
        }
        tvtime_osd_list_set_hilight( osd, curmethod + 1 );
        tvtime_osd_show_list( osd, 1, 0 );
    }
}

static void build_deinterlacer_menu( menu_t *menu, int curmethod )
{
    int nummethods = get_num_deinterlace_methods();
    char string[ 128 ];
    int i;

    menu_set_back_command( menu, TVTIME_SHOW_MENU, "processing" );
    for( i = 0; i < nummethods; i++ ) {
        if( i == curmethod ) {
            snprintf( string, sizeof( string ), TVTIME_ICON_RADIOON "  %s",
                      get_deinterlace_method( i )->name );
        } else {
            snprintf( string, sizeof( string ), TVTIME_ICON_RADIOOFF "  %s",
                      get_deinterlace_method( i )->name );
        }
        menu_set_text( menu, i + 1, string );
        menu_set_enter_command( menu, i + 1, TVTIME_SET_DEINTERLACER,
                                get_deinterlace_method( i )->short_name );
    }

    snprintf( string, sizeof( string ), TVTIME_ICON_PLAINLEFTARROW "  %s", 
              _("Back") );
    menu_set_back_command( menu, TVTIME_SHOW_MENU, "processing" );
    menu_set_text( menu, nummethods + 1, string );
    menu_set_enter_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
}

static void build_deinterlacer_description_menu( menu_t *menu, int curmethod )
{
    char string[ 128 ];
    int i;

    if( get_deinterlace_method( curmethod ) ) {
        menu_set_text( menu, 0, get_deinterlace_method( curmethod )->name );
        menu_set_back_command( menu, TVTIME_SHOW_MENU, "processing" );
        for( i = 0; i < 10; i++ ) {
            menu_set_text( menu, i + 1, get_deinterlace_method( curmethod )->description[ i ] );
            menu_set_enter_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
        }
        snprintf( string, sizeof( string ), TVTIME_ICON_PLAINLEFTARROW "  %s",
                  _("Back") );
        menu_set_default_cursor( menu, i );
        menu_set_back_command( menu, TVTIME_SHOW_MENU, "processing" );
        menu_set_text( menu, i + 1, string );
        menu_set_enter_command( menu, i + 1, TVTIME_SHOW_MENU, "processing" );
    }
}

static void build_framerate_menu( menu_t *menu, double maxrate, int mode )
{
    char string[ 128 ];
    char text[ 128 ];

    snprintf( text, sizeof( text ), _("Full rate: %.2f fps"), maxrate );
    snprintf( string, sizeof( string ), mode == 0 ?
              TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s", text );
    menu_set_text( menu, 1, string );
    menu_set_back_command( menu, TVTIME_SHOW_MENU, "processing" );
    menu_set_enter_command( menu, 1, TVTIME_SET_FRAMERATE, "full" );

    snprintf( text, sizeof( text ),
              _("Half rate, deinterlace top fields: %.2f fps"), maxrate / 2 );
    snprintf( string, sizeof( string ), mode == 1 ?
              TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s", text );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SET_FRAMERATE, "top" );

    snprintf( text, sizeof( text ),
              _("Half rate, deinterlace bottom fields: %.2f fps"), maxrate / 2 );
    snprintf( string, sizeof( string ), mode == 2 ?
              TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s", text );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SET_FRAMERATE, "bottom" );

    snprintf( string, sizeof( string ), TVTIME_ICON_PLAINLEFTARROW "  %s",
              _("Back") );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
}

static void build_output_menu( menu_t *menu, int widescreen,
                               int fullscreen, int alwaysontop,
                               int fullscreen_supported,
                               int alwaysontop_supported,
                               int overscan_supported,
                               int quiet_screenshots )
{
    char string[ 128 ];
    int cur = 1;

    menu_set_back_command( menu, TVTIME_SHOW_MENU, "root" );
    if( overscan_supported ) {
        snprintf( string, sizeof( string ), TVTIME_ICON_OVERSCANSETTING "  %s",
                  _("Overscan setting") );
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "overscan" );
        cur++;
    }

    snprintf( string, sizeof( string ), TVTIME_ICON_APPLYMATTE "  %s",
              _("Apply matte") );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "matte" );
    cur++;

    snprintf( string, sizeof( string ), widescreen ?
              TVTIME_ICON_GENERALTOGGLEON "  %s" :
              TVTIME_ICON_GENERALTOGGLEOFF "  %s",
              _("16:9 output") );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_TOGGLE_ASPECT, "" );
    cur++;

    snprintf( string, sizeof( string ), 
              TVTIME_ICON_TELEVISIONSTANDARD "  %s",
              _("Resize window to match contents") );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_AUTO_ADJUST_WINDOW, "" );
    cur++;

    if( fullscreen_supported ) {
        snprintf( string, sizeof( string ), fullscreen ?
                  TVTIME_ICON_GENERALTOGGLEON "  %s" :
                  TVTIME_ICON_GENERALTOGGLEOFF "  %s",
                  _("Fullscreen") );

        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_TOGGLE_FULLSCREEN, "" );
        cur++;

        snprintf( string, sizeof( string ), TVTIME_ICON_SETFSPOS "  %s",
                  _("Set fullscreen position") );
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "fspos" );
        cur++;
    }

    if( alwaysontop_supported ) {
        snprintf( string, sizeof( string ), alwaysontop ?
                  TVTIME_ICON_GENERALTOGGLEON "  %s" :
                  TVTIME_ICON_GENERALTOGGLEOFF "  %s",
                  _("Always-on-top") );
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_TOGGLE_ALWAYSONTOP, "" );
        cur++;
    }

    snprintf( string, sizeof( string ), quiet_screenshots ?
              TVTIME_ICON_GENERALTOGGLEON "  %s" :
              TVTIME_ICON_GENERALTOGGLEOFF "  %s",
              _("Quiet screenshots") );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_TOGGLE_QUIET_SCREENSHOTS, "" );
    cur++;

    snprintf( string, sizeof( string ), TVTIME_ICON_PLAINLEFTARROW "  %s",
              _("Back") );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "root" );
}

static void build_fspos_menu( menu_t *menu, int fspos )
{
    char string[ 128 ];

    snprintf( string, sizeof( string ), (fspos == 0) ?
              TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
              _("Centre") );
    menu_set_text( menu, 1, string );
    menu_set_back_command( menu, TVTIME_SHOW_MENU, "output" );
    menu_set_enter_command( menu, 1, TVTIME_SET_FULLSCREEN_POSITION, "centre" );

    snprintf( string, sizeof( string ), (fspos == 1) ?
              TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
              _("Top") );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SET_FULLSCREEN_POSITION, "top" );

    snprintf( string, sizeof( string ), (fspos == 2) ?
              TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
              _("Bottom") );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SET_FULLSCREEN_POSITION, "bottom" );
    snprintf( string, sizeof( string ), TVTIME_ICON_PLAINLEFTARROW "  %s",
              _("Back") );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "output" );
}

static void osd_list_framerates( tvtime_osd_t *osd, double maxrate, int mode )
{
    char text[ 200 ];

    tvtime_osd_list_set_lines( osd, 4 );
    tvtime_osd_list_set_text( osd, 0, _("Attempted framerate") );

    snprintf( text, sizeof( text ), _("Full rate: %.2f fps"), maxrate );
    tvtime_osd_list_set_text( osd, 1, text );

    snprintf( text, sizeof( text ),
              _("Half rate, deinterlace top fields: %.2f fps"), maxrate / 2 );
    tvtime_osd_list_set_text( osd, 2, text );

    snprintf( text, sizeof( text ),
              _("Half rate, deinterlace bottom fields: %.2f fps"),
              maxrate / 2 );
    tvtime_osd_list_set_text( osd, 3, text );

    tvtime_osd_list_set_hilight( osd, mode + 1 );
    tvtime_osd_show_list( osd, 1, 0 );
}

static void osd_list_statistics( tvtime_osd_t *osd, performance_t *perf,
                                 const char *deinterlacer, int width,
                                 int height, int framesize, int fieldtime,
                                 int frameratemode, int norm )
{
    double blit_time = performance_get_estimated_blit_time( perf );
    char text[ 200 ];

    tvtime_osd_list_set_lines( osd, frameratemode ? 7 : 8 );
    tvtime_osd_list_set_text( osd, 0, _("Performance estimates") );

    snprintf( text, sizeof( text ), "%s: %s", _("Deinterlacer"), deinterlacer );
    tvtime_osd_list_set_text( osd, 1, text );

    snprintf( text, sizeof( text ), _("Input: %s at %dx%d pixels"),
              videoinput_get_norm_name( norm ), width, height );
    tvtime_osd_list_set_text( osd, 2, text );

    snprintf( text, sizeof( text ), _("Attempted framerate: %.2f fps"),
             frameratemode ? 1000000.0 / ((double) (fieldtime*2)) : 1000000.0 / ((double) fieldtime) );
    tvtime_osd_list_set_text( osd, 3, text );

    snprintf( text, sizeof( text ), _("Average blit time: %.2f ms (%.0f MB/sec)"),
             blit_time, (((double) framesize) * 0.00095367431640625000) / blit_time );
    tvtime_osd_list_set_text( osd, 4, text );

    snprintf( text, sizeof( text ), _("Average render time: %5.2f ms"),
              performance_get_estimated_rendering_time( perf ) );
    tvtime_osd_list_set_text( osd, 5, text );

    snprintf( text, sizeof( text ), _("Dropped frames: %d"),
             performance_get_dropped_frames( perf ) );
    tvtime_osd_list_set_text( osd, 6, text );

    if( !frameratemode ) {
        snprintf( text, sizeof( text ), 
                  _("Blit spacing: %4.1f/%4.1f ms (want %4.1f ms)"),
                  performance_get_time_top_to_bot( perf ),
                  performance_get_time_bot_to_top( perf ),
                  ((double) fieldtime) * 0.001 );
        tvtime_osd_list_set_text( osd, 7, text );
    }

    tvtime_osd_list_set_hilight( osd, -1 );
    tvtime_osd_show_list( osd, 1, 0 );
}

static void build_matte_menu( menu_t *menu, int mode, int sixteennine )
{
    char string[ 128 ];

    menu_set_back_command( menu, TVTIME_SHOW_MENU, "output" );
    if( sixteennine ) {
        snprintf( string, sizeof( string ), (mode == 0) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("16:9 + Overscan") );
        menu_set_text( menu, 1, string );
        menu_set_enter_command( menu, 1, TVTIME_SET_MATTE, "16:9" );
        snprintf( string, sizeof( string ), (mode == 1) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("1.85:1") );
        menu_set_text( menu, 2, string );
        menu_set_enter_command( menu, 2, TVTIME_SET_MATTE, "1.85:1" );
        snprintf( string, sizeof( string ), (mode == 2) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("2.35:1") );
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SET_MATTE, "2.35:1" );
        snprintf( string, sizeof( string ), (mode == 3) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("4:3 centre") );
        menu_set_text( menu, 4, string );
        menu_set_enter_command( menu, 4, TVTIME_SET_MATTE, "4:3" );
        snprintf( string, sizeof( string ), (mode == 4) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("16:10") );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_SET_MATTE, "16:10" );
    } else {
        snprintf( string, sizeof( string ), (mode == 0) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("4:3 + Overscan") );
        menu_set_text( menu, 1, string );
        menu_set_enter_command( menu, 1, TVTIME_SET_MATTE, "4:3" );
        snprintf( string, sizeof( string ), (mode == 1) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("16:9") );
        menu_set_text( menu, 2, string );
        menu_set_enter_command( menu, 2, TVTIME_SET_MATTE, "16:9" );
        snprintf( string, sizeof( string ), (mode == 2) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("16:10") );
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SET_MATTE, "16:10" );
        snprintf( string, sizeof( string ), (mode == 3) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("1.85:1") );
        menu_set_text( menu, 4, string );
        menu_set_enter_command( menu, 4, TVTIME_SET_MATTE, "1.85:1" );
        snprintf( string, sizeof( string ), (mode == 4) ?
                  TVTIME_ICON_RADIOON "  %s" : TVTIME_ICON_RADIOOFF "  %s",
                  _("2.35:1") );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_SET_MATTE, "2.35:1" );
    }

    snprintf( string, sizeof( string ), TVTIME_ICON_PLAINLEFTARROW "  %s",
              _("Back") );
    menu_set_text( menu, 6, string );
    menu_set_enter_command( menu, 6, TVTIME_SHOW_MENU, "output" );
}

static void osd_list_matte( tvtime_osd_t *osd, int mode, int sixteennine )
{
    tvtime_osd_list_set_lines( osd, 6 );
    if( sixteennine ) {
        tvtime_osd_list_set_text( osd, 0, _("Matte setting (Anamorphic input)") );
        tvtime_osd_list_set_text( osd, 1, _("16:9 + Overscan") );
        tvtime_osd_list_set_text( osd, 2, "1.85:1" );
        tvtime_osd_list_set_text( osd, 3, "2.35:1" );
        tvtime_osd_list_set_text( osd, 4, _("4:3 centre") );
        tvtime_osd_list_set_text( osd, 5, "16:10" );
    } else {
        tvtime_osd_list_set_text( osd, 0, _("Matte setting (4:3 input)") );
        tvtime_osd_list_set_text( osd, 1, _("4:3 + Overscan") );
        tvtime_osd_list_set_text( osd, 2, "16:9" );
        tvtime_osd_list_set_text( osd, 3, "16:10" );
        tvtime_osd_list_set_text( osd, 4, "1.85:1" );
        tvtime_osd_list_set_text( osd, 5, "2.35:1" );
    }
    tvtime_osd_list_set_hilight( osd, mode + 1 );
    tvtime_osd_show_list( osd, 1, 0 );
}

/*
 * setup_i18n() is run by main() before tvtime_main(),
 * so using gettext here is safe.
 */
int tvtime_main( rtctimer_t *rtctimer, int read_stdin, int realtime,
                 int argc, char **argv )
{
    videoinput_t *vidin = 0;
    station_mgr_t *stationmgr = 0;
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
    int has_signal = 0;
    vbidata_t *vbidata = 0;
    vbiscreen_t *vs = 0;
    fifo_t *fifo = 0;
    char *fifofile = 0;
    commands_t *commands = 0;
    int fadepos = 0;
    int scanwait = scan_delay;
    int scanning = 0;
    int numscanned = 0;
    int framerate_mode = -1;
    char kbd_cmd[ 1024 ];
    int kbd_pos = 0;
    int kbd_available;
    char *error_string = 0;
    double pixel_aspect;
    char number[ 6 ];
    tvtime_t *tvtime;
    deinterlace_method_t *curmethod;
    int curmethodid;
    int matte_x = 0;
    int matte_w = 0;
    int matte_y = 0;
    int matte_h = 0;
    int matte_mode = 0;
    int restarttvtime = 0;
    int return_value = 0;
    int last_current_id = -1;
    int quiet_screenshots = 0;
    char prevloc[ 256 ];
    int i;

    ct = config_new();
    if( !ct ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
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

    if( setpriority( PRIO_PROCESS, 0, config_get_priority( ct ) ) < 0 ) {
        if( verbose ) {
            fprintf( stderr, "%s: Cannot set priority to %d: %s.\n",
                     argv[ 0 ], config_get_priority( ct ), strerror( errno ) );
        }
    }

    send_fields = config_get_send_fields( ct );

    /* Setup the output. */
    output = get_xv_output();

    sixteennine = config_get_aspect( ct );

    if( !output || !output->init( config_get_geometry( ct ),
                                  sixteennine, verbose ) ) {
        /* Error messages are driver specific. */
        return 1;
    }

    /* Setup the speedy calls. */
    setup_speedy_calls( mm_accel(), verbose );

    /* Setup the tvtime object. */
    tvtime = tvtime_new();
    if( !tvtime ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
    }

    if( !output->is_interlaced() ) {
        register_deinterlace_method( linear_get_method() );
        register_deinterlace_method( scalerbob_get_method() );

        register_deinterlace_method( linearblend_get_method() );
        register_deinterlace_method( vfir_get_method() );

#ifdef ARCH_X86
        register_deinterlace_method( dscaler_tomsmocomp_get_method() );
        register_deinterlace_method( dscaler_greedyh_get_method() );
#endif
        register_deinterlace_method( greedy_get_method() );

        register_deinterlace_method( weave_get_method() );
        register_deinterlace_method( weavetff_get_method() );
        register_deinterlace_method( weavebff_get_method() );
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
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }

    stationmgr = station_new( videoinput_get_norm_name( norm ),
                              config_get_v4l_freq( ct ),
                              config_get_ntsc_cable_mode( ct ), verbose );
    if( !stationmgr ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }
    station_set( stationmgr, config_get_prev_channel( ct ) );
    station_set( stationmgr, config_get_start_channel( ct ) );


    /* Default to a width specified on the command line. */
    width = config_get_inputwidth( ct );

    vidin = videoinput_new( config_get_v4l_device( ct ), 
                            config_get_inputwidth( ct ), 
                            config_get_audio_boost( ct ), 
                            norm, verbose );
    if( !vidin ) {
        if( asprintf( &error_string,
                      _("Cannot open capture device %s."),
                      config_get_v4l_device( ct ) ) < 0 ) {
            error_string = 0;
        }
    } else if( videoinput_get_numframes( vidin ) < 2 ) {
        lfprintf( stderr, _("\n"
    "    Your capture card driver, %s, does not seem\n"
    "    to support full framerate capture.  Please check to see if it is\n"
    "    misconfigured, or if you have selected the wrong capture\n"
    "    device (%s).\n\n"), videoinput_get_driver_name( vidin ),
                             config_get_v4l_device( ct ) );
        videoinput_delete( vidin );
        vidin = 0;
     } else {
        const char *audiomode = config_get_audio_mode( ct );
        videoinput_set_input_num( vidin, config_get_inputnum( ct ) );

        if( audiomode ) {
            if( !strcasecmp( audiomode, "mono" ) ) {
                videoinput_set_audio_mode( vidin, VIDEOINPUT_MONO );
            } else if( !strcasecmp( audiomode, "stereo" ) ) {
                videoinput_set_audio_mode( vidin, VIDEOINPUT_STEREO );
            } else if( !strcasecmp( audiomode, "sap" )
                       || !strcasecmp( audiomode, "lang1" ) ) {
                videoinput_set_audio_mode( vidin, VIDEOINPUT_LANG1 );
            } else {
                videoinput_set_audio_mode( vidin, VIDEOINPUT_LANG2 );
            }
        }

        width = videoinput_get_width( vidin );
        height = videoinput_get_height( vidin );
        if( verbose ) {
            fprintf( stderr, "tvtime: Sampling input at %d pixels "
                               "per scanline.\n", width );
        }
    }

    /* Set input size. */
    if( !output->set_input_size( width, height ) ) {
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
    if( vidin ) {
        if( videoinput_get_numframes( vidin ) == 2 ) {
            fieldsavailable = 1;
        } else if( videoinput_get_numframes( vidin ) == 3 ) {
            fieldsavailable = 2;
        } else {
            fieldsavailable = 4;
        }

        if( fieldsavailable < 4 ) {
            if( videoinput_is_bttv( vidin ) ) {
                lfprintf( stderr,
                      _("\n"
        "    You are using the bttv driver, but have not configured enough\n"
        "    buffers for tvtime to process the video optimally.  This is\n"
        "    true by default with bttv in kernels before 2.4.21.  Please\n"
        "    set the option gbuffers=4 when loading bttv.  For more\n"
        "    information see our support page at %s\n\n"), PACKAGE_BUGREPORT );
            } else {
                lfprintf( stderr, _("\n"
     "    Your capture card driver, %s, is not providing\n"
     "    enough buffers for tvtime to process the video.  Please check with\n"
     "    your driver documentation to see if you can increase the number\n"
     "    of buffers provided to applications, and report this to the tvtime\n"
     "    bug tracker at %s\n\n"), videoinput_get_driver_name( vidin ),
                                   PACKAGE_BUGREPORT );
            }
        }
    } else {
        fieldsavailable = 4;
    }

    if( output->is_interlaced() ) {
        curmethodid = 0;
        curmethod = 0;
    } else {
        filter_deinterlace_methods( speedy_get_accel(), fieldsavailable );
        curmethodid = 0;
        curmethod = get_deinterlace_method( 0 );
        while( strcasecmp( config_get_deinterlace_method( ct ),
                           curmethod->short_name ) ) {
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
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }
    build_colourbars( colourbars, width, height );
    build_blue_frame( blueframe, width, height );
    blit_packed422_scanline( saveframe, blueframe, width * height );
    secondlastframe = lastframe = blueframe;

    /* Set the mixer device. */
    mixer_set_device( config_get_mixer_device( ct ) );
    mixer_set_state( config_get_muted( ct ), config_get_unmute_volume( ct ) );

    /* Setup OSD stuff. */
    pixel_aspect = ( (double) width ) /
        ( ( (double) height ) * ( sixteennine ? (16.0 / 9.0) : (4.0 / 3.0) ) );
    osd = tvtime_osd_new( width, height, pixel_aspect, fieldtime,
                          config_get_channel_text_rgb( ct ),
                          config_get_other_text_rgb( ct ) );
    if( !osd ) {
        lfputs( _("On screen display failed to initialize, disabled.\n"),
                stderr );
    } else {
        tvtime_osd_set_timeformat( osd, config_get_timeformat( ct ) );
        if( curmethod ) {
            tvtime_osd_set_deinterlace_method( osd, curmethod->name );
        }
        if( vidin ) {
            tvtime_osd_set_input( osd, videoinput_get_input_name( vidin ) );
            tvtime_osd_set_norm
                ( osd,
                  videoinput_get_norm_name( videoinput_get_norm( vidin ) ) );
        } else {
            tvtime_osd_set_input( osd, _("No video source") );
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
        outputfilter_set_pixel_aspect( tvtime->outputfilter, pixel_aspect );
    }

    commands = commands_new( ct, vidin, stationmgr, osd, fieldtime );
    if( !commands ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }
    build_deinterlacer_menu( commands_get_menu( commands, "deinterlacer" ),
                             curmethodid );
    build_deinterlacer_description_menu( commands_get_menu( commands,
                                         "deintdescription" ), curmethodid );

    /* Setup the video correction tables. */
    videofilter_set_colour_invert( tvtime->inputfilter,
                                   commands_apply_colour_invert( commands ) );
    videofilter_set_mirror( tvtime->inputfilter,
                            commands_apply_mirror( commands ) );
    videofilter_set_chroma_kill( tvtime->inputfilter,
                                 commands_apply_chroma_kill( commands ) );
    if( vidin && videoinput_is_uyvy( vidin ) ) {
        videofilter_enable_uyvy_conversion( tvtime->inputfilter );
    }

    fifofile = get_tvtime_fifo_filename( config_get_uid( ct ) );
    if( !fifofile ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
    } else {
        fifo = fifo_new( fifofile );
        if( !fifo ) {
            lfputs( _("Cannot create FIFO, "
                      "remote control of tvtime disabled.\n"), stderr );
        }
    }

    /* We no longer need the fifo filename. */
    if( fifofile ) free( fifofile );

    in = input_new( ct, commands, verbose );
    if( !in ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }

    /* Setup VBI stuff for NTSC-like norms. */
    if( vidin && height == 480 ) {
        vs = vbiscreen_new( width, height, pixel_aspect, verbose );
        if( !vs ) {
            lfputs( _("Closed caption display failed to "
                      "initialize, disabled.\n"), stderr );
        }

        vbidata = vbidata_new( config_get_vbi_device( ct ), vs, verbose );
        if( !vbidata ) {
            lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        } else {
            vbidata_capture_mode( vbidata, config_get_cc( ct )
                                  ? CAPTURE_CC1 : CAPTURE_OFF );
            vbidata_capture_xds( vbidata, config_get_usexds( ct ) );
        }
        commands_set_vbidata( commands, vbidata );
    }
    if( tvtime->outputfilter ) {
        outputfilter_set_vbiscreen( tvtime->outputfilter, vs );
    }

    /* Randomly assign a tagline as the window caption. */
    srand( time( 0 ) );
    tagline = taglines[ rand() % numtaglines ];

    /* Set the fullscreen position. */
    output->set_fullscreen_position( config_get_fullscreen_position( ct ) );

    /* If we start fullscreen, go into fullscreen mode now. */
    if( config_get_fullscreen( ct ) ) {
        commands_handle( commands, TVTIME_TOGGLE_FULLSCREEN, 0 );
    }

    /* If we start always-on-top, enter this state now. */
    if( config_get_alwaysontop( ct ) ) {
        commands_handle( commands, TVTIME_TOGGLE_ALWAYSONTOP, 0 );
    }

    /* If we start half-framerate, toggle that now. */
    for( i = 0; i < config_get_framerate_mode( ct ); i++ ) {
        commands_handle( commands, TVTIME_TOGGLE_FRAMERATE, 0 );
    }

    /* If we want quiet screenshots, make sure we do that. */
    quiet_screenshots = config_get_quiet_screenshots( ct );

    /* If we are a new install, show the menu. */
    if( station_is_new_install( stationmgr ) ) {
        commands_handle( commands, TVTIME_SHOW_MENU, 0 );
    }

    /* Begin capturing frames. */
    if( vidin ) {
        if( fieldsavailable == 2 ) {
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        } else if( fieldsavailable == 4 ) {
            secondlastframe = videoinput_next_frame( vidin,
                                                     &secondlastframeid );
            lastframe = videoinput_next_frame( vidin, &lastframeid );
        }
    }

    build_output_menu( commands_get_menu( commands, "output" ), sixteennine,
                       output->is_fullscreen(), output->is_alwaysontop(),
                       output->is_fullscreen_supported(),
                       output->is_alwaysontop_supported(),
                       output->is_overscan_supported(),
                       quiet_screenshots );
    build_matte_menu( commands_get_menu( commands, "matte" ),
                      matte_mode, sixteennine );
    build_fspos_menu( commands_get_menu( commands, "fspos" ),
                      config_get_fullscreen_position( ct ) );

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
        int current_id;

        if( vidin && videoinput_has_tuner( vidin ) ) {
            current_id = station_get_current_id( stationmgr );
        } else {
            current_id = 0;
        }

        if( matte_mode ) {
            output_x = matte_x;
            output_w = matte_w;
            output_y = matte_y;
            output_h = matte_h;
        } else {
            output_x = (int) ((((double) width) *
                               commands_get_overscan( commands )) + 0.5);
            output_w = (int) ((((double) width) -
                               (((double) width) *
                                commands_get_overscan( commands ) * 2.0)) +
                              0.5);
            output_y = (int) ((((double) height) *
                               commands_get_overscan( commands )) + 0.5);
            output_h = (int) ((((double) height) -
                               (((double) height) *
                                commands_get_overscan( commands ) * 2.0)) +
                              0.5);
        }

        if( fifo ) {
            int cmd;
            cmd = fifo_get_next_command( fifo );
            fifo_args = fifo_get_arguments( fifo );
            if( cmd != TVTIME_NOCOMMAND ) {
                output->update_server_time( 0 );
                commands_handle( commands, cmd, fifo_args );
            }
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

        if( curmethod ) {
            commands_set_half_size( commands, curmethod->doscalerbob );
        }
        output->poll_events( in );

        if( commands_quit( commands ) ) break;
        if( commands_restart_tvtime( commands ) ) {
            restarttvtime = 1;
            break;
        }
        /* Check if it's time to sleep and dream well */
        if( commands_sleeptimer( commands ) &&
            commands_sleeptimer_do_shutdown( commands ) ) {
            /**
             * Return the special constant 3 if we've exited
             * from a sleep timer.
             */
            return_value = 3;
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
        if( framerate_mode < 0
            || framerate_mode != commands_get_framerate( commands ) ) {
            int showlist = !(framerate_mode < 0 ) &&
                !commands_menu_active( commands );
            framerate_mode = commands_get_framerate( commands );

            if( osd ) {
                build_framerate_menu
                    ( commands_get_menu( commands, "framerate" ),
                      1000000.0 / ((double) fieldtime), framerate_mode );
                commands_refresh_menu( commands );
                if( showlist ) {
                    osd_list_framerates( osd, 1000000.0 / ((double) fieldtime),
                                         framerate_mode );
                }
            }
        }
        if( commands_toggle_fullscreen( commands ) ) {
            if( output->toggle_fullscreen( 0, 0 ) ) {
                config_save( ct, "FullScreen", "1" );
            } else {
                config_save( ct, "FullScreen", "0" );
            }
            build_output_menu( commands_get_menu( commands, "output" ),
                               sixteennine,
                               output->is_fullscreen(),
                               output->is_alwaysontop(),
                               output->is_fullscreen_supported(),
                               output->is_alwaysontop_supported(),
                               output->is_overscan_supported(),
                               quiet_screenshots );
            commands_refresh_menu( commands );
        }
        if( commands_toggle_alwaysontop( commands ) ) {
            if( output->toggle_alwaysontop() ) {
                if( osd ) {
                    tvtime_osd_show_message( osd,
                          _("Always-on-top enabled.") );
                }
            } else {
                if( osd ) {
                    tvtime_osd_show_message( osd,
                          _("Always-on-top disabled.") );
                }
            }
            build_output_menu( commands_get_menu( commands, "output" ),
                               sixteennine,
                               output->is_fullscreen(),
                               output->is_alwaysontop(),
                               output->is_fullscreen_supported(),
                               output->is_alwaysontop_supported(),
                               output->is_overscan_supported(),
                               quiet_screenshots );
            commands_refresh_menu( commands );
        }
        if( commands_toggle_aspect( commands ) ) {
            matte_mode = 0;
            output->set_matte( 0, 0 );
            if( output->toggle_aspect() ) {
                sixteennine = 1;
                if( osd ) {
                    tvtime_osd_show_message( osd,
                          _("16:9 display mode active.") );
                }
                config_save( ct, "WideScreen", "1" );
                pixel_aspect = ( (double) width ) /
                    ( ( (double) height ) * (16.0 / 9.0) );
            } else {
                sixteennine = 0;
                if( osd ) {
                    tvtime_osd_show_message( osd,
                          _("4:3 display mode active.") );
                }
                config_save( ct, "WideScreen", "0" );
                pixel_aspect =
                    ( (double) width ) / ( ( (double) height ) * (4.0 / 3.0) );
            }
            if( osd ) {
                tvtime_osd_show_list( osd, 0, 0 );
                tvtime_osd_set_pixel_aspect( osd, pixel_aspect );
            }
            build_output_menu( commands_get_menu( commands, "output" ),
                               sixteennine,
                               output->is_fullscreen(),
                               output->is_alwaysontop(),
                               output->is_fullscreen_supported(),
                               output->is_alwaysontop_supported(),
                               output->is_overscan_supported(),
                               quiet_screenshots );
            build_matte_menu( commands_get_menu( commands, "matte" ),
                              matte_mode, sixteennine );
            commands_refresh_menu( commands );
        }
        if( commands_get_fs_pos( commands ) ) {
            const char *fspos = commands_get_fs_pos( commands );
            int newpos;
            if( tolower( fspos[ 0 ] ) == 't' ) {
                newpos = 1;
            } else if( tolower( fspos[ 0 ] ) == 'b' ) {
                newpos = 2;
            } else {
                newpos = 0;
            }
            output->set_fullscreen_position( newpos );
            config_save( ct, "FullscreenPosition",
                         commands_get_fs_pos( commands ) );
            build_fspos_menu( commands_get_menu( commands, "fspos" ), newpos );
            commands_refresh_menu( commands );
        }
        if( commands_toggle_quiet_screenshots( commands ) ) {
            quiet_screenshots = !quiet_screenshots;
            build_output_menu( commands_get_menu( commands, "output" ),
                               sixteennine,
                               output->is_fullscreen(),
                               output->is_alwaysontop(),
                               output->is_fullscreen_supported(),
                               output->is_alwaysontop_supported(),
                               output->is_overscan_supported(),
                               quiet_screenshots );
            if( osd ) {
                if( quiet_screenshots ) {
                    tvtime_osd_show_message( osd, _("Screenshot messages disabled.") );
                } else {
                    tvtime_osd_show_message( osd, _("Screenshot messages enabled.") );
                }
            }
            commands_refresh_menu( commands );
        }
        if( commands_toggle_matte( commands ) ||
            commands_get_matte_mode( commands ) ) {
            double matte = 4.0 / 3.0;
            int sqwidth = sixteennine ?
                ((height * 16) / 9) : ((height * 4) / 3);
            int sqheight = sixteennine ?
                ((width * 9) / 16) : ((width * 3) / 4);

            matte_x = 0;
            matte_w = width;
            if( commands_toggle_matte( commands ) ) {
                matte_mode = (matte_mode + 1) % 5;
            } else {
                if( !strcmp( commands_get_matte_mode( commands ), "16:9" ) ) {
                    matte_mode = sixteennine ? 0 : 1;
                } else if( !strcmp( commands_get_matte_mode( commands ),
                                    "16:10" ) ) {
                    matte_mode = sixteennine ? 4 : 2;
                } else if( !strcmp( commands_get_matte_mode( commands ),
                                    "1.85:1" ) ) {
                    matte_mode = sixteennine ? 1 : 3;
                } else if( !strcmp( commands_get_matte_mode( commands ),
                                    "2.35:1" ) ) {
                    matte_mode = sixteennine ? 2 : 4;
                } else {
                    matte_mode = sixteennine ? 3 : 0;
                }
            }

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
                    /* We're cropping the sides off so we add overscan to avoid mess
                     * at the top of the screen. */
                    matte_y = commands_get_overscan( commands ) * height / 2;
                    matte_h = height - matte_y;
                    output->set_matte( (matte_h * 4) / 3, matte_h );
                } else if( matte_mode == 4 ) {
                    matte = 1.6;
                    matte_w = (int) (((double) sqheight * matte) + 0.5);
                    matte_x = (width - matte_w) / 2;
                    /* We're cropping the sides off so we add overscan to avoid mess
                     * at the top of the screen. */
                    matte_y = commands_get_overscan( commands ) * height / 2;
                    matte_h = height - matte_y;
                    output->set_matte( (matte_h * 16) / 10, matte_h );
                }
            } else {
                if( matte_mode == 1 ) {
                    matte = 16.0 / 9.0;
                } else if( matte_mode == 2 ) {
                    matte = 1.6;
                } else if( matte_mode == 3 ) {
                    matte = 1.85;
                } else if( matte_mode == 4 ) {
                    matte = 2.35;
                }
            }
            if( !matte_x ) {
                matte_h = (int) ((((double) sqwidth)/matte) + 0.5);
                matte_y = (height - matte_h) / 2;
                output->set_matte( sqwidth, matte_h );
            }
            if( osd && !commands_menu_active( commands ) ) {
                osd_list_matte( osd, matte_mode, sixteennine );
            }
            build_matte_menu( commands_get_menu( commands, "matte" ),
                              matte_mode, sixteennine );
            commands_refresh_menu( commands );
        }
        if( commands_toggle_pulldown_detection( commands ) ) {
            if( height == 480 ) {
                tvtime->pulldown_alg =
                    (tvtime->pulldown_alg + 1) % PULLDOWN_MAX;
                commands_set_pulldown_alg( commands, tvtime->pulldown_alg );
                if( osd ) {
                    if( tvtime->pulldown_alg == PULLDOWN_NONE ) {
                        tvtime_osd_show_message( osd,
                              _("2-3 pulldown inversion disabled.") );
                    } else if( tvtime->pulldown_alg == PULLDOWN_VEKTOR ) {
                        tvtime_osd_show_message( osd,
                              _("2-3 pulldown inversion enabled.") );
                    }
                    commands_refresh_menu( commands );
                }
            } else if( osd ) {
                tvtime_osd_show_message( osd,
                    _("2-3 pulldown inversion is not"
                      " valid with your TV norm.") );
            }
        }
        if( commands_show_deinterlacer_info( commands ) ) {
            osd_list_deinterlacer_info( osd, curmethodid );
        }
        if( !output->is_interlaced() &&
            commands_toggle_deinterlacer( commands ) ) {
            curmethodid = (curmethodid + 1) % get_num_deinterlace_methods();
            curmethod = get_deinterlace_method( curmethodid );
            tvtime_set_deinterlacer( tvtime, curmethod );
            if( osd ) {
                build_deinterlacer_menu ( commands_get_menu( commands,
                                                             "deinterlacer" ),
                                          curmethodid );
                build_deinterlacer_description_menu
                    ( commands_get_menu ( commands, "deintdescription" ),
                      curmethodid );
                commands_refresh_menu( commands );
                if( !commands_menu_active( commands ) )
                    osd_list_deinterlacers( osd, curmethodid );
                tvtime_osd_set_deinterlace_method( osd, curmethod->name );
                tvtime_osd_show_info( osd );
            }
            config_save( ct, "DeinterlaceMethod", curmethod->short_name );
        }
        if( !output->is_interlaced() && commands_set_deinterlacer( commands ) ) {
            curmethodid = 0;
            curmethod = get_deinterlace_method( 0 );
            while( strcasecmp( commands_get_new_deinterlacer( commands ),
                               curmethod->short_name ) ) {
                curmethodid =
                    (curmethodid + 1) % get_num_deinterlace_methods();
                curmethod = get_deinterlace_method( curmethodid );
                if( !curmethodid ) break;
            }
            if( osd ) {
                build_deinterlacer_menu
                    ( commands_get_menu( commands, "deinterlacer" ),
                      curmethodid );
                build_deinterlacer_description_menu
                    ( commands_get_menu( commands, "deintdescription" ),
                      curmethodid );
                commands_refresh_menu( commands );
            }
            tvtime_set_deinterlacer( tvtime, curmethod );
            config_save( ct, "DeinterlaceMethod", curmethod->short_name );
        }
        if( commands_set_freq_table( commands ) ) {
            station_writeconfig( stationmgr );
            station_delete( stationmgr );
            stationmgr = station_new( videoinput_get_norm_name( norm ),
                                      commands_get_new_freq_table( commands ),
                                      config_get_ntsc_cable_mode( ct ),
                                      verbose );
            commands_set_station_mgr( commands, stationmgr );
            config_save( ct, "Frequencies",
                         commands_get_new_freq_table( commands ) );
        }
        if( commands_resize_window( commands ) ) {
            output->set_window_height( output->get_visible_height() );
        }
        if( current_id != last_current_id || commands_xmltv_updated( commands ) ) {
            const char *curline = commands_get_xmltv_title( commands );
            char caption[ 256 ];
            if( !curline && config_get_show_taglines( ct ) ) {
                curline = tagline;
            }
            if( curline ) {
                if( current_id ) {
                    snprintf( caption, sizeof( caption ), "tvtime [%d]: %s",
                              current_id, curline );
                } else {
                    snprintf( caption, sizeof( caption ), "tvtime: %s",
                              curline );
                }
            } else {
                if( current_id ) {
                    snprintf( caption, sizeof( caption ), "tvtime [%d]",
                              current_id );
                } else {
                    snprintf( caption, sizeof( caption ), "tvtime" );
                }
            }
            output->set_window_caption( caption );
            last_current_id = current_id;
        }
        if( commands_tuner_reset( commands ) && vidin ) {
            output->update_xawtv_station( videoinput_get_tuner_freq( vidin ),
                                          station_get_current_id( stationmgr ),
                                          station_get_current_channel_name( stationmgr ) );
        }

        commands_next_frame( commands );

        /* Notice this because it's cheap. */
        videofilter_set_colour_invert( tvtime->inputfilter,
                                commands_apply_colour_invert( commands ) );
        videofilter_set_mirror( tvtime->inputfilter,
                                commands_apply_mirror( commands ) );
        videofilter_set_chroma_kill( tvtime->inputfilter,
                                     commands_apply_chroma_kill( commands ) );

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
        if( vidin ) {
            tuner_state = videoinput_check_for_signal
                ( vidin, commands_check_freq_present( commands ) );
        } else {
            tuner_state = TUNER_STATE_NO_SIGNAL;
        }

        if( has_signal && tuner_state != TUNER_STATE_HAS_SIGNAL ) {
            has_signal = 0;
            if( lastframe ) {
                save_last_frame( saveframe, lastframe, width, height,
                                 width * 2, width * 2 );
            }
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

        if( !paused && (tuner_state == TUNER_STATE_HAS_SIGNAL ||
                        tuner_state == TUNER_STATE_SIGNAL_DETECTED) ) {
            acquired = 1;
            if( vidin ) {
                curframe = videoinput_next_frame( vidin, &curframeid );
                if( !curframe ) {
                    has_signal = 0;
                    if( lastframe ) {
                        save_last_frame( saveframe, lastframe, 
                                         width, height, width*2, width*2 );
                    }
                    fadepos = 0;
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
                        if( videoinput_buffer_invalid( vidin,
                                                       secondlastframeid ) ) {
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
                       < ( (fieldtime*2) -
                           (rtctimer_get_usecs( rtctimer ) / 2) ) ) {
                    rtctimer_next_tick( rtctimer );
                }
            } else {
                int timeleft = performance_get_usecs_since_frame_acquired( perf );
                if( timeleft < fieldtime ) {
                    usleep( fieldtime - timeleft );
                }
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
                    numscanned +=
                        (after_pos + station_get_num_stations( stationmgr ) -
                         before_pos) % station_get_num_stations( stationmgr );
                }

                /* Stop when we loop around. */
                if( numscanned > station_get_num_stations( stationmgr ) ||
                    !station_get_num_stations( stationmgr ) ) {
                    commands_handle( commands, TVTIME_CHANNEL_SCAN, 0 );
                }
            }
        }

        performance_checkpoint_acquired_input_frame( perf );


        /* Print statistics and check for missed frames. */
        if( printdebug ) {
            int framesize = width * height * 2;
            fprintf( stderr, "tvtime: Stats using '%s' at %dx%d%s%s.\n",
                      (curmethod) ? curmethod->name : "interlaced passthrough", 
                      width, height, realtime ? " [RT]" : "", rtctimer ? " [RTC]" : " [no RTC]" );
            if( curmethod && curmethod->doscalerbob ) {
                framesize = width * height;
            }
            performance_print_last_frame_stats( perf, framesize );
            if( osd ) {
                if( vidin ) {
                    osd_list_statistics( osd, perf,
                          (curmethod) ? curmethod->name : "interlaced passthrough",
                          width, height, framesize, fieldtime, framerate_mode,
                          videoinput_get_norm( vidin ) );
                } else {
                    osd_list_statistics( osd, perf,
                          (curmethod) ? curmethod->name : "interlaced passthrough",
                          width, height, framesize, fieldtime, framerate_mode,
                          (height == 576) );
                }
            }
        }
        if( config_get_debug( ct ) &&
            commands_check_freq_present( commands ) ) {
            if( curmethod )  {
                performance_print_frame_drops( perf, curmethod->doscalerbob ?
                                               (width * height) :
                                               (width * height * 2) );
            } else {
                performance_print_frame_drops( perf, width * height * 2 );
            }
        }

        /**
         * Show the bottom field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL ||
                        framerate_mode == FRAMERATE_HALF_TFF) &&
            !output->is_interlaced() ) {
            if( curmethod->doscalerbob && !showbars ) {
                output_success = output->show_frame( output_x, output_y/2,
                                                     output_w, output_h/2 );
            } else {
                output_success = output->show_frame( output_x, output_y,
                                                     output_w, output_h );
            }
        }
        performance_checkpoint_show_bot_field( perf );
        if( !output_success ) break;


        /**
         * Deinterlace the top field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL ||
                        framerate_mode == FRAMERATE_HALF_BFF) ) {
            if( output->is_interlaced() ) {
                if( send_fields ) {
                    /* Wait until we can draw the even field. */
                    output->wait_for_sync( 0 );

                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( tvtime,
                                                   output->get_output_buffer(),
                                                   curframe, 0, width, height,
                                                   width * 2, 
                                                   output->get_output_stride(),
                                                   0);
                    output->unlock_output_buffer();
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                } else {
                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame( tvtime,
                                                   output->get_output_buffer(),
                                                   curframe, 0, width, height,
                                                   width * 2,
                                                   output->get_output_stride(),
                                                   1 );
                    output->unlock_output_buffer();
                    output->show_frame( output_x, output_y/2, 
                                        output_w, output_h/2 );
                }
            } else {
                /* Build the output from the top field. */
                output->lock_output_buffer();
                if( showbars ) {
                    for( i = 0; i < height; i++ ) {
                        blit_packed422_scanline( output->get_output_buffer() +
                              i*output->get_output_stride(),
                              colourbars + (i*width*2), width );
                    }
                } else {
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( tvtime,
                              output->get_output_buffer(),
                              curframe, 0, width, height, width * 2,
                              output->get_output_stride () );
                    } else {
                        tvtime_build_deinterlaced_frame( tvtime,
                              output->get_output_buffer(),
                              curframe, lastframe, secondlastframe,
                              0, 0, width, height, width * 2,
                              output->get_output_stride() );
                    }
                }

                /*
                 * For the screenshot, use the output
                 * after we build the top field.
                 */
                if( screenshot ) {
                    const char *basename;
                    const char *outfile;
                    char *filename;
                    char *pathfilename;
                    char *message;
                    char timestamp[ 256 ];
                    time_t tm = time( 0 );

                    if( *commands_screenshot_filename( commands ) ) {
                        basename = outfile = commands_screenshot_filename( commands );
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
                    asprintf( &message, _("Screenshot: %s"), basename );
                    if( osd && !quiet_screenshots ) {
                        tvtime_osd_show_message( osd, message );
                    }
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
                if( secondlastframeid >= 0 )
                    videoinput_free_frame( vidin, secondlastframeid );
                if( vbidata )
                    vbidata_process_frame( vbidata, printdebug );
            } else if( fieldsavailable == 2 ) {
               if( lastframeid >= 0 )
                   videoinput_free_frame( vidin, lastframeid );
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
        } else {
            int timeleft = performance_get_usecs_since_frame_acquired( perf );

            if( timeleft < fieldtime ) {
                usleep( fieldtime - timeleft );
            }
        }
        performance_checkpoint_wait_for_bot_field( perf );


        /**
         * Show the top field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL ||
                        framerate_mode == FRAMERATE_HALF_BFF) &&
            !output->is_interlaced() ) {
            if( curmethod->doscalerbob && !showbars ) {
                output_success = output->show_frame( output_x, output_y/2,
                                                     output_w, output_h/2 );
            } else {
                output_success = output->show_frame( output_x, output_y,
                                                     output_w, output_h );
            }
        }
        performance_checkpoint_show_top_field( perf );
        if( !output_success ) break;


        /**
         * Deinterlace the bottom field.
         */
        if( exposed && (framerate_mode == FRAMERATE_FULL ||
                        framerate_mode == FRAMERATE_HALF_TFF) ) {
            if( output->is_interlaced() ) {
                if( send_fields ) {
                    /* Wait until we can draw the odd field. */
                    output->wait_for_sync( 1 );
                    
                    output->lock_output_buffer();
                    tvtime_build_interlaced_frame
                        ( tvtime, output->get_output_buffer(), curframe, 1,
                          width, height, width * 2, 
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
                        blit_packed422_scanline
                            ( output->get_output_buffer() +
                              i*output->get_output_stride(),
                              colourbars + (i*width*2), width );
                    }
                } else {
                    if( curmethod->doscalerbob ) {
                        tvtime_build_copied_field( tvtime,
                              output->get_output_buffer(),
                              curframe, 1, width, height, width * 2,
                              output->get_output_stride() );
                    } else {
                        tvtime_build_deinterlaced_frame( tvtime,
                              output->get_output_buffer(),
                              curframe, lastframe, secondlastframe, 1, 1,
                              width, height, width * 2,
                              output->get_output_stride() );
                    }
                }

                /*
                 * For the screenshot, use may need to use
                 * the output after we build the bot field.
                 */
                if( screenshot ) {
                    const char *basename;
                    const char *outfile;
                    char filename[ 256 ];
                    char message[ 512 ];
                    char pathfilename[ 256 ];
                    char timestamp[ 50 ];
                    time_t tm = time( 0 );

                    if( *commands_screenshot_filename( commands ) ) {
                        basename = outfile = commands_screenshot_filename( commands );
                    } else {
                        strftime( timestamp, sizeof( timestamp ),
                                  config_get_timeformat( ct ),
                                  localtime( &tm ) );
                        snprintf( filename, sizeof( filename ),
                                  "tvtime-output-%s.png", timestamp );
                        snprintf( pathfilename, sizeof( pathfilename ),
                                  "%s/%s",
                                  config_get_screenshot_dir( ct ), filename );
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
                    snprintf( message, sizeof( message ),
                              _("Screenshot: %s"), basename );
                    if( osd && !quiet_screenshots ) {
                        tvtime_osd_show_message( osd, message );
                    }
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
                if( curframeid >= 0 )
                    videoinput_free_frame( vidin, curframeid );
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

    if( commands_get_new_input_width( commands ) ) {
        snprintf( number, 4, "%d", commands_get_new_input_width( commands ) );
        config_save( ct, "InputWidth", number );
    }

    snprintf( number, 4, "%d", commands_get_global_brightness( commands ) );
    config_save( ct, "DefaultBrightness", number );
    snprintf( number, 4, "%d", commands_get_global_contrast( commands ) );
    config_save( ct, "DefaultContrast", number );
    snprintf( number, 4, "%d", commands_get_global_saturation( commands ) );
    config_save( ct, "DefaultSaturation", number );
    snprintf( number, 4, "%d", commands_get_global_hue( commands ) );
    config_save( ct, "DefaultHue", number );

    if( commands_get_new_norm( commands ) ) {
        config_save( ct, "Norm", commands_get_new_norm( commands ) );
    }

    snprintf( number, 4, "%d", station_get_prev_id( stationmgr ) );
    config_save( ct, "PrevChannel", number );

    snprintf( number, 4, "%d", station_get_current_id( stationmgr ) );
    config_save( ct, "Channel", number );

    snprintf( number, 4, "%d", framerate_mode );
    config_save( ct, "FramerateMode", number );

    /* Make sure floating point numbers are always written to the config
     * file in "C" locale format.
     */
    strncpy( prevloc, setlocale( LC_NUMERIC, NULL ), sizeof( prevloc ) );
    setlocale( LC_NUMERIC, "C" );
    snprintf( number, 4, "%2.1f",
              commands_get_overscan( commands ) * 2.0 * 100.0 );
    config_save( ct, "OverScan", number );
    setlocale( LC_NUMERIC, prevloc );

    snprintf( number, 4, "%d", commands_check_freq_present( commands ) );
    config_save( ct, "CheckForSignal", number );

    snprintf( number, 4, "%d", commands_get_audio_boost( commands ) );
    config_save( ct, "AudioBoost", number );

    snprintf( number, 4, "%d", output->is_alwaysontop() );
    config_save( ct, "AlwaysOnTop", number );

    snprintf( number, 4, "%d", quiet_screenshots );
    config_save( ct, "QuietScreenshots", number );

    snprintf( number, 6, "%d", mixer_get_unmute_volume() );
    config_save( ct, "UnmuteVolume", number );

    snprintf( number, 4, "%d", mixer_ismute() );
    config_save( ct, "Muted", number );

    if( config_get_mute_on_exit( ct ) ) {
        mixer_mute( 1 );
    }

    if( vidin ) {
        snprintf( number, 4, "%d", videoinput_get_input_num( vidin ) );
        config_save( ct, "V4LInput", number );

        if( videoinput_get_audio_mode( vidin ) == VIDEOINPUT_MONO ) {
            config_save( ct, "AudioMode", "mono" );
        } else if( videoinput_get_audio_mode( vidin ) == VIDEOINPUT_LANG1 ) {
            config_save( ct, "AudioMode", "lang1" );
        } else if( videoinput_get_audio_mode( vidin ) == VIDEOINPUT_LANG2 ) {
            config_save( ct, "AudioMode", "lang2" );
        } else {
            config_save( ct, "AudioMode", "stereo" );
        }

        snprintf( number, 4, "%d", videoinput_get_pal_audio_mode( vidin ) );
        config_save( ct, "PalDKMode", number );
    }

    output->shutdown();

    if( verbose ) {
        fprintf( stderr, "tvtime: Cleaning up.\n" );
    }
    if( vidin ) {
        videoinput_delete( vidin );
    }
    config_delete( ct );
    input_delete( in );
    commands_delete( commands );
    performance_delete( perf );
    station_writeconfig( stationmgr );
    station_delete( stationmgr );
    if( fifo ) {
        fifo_delete( fifo );
    }
    if( vbidata ) {
        vbidata_delete( vbidata );
    }
    if( vs ) {
        vbiscreen_delete( vs );
    }
    if( osd ) {
        tvtime_osd_delete( osd );
    }
    mixer_close_device();

    /* Free temporary memory. */
    free( colourbars );
    free( saveframe );
    free( fadeframe );
    free( blueframe );

    /* Free state. */
    tvtime_delete( tvtime );

    xmlCleanupParser();

    if( restarttvtime ) {
        lfprintf( stderr, _("Restarting tvtime.\n") );
        return 2;
    }

    lfprintf( stderr, _("Thank you for using tvtime.\n") );
    return return_value;
}

int main( int argc, char **argv )
{
    rtctimer_t *rtctimer = 0;
    int read_stdin = 1;
    int result = 0;
    int realtime = 0;

    /*
     * Setup i18n. This has to be done as early as possible in order
     * to show startup messages in the users preferred language.
     */
    setup_i18n();

    /*
     * Make gettext return UTF-8 strings, and setup lfprintf.
     */
    setup_utf8();

    lfprintf( stderr, _("Running %s.\n"), PACKAGE_STRING );

    /* Disable this code for a release. */
    /*
    fprintf( stderr, "\n"
             "*** WARNING: This is a DEVELOPMENT version of tvtime.\n"
             "*** Please do not redistribute snapshots, and please submit\n"
             "*** bug reports for released versions only!\n\n" );
    */

    /* Steal system resources in the name of performance. */
    /* Get maximum priority before dropping root privileges. We'll drop back */
    /* to the value specified in the config file (or the default) later. */
    setpriority( PRIO_PROCESS, 0, -19 );
    if( set_realtime_priority( 0 ) ) {
        realtime = 1;
    }

    rtctimer = rtctimer_new( 0 );
    if( rtctimer ) {
        if( !rtctimer_set_interval( rtctimer, 1024 ) &&
            !rtctimer_set_interval( rtctimer, 64 ) ) {
            rtctimer_delete( rtctimer );
            rtctimer = 0;
        } else {
            rtctimer_start_clock( rtctimer );
            if( rtctimer_get_resolution( rtctimer ) < 1024 ) {
                rtctimer_delete( rtctimer );
                rtctimer = 0;
            }
        }
    }


    /* We've now stolen all our root-requiring resources, drop to a user. */
    if( setuid( getuid() ) == -1 ) {
        /*
         * This used to say "Unknown problems", but we're printing an
         * error string, so that didn't really make sense, did it?
         */
        lfprintf( stderr, _("\n"
    "    Failed to drop root privileges: %s.\n"
    "    tvtime will now exit to avoid security problems.\n\n"),
            strerror( errno ) );
        return 1;
    }


    /* Ditch stdin early. */
    if( isatty( STDIN_FILENO ) ) {
        read_stdin = 0;
        close( STDIN_FILENO );
    }

    /* Run tvtime. */
    for(;;) {
        if( result == 2 ) {
            char *new_argv[ 2 ];

            new_argv[ 0 ] = "tvtime";
            new_argv[ 1 ] = 0;

            result = tvtime_main( rtctimer, read_stdin, realtime, 0, new_argv );
        } else {
            result = tvtime_main( rtctimer, read_stdin, realtime, argc, argv );
        }

        if( result != 2 ) break;
    }

    if( rtctimer ) {
        rtctimer_delete( rtctimer );
    }

    return result;
}

