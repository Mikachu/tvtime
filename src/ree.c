
#include "diffcomp.h"
#include "ree.h"

int ree_is_video_packet( ree_packet_t *pkt )
{
    return ( ( pkt->hdr.id == 1 ) || ( pkt->hdr.id >= 4 ) );
}

int ree_is_audio_packet( ree_packet_t *pkt )
{
    return ( pkt->hdr.id == 2 );
}

unsigned char *tmpimage = 0;

void ree_decode_video_packet( ree_packet_t *pkt, unsigned char *luma,
                              unsigned char *cb, unsigned char *cr,
                              int width, int height )
{
    if( pkt->hdr.id == REE_VIDEO_YCBCR420 ) {
        unsigned char *srcluma = pkt->data;
        unsigned char *srccb = srcluma + ( width * height );
        unsigned char *srccr = srccb + ( width/2 * height/2 );

        memcpy( luma, srcluma, width * height );
        memcpy( cb, srccb, width/2 * height/2 );
        memcpy( cr, srccr, width/2 * height/2 );

    } else if( pkt->hdr.id == REE_VIDEO_DIFFCOMP ) {
        ree_split_packet_t *diffpkt = (ree_split_packet_t *) pkt;

        diffcomp_decompress_plane( luma, diffpkt->data,
                                   width, height );
        diffcomp_decompress_plane( cb, diffpkt->data + diffpkt->lumasize,
                                   width/2, height/2 );
        diffcomp_decompress_plane( cr, diffpkt->data + diffpkt->lumasize + diffpkt->cbsize,
                                   width/2, height/2 );
    }
}

