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
#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/convert.h>
#include "mpeg2input.h"

struct convert_init_s;
typedef struct {
    void (* convert) (int, int, uint32_t, void *, struct convert_init_s *);
} vo_setup_result_t;

typedef struct vo_instance_s vo_instance_t;
struct vo_instance_s {
    int (* setup) (vo_instance_t * instance, int width, int height,
		   vo_setup_result_t * result);
    void (* setup_fbuf) (vo_instance_t * instance, uint8_t ** buf, void ** id);
    void (* set_fbuf) (vo_instance_t * instance, uint8_t ** buf, void ** id);
    void (* start_fbuf) (vo_instance_t * instance,
			 uint8_t * const * buf, void * id);
    void (* draw) (vo_instance_t * instance, uint8_t * const * buf, void * id);
    void (* discard) (vo_instance_t * instance,
		      uint8_t * const * buf, void * id);
    void (* close) (vo_instance_t * instance);
};

typedef vo_instance_t * vo_open_t (void);

typedef struct {
    char * name;
    vo_open_t * open;
} vo_driver_t;

void vo_accel (uint32_t accel);

/* return NULL terminated array of all drivers */
vo_driver_t * vo_drivers (void);

#define BUFFER_SIZE 4096
static uint8_t buffer[BUFFER_SIZE];
static FILE *in_file;
static int demux_track = 0;
static mpeg2dec_t *mpeg2dec;
static vo_open_t *output_open = 0;
static vo_instance_t * output;

typedef struct {
    void * data;
} tvtime_frame_t;

struct mpeg2input_s {
    vo_instance_t vo;
    tvtime_frame_t frame[3];
    int index;
    int width;
    int height;
    char * curframe;
    int proceed;
};


static void tvtime_setup_fbuf (vo_instance_t * _instance,
			    uint8_t ** buf, void ** id)
{
    mpeg2input_t * instance = (mpeg2input_t *) _instance;

    buf[0] = (uint8_t *) instance->frame[instance->index].data;
    buf[1] = buf[2] = NULL;
    *id = instance->frame + instance->index++;
}

static void tvtime_draw_frame (vo_instance_t * _instance,
			    uint8_t * const * buf, void * id)
{
    tvtime_frame_t * frame;
    mpeg2input_t * instance;

    frame = (tvtime_frame_t *) id;
    instance = (mpeg2input_t *) _instance;

    /* draw frame->data */
    instance->curframe = (char *)frame->data;
    instance->proceed = 0;
}

static int tvtime_alloc_frames (mpeg2input_t * instance)
{
    int size;
    char * alloc;
    int i;

    size = 0;
    alloc = NULL;
    for (i = 0; i < 3; i++) {
	if (i == 0) {
	    size = (instance->width * instance->height * 3);
	    alloc = (char *) malloc( 3 * size );
	    if ( alloc == NULL )
            return 1;
	}
	instance->frame[i].data = alloc;
	alloc += size;
    }
    instance->index = 0;
    return 0;
}

static void tvtime_close (vo_instance_t * _instance)
{
    mpeg2input_t * instance = (mpeg2input_t *) _instance;

    if ( !instance ) return;
    if ( instance->frame[0].data ) free( instance->frame[0].data );
    free( instance );
}

static int common_setup (mpeg2input_t * instance, int width, int height,
			 vo_setup_result_t * result)
{
    fprintf( stderr, "common_setup\n");

    instance->vo.set_fbuf = NULL;
    instance->vo.discard = NULL;
    instance->vo.start_fbuf = NULL;
    instance->width = width;
    instance->height = height;
    instance->curframe = (char *)NULL;

	if (tvtime_alloc_frames (instance))
	    return 1;
	instance->vo.setup_fbuf = tvtime_setup_fbuf;
	instance->vo.draw = tvtime_draw_frame;
	instance->vo.close = tvtime_close;

	result->convert = NULL;

    return 0;
}

static int tvtime_setup (vo_instance_t * instance, int width, int height,
		      vo_setup_result_t * result)
{
    return common_setup ((mpeg2input_t *)instance, width, height, result);
}

static vo_instance_t * vo_tvtime_open (void)
{
    mpeg2input_t * instance;

    instance = (mpeg2input_t *) malloc (sizeof(mpeg2input_t));
    if (instance == NULL)
	return NULL;

    instance->vo.setup = tvtime_setup;
    instance->proceed = 1;

    return (vo_instance_t *) instance;
}

static void decode_mpeg2 (uint8_t * current, uint8_t * end)
{
    const mpeg2_info_t * info;
    int state;
    vo_setup_result_t setup_result;

    mpeg2_buffer (mpeg2dec, current, end);

    info = mpeg2_info (mpeg2dec);
    while (1) {
	state = mpeg2_parse (mpeg2dec);
	switch (state) {
	case -1:
	    return;
	case STATE_SEQUENCE:
        fprintf( stderr, "in sequence\n" );
	    /* might set nb fbuf, convert format, stride */
	    /* might set fbufs */
	    if (output->setup (output, info->sequence->width,
			       info->sequence->height, &setup_result)) {
		fprintf (stderr, "display setup failed\n");
		exit (1);
	    }
	    if (setup_result.convert)
		mpeg2_convert (mpeg2dec, setup_result.convert, NULL);
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
        fprintf( stderr, "in picture\n" );
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
        fprintf( stderr, "in picture 2\n" );
	    break;
	case STATE_SLICE:
	case STATE_END:
	    /* draw current picture */
	    /* might free frame buffer */
        fprintf( stderr, "in slice or end\n" );
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
static int demux (uint8_t * buf, uint8_t * end, int flags, mpeg2input_t *instance)
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
	if ((state_bytes > end - buf)) {
	    decode_mpeg2 (buf, end);
	    state_bytes -= end - buf;
	    return 0;
	}
	decode_mpeg2 (buf, buf + state_bytes);
	buf += state_bytes;
	break;
    case DEMUX_SKIP:
	if ((state_bytes > end - buf)) {
	    state_bytes -= end - buf;
	    return 0;
	}
	buf += state_bytes;
	break;
    }

    while (1 && instance->proceed) {
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
		if ((bytes > end - buf)) {
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

static void ps_loop( mpeg2input_t * instance )
{
    uint8_t * end;

    fprintf( stderr, "mpeg2input: ps_loop, proceed %d\n", instance->proceed );

    if( instance->proceed ) {
        do {
            int tmp;

            fprintf( stderr, "in ps_loop 2\n" );
            end = buffer + fread( buffer, 1, BUFFER_SIZE, in_file );
            tmp = demux( buffer, end, 0, instance );
            if( tmp == 1 ) {
                break; /* hit program_end_code */
            } else if (tmp < 0) {
            }
        } while( end == buffer + BUFFER_SIZE && instance->proceed );
    }
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
        mpeg2_accel( 0 );
    }

    in_file = fopen( filename, "rb" );
    if( !in_file ) {
        fprintf( stderr, "mpeg2input: %s - could not open file %s\n", 
                 strerror( errno ), filename );
        return 0;
    }

    output_open = vo_tvtime_open;
    output = output_open();
    if (output == NULL) {
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

uint8_t *mpeg2input_get_curframe( mpeg2input_t *instance )
{
    ps_loop( instance );

    instance->proceed = 1;
    return (uint8_t *) instance->curframe;
}


int tvtime_mpeg2dec_get_width( mpeg2input_t * instance )
{
    return instance->width;
}

int tvtime_mpeg2dec_get_height( mpeg2input_t * instance )
{
    return instance->height;
}

