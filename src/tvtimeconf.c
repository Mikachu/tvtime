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
#include <dirent.h>
#include <langinfo.h>
#include <pwd.h>
#include <errno.h>
#include <libxml/parser.h>
#include "tvtimeconf.h"
#include "input.h"
#include "station.h"
#include "commands.h"

#define MAX_KEYSYMS 350
#define MAX_BUTTONS 10

/* Mode list. */

typedef struct tvtime_modelist_s tvtime_modelist_t;

struct tvtime_modelist_s
{
    tvtime_mode_settings_t settings;
    tvtime_modelist_t *next;
};

/* Key names. */
typedef struct key_name_s key_name_t;

struct key_name_s {
    char *name;
    int key;
};

static key_name_t key_names[] = {
    { "Up", I_UP },
    { "Down", I_DOWN },
    { "Left", I_LEFT },
    { "Right", I_RIGHT },
    { "Insert", I_INSERT },
    { "Home", I_HOME },
    { "End", I_END },
    { "pgup", I_PGUP },
    { "pgdn", I_PGDN },
    { "pg up", I_PGUP },
    { "pg dn", I_PGDN },
    { "PageUp", I_PGUP },
    { "PageDown", I_PGDN },
    { "Page Up", I_PGUP },
    { "Page Down", I_PGDN },
    { "F1", I_F1 },
    { "F2", I_F2 },
    { "F3", I_F3 },
    { "F4", I_F4 },
    { "F5", I_F5 },
    { "F6", I_F6 },
    { "F7", I_F7 },
    { "F8", I_F8 },
    { "F9", I_F9 },
    { "F10", I_F10 },
    { "F11", I_F11 },
    { "F12", I_F12 },
    { "F13", I_F13 },
    { "F14", I_F14 },
    { "F15", I_F15 },
    { "Backspace", I_BACKSPACE },
    { "Escape", I_ESCAPE },
    { "Enter", I_ENTER },
    { "Print", I_PRINT },
    { "Menu", I_MENU },
    { 0, 0 }
};

struct config_s
{
    int outputwidth;
    int verbose;
    int aspect;
    int debug;
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
    char ssdir[ 256 ];
    unsigned int menu_bg_rgb;
    unsigned int channel_text_rgb;
    unsigned int other_text_rgb;

    uid_t uid;
    char command_pipe[ 256 ];

    char *rvr_filename;

    int preferred_deinterlace_method;
    int check_freq_present;

    int use_vbi;
    char *vbidev;

    int start_channel;
    int prev_channel;
    int framerate;

    double hoverscan;
    double voverscan;

    char config_filename[ 1024 ];

    configsave_t *configsave;

    int nummodes;
    tvtime_modelist_t *modelist;
};

static unsigned int parse_colour( const char *str )
{
    unsigned int a, r, g, b;
    int ret;
    
    if( !str || !*str ) return 0;

    if( strlen( str ) == 1 ) return (unsigned int)atoi( str );

    if( str[0] == '0' && str[1] == 'x' ) {
        ret = sscanf( str, "0x%x", &a );
    } else {
        ret = sscanf( str, "%u %u %u %u", &a, &r, &g, &b );
    }

    if( ret == 1 ) {
        return a;
    } else if( ret == 2 ) {
        return 0xff000000 | ( (a & 0xff) << 8 ) | (r & 0xff);
    } else if( ret == 3 ) {
        return 0xff000000 | ( (a & 0xff) << 16 ) | ( (r & 0xff) << 8 ) | ( g & 0xff);
    } else if( ret == 4 ) {
        return ( (a & 0xff) << 24 ) | ( (r & 0xff) << 16 ) | ( ( g & 0xff) << 8 ) | (b & 0xff);
    }

    return 0;
}

static int match_special_key( const char *str )
{
    int count;

    for( count = 0; key_names[ count ].name; count++ ) {
        if( !xmlStrcasecmp( BAD_CAST str, BAD_CAST key_names[ count ].name ) ) {
            return key_names[ count ].key;
        }
    }

    return 0;
}

