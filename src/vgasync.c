
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/io.h>
#include <sys/time.h>

#define BASEPORT 0x3da

void vgasync_cleanup( void )
{
    fprintf( stderr, "vgasync: Cleaning up.\n" );
    if( ioperm( BASEPORT, 1, 0 ) ) {
        perror( "ioperm" );
        exit( 1 );
    }
}

int vgasync_init( int verbose )
{
    /* Get access to the ports */
    if( ioperm( BASEPORT, 1, 1 ) ) {
        if( verbose ) {
            fprintf( stderr, "vgasync: Can't access VGA port: %s\n",
                     strerror( errno ) );
        }
        return 0;
    }

    atexit( vgasync_cleanup );
    return 1;
}

void vgasync_spin_until_end_of_next_refresh( void )
{
    /* TODO: A better timeout. */
    int i = 0x00ffffff;

    for(;i;--i) {
        /* wait for the refresh to occur. */
        if( (inb( BASEPORT ) & 8) ) break;
    }
    /* now we're inside the refresh */
    for(;i;--i) {
        /* wait for the refresh to stop. */
        if( !(inb( BASEPORT ) & 8) ) break;
    }

    if( !i ) fprintf( stderr, "vgasync: Timeout hit.\n" );
}

void vgasync_spin_until_out_of_refresh( void )
{
    /* TODO: A better timeout. */
    int i = 0x00ffffff;

    if( inb( BASEPORT ) & 8 ) {
        for(;i;--i) {
            /* wait for the refresh to stop. */
            if( !(inb( BASEPORT ) & 8) ) break;
        }
    }

    if( !i ) fprintf( stderr, "vgasync: Timeout hit.\n" );
}

