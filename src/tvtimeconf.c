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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <langinfo.h>
#include "videotools.h"
#include "parser.h"
#include "tvtimeconf.h"
#include "input.h"
#include "station.h"

#define MAX_KEYSYMS 350
#define MAX_BUTTONS 10

struct config_s
{
    int outputwidth;
    int verbose;
    int aspect;
    int debug;
    int finetune;
    int fullscreen;
    int priority;

    int ntsc_mode;

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
    unsigned int menu_bg_rgb;
    unsigned int channel_text_rgb;
    unsigned int other_text_rgb;
    char command_pipe[ 256 ];

    int preferred_deinterlace_method;
    int check_freq_present;

    int use_vbi;
    char *vbidev;

    int start_channel;

    double hoverscan;
    double voverscan;

    char config_filename[ 1024 ];
};

static int string_to_key( const char *str )
{
    int key = 0;
    const char *ptr;

    if( !str ) return 0;

    if( strlen( str ) == 1) return (int)(*str);

    ptr = str;
    while( *ptr ) {
        unsigned int onumber;
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
                if( *ptr && sscanf( ptr, "%o%n", &onumber, &digits) ) {
                    if( digits == 3 && onumber < 512 ) {
                        key |= onumber;
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

static unsigned int parse_colour( const char *str )
{
    unsigned int a,r,g,b;
    int ret;
    
    if( !str || !*str ) return 0;

    if( strlen( str ) == 1 ) return (unsigned int)atoi( str );

    if( str[0] == '0' && str[1] == 'x' ) {
        ret = sscanf( str, "0x%2x%2x%2x%2x", &a, &r, &g, &b );
    } else {
        ret = sscanf( str, "%u %u %u %u", &a, &r, &g, &b );
    }
    switch( ret ) {
    case 0:
        return 0;
        break;
    case 1:
        return a;
        break;
    case 2:
        return 0xff000000 | ( (a & 0xff) << 8 ) | (r & 0xff);
        break;
    case 3:
        return 0xff000000 | ( (a & 0xff) << 16 ) | ( (r & 0xff) << 8 ) | ( g & 0xff);
        break;
    case 4:
        return ( (a & 0xff) << 24 ) | ( (r & 0xff) << 16 ) | ( ( g & 0xff) << 8 ) | (b & 0xff);
    }

    return 0;
}

static void config_init_keymap( config_t *ct, parser_file_t *pf )
{
    const char *tmp;
    int key, cmd=0, i=1;
    
    if( !ct->keymap ) {
        fprintf( stderr, "config: No keymap. No keybindings.\n" );
        return;
    }

    for( cmd=0; cmd < tvtime_num_commands(); cmd++ ) {
        char keystr[ 5+MAX_CMD_NAMELEN ];
        sprintf( keystr, "key_%s", tvtime_get_command( cmd ) );

        for(i=1;;i++)
            if( (tmp = parser_get( pf, keystr, i )) ) {
                key = string_to_key( tmp );
                ct->keymap[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ] = tvtime_get_command_id( cmd );
            } else { break; }
    }   
}

static void config_init_buttonmap( config_t *ct, parser_file_t *pf )
{
    int button=0, cmd=0;
    const char *tmp;

    for( button=0; button < MAX_BUTTONS; button++ ) {
        char butstr[ 14+MAX_CMD_NAMELEN ];
        sprintf( butstr, "mouse_button_%d", button );

        if( (tmp = parser_get( pf, butstr, 1 )) ) {
            cmd = tvtime_string_to_command( tmp );
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

static void config_init( config_t *ct, parser_file_t *pf )
{
    const char *tmp;

    if( (tmp = parser_get( pf, "OutputWidth", 1 )) ) {
        ct->outputwidth = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "InputWidth", 1 )) ) {
        ct->inputwidth = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "Verbose", 1 )) ) {
        ct->verbose = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "Widescreen", 1 )) ) {
        ct->aspect = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "DebugMode", 1 )) ) {
        ct->debug = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "ApplyLumaCorrection", 1 )) ) {
        ct->apply_luma_correction = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "LumaCorrection", 1 )) ) {
        ct->luma_correction = atof( tmp );
    }

    if( (tmp = parser_get( pf, "V4LDevice", 1 )) ) {
        free( ct->v4ldev );
        ct->v4ldev = strdup( tmp );
    }

    if( (tmp = parser_get( pf, "VBIDevice", 1 )) ) {
        free( ct->vbidev );
        ct->vbidev = strdup( tmp );
    }

    if( (tmp = parser_get( pf, "CaptureSource", 1 )) ) {
        ct->inputnum = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "UseVBI", 1 )) ) {
        ct->use_vbi = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "ProcessPriority", 1 )) ) {
        ct->priority = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "Fullscreen", 1 )) ) {
        ct->fullscreen = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "Norm", 1 )) ) {
        free( ct->norm );
        ct->norm = strdup( tmp );
    }

    if( (tmp = parser_get( pf, "Frequencies", 1 )) ) {
        free( ct->freq );
        ct->freq = strdup( tmp );
    }

    if( (tmp = parser_get( pf, "CommandPipe", 1 )) ) {
        if( tmp[ 0 ] == '~' && getenv( "HOME" ) ) {
            snprintf( ct->command_pipe, sizeof( ct->command_pipe ), "%s%s", getenv( "HOME" ), tmp + 1 );
        } else {
            strncpy( ct->command_pipe, tmp, 255 );
        }
    }

    if( (tmp = parser_get( pf, "TimeFormat", 1 )) ) {
        free( ct->timeformat );
        ct->timeformat = strdup( tmp );
    }

    if( (tmp = parser_get( pf, "MenuBG", 1 )) ) {
        ct->menu_bg_rgb = parse_colour( tmp );
    }

    if( (tmp = parser_get( pf, "ChannelTextFG", 1 )) ) {
        ct->channel_text_rgb = parse_colour( tmp );
    }

    if( (tmp = parser_get( pf, "OtherTextFG", 1 )) ) {
        ct->other_text_rgb = parse_colour( tmp );
    }

    if( (tmp = parser_get( pf, "StartChannel", 1 )) ) {
        ct->start_channel = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "FinetuneOffset", 1 )) ) {
        ct->finetune = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "NTSCCableMode", 1 )) ) {
        if( !strcasecmp( tmp, "IRC" ) ) {
            ct->ntsc_mode = NTSC_CABLE_MODE_IRC;
        } else if( !strcasecmp( tmp, "HRC" ) ) {
            ct->ntsc_mode = NTSC_CABLE_MODE_HRC;
        } else {
            ct->ntsc_mode = NTSC_CABLE_MODE_NOMINAL;
        }
    }

    if( (tmp = parser_get( pf, "PreferredDeinterlaceMethod", 1 )) ) {
        ct->preferred_deinterlace_method = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "CheckForSignal", 1 )) ) {
        ct->check_freq_present = atoi( tmp );
    }

    if( (tmp = parser_get( pf, "Overscan", 1 )) ) {
        ct->hoverscan = ( atof( tmp ) / 2.0 ) / 100.0;
        ct->voverscan = ( atof( tmp ) / 2.0 ) / 100.0;
    }

    config_init_keymap( ct, pf );
    config_init_buttonmap( ct, pf );
}