static void parse_option( config_t *ct, xmlNodePtr node )
{
    xmlChar *name;
    xmlChar *value;

    name = xmlGetProp( node, BAD_CAST "name" );
    value = xmlGetProp( node, BAD_CAST "value" );

    if( name && value ) {
        char *curval = (char *) value;

        if( !xmlStrcasecmp( name, BAD_CAST "OutputWidth" ) ) {
            ct->outputwidth = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "InputWidth" ) ) {
            ct->inputwidth = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Verbose" ) ) {
            ct->verbose = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Widescreen" ) ) {
            ct->aspect = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DebugMode" ) ) {
            ct->debug = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ApplyLumaCorrection" ) ) {
            ct->apply_luma_correction = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "LumaCorrection" ) ) {
            ct->luma_correction = atof( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "V4LDevice" ) ) {
            free( ct->v4ldev );
            ct->v4ldev = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "VBIDevice" ) ) {
            free( ct->vbidev );
            ct->vbidev = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "V4LInput" ) ) {
            ct->inputnum = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "UseVBI" ) ) {
            ct->use_vbi = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ProcessPriority" ) ) {
            ct->priority = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "StartFullscreen" ) ) {
            ct->fullscreen = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "FramerateMode" ) ) {
            ct->framerate = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Norm" ) ) {
            free( ct->norm );
            ct->norm = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Frequencies" ) ) {
            free( ct->freq );
            ct->freq = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "TimeFormat" ) ) {
            free( ct->timeformat );
            ct->timeformat = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ScreenShotDir" ) ) {
            if( curval[ 0 ] == '~' && getenv( "HOME" ) ) {
                snprintf( ct->ssdir, sizeof( ct->ssdir ), "%s%s", getenv( "HOME" ), curval + 1 );
            } else {
                strncpy( ct->ssdir, curval, 255 );
            }
        }

        if( !xmlStrcasecmp( name, BAD_CAST "MenuBG" ) ) {
            ct->menu_bg_rgb = parse_colour( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ChannelTextFG" ) ) {
            ct->channel_text_rgb = parse_colour( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "OtherTextFG" ) ) {
            ct->other_text_rgb = parse_colour( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "PrevChannel" ) ) {
            ct->prev_channel = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "StartChannel" ) ) {
            ct->start_channel = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "NTSCCableMode" ) ) {
            if( !xmlStrcasecmp( value, BAD_CAST "IRC" ) ) {
                ct->ntsc_mode = NTSC_CABLE_MODE_IRC;
            } else if( !xmlStrcasecmp( value, BAD_CAST "HRC" ) ) {
                ct->ntsc_mode = NTSC_CABLE_MODE_HRC;
            } else {
                ct->ntsc_mode = NTSC_CABLE_MODE_NOMINAL;
            }
        }

        if( !xmlStrcasecmp( name, BAD_CAST "PreferredDeinterlaceMethod" ) ) {
            ct->preferred_deinterlace_method = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "CheckForSignal" ) ) {
            ct->preferred_deinterlace_method = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Overscan" ) ) {
            ct->hoverscan = ( atof( curval ) / 2.0 ) / 100.0;
            ct->voverscan = ( atof( curval ) / 2.0 ) / 100.0;
        }
    }

    if( name ) xmlFree( name );
    if( value ) xmlFree( value );
}

static void parse_bind( config_t *ct, xmlNodePtr node )
{
    xmlChar *command = xmlGetProp( node, BAD_CAST "command" );

    if( command ) {
        xmlNodePtr binding = node->xmlChildrenNode;

        while( binding ) {
            if( !xmlStrcasecmp( binding->name, BAD_CAST "keyboard" ) ) {
                xmlChar *key = xmlGetProp( binding, BAD_CAST "key" );
                if( key ) {
                    int keycode = match_special_key( (const char *) key );
                    if( !keycode ) {
                        keycode = (*key);
                    }
                    ct->keymap[ keycode ] = tvtime_string_to_command( (const char *) command );
                    xmlFree( key );
                }
            } else if( !xmlStrcasecmp( binding->name, BAD_CAST "mouse" ) ) {
                xmlChar *button = xmlGetProp( binding, BAD_CAST "button" );
                if( button ) {
                    int id;
                    if( sscanf( (const char *) button, "button_%d", &id ) ) {
                        if( (id > 0) && (id < MAX_BUTTONS) ) {
                            ct->buttonmap[ id ] = tvtime_string_to_command( (const char *) command );
                        }
                    }
                    xmlFree( button );
                }
            }
            binding = binding->next;
        }

        xmlFree( command );
    }
}

