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
#include <ctype.h>
#include "parser.h"
#include "tvtimeconf.h"
#include "input.h"

struct config_s
{
    parser_file_t pf;

    int outputwidth;
    int verbose;
    int aspect;
    int debug;

    int apply_luma_correction;
    double luma_correction;
    int bt8x8_correction;

    int inputwidth;
    int inputnum;
    char *v4ldev;
    char *norm;
    char *freq;
    int tuner_number;
    int *keymap;
    char *timeformat;
};

void config_init( config_t *ct );
void config_init_keymap( config_t *ct );

static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-vasb] [-w <width>] [-I <sampling>] "
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
                     "\t-b\tse bt8x8 correction when applying luma correction.\n"

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
    ct->apply_luma_correction = 0;
    ct->luma_correction = 1.0;
    ct->bt8x8_correction = 0;
    ct->inputnum = 0;
    ct->tuner_number = 0;
    ct->v4ldev = strdup( "/dev/video0" );
    ct->norm = strdup( "ntsc" );
    ct->freq = strdup( "us-cable" );
    ct->timeformat = strdup( "%r" );
    ct->keymap = (int *) malloc( TVTIME_LAST * sizeof( int ) );

    if( !ct->keymap ) {
        fprintf( stderr, "config: Could not aquire memory for keymap.\n" );
        free( ct );
        return 0;
    }

    ct->keymap[ TVTIME_NOCOMMAND ]      = 0;
    ct->keymap[ TVTIME_QUIT ]           = I_ESCAPE;
    ct->keymap[ TVTIME_CHANNEL_UP ]     = I_UP;
    ct->keymap[ TVTIME_CHANNEL_DOWN ]   = I_DOWN;
    ct->keymap[ TVTIME_LUMA_CORRECTION_TOGGLE ] = 'c';
    ct->keymap[ TVTIME_LUMA_UP ]        = 'j';
    ct->keymap[ TVTIME_LUMA_DOWN ]      = 'h';
    ct->keymap[ TVTIME_MIXER_MUTE ]     = 'm';
    ct->keymap[ TVTIME_MIXER_UP ]       = '+';
    ct->keymap[ TVTIME_MIXER_DOWN ]     = '-';
    ct->keymap[ TVTIME_ENTER ]          = I_ENTER;
    ct->keymap[ TVTIME_CHANNEL_CHAR ]   = 0; 
    ct->keymap[ TVTIME_TV_VIDEO ]       = I_F9;
    ct->keymap[ TVTIME_HUE_DOWN ]       = I_F1;
    ct->keymap[ TVTIME_HUE_UP ]         = I_F2;
    ct->keymap[ TVTIME_BRIGHT_DOWN ]    = I_F3; 
    ct->keymap[ TVTIME_BRIGHT_UP ]      = I_F4;
    ct->keymap[ TVTIME_CONT_DOWN ]      = I_F5;
    ct->keymap[ TVTIME_CONT_UP ]        = I_F6;
    ct->keymap[ TVTIME_COLOUR_DOWN ]    = I_F7;
    ct->keymap[ TVTIME_COLOUR_UP ]      = I_F8;
    ct->keymap[ TVTIME_SHOW_BARS ]      = I_F11;
    ct->keymap[ TVTIME_SHOW_TEST ]      = I_F12;
    ct->keymap[ TVTIME_DEBUG ]          = 'd';
    ct->keymap[ TVTIME_FULLSCREEN ]     = 'f';
    ct->keymap[ TVTIME_ASPECT ]         = 'a';
    ct->keymap[ TVTIME_SCREENSHOT ]     = 's';
    ct->keymap[ TVTIME_DEINTERLACINGMODE ] = 't';
    ct->keymap[ TVTIME_MENUMODE ]       = I_HOME;

    if( !configFile ) {
        strncpy( base, getenv( "HOME" ), 245 );
        strcat( base, "/.tvtimerc" );
        configFile = base;
    }

    if( parser_new( &(ct->pf), configFile ) ) {
        config_init( ct );
    }

    while( (c = getopt( argc, argv, "hw:I:avcbs:d:i:l:n:f:t:F:" )) != -1 ) {
        switch( c ) {
        case 'w': ct->outputwidth = atoi( optarg ); break;
        case 'I': ct->inputwidth = atoi( optarg ); break;
        case 'v': ct->verbose = 1; break;
        case 'a': ct->aspect = 1; break;
        case 's': ct->debug = 1; break;
        case 'b': ct->bt8x8_correction = 1; break;
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

    /* Sanity check parameters into reasonable ranges here. */
    if( ct->inputwidth & 1 ) {
        ct->inputwidth -= 1;
        fprintf( stderr, "config: Odd values for input width not allowed, "
                         "using %d instead.\n", ct->inputwidth );
    }

    return ct;
}

void config_delete( config_t *ct )
{
    if( ct->keymap ) free( ct->keymap );
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

    if( (tmp = parser_get( &(ct->pf), "Bt8x8Correction")) ) {
        ct->bt8x8_correction = atoi( tmp );
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

    if( (tmp = parser_get( &(ct->pf), "TimeFormat")) ) {
        free( ct->timeformat );
        ct->timeformat = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "TunerNumber")) ) {
        ct->tuner_number = atoi( tmp );
    }

    config_init_keymap( ct );
}

