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


#define MAX_KEYSYMS 350
#define MAX_CMD_NAMELEN 64
#define MAX_BUTTONS 10

typedef struct {
    char name[MAX_CMD_NAMELEN];
    int command;
} Cmd_Names;

static Cmd_Names cmd_table[] = {

    { "QUIT", TVTIME_QUIT },
    { "CHANNEL_UP", TVTIME_CHANNEL_UP },
    { "CHANNEL_DOWN", TVTIME_CHANNEL_DOWN },
    { "LUMA_CORRECTION_TOGGLE", TVTIME_LUMA_CORRECTION_TOGGLE },
    { "LUMA_UP", TVTIME_LUMA_UP },
    { "LUMA_DOWN", TVTIME_LUMA_DOWN },
    { "MIXER_MUTE", TVTIME_MIXER_MUTE },
    { "MIXER_UP", TVTIME_MIXER_UP },
    { "MIXER_DOWN", TVTIME_MIXER_DOWN },
    { "TV_VIDEO", TVTIME_TV_VIDEO },
    { "HUE_DOWN", TVTIME_HUE_DOWN },
    { "HUE_UP", TVTIME_HUE_UP },
    { "BRIGHT_DOWN", TVTIME_BRIGHT_DOWN },
    { "BRIGHT_UP", TVTIME_BRIGHT_UP },
    { "CONT_DOWN", TVTIME_CONT_DOWN },
    { "CONT_UP", TVTIME_CONT_UP },
    { "COLOUR_DOWN", TVTIME_COLOUR_DOWN },
    { "COLOUR_UP", TVTIME_COLOUR_UP },

    { "FINETUNE_DOWN", TVTIME_FINETUNE_DOWN },
    { "FINETUNE_UP", TVTIME_FINETUNE_UP },

    { "SHOW_BARS", TVTIME_SHOW_BARS },
    { "DEBUG", TVTIME_DEBUG },

    { "FULLSCREEN", TVTIME_FULLSCREEN },
    { "ASPECT", TVTIME_ASPECT },
    { "SCREENSHOT", TVTIME_SCREENSHOT },
    { "DEINTERLACING_MODE", TVTIME_DEINTERLACINGMODE },

    { "MENUMODE", TVTIME_MENUMODE },
    { "DISPLAY_INFO", TVTIME_DISPLAY_INFO }
};


#define NUM_CMDS (sizeof(cmd_table)/sizeof(Cmd_Names))


struct config_s
{
    parser_file_t pf;

    int outputwidth;
    int verbose;
    int aspect;
    int debug;
    int finetune;

    int apply_luma_correction;
    double luma_correction;

    int inputwidth;
    int inputnum;
    char *v4ldev;
    char *norm;
    char *freq;
    int *keymap;
    char *timeformat;
    int *buttonmap;
};

void config_init( config_t *ct );
void config_init_keymap( config_t *ct );
void config_init_buttonmap( config_t *ct );

