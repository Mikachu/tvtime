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

#ifndef REE_H_INCLUDED
#define REE_H_INCLUDED

#include <stdint.h>

/**
 * 'ree' is short for 'reet'.
 */

/* Packet types. */
#define REE_VIDEO_RTJPEG   1
#define REE_AUDIO_44100_ST 2
#define REE_CC_TEXT        3
#define REE_VIDEO_YCBCR420 4
#define REE_VIDEO_LZO      5
#define REE_VIDEO_DIFFCOMP 6
#define REE_VIDEO_YCBCR422 7
#define REE_VIDEO_DIFFC422 8

/* File identification dword (LE). */
#define REE_FILE_ID        ( ( 'r' << 24 ) | ( 'e' << 16 ) | ( 'e' << 8 ) | 't' )

typedef struct ree_file_header_s
{
    int32_t reetid;
    int32_t headersize;
    int32_t width;
    int32_t height;
    uint32_t dequant_values[ 128 ];
} ree_file_header_t;

typedef struct ree_packet_header_s
{
    int32_t id;
    int32_t frameid;
    int32_t tv_sec;
    int32_t tv_usec;
    int32_t payloadsize;
    int32_t datasize;
} ree_packet_header_t;

typedef struct ree_packet_s
{
    ree_packet_header_t hdr;
    uint8_t data[ 720 * 576 * 4 ];
} ree_packet_t;

typedef struct ree_split_packet_s
{
    ree_packet_header_t hdr;
    int32_t lumasize;
    int32_t cbsize;
    int32_t crsize;
    uint8_t data[ 720 * 576 * 4 ];
} ree_split_packet_t;

/**
 * Returns 1 if the given packet is a video packet.
 */
int ree_is_video_packet( ree_packet_t *pkt );

/**
 * Returns 1 if the given packet is an audio packet.
 */
int ree_is_audio_packet( ree_packet_t *pkt );

/**
 * Decodes the given video packet into the given buffers.
 */
void ree_decode_video_packet( ree_packet_t *pkt, uint8_t *data, int width, int height );

#endif /* REE_H_INCLUDED */
