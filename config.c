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
#include <getopt.h>
#include "parser.h"
#include "config.h"

struct config_s
{
    parser_file_t pf;

    int outputwidth;
    int verbose;
    int aspect;
    int debug;

    int apply_luma_correction;
    double luma_correction;

    int inputwidth;
    int inputnum;
    char *v4ldev;
    char *norm;
    char *freq;
    int tuner_number;
};

void config_init( config_t *ct );

static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-vas] [-w <width>] [-I <sampling>] "
                     "[-d <device>] [-i <input>] [-n <norm>] "
                     "[-f <frequencies>] [-t <tuner>]\n"
                     "\t-v\tShow verbose messages.\n"
                     "\t-a\t16:9 mode.\n"
                     "\t-s\tPrint frame skip information (for debugging).\n"
                     "\t-I\tV4L input scanline sampling, defaults to 720.\n"
                     "\t-w\tOutput window width, defaults to 800.\n"
                     "\t-d\tvideo4linux device (defaults to /dev/video0).\n"
                     "\t-i\tvideo4linux input number (defaults to 0).\n"
                     "\t-c\tApply luma correction.\n"
                     "\t-l\tLuma correction value (defaults to 1.0, use of this implies -c).\n"
                     "\t-n\tThe mode to set the tuner to: PAL, NTSC or SECAM.\n"
                     "\t  \t(defaults to NTSC)\n"
                     "\t-f\tThe channels you are receiving with the tuner\n"
                     "\t  \t(defaults to us-cable).\n"
                     "\t  \tValid values are:\n"
                     "\t  \t\tus-bcast\n"
                     "\t  \t\tus-cable\n"
                     "\t  \t\tus-cable-hrc\n"
                     "\t  \t\tjapan-bcast\n"
                     "\t  \t\tjapan-cable\n"
                     "\t  \t\teurope-west\n"
                     "\t  \t\teurope-east\n"
                     "\t  \t\titaly\n"
                     "\t  \t\tnewzealand\n"
                     "\t  \t\taustralia\n"
                     "\t  \t\tireland\n"
                     "\t  \t\tfrance\n"
                     "\t  \t\tchina-bcast\n"
                     "\t  \t\tsouthafrica\n"
                     "\t  \t\targentina\n"
                     "\t  \t\tcanada-cable\n"
                     "\t  \t\taustralia-optus\n"
                     "\t-t\tThe tuner number for this input (defaults to 0).\n"
                     "\n\tSee the README for more details.\n",
                     argv[ 0 ] );
}


config_t *config_new( int argc, char **argv )
{
    char c, *configFile = NULL, base[255];

    config_t *ct = (config_t *) malloc( sizeof( config_t ) );
    if( !ct ) {
        return 0;
    }

    ct->outputwidth = 800;
    ct->inputwidth = 720;
    ct->verbose = 0;
    ct->aspect = 0;
    ct->debug = 0;
    ct->apply_luma_correction = 1;
    ct->luma_correction = 1.0;
    ct->inputnum = 0;
    ct->tuner_number = 0;
    ct->v4ldev = strdup("/dev/video0");
    ct->norm = strdup("ntsc");
    ct->freq = strdup("us-cable");

    if( !configFile ) {
        strncpy( base, getenv("HOME"), 245 );
        strcat( base, "/.tvtime" );
        configFile = base;
    }

    if( !parser_new( &(ct->pf), configFile ) ) {
        fprintf( stderr, "config: Could not read configuration from %s\n", 
                 configFile );
    } else {
        config_init( ct );
    }

    while( (c = getopt( argc, argv, "hw:I:avcs:d:i:l:n:f:t:F:" )) != -1 ) {
        switch( c ) {
        case 'w': ct->outputwidth = atoi( optarg ); break;
        case 'I': ct->inputwidth = atoi( optarg ); break;
        case 'v': ct->verbose = 1; break;
        case 'a': ct->aspect = 1; break;
        case 's': ct->debug = 1; break;
        case 'c': ct->apply_luma_correction = 1; break;
        case 'd': ct->v4ldev = strdup( optarg ); break;
        case 'i': ct->inputnum = atoi( optarg ); break;
        case 'l': ct->luma_correction = atof( optarg );
                  ct->apply_luma_correction = 1; break;
        case 'n': ct->norm = strdup( optarg ); break;
        case 'f': ct->freq = strdup( optarg ); break;
        case 't': ct->tuner_number = atoi( optarg ); break;
        case 'F': configFile = strdup( optarg ); break;
        default:
            print_usage( argv );
            return NULL;
        }
    }

    if( configFile != base ) {
        parser_delete( &(ct->pf) );
        
        if( !parser_new( &(ct->pf), configFile ) ) {
            fprintf( stderr, "config: Could not read configuration from %s\n", 
                     configFile );
        } else {
            config_init( ct );
        }
    }

    if( configFile && configFile != base ) free( configFile );

    return ct;
}

void config_init( config_t *ct )
{
    const char *tmp;

    if( !ct ) {
        fprintf( stderr, "config: NULL received as config structure.\n" );
        return;
    }

    if( (tmp = parser_get( &(ct->pf), "OutputWidth")) ) {
        ct->outputwidth = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "InputWidth")) ) {
        ct->inputwidth = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Verbose")) ) {
        ct->verbose = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Widescreen")) ) {
        ct->aspect = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "DebugMode")) ) {
        ct->debug = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "ApplyLumaCorrection")) ) {
        ct->apply_luma_correction = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "LumaCorrection")) ) {
        ct->luma_correction = atof( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "V4LDevice")) ) {
        free( ct->v4ldev );
        ct->v4ldev = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "CaptureSource")) ) {
        ct->inputnum = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Norm")) ) {
        free( ct->norm );
        ct->norm = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Frequencies")) ) {
        free( ct->freq );
        ct->freq = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "TunerNumber")) ) {
        ct->tuner_number = atoi( tmp );
    }

    config_dump( ct );
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

void config_set_luma_correction( config_t *ct, double luma_correction )
{
    ct->luma_correction = luma_correction;
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

    ct = config_new( "/home/drbell/.tvtime" );
    config_init( ct );
 
}

#endif
