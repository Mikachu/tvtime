
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "rtctimer.h"
#include "videoinput.h"
#include "reepktq.h"
#include "diffcomp.h"
#include "ree.h"

static inline void get_time( int64_t *const ptime )
{
   asm volatile ( "rdtsc" : "=A" (*ptime) );
}

static int timediff( struct timeval *large, struct timeval *small )
{
    return (   ( ( large->tv_sec * 1000 * 1000 ) + large->tv_usec )
             - ( ( small->tv_sec * 1000 * 1000 ) + small->tv_usec ) );
}


static const char *videodev = "/dev/video0";

static const int block_size = 4096;

static const int use_hufftftm = 1;
static unsigned int blocks_written = 0;

static ree_file_header_t *fileheader;
static videoinput_t *vidin;
static reepktq_t *video_queue;
static pthread_t video_capture_thread;
static pthread_t disk_writer_thread;
static int outfd;
static struct timeval basetime;
static pthread_mutex_t rec_start_mut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rec_start_cond = PTHREAD_COND_INITIALIZER;
static int rec_start_now = 0;


static int32_t time_diff( int32_t big_tv_sec, int32_t big_tv_usec,
                          int32_t small_tv_sec, int32_t small_tv_usec )
{
    return ( ( ( big_tv_sec  * 1000 * 1000 ) + big_tv_usec )
             - ( ( small_tv_sec * 1000 * 1000 ) + small_tv_usec ) );
}

/**
 * Capture the video and put it in the queue.
 */
static void *video_capture_thread_main( void *crap )
{
    unsigned char *tmp420space;
    struct timeval starttime;
    int gotframes = 0;
    int frameid;

    pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, 0 );

    /**
     * We run the video recorder at realtime priority (SCHED_FIFO), but at a
     * lower priority than the audio thread (theoretically).
     */
    set_realtime_priority( 0 );


    /**
     * Allocate some space (more than enough for kicks) to store our results
     * from the conversion 4:2:2->4:2:0.
     */
    tmp420space = malloc( ( fileheader->width * fileheader->height * 3 ) / 2 );


    pthread_mutex_lock( &rec_start_mut );
    rec_start_now = 1;
    pthread_cond_broadcast( &rec_start_cond );
    pthread_mutex_unlock( &rec_start_mut );

    videoinput_next_frame( vidin, &frameid );
    videoinput_free_frame( vidin, frameid );
    gettimeofday( &starttime, 0 );

    for(;;) {
        ree_packet_t *vpkt;
        unsigned char *curimage;
        int outsize;
        struct timeval curtime;
        int ccframes;
        int full;
        int frameid;


        curimage = videoinput_next_frame( vidin, &frameid );
        gettimeofday( &curtime, 0 );

        vpkt = (ree_packet_t *) reepktq_enqueue( video_queue );

        gotframes++;
        ccframes = timediff( &curtime, &starttime ) / (16666*2);

        // We may have to drop a frame if the queue is full.
        if( !vpkt ) {
            fprintf( stderr, "video: Frame dropped (queue full!).\n" );
            videoinput_free_frame( vidin, frameid );
            pthread_testcancel();
            continue;
        }

        vpkt->hdr.tv_sec = curtime.tv_sec - basetime.tv_sec;
        vpkt->hdr.tv_usec = curtime.tv_usec;
        vpkt->hdr.frameid = ccframes - gotframes + 1;

        /**
         * Here we compress the mmap'ed buffer directly into the
         * queue memory and signal to the capture card that it can
         * continue.
         */
        if( use_hufftftm ) {
            ree_split_packet_t *huffpkt = (ree_split_packet_t *) vpkt;

            huffpkt->hdr.id = REE_VIDEO_DIFFC422;
            huffpkt->hdr.datasize = sizeof( int32_t ) * 3;
            huffpkt->hdr.datasize += diffcomp_compress_packed422( huffpkt->data, curimage,
                                                                  fileheader->width, fileheader->height );
        } else {
            vpkt->hdr.id = REE_VIDEO_YCBCR422;
            vpkt->hdr.datasize = fileheader->width * fileheader->height * 2;
            memcpy( vpkt->data, curimage, fileheader->width * fileheader->height * 2 );
        }
        videoinput_free_frame( vidin, frameid );

        full = fileheader->width * fileheader->height * 2;
        fprintf( stderr, "\rrvr: video compression %3.0f%% "
                         "[q: %4d, d: %4d, wrote %02ld:%02ld:%02ld in %4dM]\r",
                 100.0 * ( (double) vpkt->hdr.datasize / (double) full ),
                 reepktq_queue_size( video_queue ),
                 ccframes - gotframes + 1,
                 ( curtime.tv_sec - starttime.tv_sec ) / ( 60 * 60 ),
                 ( ( curtime.tv_sec - starttime.tv_sec ) / 60 ) % 60,
                 ( curtime.tv_sec - starttime.tv_sec ) % 60,
                 blocks_written / (1024*1024/block_size) );

        /* Round to the nearest block size and continue. */
        outsize = vpkt->hdr.datasize + sizeof( ree_packet_header_t );
        outsize += block_size - ( outsize % block_size );
        vpkt->hdr.payloadsize = outsize - sizeof( ree_packet_header_t );
        reepktq_complete_enqueue( video_queue );

        /* Watch for cancellation. */
        pthread_testcancel();
    }

    return 0;
}

