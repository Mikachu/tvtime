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

