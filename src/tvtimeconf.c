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
#include <libxml/parser.h>
#include "tvtimeconf.h"
#include "input.h"
#include "station.h"
#include "commands.h"

#define MAX_KEYSYMS 350
#define MAX_BUTTONS 10

/* Key names. */
typedef struct key_name_s key_name_t;

struct key_name_s {
    char *name;
    int key;
};

key_name_t key_names[] = {
    { "up", I_UP },
    { "down", I_DOWN },
    { "left", I_LEFT },
    { "right", I_RIGHT },
    { "insert", I_INSERT },
    { "home", I_HOME },
    { "end", I_END },
    { "pgup", I_PGUP },
    { "pgdn", I_PGDN },
    { "f1", I_F1 },
    { "f2", I_F2 },
    { "f3", I_F3 },
    { "f4", I_F4 },
    { "f5", I_F5 },
    { "f6", I_F6 },
    { "f7", I_F7 },
    { "f8", I_F8 },
    { "f9", I_F9 },
    { "f10", I_F10 },
    { "f11", I_F11 },
    { "f12", I_F12 },
    { "f13", I_F13 },
    { "f14", I_F14 },
    { "f15", I_F15 },
    { "backspace", I_BACKSPACE },
    { "escape", I_ESCAPE },
    { "enter", I_ENTER },
    { "print", I_PRINT },
    { "menu", I_MENU },
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
    char *ssdir;
    unsigned int menu_bg_rgb;
    unsigned int channel_text_rgb;
    unsigned int other_text_rgb;
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
    } else if( ret == 3 ) {
        return 0xff000000 | ( (a & 0xff) << 16 ) | ( (r & 0xff) << 8 ) | ( g & 0xff);
    } else if( ret == 4 ) {
        return ( (a & 0xff) << 24 ) | ( (r & 0xff) << 16 ) | ( ( g & 0xff) << 8 ) | (b & 0xff);
    }

    return 0;
}

int match_special_key(const char *str)
{
    int count;

    for( count = 0; key_names[ count ].name; count++ ) {
        if( !xmlStrcasecmp( BAD_CAST str, BAD_CAST key_names[ count ].name ) ) {
            return key_names[ count ].key;
        }
    }

    return 0;
}

int parse_global( config_t *ct, xmlDocPtr doc, xmlNodePtr node)
{
    xmlChar *buf;

    while( node ) {
        if( !xmlIsBlankNode( node ) && ((buf = xmlGetProp( node, BAD_CAST "value" ) ) != NULL) ) {

            if( !xmlStrcasecmp( node->name, BAD_CAST "outputwidth" ) ) {
                ct->outputwidth = atoi((const char *)buf);
            } else if( !xmlStrcasecmp( node->name, BAD_CAST "inputwidth" ) ) {
                ct->inputwidth = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "verbose")) {
                ct->verbose = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "widescreen")) {
                ct->aspect = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "debugmode")) {
                ct->debug = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "applylumacorrection")) {
                ct->apply_luma_correction = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "lumacorrection")) {
                ct->luma_correction = atof((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "v4ldevice")) {
                free(ct->v4ldev);
                ct->v4ldev = strdup(buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "vbidevice")) {
                free(ct->vbidev);
                ct->vbidev = strdup(buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "capturesource")) {
                ct->inputnum = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "usevbi")) {
                ct->use_vbi = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "processpriority")) {
                ct->priority = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "fullscreen")) {
                ct->fullscreen = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "norm")) {
                free(ct->norm);
                ct->norm = strdup(buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "frequencies")) {
                free(ct->freq);
                ct->freq = strdup(buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "commandpipe")) {
                if(buf[0] == '~' && getenv("HOME") ) {
                    snprintf(ct->command_pipe, sizeof(ct->command_pipe), "%s%s", getenv("HOME"), buf+1);
                } else {
                    strncpy(ct->command_pipe, buf, 255);
                }
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "timeformat")) {
                free(ct->timeformat);
                ct->timeformat = strdup(buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "screenshotdir")) {
                free(ct->ssdir);
                ct->ssdir = strdup(buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "menubg")) {
                ct->menu_bg_rgb = parse_colour((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "channeltextfg")) {
                ct->channel_text_rgb = parse_colour((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "othertextfg")) {
                ct->other_text_rgb = parse_colour((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "startchannel")) {
                ct->start_channel = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "ntsccablemode")) {
                if( !strcasecmp((const char *)buf, "irc") ) {
                    ct->ntsc_mode = NTSC_CABLE_MODE_IRC;
                } else if ( !strcasecmp((const char *)buf, "hrc") ) {
                    ct->ntsc_mode = NTSC_CABLE_MODE_HRC;
                } else {
                    ct->ntsc_mode = NTSC_CABLE_MODE_NOMINAL;
                }
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "preferreddeinterlacemethod")) {
                ct->preferred_deinterlace_method = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "checkforsignal")) {
                ct->check_freq_present = atoi((const char *)buf);
            } else if( !xmlStrcasecmp(node->name, BAD_CAST "overscan")) {
                ct->hoverscan = (atof((const char *)buf) / 2.0) / 100.0;
                ct->voverscan = (atof((const char *)buf) / 2.0) / 100.0;
            }
        }
        node = node->next;
    }
    return 1;
}

