
#ifndef REE_H_INCLUDED
#define REE_H_INCLUDED

#include <inttypes.h>

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
void ree_decode_video_packet( ree_packet_t *pkt, unsigned char *luma,
                              unsigned char *cb, unsigned char *cr,
                              int width, int height );

#endif /* REE_H_INCLUDED */