static int file_max = 2621440;

/**
 * Service the queue by writing to disk.
 */
static void *disk_writer_thread_main( void *crap )
{
    pthread_setcanceltype( PTHREAD_CANCEL_DEFERRED, 0 );

    for(;;) {
        ree_packet_t *curpack;

        /* Watch for cancellation. */
        pthread_testcancel();

        if( blocks_written > file_max ) {
            close( outfd );
            outfd = open( "/disks/blip/spill.rvr", O_WRONLY|O_CREAT|O_LARGEFILE,
                          S_IREAD|S_IWRITE|S_IRGRP|S_IROTH );
        }

        /* Service the video queue. */
        if( reepktq_head( video_queue ) ) {
            curpack = (ree_packet_t *) reepktq_head( video_queue );
            write( outfd, curpack, curpack->hdr.payloadsize + sizeof( ree_packet_header_t ) );
            blocks_written += ( curpack->hdr.payloadsize + sizeof( ree_packet_header_t ) ) / block_size;
            reepktq_dequeue( video_queue );
        }


        /* Throttle ourselves. */
        usleep( 20 );
    }

    return 0;
}

int main( int argc, char **argv )
{
    int headersize, i;

    /* Check args. */
    if( argc < 2 ) {
        fprintf( stderr, "Usage: %s <output filename>\n", argv[ 0 ] );
        return 1;
    }

    /* Open file for writing early, to make sure we can. */
    outfd = open( argv[ 1 ], O_WRONLY|O_CREAT|O_LARGEFILE, S_IREAD|S_IWRITE|S_IRGRP|S_IROTH );
    if( outfd < 0 ) {
        fprintf( stderr, "rvr: Can't open %s for writing (%s).\n",
                 argv[ 1 ], strerror( errno ) );
        return 1;
    }

    /* Open the capture card. */
    vidin = videoinput_new( videodev, 480, VIDEOINPUT_NTSC, 1 );
    if( !vidin ) {
        fprintf( stderr, "rvr: Can't open video input device.\n" );
        close( outfd );
        return 1;
    }
    videoinput_set_input_num( vidin, 1 );

    /* Create our file header. */
    fileheader = (ree_file_header_t *) malloc( sizeof( ree_file_header_t ) );
    if( !fileheader ) {
        fprintf( stderr, "rvr: Can't allocate file header.\n" );
        videoinput_delete( vidin );
        close( outfd );
        return 1;
    }

    /* Fill in the header. */
    fileheader->reetid = REE_FILE_ID;
    fileheader->width = videoinput_get_width( vidin );
    fileheader->height = videoinput_get_height( vidin );

    /* Write out the header and pad to the block size with 0s. */
    headersize  = sizeof( ree_file_header_t );
    headersize += block_size - ( headersize % block_size );
    fileheader->headersize = headersize;
    write( outfd, fileheader, sizeof( ree_file_header_t ) );
    headersize = 0;
    for( i = 0; i < fileheader->headersize - sizeof( ree_file_header_t ); i++ ) {
        write( outfd, &headersize, 1 );
    }

    /* Tell the user how happy we are. */
    fprintf( stderr, "rvr: Capturing from %s at %dx%d.\n",
             videodev, fileheader->width, fileheader->height );

    /* Make the queues (the first will take a while! */
    fprintf( stderr, "rvr: Creating video packet queue.\n" );
    video_queue = reepktq_new( 256, 2*262144 );


    /* Create the threads. */
    fprintf( stderr, "rvr: Starting record threads.\n" );
    gettimeofday( &basetime, 0 );
    pthread_create( &video_capture_thread, 0, video_capture_thread_main, 0 );
    pthread_create( &disk_writer_thread, 0, disk_writer_thread_main, 0 );

    fprintf( stderr, "rvr: Hit 'q' and then <enter> to stop recording.\n" );

    /* Wait for the user to request that we stop. */
    for(;;) {
        char curcrap[ 200 ];
        read( 0, curcrap, 199 );
        if( curcrap[ 0 ] == 'q' ) break;
    }

    /* Shut down all the threads. */
    fprintf( stderr, "rvr: Shutting down.\n" );
    pthread_cancel( video_capture_thread );
    pthread_cancel( disk_writer_thread );
    pthread_join( video_capture_thread, 0 );
    pthread_join( disk_writer_thread, 0 );

    /* Close all the shit. */
    fprintf( stderr, "rvr: Recording halted, cleaning up.\n" );
    reepktq_delete( video_queue );
    close( outfd );
    videoinput_delete( vidin );
    return 0;
}

