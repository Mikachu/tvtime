
#include <string.h>
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

void ree_decode_video_packet( ree_packet_t *pkt, unsigned char *data, int width, int height )
{
    if( pkt->hdr.id == REE_VIDEO_YCBCR422 ) {
        unsigned char *src = pkt->data;

        memcpy( data, src, width * height * 2 );

    } else if( pkt->hdr.id == REE_VIDEO_DIFFC422 ) {
        ree_split_packet_t *diffpkt = (ree_split_packet_t *) pkt;

        diffcomp_decompress_packed422( data, diffpkt->data, width, height );
    }
}