static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-vamsb] [-w <width>] [-I <sampling>] "
                     "[-d <device>]\n\t\t[-i <input>] [-n <norm>] "
                     "[-f <frequencies>] [-t <tuner>] "
                     "[-D <deinterlace method>]\n", argv[ 0 ] );
    fprintf( stderr, "\t-v\tShow verbose messages.\n" );
    fprintf( stderr, "\t-a\t16:9 mode.\n" );
    fprintf( stderr, "\t-s\tPrint frame skip information (for debugging).\n" );
    fprintf( stderr, "\t-I\tV4L input scanline sampling, defaults to 720.\n" );
    fprintf( stderr, "\t-w\tOutput window width, defaults to 800.\n" );

    fprintf( stderr, "\t-d\tvideo4linux device (defaults to /dev/video0).\n" );
    fprintf( stderr, "\t-i\tvideo4linux input number (defaults to 0).\n" );

    fprintf( stderr, "\t-m\tStart tvtime in fullscreen mode.\n" );

    fprintf( stderr, "\t-c\tApply luma correction.\n" );
    fprintf( stderr, "\t-l\tLuma correction value (defaults to 1.0, use of this implies -c).\n" );

    fprintf( stderr, "\t-n\tThe mode to set the tuner to: PAL, NTSC, SECAM, PAL-NC,\n"
                     "\t  \tPAL-M, PAL-N or NTSC-JP (defaults to NTSC).\n" );
    fprintf( stderr, "\t-f\tThe channels you are receiving with the tuner\n"
                     "\t  \t(defaults to us-cable).\n"
                     "\t  \tValid values are:\n"
                     "\t  \t\tus-cable\n"
                     "\t  \t\tus-broadcast\n"
                     "\t  \t\tjapan-cable\n"
                     "\t  \t\tjapan-broadcast\n"
                     "\t  \t\teurope\n"
                     "\t  \t\taustralia\n"
                     "\t  \t\tnewzealand\n"
                     "\t  \t\tfrance\n"
                     "\t  \t\trussia\n" );
    fprintf( stderr, "\t-D\tThe deinterlace method tvtime will use on startup\n"
                     "\t  \t(defaults to 0)\n");
}