static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-vasb] [-w <width>] [-I <sampling>] "
                     "[-d <device>]\n\t\t[-i <input>] [-n <norm>] "
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

                     "\t-n\tThe mode to set the tuner to: PAL, NTSC, SECAM, PAL-NC,\n"
                     "\t  \tPAL-M, PAL-N or NTSC-JP (defaults to NTSC).\n"
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
                     "\t  \t\taustralia-optus\n\n",
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
    ct->inputnum = 0;
    ct->v4ldev = strdup( "/dev/video0" );
    ct->norm = strdup( "ntsc" );
    ct->freq = strdup( "us-cable" );
    ct->timeformat = strdup( "%r" );
    ct->finetune = 0;
    ct->keymap = (int *) malloc( 8*MAX_KEYSYMS * sizeof( int ) );

    if( !ct->keymap ) {
        fprintf( stderr, "config: Could not aquire memory for keymap.\n" );
        free( ct );
        return 0;
    }

    ct->buttonmap = (int *) malloc( MAX_BUTTONS * sizeof( int ) );
    if( !ct->buttonmap ) {
        fprintf( stderr, "config: Could not aquire memory for buttonmap.\n" );
        free( ct->keymap );
        free( ct );
        return 0;
    }

    memset( ct->keymap, 0, 8*MAX_KEYSYMS * sizeof( int ) );
    ct->keymap[ 0 ] = TVTIME_NOCOMMAND;
    ct->keymap[ I_ESCAPE ] = TVTIME_QUIT;
    ct->keymap[ 'q' ] = TVTIME_QUIT;
    ct->keymap[ I_UP ] = TVTIME_CHANNEL_UP;
    ct->keymap[ I_DOWN ] = TVTIME_CHANNEL_DOWN;
    ct->keymap[ 'c' ] = TVTIME_LUMA_CORRECTION_TOGGLE;
    ct->keymap[ 'j' ] = TVTIME_LUMA_UP;
    ct->keymap[ 'h' ] = TVTIME_LUMA_DOWN;
    ct->keymap[ 'm' ] = TVTIME_MIXER_MUTE;
    ct->keymap[ '+' ] = TVTIME_MIXER_UP;
    ct->keymap[ '-' ] = TVTIME_MIXER_DOWN;
    ct->keymap[ I_ENTER ] = TVTIME_ENTER;
    ct->keymap[ I_F9 ] = TVTIME_TV_VIDEO;
    ct->keymap[ I_F1 ] = TVTIME_HUE_DOWN;
    ct->keymap[ I_F2 ] = TVTIME_HUE_UP;
    ct->keymap[ I_F3 ] = TVTIME_BRIGHT_DOWN;
    ct->keymap[ I_F4 ] = TVTIME_BRIGHT_UP;
    ct->keymap[ I_F5 ] = TVTIME_CONT_DOWN;
    ct->keymap[ I_F6 ] = TVTIME_CONT_UP;
    ct->keymap[ I_F7 ] = TVTIME_COLOUR_DOWN;
    ct->keymap[ I_F8 ] = TVTIME_COLOUR_UP;
    ct->keymap[ I_F11 ] = TVTIME_SHOW_BARS;
    ct->keymap[ 'd' ] = TVTIME_DEBUG;
    ct->keymap[ 'f' ] = TVTIME_FULLSCREEN;
    ct->keymap[ 'a' ] = TVTIME_ASPECT;
    ct->keymap[ 's' ] = TVTIME_SCREENSHOT;
    ct->keymap[ 't' ] = TVTIME_DEINTERLACINGMODE;
    ct->keymap[ I_HOME ] = TVTIME_MENUMODE ;
    ct->keymap[ ',' ] = TVTIME_FINETUNE_DOWN;
    ct->keymap[ '.' ] = TVTIME_FINETUNE_UP;

    memset( ct->buttonmap, 0, MAX_BUTTONS * sizeof(int) );
    ct->buttonmap[ 1 ] = TVTIME_DISPLAY_INFO;
    ct->buttonmap[ 2 ] = TVTIME_MIXER_MUTE;
    ct->buttonmap[ 3 ] = TVTIME_TV_VIDEO;
    ct->buttonmap[ 4 ] = TVTIME_CHANNEL_UP;
    ct->buttonmap[ 5 ] = TVTIME_CHANNEL_DOWN;

    if( !configFile ) {
        strncpy( base, getenv( "HOME" ), 245 );
        strcat( base, "/.tvtimerc" );
        configFile = base;
    }

    if( parser_new( &(ct->pf), configFile ) ) {
        config_init( ct );
    }

    while( (c = getopt( argc, argv, "hw:I:avcsd:i:l:n:f:t:F:" )) != -1 ) {
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
        case 'F': configFile = strdup( optarg ); break;
        default:
            print_usage( argv );
            return 0;
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
    if( ct->buttonmap ) free( ct->buttonmap );
}