int string_to_key( const char *str )
{
    int key = 0;
    const char *ptr;

    if( !str ) return 0;

    if( strlen( str ) == 1) return (int)(*str);

    ptr = str;
    while( *ptr ) {
        int number, digits;

        /* skip spaces */
        while( *ptr == ' ' ) ptr++;

        switch( *ptr ) {
        case 'c':
        case 'C':
            if( *++ptr && *ptr == '+') {
                key |= I_CTRL;
            } else {
                key |= *ptr;
            }
            ptr++;
            break;

        case 'm':
        case 'M':
            if( *++ptr && *ptr == '+') {
                key |= I_META;
            } else {
                key |= *ptr;
            }
            ptr++;
            break;

        case 's':
        case 'S':
            if( *++ptr && *ptr == '+') {
                key |= I_SHIFT;
            } else {
                key |= *ptr;
            }
            ptr++;
            break;

        case 'f':
        case 'F':
            ptr++;
            if( *ptr && sscanf( ptr, "%d%n", &number, &digits ) ) {
                if( number > 0 && number < 16 ) {
                    key |= 281 + number;
                    ptr += digits;
                } else {
                    fprintf( stderr, "config: Error parsing keybinding.\n" );
                    return 0;
                }
            } else {
                key |= *ptr;
                ptr++;
            }
            break;

        case '\\':
            ptr++;
            switch( *ptr ) {

            case 'b':
                key |= '\b';
                ptr++;
                break;
                
            case 't':
                key |= '\t';
                ptr++;
                break;

            case 's':
                key |= ' ';
                ptr++;
                break;

            case '0':
                ptr++;
                if( *ptr && sscanf( ptr, "%o%n", &number, &digits) ) {
                    if( digits == 3 && number < 512 ) {
                        key |= number;
                        ptr += digits;
                    } else {
                        fprintf( stderr, "config: Invalid octal keycode.\n" );
                        return 0;
                    }
                } else {
                    fprintf( stderr, "config: Invalid escape sequence.\n" );
                    return 0;
                }
                break;

            default:
                key |= *ptr;
                ptr++;
                break;
            }
            break;

        default:
            key |= *ptr;
            ptr++;
            break;
        }
    }
    return key;
}

void config_init_keymap( config_t *ct )
{
    const char *tmp;
    int key;
    
    if( !ct->keymap ) {
        fprintf( stderr, "config: No keymap. No keybindings.\n" );
        return;
    }

    if( (tmp = parser_get( &(ct->pf), "key_quit")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_QUIT ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_channel_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_CHANNEL_UP ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_channel_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_CHANNEL_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_luma_correction_toggle")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_LUMA_CORRECTION_TOGGLE ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_luma_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_LUMA_UP ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_luma_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_LUMA_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_mixer_mute")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_MIXER_MUTE ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_mixer_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_MIXER_UP ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_mixer_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_MIXER_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_tv_video")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_TV_VIDEO ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_hue_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_HUE_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_hue_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_HUE_UP ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_bright_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_BRIGHT_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_bright_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_BRIGHT_UP ] = key;
    }
 
    if( (tmp = parser_get( &(ct->pf), "key_cont_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_CONT_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_cont_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_CONT_UP ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_colour_down")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_COLOUR_DOWN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_colour_up")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_COLOUR_UP ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_show_bars")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_SHOW_BARS ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_show_test")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_SHOW_TEST ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_debug")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_DEBUG ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_fullscreen")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_FULLSCREEN ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_aspect")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_ASPECT ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_screenshot")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_SCREENSHOT ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_deinterlacing_mode")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_DEINTERLACINGMODE ] = key;
    }

    if( (tmp = parser_get( &(ct->pf), "key_menu_mode")) ) {
        key = string_to_key( tmp );
        ct->keymap[ TVTIME_MENUMODE ] = key;
    }
   
}

int config_key_to_command( config_t *ct, int key )
{
    int i;

    if( !ct || !ct->keymap ) {
        fprintf( stderr, "config: key_to_command: Invalid config obj "
                 "or no keymap.\n" );
        return TVTIME_NOCOMMAND;
    }

    if( !key ) return TVTIME_NOCOMMAND;

    for( i=0; i < TVTIME_LAST; i++ ) {
        if( ct->keymap[i] == key ) return i;
    }

    if( isalnum(key) ) return TVTIME_CHANNEL_CHAR;
    if( key == I_ENTER ) return TVTIME_ENTER;
        
    return TVTIME_NOCOMMAND;
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

void config_set_apply_luma_correction( config_t *ct, int apply_luma_correction )
{
    ct->apply_luma_correction = apply_luma_correction;
}

double config_get_luma_correction( config_t *ct )
{
    return ct->luma_correction;
}

void config_set_luma_correction( config_t *ct, double luma_correction )
{
    ct->luma_correction = luma_correction;
}

int config_get_bt8x8_correction( config_t *ct )
{
    return ct->bt8x8_correction;
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

const char *config_get_timeformat( config_t *ct )
{
    return ct->timeformat;
}

#ifdef TESTHARNESS

int main() {
    config_t *ct;

    ct = config_new( "/home/drbell/.tvtimerc" );
    config_init( ct );
 
}

#endif
