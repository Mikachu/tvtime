/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 *
 * Mixer routines stolen from mplayer, http://mplayer.sourceforge.net.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parser.h"
#include "config.h"

int config_new( struct config_t *ct, const char *filename )
{
    if( !parser_new( &(ct->pf), filename ) ) {
        return 0;
    }

    return 1;
}

int config_init( struct config_t *ct )
{
    char *tmp;

    if( !ct ) {
        fprintf( stderr, "config: NULL received as config structure.\n" );
        return 0;
    }

    if( parser_get( &(ct->pf), "OutputWidth", "800", &tmp) ) {
        ct->outputwidth = atoi( tmp );
        free( tmp );
    } else {
        ct->outputwidth = 800;
    }
    
    if( parser_get( &(ct->pf), "InputWidth", "720", &tmp) ) {
        ct->inputwidth = atoi( tmp );
        free( tmp );
    } else {
        ct->inputwidth = 720;
    }

    if( parser_get( &(ct->pf), "Verbose", "0", &tmp) ) {
        ct->verbose = atoi( tmp );
        free( tmp );
    } else {
        ct->verbose = 0;
    }

    if( parser_get( &(ct->pf), "Widescreen", "0", &tmp) ) {
        ct->aspect = atoi( tmp );
        free( tmp );
    } else {
        ct->aspect = 0;
    }

    if( parser_get( &(ct->pf), "DebugMode", "0", &tmp) ) {
        ct->debug = atoi( tmp );
        free( tmp );
    } else {
        ct->debug = 0;
    }

    if( parser_get( &(ct->pf), "ApplyLumaCorrection", "1", &tmp) ) {
        ct->apply_luma_correction = atoi( tmp );
        free( tmp );
    } else {
        ct->apply_luma_correction = 1;
    }

    if( parser_get( &(ct->pf), "LumaCorrection", "1.0", &tmp) ) {
        ct->luma_correction = atof( tmp );
        free( tmp );
    } else {
        ct->luma_correction = 1.0;
    }

    if( parser_get( &(ct->pf), "Output420", "0", &tmp) ) {
        ct->output420 = atoi( tmp );
        free( tmp );
    } else {
        ct->output420 = 0;
    }

    if( !parser_get( &(ct->pf), "V4LDevice", "/dev/video0", &(ct->v4ldev)) ) {
        ct->v4ldev = strdup("/dev/video0");
        if( !ct->v4ldev ) fprintf( stderr, "config: Error setting v4ldev.\n" );
    }

    if( parser_get( &(ct->pf), "CaptureSource", "0", &tmp) ) {
        ct->inputnum = atoi( tmp );
        free( tmp );
    } else {
        ct->inputnum = 0;
    }

    if( !parser_get( &(ct->pf), "Norm", "ntsc", &(ct->norm)) ) {
        ct->norm = strdup("ntsc");
        if( !ct->norm ) fprintf( stderr, "config: Error setting norm.\n" );
    }

    if( !parser_get( &(ct->pf), "Frequencies", "us-cable", &(ct->freq)) ) {
        ct->freq = strdup("us-cable");
        if( !ct->freq ) fprintf( stderr, "config: Error setting freq.\n" );
    }

    if( parser_get( &(ct->pf), "TunerNumber", "0", &tmp) ) {
        ct->tuner_number = atoi( tmp );
        free( tmp );
    } else {
        ct->tuner_number = 0;
    }

    config_dump( ct );

    return 1;
}

int config_dump( struct config_t *ct )
{
    if( !ct ) return 0;

    return parser_dump( &(ct->pf) );
}