void config_init( config_t *ct )
{
    const char *tmp;

    if( !ct ) {
        fprintf( stderr, "config: NULL received as config structure.\n" );
        return;
    }

    if( (tmp = parser_get( &(ct->pf), "OutputWidth", 1 )) ) {
        ct->outputwidth = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "InputWidth", 1 )) ) {
        ct->inputwidth = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Verbose", 1 )) ) {
        ct->verbose = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Widescreen", 1 )) ) {
        ct->aspect = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "DebugMode", 1 )) ) {
        ct->debug = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "ApplyLumaCorrection", 1 )) ) {
        ct->apply_luma_correction = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "LumaCorrection", 1 )) ) {
        ct->luma_correction = atof( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "V4LDevice", 1 )) ) {
        free( ct->v4ldev );
        ct->v4ldev = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "CaptureSource", 1 )) ) {
        ct->inputnum = atoi( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Norm", 1 )) ) {
        free( ct->norm );
        ct->norm = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "Frequencies", 1 )) ) {
        free( ct->freq );
        ct->freq = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "TimeFormat", 1 )) ) {
        free( ct->timeformat );
        ct->timeformat = strdup( tmp );
    }

    if( (tmp = parser_get( &(ct->pf), "FineTuneOffset", 1 )) ) {
        ct->finetune = atoi( tmp );
    }

    config_init_keymap( ct );
    config_init_buttonmap( ct );
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
    int key, cmd=0, i=1;
    
    if( !ct->keymap ) {
        fprintf( stderr, "config: No keymap. No keybindings.\n" );
        return;
    }

    for( cmd=0; cmd < NUM_CMDS; cmd++ ) {
        char keystr[ 5+MAX_CMD_NAMELEN ];
        sprintf( keystr, "key_%s", cmd_table[cmd].name );

        for(i=1;;i++)
            if( (tmp = parser_get( &(ct->pf), keystr, i )) ) {
                key = string_to_key( tmp );
                ct->keymap[ MAX_KEYSYMS*(key & 0x70000) + (key & 0x1ff) ] = cmd_table[cmd].command;
            } else { break; }
    }   
}

int config_key_to_command( config_t *ct, int key )
{
    if( !ct || !ct->keymap ) {
        fprintf( stderr, "config: key_to_command: Invalid config obj "
                 "or no keymap.\n" );
        return TVTIME_NOCOMMAND;
    }

    if( !key ) return TVTIME_NOCOMMAND;

    if( ct->keymap[ MAX_KEYSYMS*(key & 0x70000) + (key & 0x1ff) ] ) 
        return ct->keymap[ MAX_KEYSYMS*(key & 0x70000) + (key & 0x1ff) ];

    if( isalnum(key) ) return TVTIME_CHANNEL_CHAR;
        
    return TVTIME_NOCOMMAND;
}

int string_to_command( const char *str )
{
    int i=0;

    while( i < NUM_CMDS ) {
        if( !strcasecmp( cmd_table[i].name, str ) ) {
            return cmd_table[i].command;
        }
        i++;
    }
    return -1;
}

void config_init_buttonmap( config_t *ct )
{
    int button=0, cmd=0;
    const char *tmp;

    for( button=0; button < MAX_BUTTONS; button++ ) {
        char butstr[ 14+MAX_CMD_NAMELEN ];
        sprintf( butstr, "mouse_button_%d", button );

        if( (tmp = parser_get( &(ct->pf), butstr, 1 )) ) {
            cmd = string_to_command( tmp );
            if( cmd == -1 ) {
                fprintf( stderr, 
                         "config_init_buttonmap: %s is not a valid command.\n", 
                         tmp );
                continue;
            }

            ct->buttonmap[ button ] = cmd;
        }
    }

}

int config_button_to_command( config_t *ct, int button )
{
    if( !ct || !ct->buttonmap ) return 0;
    if( button < 0 || button >= MAX_BUTTONS ) return 0;
    return ct->buttonmap[button];
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

int config_get_finetune( config_t *ct )
{
    return ct->finetune;
}

#ifdef TESTHARNESS

int main() {
    config_t *ct;

    ct = config_new( "/home/drbell/.tvtimerc" );
    config_init( ct );
 
}

#endif
