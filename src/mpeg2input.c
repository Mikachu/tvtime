/*
 * video_out.h
 * Copyright (C) 2000-2002 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include "mpeg2input.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_LIBMPEG2
#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/convert.h>

typedef struct vo_instance_s vo_instance_t;

struct vo_instance_s
{
    int (* setup) (vo_instance_t * instance, int width, int height);
    void (* setup_fbuf) (vo_instance_t * instance, uint8_t ** buf, void ** id);
    void (* set_fbuf) (vo_instance_t * instance, uint8_t ** buf, void ** id);
    void (* start_fbuf) (vo_instance_t * instance,
			 uint8_t * const * buf, void * id);
    void (* draw) (vo_instance_t * instance, uint8_t * const * buf, void * id);
    void (* discard) (vo_instance_t * instance,
		      uint8_t * const * buf, void * id);
    void (* close) (vo_instance_t * instance);
};

typedef vo_instance_t * vo_open_t( void );

#define BUFFER_SIZE 4096
static uint8_t buffer[BUFFER_SIZE];
static FILE *in_file;
static int demux_track = 0;
static mpeg2dec_t *mpeg2dec;
static vo_open_t *output_open = 0;
static vo_instance_t *output;

typedef struct {
    void *data;
} tvtime_frame_t;

struct mpeg2input_s {
    vo_instance_t vo;
    tvtime_frame_t frame[ 3 ];
    int index;
    int width;
    int height;
    uint8_t *curframe;
    int proceed;
    uint8_t *frames422;
    int curout;
};

static void tvtime_setup_fbuf( vo_instance_t *_instance,
                               uint8_t **buf, void **id )
{
    mpeg2input_t * instance = (mpeg2input_t *) _instance;

    buf[0] = (uint8_t *) instance->frame[instance->index].data;
    buf[1] = buf[0] + instance->width*instance->height;
    buf[2] = buf[1] + ((instance->width/2)*(instance->height/2));
    *id = instance->frame + instance->index++;
}

static void planar420_to_packed422_frame( uint8_t *output,
                                          uint8_t *input,
                                          int width, int height )
{
    uint8_t *y = input;
    uint8_t *cb = input + width*height;
    uint8_t *cr = cb + ((width/2)*(height/2));
    int i;

    for( i = 0; i < height; i++ ) {
        uint8_t *cury = y + (width*i);
        uint8_t *curcb = cb + ((width/2)*(i/2));
        uint8_t *curcr = cr + ((width/2)*(i/2));
        uint8_t *curout = output + (width*2*i);
        int x;

        for( x = 0; x < (width/2); x++ ) {
            *curout++ = *cury++;
            *curout++ = *curcb++;
            *curout++ = *cury++;
            *curout++ = *curcr++;
        }
    }
}

static void tvtime_draw_frame( vo_instance_t * _instance,
                               uint8_t * const * buf, void * id)
{
    tvtime_frame_t *frame = (tvtime_frame_t *) id;
    mpeg2input_t *mpegin = (mpeg2input_t *) _instance;

    /* draw frame->data */
    mpegin->curframe = frame->data;
    mpegin->proceed = 0;
    mpegin->curout = (mpegin->curout + 1) % 3;
    planar420_to_packed422_frame( mpegin->frames422 + (mpegin->curout * mpegin->width * mpegin->height * 2),
                                  mpegin->curframe, mpegin->width, mpegin->height );
    // fprintf( stderr, "draw\n" );
}

static int tvtime_alloc_frames( mpeg2input_t * instance )
{
    int size;
    char * alloc;
    int i;

    size = 0;
    alloc = 0;
    for( i = 0; i < 3; i++ ) {
        if( i == 0 ) {
            size = (instance->width * instance->height * 3);
            alloc = malloc( 3 * size );
            fprintf( stderr, "mpeg2input: Input is %dx%d.\n",
                     instance->width, instance->height );
            instance->frames422 = malloc( instance->width *
                                          instance->height * 2 * 3 );
            if( !alloc || !instance->frames422 ) return 1;
            instance->curout = 0;
	}
        instance->frame[i].data = alloc;
        alloc += size;
    }
    instance->index = 0;
    return 0;
}

static void tvtime_close( vo_instance_t * _instance )
{
    mpeg2input_t *instance = (mpeg2input_t *) _instance;

    if( instance->frame[0].data ) free( instance->frame[0].data );
    free( instance );
}

