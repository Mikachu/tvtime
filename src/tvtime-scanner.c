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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
#else
# define _(string) string
#endif
#include "videoinput.h"
#include "tvtimeconf.h"
#include "station.h"
#include "utils.h"

int main( int argc, char **argv )
{
    config_t *cfg;
    station_mgr_t *stationmgr = 0;
    videoinput_t *vidin;
    int fi, on, tuned;
    int f, f1, f2, fc;
    int verbose, norm;
    int curstation = 1;

    /*
     * Setup i18n. This has to be done as early as possible in order
     * to show startup messages in the users preferred language.
     */
    setup_i18n();

    cfg = config_new();
    if( !cfg ) {
        fprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }

    if( !config_parse_tvtime_scanner_command_line( cfg, argc, argv ) ) {
        config_delete( cfg );
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

    fprintf( stderr, _("Scanning using TV standard %s.\n"),
             videoinput_get_norm_name( norm ) );

    stationmgr = station_new( videoinput_get_norm_name( norm ),
                              "Custom", 0, verbose );
    if( !stationmgr ) {
        lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        config_delete( cfg );
        return 1;
    }

    vidin = videoinput_new( config_get_v4l_device( cfg ), 
                            config_get_inputwidth( cfg ), 
                            norm, verbose );
    if( !vidin ) {
        station_delete( stationmgr );
        config_delete( cfg );
        return 1;
    } else {
        videoinput_set_input_num( vidin, config_get_inputnum( cfg ) );
    }

    if( !videoinput_has_tuner( vidin ) ) {
        fprintf( stderr, _("\n"
              "    No tuner found on input %d.  If you have a tuner, please\n"
              "    select a different input using --input=<num>.\n\n"),
                 config_get_inputnum( cfg ) );
        videoinput_delete( vidin );
        station_delete( stationmgr );
        config_delete( cfg );
        return 1;
    }

    /* Scan freqnencies */
    fprintf( stderr, _("Scanning from %6.2f MHz to %6.2f MHz.\n"),
             44.0, 958.0 );
    on = 0;
    fc = 0;
    f1 = 0;
    f2 = 0;
    fi = -1;

    for( f = 44*16; f <= 958*16; f += 4 ) {
        char stationmhz[ 128 ];

        fprintf( stderr, _("Checking %6.2fMHz:"), ((double) f) / 16.0 );
        videoinput_set_tuner_freq( vidin, (f * 1000) / 16 );
        usleep( 200000 ); /* 0.2 sec */
        tuned = videoinput_freq_present( vidin );

        /* state machine */
        if( 0 == on && 0 == tuned ) {
            fprintf( stderr, "  - %-30s\r", _("No signal") );
            continue;
        }
        if( 0 == on && 0 != tuned ) {
            fprintf( stderr, "  + %-30s\r", _("Signal detected") );
            f1 = f;
            /* if( i != chancount ) { fi = i; fc = f; } */
            on = 1;
            continue;
        }
        if( 0 != on && 0 != tuned ) {
            fprintf( stderr, "  * %-30s\r", _("Signal detected") );
            /* if( i != chancount ) { fi = i; fc = f; } */
            continue;
        }
        /* if (on != 0 && 0 == tuned)  --  found one, read name from vbi */
        f2 = f;
        if( 0 == fc ) {
            fc = (f1 + f2) / 2;
            /* Round to the nearest .25MHz */
            fc = ((fc + 2)/4)*4;
        }
        fprintf( stderr, "\r" );
        fprintf( stderr, _("Found a channel at %6.2f MHz (%.2f - %.2f MHz), "
                           "adding to stationlist.\n"),
                 ((double) fc) / 16.0, ((double) f1) / 16.0,
                 ((double) f2) / 16.0 );

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