/**
 * This should be moved elsewhere.
 */
static int file_is_openable_for_read( const char *filename )
{
    int fd;
    fd = open( filename, O_RDONLY );
    if( fd < 0 ) {
        return 0;
    } else {
        close( fd );
        return 1;
    }
}

config_t *config_new( int argc, char **argv )
{
    parser_file_t pf;
    char temp_dirname[ 1024 ];
    char base[ 256 ];
    char *configFile = 0;
    char c;

    config_t *ct = (config_t *) malloc( sizeof( config_t ) );
    if( !ct ) {
        return 0;
    }

    ct->outputwidth = 800;
    ct->inputwidth = 720;
    ct->verbose = 0;
    ct->aspect = 0;
    ct->debug = 0;
    ct->ntsc_mode = 0;
    ct->priority = -19;
    ct->apply_luma_correction = 0;
    ct->luma_correction = 1.0;
    ct->inputnum = 0;
    ct->v4ldev = strdup( "/dev/video0" );
    ct->vbidev = strdup( "/dev/vbi0" );
    ct->norm = strdup( "ntsc" );
    ct->freq = strdup( "us-cable" );
    strncpy( ct->command_pipe, getenv( "HOME" ), 235 );
    strncat( ct->command_pipe, "/.tvtime/tvtimefifo", 255 );
    if( strlen( nl_langinfo( T_FMT ) ) ) {
        ct->timeformat = strdup( nl_langinfo( T_FMT ) );
    } else {
        ct->timeformat = strdup( "%r" );
    }
    ct->finetune = 0;
    ct->fullscreen = 0;
    ct->menu_bg_rgb = 4278190080U; /* opaque black */
    ct->channel_text_rgb = 4294967040U; /* opaque yellow */
    ct->other_text_rgb = 4294303411U; /* opaque wheat */
    ct->keymap = (int *) malloc( 8*MAX_KEYSYMS * sizeof( int ) );
    ct->preferred_deinterlace_method = 0;
    ct->check_freq_present = 1;
    ct->use_vbi = 0;
    ct->start_channel = 1;
    ct->hoverscan = 0.0;
    ct->voverscan = 0.0;

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
    ct->keymap[ I_LEFT ] = TVTIME_FINETUNE_DOWN;
    ct->keymap[ I_RIGHT ] = TVTIME_FINETUNE_UP;
    ct->keymap[ 8 ] = TVTIME_CHANNEL_PREV; // default to backspace
    ct->keymap[ 'k' ] = TVTIME_CHANNEL_UP;
    ct->keymap[ 'j' ] = TVTIME_CHANNEL_DOWN;
    ct->keymap[ 'h' ] = TVTIME_FINETUNE_DOWN;
    ct->keymap[ 'l' ] = TVTIME_FINETUNE_UP;
    ct->keymap[ '[' ] = TVTIME_FREQLIST_DOWN;
    ct->keymap[ ']' ] = TVTIME_FREQLIST_UP;
    ct->keymap[ 'c' ] = TVTIME_TOGGLE_LUMA_CORRECTION;
    ct->keymap[ 'z' ] = TVTIME_LUMA_DOWN;
    ct->keymap[ 'x' ] = TVTIME_LUMA_UP;
    ct->keymap[ 'm' ] = TVTIME_MIXER_MUTE;
    ct->keymap[ '-' ] = TVTIME_MIXER_DOWN;
    ct->keymap[ '+' ] = TVTIME_MIXER_UP;
    ct->keymap[ I_ENTER ] = TVTIME_ENTER;
    ct->keymap[ I_F1 ] = TVTIME_HUE_DOWN;
    ct->keymap[ I_F2 ] = TVTIME_HUE_UP;
    ct->keymap[ I_F3 ] = TVTIME_BRIGHT_DOWN;
    ct->keymap[ I_F4 ] = TVTIME_BRIGHT_UP;
    ct->keymap[ I_F5 ] = TVTIME_CONT_DOWN;
    ct->keymap[ I_F6 ] = TVTIME_CONT_UP;
    ct->keymap[ I_F7 ] = TVTIME_COLOUR_DOWN;
    ct->keymap[ I_F8 ] = TVTIME_COLOUR_UP;
    ct->keymap[ I_F10 ] = TVTIME_CHANNEL_SCAN;
    ct->keymap[ I_F11 ] = TVTIME_TOGGLE_BARS;
    ct->keymap[ I_F12 ] = TVTIME_TOGGLE_CREDITS;
    ct->keymap[ 'd' ] = TVTIME_DEBUG;
    ct->keymap[ 'f' ] = TVTIME_TOGGLE_FULLSCREEN;
    ct->keymap[ 'i' ] = TVTIME_TOGGLE_INPUT;
    ct->keymap[ 'a' ] = TVTIME_TOGGLE_ASPECT;
    ct->keymap[ 's' ] = TVTIME_SCREENSHOT;
    ct->keymap[ 't' ] = TVTIME_DEINTERLACINGMODE;
    ct->keymap[ 'p' ] = TVTIME_TOGGLE_PULLDOWN_DETECTION;
    ct->keymap[ 'n' ] = TVTIME_TOGGLE_NTSC_CABLE_MODE;
    ct->keymap[ ' ' ] = TVTIME_AUTO_ADJUST_PICT;
    ct->keymap[ 'r' ] = TVTIME_CHANNEL_SKIP;
    ct->keymap[ '`' ] = TVTIME_TOGGLE_CONSOLE;
    ct->keymap[ I_PGUP ] = TVTIME_SCROLL_CONSOLE_UP;
    ct->keymap[ I_PGDN ] = TVTIME_SCROLL_CONSOLE_DOWN;
    ct->keymap[ 'w' ] = TVTIME_TOGGLE_CC;
    ct->keymap[ '=' ] = TVTIME_TOGGLE_HALF_FRAMERATE;
    ct->keymap[ I_END ] = TVTIME_TOGGLE_PAUSE;

    memset( ct->buttonmap, 0, MAX_BUTTONS * sizeof(int) );
    ct->buttonmap[ 1 ] = TVTIME_DISPLAY_INFO;
    ct->buttonmap[ 2 ] = TVTIME_MIXER_MUTE;
    ct->buttonmap[ 3 ] = TVTIME_TOGGLE_INPUT;
    ct->buttonmap[ 4 ] = TVTIME_CHANNEL_UP;
    ct->buttonmap[ 5 ] = TVTIME_CHANNEL_DOWN;

    /* Make the ~/.tvtime directory every time on startup, to be safe. */
    snprintf( temp_dirname, sizeof( temp_dirname ), "%s%s", getenv( "HOME" ), "/.tvtime" );
    mkdir( temp_dirname, 0777 );

    /* Warning for 0.9.7. */
    strncpy( base, getenv( "HOME" ), 235 );
    strcat( base, "/.tvtimerc" );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "\n*** Notice: tvtime user config file has changed!\n"
                           "*** Old ~/.tvtimerc now belongs at ~/.tvtime/tvtimerc\n"
                           "*** Many of the options have also changed, so please check\n"
                           "*** out the new default config file!  Old config file ignored.\n\n" );
    }

    /* First read in global settings. */
    strncpy( base, CONFDIR, 245 );
    strcat( base, "/tvtimerc" );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "config: Reading configuration from %s\n", base );
        configFile = base;
        if( parser_new( &pf, configFile ) ) {
            config_init( ct, &pf );
            parser_delete( &pf );
        }
    }

    /* Then read in local settings. */
    strncpy( base, getenv( "HOME" ), 235 );
    strcat( base, "/.tvtime/tvtimerc" );
    sprintf( ct->config_filename, "%s", base );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "config: Reading configuration from %s\n", base );
        configFile = base;
        if( parser_new( &pf, configFile ) ) {
            config_init( ct, &pf );
            parser_delete( &pf );
        }
    }

    while( (c = getopt( argc, argv, "hw:I:avcsmd:i:l:n:f:t:F:D:Ib:" )) != -1 ) {
        switch( c ) {
        case 'w': ct->outputwidth = atoi( optarg ); break;
        case 'I': ct->inputwidth = atoi( optarg ); break;
        case 'v': ct->verbose = 1; break;
        case 'a': ct->aspect = 1; break;
        case 's': ct->debug = 1; break;
        case 'c': ct->apply_luma_correction = 1; break;
        case 'd': free( ct->v4ldev ); ct->v4ldev = strdup( optarg ); break;
        case 'b': free( ct->vbidev ); ct->vbidev = strdup( optarg ); break;
        case 'i': ct->inputnum = atoi( optarg ); break;
        case 'l': ct->luma_correction = atof( optarg );
                  ct->apply_luma_correction = 1; break;
        case 'n': free( ct->norm ); ct->norm = strdup( optarg ); break;
        case 'f': free( ct->freq ); ct->freq = strdup( optarg ); break;
        case 'F': configFile = strdup( optarg ); break;
        case 'm': ct->fullscreen = 1; break;
        case 'D': ct->preferred_deinterlace_method = atoi( optarg ); break;
        default:
            print_usage( argv );
            return 0;
        }
    }

    /* Then read in additional settings. */
    if( configFile && configFile != base ) {
        sprintf( ct->config_filename, "%s", configFile );
        fprintf( stderr, "config: Reading configuration from %s\n", configFile );

        if( !parser_new( &pf, configFile ) ) {
            fprintf( stderr, "config: Could not read configuration from %s\n", configFile );
        } else {
            config_init( ct, &pf );
            parser_delete( &pf );
        }
        free( configFile );
    }

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
    free( ct->timeformat );
    free( ct->norm );
    free( ct->freq );
    free( ct->vbidev );
    free( ct->v4ldev );
    free( ct );
}

