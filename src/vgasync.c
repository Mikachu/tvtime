/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>
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