static int common_setup( mpeg2input_t * instance, int width, int height )
{
    instance->vo.set_fbuf = 0;
    instance->vo.discard = 0;
    instance->vo.start_fbuf = 0;
    instance->width = width;
    instance->height = height;
    instance->curframe = 0;

    if( tvtime_alloc_frames( instance ) ) return 1;
    instance->vo.setup_fbuf = tvtime_setup_fbuf;
    instance->vo.draw = tvtime_draw_frame;
    instance->vo.close = tvtime_close;

    return 0;
}

static int tvtime_setup( vo_instance_t * instance, int width, int height )
{
    return common_setup( (mpeg2input_t *) instance, width, height );
}

static vo_instance_t *vo_tvtime_open( void )
{
    mpeg2input_t *instance;

    instance = (mpeg2input_t *) malloc( sizeof( mpeg2input_t ) );
    if( !instance ) return 0;

    instance->vo.setup = tvtime_setup;
    instance->proceed = 1;

    return (vo_instance_t *) instance;
}

static void decode_mpeg2 (uint8_t * current, uint8_t * end)
{
    const mpeg2_info_t * info;
    int state;

    mpeg2_buffer (mpeg2dec, current, end);

    info = mpeg2_info (mpeg2dec);
    while (1) {
	state = mpeg2_parse (mpeg2dec);
    // fprintf( stderr, "parse %d\n", state );
	switch (state) {
	case -1:
	    return;
	case STATE_SEQUENCE:
	    /* might set nb fbuf, convert format, stride */
	    /* might set fbufs */
	    if (output->setup (output, info->sequence->width,
			       info->sequence->height)) {
		fprintf (stderr, "display setup failed\n");
		exit (1);
	    }
	    if (output->set_fbuf) {
		uint8_t * buf[3];
		void * id;

		mpeg2_custom_fbuf (mpeg2dec, 1);
		output->set_fbuf (output, buf, &id);
		mpeg2_set_buf (mpeg2dec, buf, id);
		output->set_fbuf (output, buf, &id);
		mpeg2_set_buf (mpeg2dec, buf, id);
	    } else if (output->setup_fbuf) {
		uint8_t * buf[3];
		void * id;

		output->setup_fbuf (output, buf, &id);
		mpeg2_set_buf (mpeg2dec, buf, id);
		output->setup_fbuf (output, buf, &id);
		mpeg2_set_buf (mpeg2dec, buf, id);
		output->setup_fbuf (output, buf, &id);
		mpeg2_set_buf (mpeg2dec, buf, id);
	    }
	    break;
	case STATE_PICTURE:
	    /* might skip */
	    /* might set fbuf */
	    if (output->set_fbuf) {
		uint8_t * buf[3];
		void * id;

		output->set_fbuf (output, buf, &id);
		mpeg2_set_buf (mpeg2dec, buf, id);
	    }
	    if (output->start_fbuf)
		output->start_fbuf (output, info->current_fbuf->buf,
				    info->current_fbuf->id);
	    break;
	case STATE_PICTURE_2ND:
	    /* should not do anything */
	    break;
	case STATE_SLICE:
	case STATE_END:
	    /* draw current picture */
	    /* might free frame buffer */
	    if (info->display_fbuf) {
		output->draw (output, info->display_fbuf->buf,
			      info->display_fbuf->id);
	    }
	    if (output->discard && info->discard_fbuf)
		output->discard (output, info->discard_fbuf->buf,
				 info->discard_fbuf->id);
	    break;
	}
    }
}

