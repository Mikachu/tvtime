/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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

struct config_s
{
    parser_file_t pf;

    int outputwidth;
    int inputwidth;
    int verbose;
    int aspect;
    int debug;
    int apply_luma_correction;
    char *v4ldev;
    int inputnum;
    double luma_correction;
    char *norm;
    char *freq;
    int tuner_number;
};


config_t *config_new( const char *filename )
{
    config_t *ct = (config_t *) malloc( sizeof( config_t ) );
    if( !ct ) {
        return 0;
    }

    if( !parser_new( &(ct->pf), filename ) ) {
        free( ct );
        return 0;
    }

    return ct;
}

int config_init( config_t *ct )
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

int config_dump( config_t *ct )
{
    if( !ct ) return 0;

    return parser_dump( &(ct->pf) );
}

int config_get_verbose( config_t *ct )
{
    return ct->verbose;
}

int config_get_debug( config_t *ct )
{
    return ct->debug;
}

int config_get_outputwidth( config_t *ct )
{
    return ct->outputwidth;
}

int config_get_inputwidth( config_t *ct )
{
    return ct->inputwidth;
}

int config_get_aspect( config_t *ct )
{
    return ct->aspect;
}

int config_get_inputnum( config_t *ct )
{
    return ct->inputnum;
}

int config_get_tuner_number( config_t *ct )
{
    return ct->tuner_number;
}

int config_get_apply_luma_correction( config_t *ct )
{
    return ct->apply_luma_correction;
}

double config_get_luma_correction( config_t *ct )
{
    return ct->luma_correction;
}

const char *config_get_v4l_device( config_t *ct )
{
    return ct->v4ldev;
}

const char *config_get_v4l_norm( config_t *ct )
{
    return ct->norm;
}

const char *config_get_v4l_freq( config_t *ct )
{
    return ct->freq;
}

#ifdef TESTHARNESS

int main() {
    config_t *ct;

    ct = config_new( "/home/drbell/.tvtimerc" );
    config_init( ct );
 
}

#endif
