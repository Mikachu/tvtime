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
#include <fcntl.h>
#include <unistd.h>
#include "commands.h"
#include "utils.h"

int main( int argc, char **argv )
{
    config_t *cfg = config_new();
    FILE *fifo;
    int fi, on, tuned, i;
    int f, f1, f2, fc;

    if( !cfg ) {
        fprintf( stderr, "tvtime-command: Can't initialize tvtime configuration, exiting.\n" );
        return 1;
    }

    for( f = 44*16; f <= 958*16; f+= 4 ) {

        /* Scan freqnencies */
        fprintf( stderr, "\nscanning freqencies...\n" );
        on = 0;
        fc = 0;
        f1 = 0;
        f2 = 0;
        fi = -1;

        for( f = 44*16; f <= 958*16; f += 4 ) {
/*
            for( i = 0; i < chancount; i++ ) {
                if( chanlist[ i ].freq * 16 == f * 1000 ) {
                    break;
                }
            }
            fprintf( stderr,"?? %6.2f MHz (%-4s): ", f / 16.0,
                     (i == chancount) ? "-" : chanlist[i].name );
            drv->setfreq( h_drv, f );
*/
            usleep( 200000 ); /* 0.2 sec */
            // tuned = drv->is_tuned( h_drv);
            tuned = 1;

            /* state machine */
            if( 0 == on && 0 == tuned ) {
                fprintf( stderr, "|   no\n" );
                continue;
            }
            if( 0 == on && 0 != tuned ) {
                fprintf( stderr, " \\  raise\n" );
                f1 = f;
/*
                if( i != chancount ) {
                    fi = i;
                    fc = f;
                }
*/
                on = 1;
                continue;
            }
            if( 0 != on && 0 != tuned ) {
                fprintf( stderr, "  | yes\n" );
/*
                if( i != chancount ) {
                    fi = i;
                    fc = f;
                }
*/
                continue;
            }
            /* if (on != 0 && 0 == tuned)  --  found one, read name from vbi */
            fprintf( stderr," /  fall\n" );
            f2 = f;
            if( 0 == fc ) {
                fc = (f1+f2)/2;
            }

/*
            fprintf( stderr, "=> %6.2f MHz (%-4s): ", fc/16.0,
                     (-1 != fi) ? chanlist[fi].name : "-" );
            drv->setfreq( h_drv, fc );
*/
           
/* 
            // name = get_vbi_name( vbi );
            fprintf(stderr,"%s\n",name ? name : "???");
            if (NULL == name) {
                sprintf(dummy,"unknown (%s)",chanlist[fi].name);
                name = dummy;
            }
            if (-1 != fi) {
                if (NULL == name) {
                    sprintf(dummy,"unknown (%s)",chanlist[fi].name);
                    name = dummy;
                }
                fprintf(conf,"[%s]\nchannel = %s\n\n",name,chanlist[fi].name);
            } else {
                if (NULL == name) {
                    sprintf(dummy,"unknown (%.3f)", fc/16.0);
                    name = dummy;
                }
                fprintf(conf,"[%s]\nfreq = %.3f\n\n", name, fc/16.0);
            }
            fflush(conf);
*/

            on = 0;
            fc = 0;
            f1 = 0;
            f2 = 0;
            fi = -1;
        }
    }

    fclose( fifo );
    config_delete( cfg );
    return 0;
}