int parse_keys(config_t *ct, xmlDocPtr doc, xmlNodePtr node)
{
    xmlChar *buf;
    int key;
    while(node != NULL) {
        if(!xmlIsBlankNode(node) && ((buf=xmlGetProp(node, BAD_CAST"value")) != NULL)) {
            if( (key=match_special_key((const char *)buf)) )
                ct->keymap[key]=tvtime_string_to_command((const char *)node->name);
            else
                ct->keymap[*buf]=tvtime_string_to_command((const char *)node->name);
        }
       node=node->next;
    }
    return 1;
}

int parse_mouse(config_t *ct, xmlDocPtr doc, xmlNodePtr node)
{
    xmlChar *buf;
    int button;
    while(node != NULL) {
        if(!xmlIsBlankNode(node) && ((buf=xmlGetProp(node, BAD_CAST"value")) != NULL)) {
            if( sscanf((const char *)buf, "button_%d", &button))
                if( (button > 0) && (button < MAX_BUTTONS))
                    ct->buttonmap[button] = tvtime_string_to_command((const char *)node->name);
        }
        node=node->next;
    }
    return 1;
}

int conf_xml_parse( config_t *ct, char *configFile)
{
    xmlDocPtr doc;
    xmlNodePtr top, node;

    if( (doc=xmlParseFile(configFile)) == NULL) {
        fprintf(stderr, "config: Error parsing configuration file %s.\n", configFile);
        return 0;
    }
    if( (top=xmlDocGetRootElement(doc)) == NULL) {
        fprintf(stderr, "config: No XML root element found in %s.\n",configFile);
        xmlFreeDoc(doc);
        return 0;
    }
    if( xmlStrcasecmp(top->name, BAD_CAST "Conf")) {
        fprintf(stderr, "config: Root node in configuration file %s should be 'Conf'.\n", configFile);
        xmlFreeDoc(doc);
        return 0;
    }

    node=top->xmlChildrenNode;
    while(node != NULL) {
        if(xmlIsBlankNode(top));
        else if(!xmlStrcasecmp(node->name, BAD_CAST "global"))
            parse_global(ct, doc, node->xmlChildrenNode);
        else if(!xmlStrcasecmp(node->name, BAD_CAST "keybindings"))
            parse_keys(ct, doc, node->xmlChildrenNode);
        else if(!xmlStrcasecmp(node->name, BAD_CAST "mousebindings"))
            parse_mouse(ct, doc,node->xmlChildrenNode);
        node=node->next;
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
    ct->ssdir = strdup( getenv( "HOME" ) );
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
    ct->keymap[ 'e' ] = TVTIME_TOGGLE_AUDIO_MODE;
    ct->keymap[ '<' ] = TVTIME_OVERSCAN_DOWN;
    ct->keymap[ '>' ] = TVTIME_OVERSCAN_UP;
    ct->keymap[ 'b' ] = TVTIME_RENUMBER_CHANNEL;

    memset( ct->buttonmap, 0, MAX_BUTTONS * sizeof(int) );
    ct->buttonmap[ 1 ] = TVTIME_DISPLAY_INFO;
    ct->buttonmap[ 2 ] = TVTIME_MIXER_MUTE;
    ct->buttonmap[ 3 ] = TVTIME_TOGGLE_INPUT;
    ct->buttonmap[ 4 ] = TVTIME_CHANNEL_UP;
    ct->buttonmap[ 5 ] = TVTIME_CHANNEL_DOWN;

    /* Make the ~/.tvtime directory every time on startup, to be safe. */
    snprintf( temp_dirname, sizeof( temp_dirname ), "%s%s", getenv( "HOME" ), "/.tvtime" );
    mkdir( temp_dirname, 0777 );

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

    return ct;
}

void config_delete( config_t *ct )
{
    if( ct->keymap ) free( ct->keymap );
    if( ct->buttonmap ) free( ct->buttonmap );
    free( ct->ssdir );
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