int config_key_to_command( config_t *ct, int key )
{
    if( !ct || !ct->keymap ) {
        fprintf( stderr, "config: key_to_command: Invalid config obj "
                 "or no keymap.\n" );
        return TVTIME_NOCOMMAND;
    }

    if( !key ) return TVTIME_NOCOMMAND;

    if( ct->keymap[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ] ) {
        return ct->keymap[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ];
    }

    if( isalnum( key & 0x1ff ) ) {
        return TVTIME_CHANNEL_CHAR;
    }
        
    return TVTIME_NOCOMMAND;
}

int config_button_to_command( config_t *ct, int button )
{
    if( !ct || !ct->buttonmap ) return 0;
    if( button < 0 || button >= MAX_BUTTONS ) return 0;
    return ct->buttonmap[button];
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

int config_get_start_channel( config_t *ct )
{
    return ct->start_channel;
}

int config_get_inputnum( config_t *ct )
{
    return ct->inputnum;
}

int config_get_apply_luma_correction( config_t *ct )
{
    return ct->apply_luma_correction;
}

int config_get_preferred_deinterlace_method( config_t *ct )
{
    return ct->preferred_deinterlace_method;
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

int config_get_fullscreen( config_t *ct )
{
    return ct->fullscreen;
}

int config_get_priority( config_t *ct )
{
    return ct->priority;
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

unsigned int config_get_menu_bg_rgb( config_t *ct )
{
    return ct->menu_bg_rgb;
}

unsigned int config_get_channel_text_rgb( config_t *ct )
{
    return ct->channel_text_rgb;
}

unsigned int config_get_other_text_rgb( config_t *ct )
{
    return ct->other_text_rgb;
}

const char *config_get_config_filename( config_t *ct )
{
    return ct->config_filename;
}

const char *config_get_command_pipe( config_t *ct )
{
    return ct->command_pipe;
}

int config_get_usevbi( config_t *ct )
{
    return ct->use_vbi;
}

char *config_get_vbidev( config_t *ct )
{
    return ct->vbidev;
}

int config_get_check_freq_present( config_t *ct )
{
    return ct->check_freq_present;
}

double config_get_horizontal_overscan( config_t *ct )
{
    return ct->hoverscan;
}

double config_get_vertical_overscan( config_t *ct )
{
    return ct->voverscan;
}

int config_get_ntsc_cable_mode( config_t *ct )
{
    return ct->ntsc_mode;
}

void config_set_horizontal_overscan( config_t *ct, double hoverscan )
{
    if( hoverscan > 0.4 ) {
        ct->hoverscan = 0.4;
    } else if( hoverscan < 0.0 ) {
        ct->hoverscan = 0.0;
    } else {
        ct->hoverscan = hoverscan;
    }
}

void config_set_vertical_overscan( config_t *ct, double voverscan )
{
    if( voverscan > 0.4 ) {
        ct->voverscan = 0.4;
    } else if( voverscan < 0.0 ) {
        ct->voverscan = 0.0;
    } else {
        ct->voverscan = voverscan;
    }
}