#define DEMUX_PAYLOAD_START 1
static int demux(uint8_t * buf, uint8_t * end, int flags, mpeg2input_t *instance)
{
    static int mpeg1_skip_table[16] = {
	0, 0, 4, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    /*
     * the demuxer keeps some state between calls:
     * if "state" = DEMUX_HEADER, then "head_buf" contains the first
     *     "bytes" bytes from some header.
     * if "state" == DEMUX_DATA, then we need to copy "bytes" bytes
     *     of ES data before the next header.
     * if "state" == DEMUX_SKIP, then we need to skip "bytes" bytes
     *     of data before the next header.
     *
     * NEEDBYTES makes sure we have the requested number of bytes for a
     * header. If we dont, it copies what we have into head_buf and returns,
     * so that when we come back with more data we finish decoding this header.
     *
     * DONEBYTES updates "buf" to point after the header we just parsed.
     */

#define DEMUX_HEADER 0
#define DEMUX_DATA 1
#define DEMUX_SKIP 2
    static int state = DEMUX_SKIP;
    static int state_bytes = 0;
    static uint8_t head_buf[264];

    uint8_t * header;
    int bytes;
    int len;

#define NEEDBYTES(x)						\
    do {							\
	int missing;						\
								\
	missing = (x) - bytes;					\
	if (missing > 0) {					\
	    if (header == head_buf) {				\
		if (missing <= end - buf) {			\
		    memcpy (header + bytes, buf, missing);	\
		    buf += missing;				\
		    bytes = (x);				\
		} else {					\
		    memcpy (header + bytes, buf, end - buf);	\
		    state_bytes = bytes + end - buf;		\
		    return 0;					\
		}						\
	    } else {						\
		memcpy (head_buf, header, bytes);		\
		state = DEMUX_HEADER;				\
		state_bytes = bytes;				\
		return 0;					\
	    }							\
	}							\
    } while (0)

#define DONEBYTES(x)		\
    do {			\
	if (header != head_buf)	\
	    buf = header + (x);	\
    } while (0)

    if (flags & DEMUX_PAYLOAD_START)
	goto payload_start;
    switch (state) {
    case DEMUX_HEADER:
	if (state_bytes > 0) {
	    header = head_buf;
	    bytes = state_bytes;
	    goto continue_header;
	}
	break;
    case DEMUX_DATA:
	if (state_bytes > end - buf) {
	    decode_mpeg2 (buf, end);
	    state_bytes -= end - buf;
	    return 0;
	}
	decode_mpeg2 (buf, buf + state_bytes);
	buf += state_bytes;
	break;
    case DEMUX_SKIP:
	if (state_bytes > end - buf) {
	    state_bytes -= end - buf;
	    return 0;
	}
	buf += state_bytes;
	break;
    }

    while (1) {
    payload_start:
	header = buf;
	bytes = end - buf;
    continue_header:
	NEEDBYTES (4);
	if (header[0] || header[1] || (header[2] != 1)) {
	    if (header != head_buf) {
		buf++;
		goto payload_start;
	    } else {
		header[0] = header[1];
		header[1] = header[2];
		header[2] = header[3];
		bytes = 3;
		goto continue_header;
	    }
	}
	switch (header[3]) {
	case 0xb9:	/* program end code */
	    /* DONEBYTES (4); */
	    /* break;         */
	    return 1;
	case 0xba:	/* pack header */
	    NEEDBYTES (12);
	    if ((header[4] & 0xc0) == 0x40) {	/* mpeg2 */
		NEEDBYTES (14);
		len = 14 + (header[13] & 7);
		NEEDBYTES (len);
		DONEBYTES (len);
		/* header points to the mpeg2 pack header */
	    } else if ((header[4] & 0xf0) == 0x20) {	/* mpeg1 */
		DONEBYTES (12);
		/* header points to the mpeg1 pack header */
	    } else {
		fprintf (stderr, "weird pack header\n");
		exit (1);
	    }
	    break;
	default:
	    if (header[3] == demux_track) {
		NEEDBYTES (7);
		if ((header[6] & 0xc0) == 0x80) {	/* mpeg2 */
		    NEEDBYTES (9);
		    len = 9 + header[8];
		    NEEDBYTES (len);
		    /* header points to the mpeg2 pes header */
		    if (header[7] & 0x80) {
			uint32_t pts;

			pts = (((buf[9] >> 1) << 30) |
			       (buf[10] << 22) | ((buf[11] >> 1) << 15) |
			       (buf[12] << 7) | (buf[13] >> 1));
			mpeg2_pts (mpeg2dec, pts);
		    }
		} else {	/* mpeg1 */
		    int len_skip;
		    uint8_t * ptsbuf;

		    len = 7;
		    while (header[len - 1] == 0xff) {
			len++;
			NEEDBYTES (len);
			if (len > 23) {
			    fprintf (stderr, "too much stuffing\n");
			    break;
			}
		    }
		    if ((header[len - 1] & 0xc0) == 0x40) {
			len += 2;
			NEEDBYTES (len);
		    }
		    len_skip = len;
		    len += mpeg1_skip_table[header[len - 1] >> 4];
		    NEEDBYTES (len);
		    /* header points to the mpeg1 pes header */
		    ptsbuf = header + len_skip;
		    if (ptsbuf[-1] & 0x20) {
			uint32_t pts;

			pts = (((ptsbuf[-1] >> 1) << 30) |
			       (ptsbuf[0] << 22) | ((ptsbuf[1] >> 1) << 15) |
			       (ptsbuf[2] << 7) | (ptsbuf[3] >> 1));
			mpeg2_pts (mpeg2dec, pts);
		    }
		}
		DONEBYTES (len);
		bytes = 6 + (header[4] << 8) + header[5] - len;
		if (bytes > end - buf) {
		    decode_mpeg2 (buf, end);
		    state = DEMUX_DATA;
		    state_bytes = bytes - (end - buf);
		    return 0;
		} else if (bytes > 0) {
		    decode_mpeg2 (buf, buf + bytes);
		    buf += bytes;
		}
	    } else if (header[3] < 0xb9) {
		fprintf (stderr,
			 "looks like a video stream, not system stream\n");
		DONEBYTES (4);
	    } else {
		NEEDBYTES (6);
		DONEBYTES (6);
		bytes = (header[4] << 8) + header[5];
		if (bytes > end - buf) {
		    state = DEMUX_SKIP;
		    state_bytes = bytes - (end - buf);
		    return 0;
		}
		buf += bytes;
	    }
	}
    }

    return -1;
}

static int ps_loop( mpeg2input_t * instance )
{
    uint8_t *end;

    if( instance->proceed ) {
        do {
            int tmp;

            end = buffer + fread( buffer, 1, BUFFER_SIZE, in_file );
            tmp = demux( buffer, end, 0, instance );
            if( tmp == 1 ) {
                /* hit program_end_code */
                return 0;
            }
        } while( end == buffer + BUFFER_SIZE && instance->proceed );
    }

    return 1;
}

mpeg2input_t *mpeg2input_new( const char *filename, int track, int accel )
{
    demux_track = track;
    if( demux_track < 0xe0 ) {
        demux_track += 0xe0;
        if( (demux_track < 0xe0) || (demux_track > 0xef) ) {
            fprintf( stderr, "mpeg2input: Invalid track number: %d\n", track );
        }
    }

    if( accel ) {
        mpeg2_accel( accel );
    }

    in_file = fopen( filename, "rb" );
    if( !in_file ) {
        fprintf( stderr, "mpeg2input: %s - could not open file %s\n", 
                 strerror( errno ), filename );
        return 0;
    }

    output_open = vo_tvtime_open;
    output = output_open();
    if( !output ) {
        fprintf( stderr, "mpeg2input: Can not open output\n" );
        return 0;
    }
    mpeg2dec = mpeg2_init();
    if( !mpeg2dec ) return 0;

    return (mpeg2input_t *) output;
}

void mpeg2input_delete( mpeg2input_t *instance )
{
    mpeg2_close( mpeg2dec );
    if( output->close ) {
        output->close( output );
    }
}

int mpeg2input_next_frame( mpeg2input_t *mpegin )
{
    if( !ps_loop( mpegin ) ) return 0;
    mpegin->proceed = 1;
    return 1;
}

uint8_t *mpeg2input_get_curframe( mpeg2input_t *mpegin )
{
    return mpegin->frames422 + (mpegin->curout * mpegin->width * mpegin->height * 2);
}

uint8_t *mpeg2input_get_lastframe( mpeg2input_t *mpegin )
{
    return mpegin->frames422 + (((mpegin->curout + 2)%3) * mpegin->width * mpegin->height * 2);
}

uint8_t *mpeg2input_get_secondlastframe( mpeg2input_t *mpegin )
{
    return mpegin->frames422 + (((mpegin->curout + 1)%3) * mpegin->width * mpegin->height * 2);
}

int mpeg2input_get_width( mpeg2input_t *mpegin )
{
    return 720; // mpegin->width;
}

int mpeg2input_get_height( mpeg2input_t *mpegin )
{
    return 480; // mpegin->height;
}

#else
mpeg2input_t *mpeg2input_new( const char *filename, int track, int accel )
{
    fprintf( stderr, "mpeg2input: Support not compiled in.\n" );
    return 0;
}
void mpeg2input_delete( mpeg2input_t *instance ) {}
int mpeg2input_next_frame( mpeg2input_t *mpegin ) { return 0; }
uint8_t *mpeg2input_get_curframe( mpeg2input_t *mpegin ) { return 0; }
uint8_t *mpeg2input_get_lastframe( mpeg2input_t *mpegin ) { return 0; }
uint8_t *mpeg2input_get_secondlastframe( mpeg2input_t *mpegin ) { return 0; }
int mpeg2input_get_width( mpeg2input_t *mpegin ) { return 0; }
int mpeg2input_get_height( mpeg2input_t *mpegin ) { return 0; }
#endif

