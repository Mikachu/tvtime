
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include "pulldown.h"
#include "rvrreader.h"

struct rvrreader_s
{
    int fd;
    ree_file_header_t *fhdr;
    ree_packet_t *pkt;
    int dataread;
    int curframe;
};

rvrreader_t *rvrreader_new( const char *filename )
{
    rvrreader_t *reader = (rvrreader_t *) malloc( sizeof( rvrreader_t ) );

    if( !reader ) return 0;

    reader->fd = open( filename, O_RDONLY|O_LARGEFILE );
    if( reader->fd < 0 ) {
        fprintf( stderr, "rvrreader: Can't open file %s for reading.\n", filename );
        free( reader );
        return 0;
    }

    reader->fhdr = (ree_file_header_t *) malloc( sizeof( ree_file_header_t ) );
    if( !reader->fhdr ) {
        fprintf( stderr, "rvrreader: Can't allocate file header.\n" );
        close( reader->fd );
        free( reader );
        return 0;
    }

    reader->pkt = (ree_packet_t *) malloc( sizeof( ree_packet_t ) );
    if( !reader->pkt ) {
        fprintf( stderr, "rvrreader: Can't allocate data packet.\n" );
        free( reader->fhdr );
        close( reader->fd );
        free( reader );
        return 0;
    }

    /* Read in the file header. */
    read( reader->fd, reader->fhdr, sizeof( ree_file_header_t ) );
    lseek( reader->fd, reader->fhdr->headersize - sizeof( ree_file_header_t ), SEEK_CUR );

    reader->curframe = 0;

    /* Read first packet. */
    reader->dataread = 0;
    read( reader->fd, reader->pkt, sizeof( ree_packet_header_t ) );
    return reader;
}

void rvrreader_delete( rvrreader_t *reader )
{
    close( reader->fd );
    free( reader );
}

ree_file_header_t *rvrreader_get_fileheader( rvrreader_t *reader )
{
    return reader->fhdr;
}

int rvrreader_get_width( rvrreader_t *reader )
{
    return reader->fhdr->width;
}

int rvrreader_get_height( rvrreader_t *reader )
{
    return reader->fhdr->height;
}

int rvrreader_get_curframe( rvrreader_t *reader )
{
    return reader->curframe;
}

static int rvrreader_next_packet( rvrreader_t *reader )
{
    int ret;
    if( !reader->dataread ) {
        lseek64( reader->fd, reader->pkt->hdr.payloadsize, SEEK_CUR );
    }
    reader->dataread = 0;

    ret = ( read( reader->fd, reader->pkt, sizeof( ree_packet_header_t ) ) == sizeof( ree_packet_header_t ) );
    if( ree_is_video_packet( reader->pkt ) ) {
        reader->curframe++;
    }
    return ret;
}

static ree_packet_t *rvrreader_cur_packet( rvrreader_t *reader )
{
    if( !reader->dataread ) {
        read( reader->fd, reader->pkt->data, reader->pkt->hdr.payloadsize );
        reader->dataread = 1;
    }
    return reader->pkt;
}

int rvrreader_next_frame( rvrreader_t *reader )
{
    while( rvrreader_next_packet( reader ) > 0 ) {
        if( ree_is_video_packet( reader->pkt ) ) {
            return 1;
        }
    }
    return 0;
}

int rvrreader_decode_curframe( rvrreader_t *reader, unsigned char *data )
{
    int width = rvrreader_get_width( reader );
    int height = rvrreader_get_height( reader );

    if( !ree_is_video_packet( reader->pkt ) ) {
        ree_decode_video_packet( rvrreader_cur_packet( reader ), data, width, height );
        return 1;
    }

    return 0;
}