static int conf_xml_parse( config_t *ct, char *configFile )
{
    xmlDocPtr doc;
    xmlNodePtr top, node;

    doc = xmlParseFile( configFile );
    if( !doc ) {
        fprintf( stderr, "config: Error parsing configuration file %s.\n", configFile );
        return 0;
    }

    top = xmlDocGetRootElement( doc );
    if( !top ) {
        fprintf( stderr, "config: No XML root element found in %s.\n", configFile );
        xmlFreeDoc( doc );
        return 0;
    }

    if( xmlStrcasecmp( top->name, BAD_CAST "tvtime" ) ) {
        fprintf( stderr, "config: Root node in configuration file %s should be 'tvtime'.\n", configFile );
        xmlFreeDoc( doc );
        return 0;
    }

    node = top->xmlChildrenNode;
    while( node ) {
        if( !xmlIsBlankNode( node ) ) {
            if( !xmlStrcasecmp( node->name, BAD_CAST "option" ) ) {
                parse_option( ct, node );
            } else if( !xmlStrcasecmp( node->name, BAD_CAST "bind" ) ) {
                parse_bind( ct, node );
            }
        }
        node = node->next;
    }

    return 1;
}
 
static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-vamsb] [-w <width>] [-I <sampling>] "
                     "[-d <device>]\n\t\t[-i <input>] [-n <norm>] "
                     "[-f <frequencies>] [-t <tuner>] "
                     "[-D <deinterlace method>] [-r rvrfile]\n", argv[ 0 ] );
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

    fprintf( stderr, "\t-r\tRVR recorded file to play.\n" );

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
    DIR *temp_dir = 0;
    char temp_dirname[ 1024 ];
    char base[ 256 ];
    char *configFile = 0;
    char c;
    struct passwd *pwuid = 0;

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
    ct->nummodes = 0;
    ct->modelist = 0;

    ct->uid = getuid( );
    pwuid = getpwuid( ct->uid );
    if( !pwuid ) {
        fprintf( stderr, "config: You don't exist, go away!\n" );
        free( ct );
        return 0;
    }
    snprintf( ct->command_pipe, sizeof( ct->command_pipe ), 
              FIFODIR "/TV-%s", pwuid->pw_name );

    if( strlen( nl_langinfo( T_FMT ) ) ) {
        ct->timeformat = strdup( nl_langinfo( T_FMT ) );
    } else {
        ct->timeformat = strdup( "%r" );
    }
    strncpy( ct->ssdir, getenv( "HOME" ), 255 );
    ct->fullscreen = 0;
    ct->menu_bg_rgb = 4278190080U; /* opaque black */
    ct->channel_text_rgb = 4294967040U; /* opaque yellow */
    ct->other_text_rgb = 4294303411U; /* opaque wheat */
    ct->keymap = (int *) malloc( 8*MAX_KEYSYMS * sizeof( int ) );
    ct->preferred_deinterlace_method = 0;
    ct->check_freq_present = 1;
    ct->use_vbi = 0;
    ct->start_channel = 1;
    ct->prev_channel = 1;
    ct->hoverscan = 0.0;
    ct->voverscan = 0.0;
    ct->rvr_filename = 0;
    ct->framerate = FRAMERATE_FULL;

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
    ct->keymap[ I_BACKSPACE ] = TVTIME_CHANNEL_PREV;
    ct->keymap[ 'k' ] = TVTIME_CHANNEL_UP;
    ct->keymap[ 'j' ] = TVTIME_CHANNEL_DOWN;
    ct->keymap[ 'h' ] = TVTIME_FINETUNE_DOWN;
    ct->keymap[ 'l' ] = TVTIME_FINETUNE_UP;
    ct->keymap[ 'c' ] = TVTIME_TOGGLE_LUMA_CORRECTION;
    ct->keymap[ 'z' ] = TVTIME_LUMA_DOWN;
    ct->keymap[ 'x' ] = TVTIME_LUMA_UP;
    ct->keymap[ 'm' ] = TVTIME_MIXER_MUTE;
    ct->keymap[ '-' ] = TVTIME_MIXER_DOWN;
    ct->keymap[ '+' ] = TVTIME_MIXER_UP;
    ct->keymap[ I_ENTER ] = TVTIME_ENTER;
    ct->keymap[ I_F1 ] = TVTIME_HUE_DOWN;
    ct->keymap[ I_F2 ] = TVTIME_HUE_UP;
    ct->keymap[ I_F3 ] = TVTIME_BRIGHTNESS_DOWN;
    ct->keymap[ I_F4 ] = TVTIME_BRIGHTNESS_UP;
    ct->keymap[ I_F5 ] = TVTIME_CONTRAST_DOWN;
    ct->keymap[ I_F6 ] = TVTIME_CONTRAST_UP;
    ct->keymap[ I_F7 ] = TVTIME_COLOUR_DOWN;
    ct->keymap[ I_F8 ] = TVTIME_COLOUR_UP;
    ct->keymap[ I_F10 ] = TVTIME_CHANNEL_SCAN;
    ct->keymap[ I_F11 ] = TVTIME_TOGGLE_BARS;
    ct->keymap[ I_F12 ] = TVTIME_TOGGLE_CREDITS;
    ct->keymap[ 'd' ] = TVTIME_SHOW_STATS;
    ct->keymap[ 'f' ] = TVTIME_TOGGLE_FULLSCREEN;
    ct->keymap[ 'i' ] = TVTIME_TOGGLE_INPUT;
    ct->keymap[ 'a' ] = TVTIME_TOGGLE_ASPECT;
    ct->keymap[ 's' ] = TVTIME_SCREENSHOT;
    ct->keymap[ 't' ] = TVTIME_TOGGLE_DEINTERLACER;
    ct->keymap[ 'p' ] = TVTIME_TOGGLE_PULLDOWN_DETECTION;
    ct->keymap[ 'n' ] = TVTIME_TOGGLE_NTSC_CABLE_MODE;
    ct->keymap[ ' ' ] = TVTIME_AUTO_ADJUST_PICT;
    ct->keymap[ 'r' ] = TVTIME_CHANNEL_SKIP;
    ct->keymap[ '`' ] = TVTIME_TOGGLE_CONSOLE;
    ct->keymap[ I_PGUP ] = TVTIME_SCROLL_CONSOLE_UP;
    ct->keymap[ I_PGDN ] = TVTIME_SCROLL_CONSOLE_DOWN;
    ct->keymap[ 'w' ] = TVTIME_TOGGLE_CC;
    ct->keymap[ '=' ] = TVTIME_TOGGLE_FRAMERATE;
    ct->keymap[ I_END ] = TVTIME_TOGGLE_PAUSE;
    ct->keymap[ 'e' ] = TVTIME_TOGGLE_AUDIO_MODE;
    ct->keymap[ '<' ] = TVTIME_OVERSCAN_DOWN;
    ct->keymap[ '>' ] = TVTIME_OVERSCAN_UP;
    ct->keymap[ 'b' ] = TVTIME_CHANNEL_RENUMBER;

    memset( ct->buttonmap, 0, MAX_BUTTONS * sizeof(int) );
    ct->buttonmap[ 1 ] = TVTIME_DISPLAY_INFO;
    ct->buttonmap[ 2 ] = TVTIME_MIXER_MUTE;
    ct->buttonmap[ 3 ] = TVTIME_TOGGLE_INPUT;
    ct->buttonmap[ 4 ] = TVTIME_CHANNEL_UP;
    ct->buttonmap[ 5 ] = TVTIME_CHANNEL_DOWN;

    /* Make the ~/.tvtime directory every time on startup, to be safe. */
    snprintf( temp_dirname, sizeof( temp_dirname ), "%s%s", getenv( "HOME" ), "/.tvtime" );
    if( mkdir( temp_dirname, S_IRWXU ) < 0) {
        if( errno != EEXIST ) {
            fprintf( stderr, "config: Cannot create %s.\n", temp_dirname );
            free( ct );
            return 0;
        }
        else {
            temp_dir = opendir( temp_dirname );
            if( !temp_dir ) {
                fprintf( stderr, "config: %s is not a directory.\n", 
                         temp_dirname );
                free( ct );
                return 0;
            }
            else {
                closedir( temp_dir );
            }
        }
        /* If the directory already exists, we didn't need to create it. */
    }

    /* First read in global settings. */
    strncpy( base, CONFDIR, 245 );
    strcat( base, "/tvtime.xml" );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "config: Reading configuration from %s\n", base );
        configFile = base;
        conf_xml_parse(ct, configFile);
    }

    /* Then read in local settings. */
    strncpy( base, getenv( "HOME" ), 235 );
    strcat( base, "/.tvtime/tvtime.xml" );
    sprintf( ct->config_filename, "%s", base );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "config: Reading configuration from %s\n", base );
        configFile = base;
        conf_xml_parse(ct, configFile);
    }

    while( (c = getopt( argc, argv, "hw:I:avcsmd:i:l:n:f:t:F:D:Ib:r:" )) != -1 ) {
        switch( c ) {
        case 'w': ct->outputwidth = atoi( optarg ); break;
        case 'I': ct->inputwidth = atoi( optarg ); break;
        case 'v': ct->verbose = 1; break;
        case 'a': ct->aspect = 1; break;
        case 's': ct->debug = 1; break;
        case 'c': ct->apply_luma_correction = 1; break;
        case 'd': free( ct->v4ldev ); ct->v4ldev = strdup( optarg ); break;
        case 'b': free( ct->vbidev ); ct->vbidev = strdup( optarg ); break;
        case 'r': if( ct->rvr_filename ) { free( ct->rvr_filename ); }
                  ct->rvr_filename = strdup( optarg ); break;
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

        conf_xml_parse(ct, configFile);
        free( configFile );
    }

    /* Sanity check parameters into reasonable ranges here. */
    if( ct->inputwidth & 1 ) {
        ct->inputwidth -= 1;
        fprintf( stderr, "config: Odd values for input width not allowed, "
                         "using %d instead.\n", ct->inputwidth );
    }

    /* I phear that users may want to know this. */
    if( ct->verbose ) {
        fprintf( stderr, "config: Screenshots saved to %s\n", ct->ssdir );
    }

    ct->configsave = configsave_open( ct->config_filename );

    return ct;
}

void config_delete( config_t *ct )
{
    if( ct->configsave ) configsave_close( ct->configsave );
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

int config_get_prev_channel( config_t *ct )
{
    return ct->prev_channel;
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

uid_t config_get_uid( config_t *ct )
{
    return ct->uid;
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

const char *config_get_screenshot_dir( config_t *ct )
{
    return ct->ssdir;
}

const char *config_get_rvr_filename( config_t *ct )
{
    return ct->rvr_filename;
}

int config_get_framerate_mode( config_t *ct )
{
    return ct->framerate;
}

configsave_t *config_get_configsave( config_t *ct )
{
    return ct->configsave;
}

int config_get_num_modes( config_t *ct )
{
    return ct->nummodes;
}

tvtime_mode_settings_t *config_get_mode_info( config_t *ct, int mode )
{
    tvtime_modelist_t *cur = ct->modelist;
    int i;

    if( !cur ) {
        /* No modes. */
        return 0;
    }

    while( i && cur->next ) {
        cur = cur->next;
        i--;
    }

    return cur;
}

