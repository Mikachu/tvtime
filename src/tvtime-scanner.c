/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * Based heavily on 'scantv.c' from xawtv,
 *   (c) 2000-2002 Gerd Knorr <kraxel@goldbach.in-berlin.de>
 * See http://bytesex.org/xawtv/
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
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "videoinput.h"
#include "commands.h"
#include "utils.h"

int main( int argc, char **argv )
{
    config_t *cfg = config_new();
    station_mgr_t *stationmgr = 0;
    videoinput_t *vidin;
    int fi, on, tuned;
    int f, f1, f2, fc;
    int verbose, norm;
    int curstation = 1;

    if( !cfg ) {
        fprintf( stderr, "tvtime-scanner: Can't initialize tvtime configuration, exiting.\n" );
        return 1;
    }

    verbose = config_get_verbose( cfg );

    if( !strcasecmp( config_get_v4l_norm( cfg ), "pal" ) ) {
        norm = VIDEOINPUT_PAL;
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "secam" ) ) {
        norm = VIDEOINPUT_SECAM;
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-nc" ) ) {
        norm = VIDEOINPUT_PAL_NC;
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-m" ) ) {
        norm = VIDEOINPUT_PAL_M;
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-n" ) ) {
        norm = VIDEOINPUT_PAL_N;
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "ntsc-jp" ) ) {
        norm = VIDEOINPUT_NTSC_JP;
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-60" ) ) {
        norm = VIDEOINPUT_PAL_60;
    } else {
        /* Only allow NTSC otherwise. */
        norm = VIDEOINPUT_NTSC;
    }

    fprintf( stderr, "tvtime-scanner: Scanning using TV standard '%s'\n",
             videoinput_get_norm_name( norm ) );

    stationmgr = station_new( videoinput_get_norm_name( norm ), "Custom", 0, verbose );
    if( !stationmgr ) {
        fprintf( stderr, "tvtime-scanner: Can't create station manager (no memory?), exiting.\n" );
        config_delete( cfg );
        return 1;
    }

    vidin = videoinput_new( config_get_v4l_device( cfg ), 
                            config_get_inputwidth( cfg ), 
                            norm, verbose );
    if( !vidin ) {
        fprintf( stderr, "tvtime-scanner: Can't open video4linux device '%s'.",
                config_get_v4l_device( cfg ) );
        station_delete( stationmgr );
        config_delete( cfg );
        return 1;
    } else {
        videoinput_set_input_num( vidin, config_get_inputnum( cfg ) );
    }

    if( !videoinput_has_tuner( vidin ) ) {
        fprintf( stderr, "tvtime-scanner: No tuner found on input %d.\n"
                 "tvtime-scanner: If you have a tuner, select a different input using --input=<num>.\n",
                 config_get_inputnum( cfg ) );
        videoinput_delete( vidin );
        station_delete( stationmgr );
        config_delete( cfg );
        return 1;
    }

    /* Scan freqnencies */
    fprintf( stderr, "tvtime-scanner: Scanning from %6.2fMHz to %6.2fMHz.\n",
             44.0, 958.0 );
    on = 0;
    fc = 0;
    f1 = 0;
    f2 = 0;
    fi = -1;

    for( f = 44*16; f <= 958*16; f += 4 ) {
        char stationmhz[ 128 ];

        videoinput_set_tuner_freq( vidin, (f * 1000) / 16 );
        fprintf( stderr, "tvtime-scanner: Checking %6.2fMHz: ", ((double) f) / 16.0 );
        usleep( 200000 ); /* 0.2 sec */
        tuned = videoinput_freq_present( vidin );

        /* state machine */
        if( 0 == on && 0 == tuned ) {
            fprintf( stderr, "|   no\r" );
            continue;
        }
        if( 0 == on && 0 != tuned ) {
            fprintf( stderr, " \\  raise\r" );
            f1 = f;
            /* if( i != chancount ) { fi = i; fc = f; } */
            on = 1;
            continue;
        }
        if( 0 != on && 0 != tuned ) {
            fprintf( stderr, "  | yes\r" );
            /* if( i != chancount ) { fi = i; fc = f; } */
            continue;
        }
        /* if (on != 0 && 0 == tuned)  --  found one, read name from vbi */
        fprintf( stderr," /  fall\r" );
        f2 = f;
        if( 0 == fc ) {
            fc = (f1+f2)/2;
        }

        fprintf( stderr, "tvtime-scanner: Found a channel at %6.2fMHz, addint to stationlist.\n",
                 ((double) fc) / 16.0 );

        sprintf( stationmhz, "%.2fMHz", ((double) fc) / 16.0 );
        station_add( stationmgr, curstation, "Custom", stationmhz, stationmhz );
        station_writeconfig( stationmgr );
        curstation++;

        on = 0;
        fc = 0;
        f1 = 0;
        f2 = 0;
        fi = -1;
    }

    station_delete( stationmgr );
    config_delete( cfg );
    return 0;
}

