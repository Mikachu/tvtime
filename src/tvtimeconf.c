/**
 * Copyright (C) 2002, 2003 Doug Bell <drbell@users.sourceforge.net>
 * Copyright (c) 2003 Aleander Belov <asbel@mail.ru>
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

#define _GNU_SOURCE
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
#include "utils.h"

#define MAX_KEYSYMS 350
#define MAX_BUTTONS 10

/* Mode list. */
typedef struct tvtime_modelist_s tvtime_modelist_t;

struct config_s
{
    int outputheight;
    int verbose;
    int aspect;
    int debug;
    int fullscreen;
    int priority;
    int ntsc_mode;
    int send_fields;
    char *output_driver;
    int apply_luma_correction;
    double luma_correction;
    int useposition;
    int x;
    int y;

    int keymap[ 8 * MAX_KEYSYMS ];
    int buttonmap[ MAX_BUTTONS ];

    int inputwidth;
    int inputnum;
    char *v4ldev;
    char *norm;
    char *freq;
    char *ssdir;
    char *timeformat;
    unsigned int channel_text_rgb;
    unsigned int other_text_rgb;

    uid_t uid;

    char *rvr_filename;

    char *mixerdev;

    char *deinterlace_method;
    int check_freq_present;

    int use_vbi;
    char *vbidev;

    int start_channel;
    int prev_channel;
    int framerate;
    int slave_mode;

    double overscan;

    char *config_filename;
    xmlDocPtr doc;

    int nummodes;
    tvtime_modelist_t *modelist;
};

/* Mode list. */
struct tvtime_modelist_s
{
    config_t settings;
    char *name;
    tvtime_modelist_t *next;
};

static void copy_config( config_t *dest, config_t *src )
{
    (*dest) = (*src);

    /* Some of these I am keeping invalid for now. */
    dest->v4ldev = 0;
    dest->vbidev = 0;
    dest->rvr_filename = 0;
    dest->mixerdev = 0;
    dest->config_filename = 0;
    dest->modelist = 0;
    dest->nummodes = 0;
    dest->doc = 0;
    dest->output_driver = 0;

    /* Useful strings must be copied. */
    dest->norm = strdup( src->norm );
    dest->freq = strdup( src->freq );
    dest->ssdir = strdup( src->ssdir );
    dest->timeformat = strdup( src->timeformat );
    dest->deinterlace_method = strdup( src->timeformat );
}

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

static xmlNodePtr find_option( xmlNodePtr node, const char *optname )
{
    while( node ) {
        if( !xmlStrcasecmp( node->name, BAD_CAST "option" ) ) {
            xmlChar *name = xmlGetProp( node, BAD_CAST "name" );

            if( name && !xmlStrcasecmp( name, BAD_CAST optname ) ) {
                xmlFree( name );
                return node;
            }
            if( name ) xmlFree( name );
        }

        node = node->next;
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

        if( !xmlStrcasecmp( name, BAD_CAST "OutputHeight" ) ) {
            ct->outputheight = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "UseWindowPosition" ) ) {
            ct->useposition = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "WindowX" ) ) {
            ct->x = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "WindowY" ) ) {
            ct->y = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "InputWidth" ) ) {
            ct->inputwidth = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Verbose" ) ) {
            ct->verbose = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DFBSendFields" ) ) {
            ct->send_fields = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "OutputDriver" ) ) {
            if( ct->output_driver ) free( ct->output_driver );
            ct->output_driver = strdup( curval );
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
            if( ct->v4ldev ) free( ct->v4ldev );
            ct->v4ldev = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "VBIDevice" ) ) {
            if( ct->vbidev ) free( ct->vbidev );
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

        if( !xmlStrcasecmp( name, BAD_CAST "Fullscreen" ) ) {
            ct->fullscreen = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "FramerateMode" ) ) {
            ct->framerate = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Norm" ) ) {
            if( ct->norm ) free( ct->norm );
            ct->norm = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Frequencies" ) ) {
            if( ct->freq ) free( ct->freq );
            ct->freq = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "TimeFormat" ) ) {
            if( ct->timeformat ) free( ct->timeformat );
            ct->timeformat = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ScreenShotDir" ) ) {
            if( ct->ssdir ) free( ct->ssdir );
            ct->ssdir = expand_user_path( curval );
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

        if( !xmlStrcasecmp( name, BAD_CAST "Channel" ) ) {
            ct->start_channel = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "NTSCCableMode" ) ) {
            if( !xmlStrcasecmp( value, BAD_CAST "IRC" ) ) {
                ct->ntsc_mode = NTSC_CABLE_MODE_IRC;
            } else if( !xmlStrcasecmp( value, BAD_CAST "HRC" ) ) {
                ct->ntsc_mode = NTSC_CABLE_MODE_HRC;
            } else {
                ct->ntsc_mode = NTSC_CABLE_MODE_STANDARD;
            }
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DeinterlaceMethod" ) ) {
            if( ct->deinterlace_method ) free( ct->deinterlace_method );
            ct->deinterlace_method = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "CheckForSignal" ) ) {
            ct->check_freq_present = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Overscan" ) ) {
            ct->overscan = ( atof( curval ) / 2.0 ) / 100.0;
        }

        if( !xmlStrcasecmp( name, BAD_CAST "MixerDevice" ) ) {
            if( ct->mixerdev ) free( ct->mixerdev );
            ct->mixerdev = strdup( curval );
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
                    int keycode, command_id;

                    keycode = input_string_to_special_key( (const char *) key );
                    if( !keycode ) {
                        keycode = (*key);
                    }

                    command_id = tvtime_string_to_command( (const char *) command );
                    if( command_id != TVTIME_NOCOMMAND ) {
                        ct->keymap[ keycode ] = command_id;
                    }

                    xmlFree( key );
                }
            } else if( !xmlStrcasecmp( binding->name, BAD_CAST "mouse" ) ) {
                xmlChar *button = xmlGetProp( binding, BAD_CAST "button" );
                if( button ) {
                    int id = atoi( (const char *) button );
                    if( (id > 0) && (id < MAX_BUTTONS) ) {
                        ct->buttonmap[ id ] = tvtime_string_to_command( (const char *) command );
                    }
                    xmlFree( button );
                }
            }
            binding = binding->next;
        }

        xmlFree( command );
    }
}

static void parse_mode( config_t *ct, xmlNodePtr node )
{
    xmlChar *name = xmlGetProp( node, BAD_CAST "name" );

    if( name ) {
        tvtime_modelist_t *mode = malloc( sizeof( tvtime_modelist_t ) );
        if( mode ) {

            /* Start with the default settings from the config file. */
            copy_config( &(mode->settings), ct );

            node = node->xmlChildrenNode;
            while( node ) {
                if( !xmlIsBlankNode( node ) ) {
                    if( !xmlStrcasecmp( node->name, BAD_CAST "option" ) ) {
                        parse_option( &(mode->settings), node );
                    }
                }
                node = node->next;
            }

            mode->name = strdup( (char *) name );
            mode->next = ct->modelist;
            ct->modelist = mode;
            ct->nummodes++;
        }
        xmlFree( name );
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
            } else if( !xmlStrcasecmp( node->name, BAD_CAST "mode" ) ) {
                parse_mode( ct, node );
            }
        }
        node = node->next;
    }

    return 1;
}

/* Attempt to parse the file for key elements and create them if they don't exist */
static xmlDocPtr configsave_open( const char *config_filename )
{
    xmlDocPtr doc;
    xmlNodePtr top;
    int create_file = 0;

    doc = xmlParseFile( config_filename );
    if( !doc ) {
        if( file_is_openable_for_read( config_filename ) ) {
            fprintf( stderr, "config: Config file exists, but cannot be parsed.\n" );
            fprintf( stderr, "config: Settings will NOT be saved.\n" );
            return 0;
        } else {
            /* Config file doesn't exist, create a new one. */
            fprintf( stderr, "config: No config file found, creating a new one.\n" );
            doc = xmlNewDoc( BAD_CAST "1.0" );
            if( !doc ) {
                fprintf( stderr, "config: Could not create new config file.\n" );
                return 0;
            }
	    create_file = 1;
        }
    }

    top = xmlDocGetRootElement( doc );
    if( !top ) {
        /* Set the DTD */
        xmlDtdPtr dtd;
        dtd = xmlNewDtd( doc, BAD_CAST "tvtime",
                         BAD_CAST "-//tvtime//DTD tvtime 1.0//EN",
                         BAD_CAST "http://tvtime.sourceforge.net/DTD/tvtime1.dtd" );
        doc->intSubset = dtd;
        if( !doc->children ) {
            xmlAddChild( (xmlNodePtr) doc, (xmlNodePtr) dtd );
        } else {
            xmlAddPrevSibling( doc->children, (xmlNodePtr) dtd );
        }

        /* Create the root node */
        top = xmlNewDocNode( doc, 0, BAD_CAST "tvtime", 0 );
        if( !top ) {
            fprintf( stderr, "config: Could not create toplevel element 'tvtime'.\n" );
            xmlFreeDoc( doc );
            return 0;
        } else {
            xmlDocSetRootElement( doc, top );
            xmlNewProp( top, BAD_CAST "xmlns",
                        BAD_CAST "http://tvtime.sourceforge.net/DTD/" );
        }
    }

    if( xmlStrcasecmp( top->name, BAD_CAST "tvtime" ) ) {
        fprintf( stderr, "config: Root node in file %s should be 'tvtime'.\n", config_filename );
        xmlFreeDoc( doc );
        return 0;
    }

    xmlKeepBlanksDefault( 0 );
    xmlSaveFormatFile( config_filename, doc, 1 );
    if (create_file) {
        if( chown( config_filename, getuid(), getgid() ) < 0 ) {
            fprintf( stderr, "config: Cannot chown %s.\n        %s.",
                    config_filename, strerror( errno ) );
        }
    }
    return doc;
}
 
static void print_usage( char **argv )
{
    fprintf( stderr, "usage: %s [-ahkmMsSv] [-F <config file>] [-r <rvrfile>] [-H <height>]\n"
                     "\t\t[-I <sampling>] [-d <device>] [-b <device>] [-i <input>]\n"
                     "\t\t[-n <norm>] [-f <frequencies>] [-c <channel>]\n"
                     "\t\t[-D <output_driver>] [-x <mixer device>[:<channel>]]\n" , argv[ 0 ] );

    fprintf( stderr, "\t-a\t16:9 mode.\n" );
    fprintf( stderr, "\t-h\tShow this help message.\n" );
    fprintf( stderr, "\t-k\tDisables keyboard input handling in tvtime (slave mode).\n" );
    fprintf( stderr, "\t-m\tStart tvtime in fullscreen mode.\n" );
    fprintf( stderr, "\t-M\tStart tvtime in windowed mode.\n" );
    fprintf( stderr, "\t-s\tPrint stats on frame drops (for debugging).\n" );

    fprintf( stderr, "\t-S\tSave command line options to the config file.\n" );

    fprintf( stderr, "\t-v\tShow verbose messages.\n" );

    fprintf( stderr, "\t-F\tAdditional config file to load settings from.\n" );

    fprintf( stderr, "\t-r\tRVR recorded file to play (for debugging).\n" );

    fprintf( stderr, "\t-H\tOutput window height (defaults to 576).\n" );
    fprintf( stderr, "\t-I\tvideo4linux input scanline sampling (defaults to 720).\n" );

    fprintf( stderr, "\t-d\tvideo4linux device (defaults to /dev/video0).\n" );
    fprintf( stderr, "\t-b\tVBI device (defaults to /dev/vbi0).\n" );
    fprintf( stderr, "\t-i\tvideo4linux input number (defaults to 0).\n" );

    fprintf( stderr, "\t-c\tTune to this channel on startup.\n" );

    fprintf( stderr, "\t-n\tThe mode to set the tuner to: PAL, NTSC, SECAM, PAL-NC,\n"
                     "\t  \tPAL-M, PAL-N or NTSC-JP (defaults to NTSC).\n" );
    fprintf( stderr, "\t-D\tThe output driver to use: Xv, DirectFB, mga, xmga (defaults to Xv).\n");

    fprintf( stderr, "\t-x\tThe mixer device and channel.  (defaults to /dev/mixer:line)\n"
                     "\t  \tValid channels are:\n"
                     "\t  \t\tvol, bass, treble, synth, pcm, speaker, line, mic,\n"
                     "\t  \t\tcd, mix, pcm2, rec, igain, ogain, line1, line2,\n"
                     "\t  \t\tline3, dig1, dig2, dig3, phin, phout, video,\n"
                     "\t  \t\tradio, monitor\n" );

    fprintf( stderr, "\t-f\tThe channels you are receiving with the tuner\n"
                     "\t  \t(defaults to us-cable).\n"
                     "\t  \tValid values are:\n"
                     "\t  \t\tus-cable\n"
                     "\t  \t\tus-broadcast\n"
                     "\t  \t\tjapan-cable\n"
                     "\t  \t\tjapan-broadcast\n"
                     "\t  \t\teurope\n"
                     "\t  \t\taustralia\n"
                     "\t  \t\taustralia-optus\n"
                     "\t  \t\tnewzealand\n"
                     "\t  \t\tfrance\n"
                     "\t  \t\trussia\n" );
}

config_t *config_new( void )
{
    char *temp_dirname;
    char *base;

    config_t *ct = malloc( sizeof( config_t ) );
    if( !ct ) {
        return 0;
    }

    ct->outputheight = 576;
    ct->useposition = 0;
    ct->x = 320;
    ct->y = 240;
    ct->inputwidth = 720;
    ct->verbose = 0;
    ct->send_fields = 0;
    ct->output_driver = strdup( "xv" );
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
    ct->fullscreen = 0;
    ct->channel_text_rgb = 0xffffff00; /* opaque yellow */
    ct->other_text_rgb = 0xfff5deb3;   /* opaque wheat */
    ct->deinterlace_method = strdup( "GreedyH" );
    ct->check_freq_present = 1;
    ct->use_vbi = 0;
    ct->start_channel = 1;
    ct->prev_channel = 1;
    ct->overscan = 0.0;
    ct->framerate = FRAMERATE_FULL;
    ct->ssdir = strdup( getenv( "HOME" ) );
    ct->timeformat = strdup( "%X" );
    ct->mixerdev = strdup( "/dev/mixer:line" );

    /* We set these to 0 so we can delete safely if necessary. */
    ct->rvr_filename = 0;
    ct->config_filename = 0;
    ct->doc = 0;

    ct->uid = getuid();
    if( !getpwuid( ct->uid ) ) {
        fprintf( stderr, "config: You don't exist, go away!\n" );
        config_delete( ct );
        return 0;
    }

    memset( ct->keymap, 0, sizeof( ct->keymap ) );
    ct->keymap[ 0 ] = TVTIME_NOCOMMAND;

    ct->keymap[ I_ESCAPE ] = TVTIME_QUIT;
    ct->keymap[ 'q' ] = TVTIME_QUIT;
    ct->keymap[ I_UP ] = TVTIME_CHANNEL_INC;
    ct->keymap[ I_DOWN ] = TVTIME_CHANNEL_DEC;
    ct->keymap[ I_LEFT ] = TVTIME_FINETUNE_DOWN;
    ct->keymap[ I_RIGHT ] = TVTIME_FINETUNE_UP;
    ct->keymap[ I_BACKSPACE ] = TVTIME_CHANNEL_PREV;
    ct->keymap[ 'k' ] = TVTIME_CHANNEL_INC;
    ct->keymap[ 'j' ] = TVTIME_CHANNEL_DEC;
    ct->keymap[ 'h' ] = TVTIME_FINETUNE_DOWN;
    ct->keymap[ 'l' ] = TVTIME_FINETUNE_UP;
    ct->keymap[ 'c' ] = TVTIME_TOGGLE_LUMA_CORRECTION;
    ct->keymap[ 'z' ] = TVTIME_LUMA_DOWN;
    ct->keymap[ 'x' ] = TVTIME_LUMA_UP;
    ct->keymap[ 'm' ] = TVTIME_TOGGLE_MUTE;
    ct->keymap[ '-' ] = TVTIME_MIXER_DOWN;
    ct->keymap[ '+' ] = TVTIME_MIXER_UP;
    ct->keymap[ ',' ] = TVTIME_MIXER_TOGGLE_MUTE;
    ct->keymap[ I_ENTER ] = TVTIME_ENTER;
    ct->keymap[ I_F1 ] = TVTIME_BRIGHTNESS_DOWN;
    ct->keymap[ I_F2 ] = TVTIME_BRIGHTNESS_UP;
    ct->keymap[ I_F3 ] = TVTIME_CONTRAST_DOWN;
    ct->keymap[ I_F4 ] = TVTIME_CONTRAST_UP;
    ct->keymap[ I_F5 ] = TVTIME_COLOUR_DOWN;
    ct->keymap[ I_F6 ] = TVTIME_COLOUR_UP;
    ct->keymap[ I_F7 ] = TVTIME_HUE_DOWN;
    ct->keymap[ I_F8 ] = TVTIME_HUE_UP;
    ct->keymap[ I_F9 ] = TVTIME_CHANNEL_SAVE_TUNING;
    ct->keymap[ I_F10 ] = TVTIME_CHANNEL_SCAN;
    ct->keymap[ I_F11 ] = TVTIME_CHANNEL_ACTIVATE_ALL;
    ct->keymap[ I_F12 ] = TVTIME_CHANNEL_SKIP;
    ct->keymap[ 'r' ] = TVTIME_CHANNEL_RENUMBER;
    ct->keymap[ 'd' ] = TVTIME_SHOW_STATS;
    ct->keymap[ 'f' ] = TVTIME_TOGGLE_FULLSCREEN;
    ct->keymap[ 'i' ] = TVTIME_TOGGLE_INPUT;
    ct->keymap[ 'a' ] = TVTIME_TOGGLE_ASPECT;
    ct->keymap[ 's' ] = TVTIME_SCREENSHOT;
    ct->keymap[ 't' ] = TVTIME_TOGGLE_DEINTERLACER;
    ct->keymap[ 'p' ] = TVTIME_TOGGLE_PULLDOWN_DETECTION;
    ct->keymap[ 'o' ] = TVTIME_TOGGLE_NTSC_CABLE_MODE;
    ct->keymap[ ' ' ] = TVTIME_AUTO_ADJUST_PICT;
    ct->keymap[ '`' ] = TVTIME_TOGGLE_CONSOLE;
    ct->keymap[ I_PGUP ] = TVTIME_SCROLL_CONSOLE_UP;
    ct->keymap[ I_PGDN ] = TVTIME_SCROLL_CONSOLE_DOWN;
    ct->keymap[ 'w' ] = TVTIME_TOGGLE_CC;
    ct->keymap[ '=' ] = TVTIME_TOGGLE_FRAMERATE;
    ct->keymap[ I_END ] = TVTIME_TOGGLE_PAUSE;
    ct->keymap[ 'e' ] = TVTIME_TOGGLE_AUDIO_MODE;
    ct->keymap[ '<' ] = TVTIME_OVERSCAN_DOWN;
    ct->keymap[ '>' ] = TVTIME_OVERSCAN_UP;
    ct->keymap[ 'b' ] = TVTIME_TOGGLE_BARS;
    ct->keymap[ '*' ] = TVTIME_TOGGLE_MODE;
    ct->keymap[ '/' ] = TVTIME_AUTO_ADJUST_WINDOW;
    ct->keymap[ 'n' ] = TVTIME_TOGGLE_COMPATIBLE_NORM;

    memset( ct->buttonmap, 0, sizeof( ct->buttonmap ) );
    ct->buttonmap[ 1 ] = TVTIME_DISPLAY_INFO;
    ct->buttonmap[ 2 ] = TVTIME_TOGGLE_MUTE;
    ct->buttonmap[ 3 ] = TVTIME_TOGGLE_INPUT;
    ct->buttonmap[ 4 ] = TVTIME_CHANNEL_INC;
    ct->buttonmap[ 5 ] = TVTIME_CHANNEL_DEC;

    /* Make the ~/.tvtime directory every time on startup, to be safe. */
    asprintf( &temp_dirname, "%s/.tvtime", getenv( "HOME" ) );
    if( mkdir( temp_dirname, S_IRWXU ) < 0 ) {
        if( errno != EEXIST ) {
            fprintf( stderr, "config: Cannot create %s.\n", temp_dirname );
        } else {
            DIR *temp_dir = opendir( temp_dirname );
            if( !temp_dir ) {
                fprintf( stderr, "config: %s is not a directory.\n", 
                        temp_dirname );
            } else {
                closedir( temp_dir );
            }
        }
        /* If the directory already exists, we didn't need to create it. */
    } else {
        /* We created the directory, now force it to be owned by the user. */
        if( chown( temp_dirname, getuid(), getgid() ) < 0 ) {
            fprintf( stderr, "config: Cannot chown %s.\n"
                             "        %s",
                     temp_dirname, strerror( errno ) );
        }
    }
    free( temp_dirname );

    /* First read in global settings. */
    asprintf( &base, "%s/tvtime.xml", CONFDIR );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "config: Reading configuration from %s\n", base );
        conf_xml_parse( ct, base );
    }
    free( base );

    /* Then read in local settings. */
    asprintf( &base, "%s/.tvtime/tvtime.xml", getenv( "HOME" ) );
    ct->config_filename = strdup( base );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, "config: Reading configuration from %s\n", base );
        conf_xml_parse( ct, base );
    }
    free( base );

    return ct;
}

int config_parse_tvtime_command_line( config_t *ct, int argc, char **argv )
{
    char *configFile = 0;
    int saveoptions = 0;
    char c;

    while( (c = getopt( argc, argv, "ahkmMsSvF:r:H:I:d:b:i:c:n:D:f:x:" )) != -1 ) {
        switch( c ) {
        case 'a': ct->aspect = 1; break;
        case 'k': ct->slave_mode = 1; break;
        case 'm': ct->fullscreen = 1; break;
        case 'M': ct->fullscreen = 0; break;
        case 's': ct->debug = 1; break;
        case 'S': saveoptions = 1; break;
        case 'v': ct->verbose = 1; break;
        case 'F': if( configFile ) { free( configFile ); }
                  configFile = strdup( optarg ); break;
        case 'r': if( ct->rvr_filename ) { free( ct->rvr_filename ); }
                  ct->rvr_filename = strdup( optarg ); break;
        case 'x': if( ct->mixerdev ) { free( ct->mixerdev ); }
                  ct->mixerdev = strdup( optarg ); break;
        case 'H': ct->outputheight = atoi( optarg ); break;
        case 'I': ct->inputwidth = atoi( optarg ); break;
        case 'd': free( ct->v4ldev ); ct->v4ldev = strdup( optarg ); break;
        case 'b': ct->use_vbi = 1; free( ct->vbidev ); ct->vbidev = strdup( optarg ); break;
        case 'i': ct->inputnum = atoi( optarg ); break;
        case 'c': ct->prev_channel = ct->start_channel;
                  ct->start_channel = atoi( optarg ); break;
        case 'n': free( ct->norm ); ct->norm = strdup( optarg ); break;
        case 'D': if( ct->output_driver ) { free( ct->output_driver ); }
                  ct->output_driver = strdup( optarg ); break;
        case 'f': free( ct->freq ); ct->freq = strdup( optarg ); break;
        default:
            print_usage( argv );
            return 0;
        }
    }

    /* Then read in additional settings. */
    if( configFile ) {
        char *temp = expand_user_path( configFile );
        if( temp ) {
            free( configFile );
            configFile = temp;
        }
    }
    if( configFile ) {
        if( ct->config_filename ) free( ct->config_filename );
        ct->config_filename = configFile;

        fprintf( stderr, "config: Reading configuration from %s\n", configFile );
        conf_xml_parse( ct, configFile );
    }

    ct->doc = configsave_open( ct->config_filename );

    if( ct->doc && saveoptions ) {
        char tempstring[ 32 ];
        fprintf( stderr, "config: Saving command line options.\n" );

        /**
         * Options that aren't specified on the command line
         * will match the config file anyway, so save everything that
         * you can save on the command line.
         */
        snprintf( tempstring, sizeof( tempstring ), "%d", ct->aspect );
        config_save( ct, "Widescreen", tempstring );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->fullscreen );
        config_save( ct, "Fullscreen", tempstring );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->verbose );
        config_save( ct, "Verbose", tempstring );

        config_save( ct, "OutputDriver", ct->output_driver );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->outputheight );
        config_save( ct, "OutputHeight", tempstring );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->inputwidth );
        config_save( ct, "InputWidth", tempstring );

        config_save( ct, "V4LDevice", ct->v4ldev );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->use_vbi );
        config_save( ct, "UseVBI", tempstring );
        config_save( ct, "VBIDevice", ct->vbidev );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->inputnum );
        config_save( ct, "V4LInput", tempstring );

        config_save( ct, "Norm", ct->norm );
        config_save( ct, "Frequencies", ct->freq );

        config_save( ct, "MixerDevice", ct->mixerdev );
    }

    return 1;
}

void config_free_data( config_t *ct )
{
    if( ct->doc ) xmlFreeDoc( ct->doc );
    if( ct->v4ldev ) free( ct->v4ldev );
    if( ct->norm ) free( ct->norm );
    if( ct->freq ) free( ct->freq );
    if( ct->ssdir ) free( ct->ssdir );
    if( ct->timeformat ) free( ct->timeformat );
    if( ct->output_driver ) free( ct->output_driver );
    if( ct->rvr_filename ) free( ct->rvr_filename );
    if( ct->mixerdev ) free( ct->mixerdev );
    if( ct->vbidev ) free( ct->vbidev );
    if( ct->config_filename ) free( ct->config_filename );
    if( ct->deinterlace_method ) free( ct->deinterlace_method );
}

void config_delete( config_t *ct )
{
    while( ct->modelist ) {
        tvtime_modelist_t *mode = ct->modelist;
        ct->modelist = mode->next;
        config_free_data( &(mode->settings) );
        free( mode->name );
        free( mode );
    }
    config_free_data( ct );
    free( ct );
}

void config_save( config_t *ct, const char *name, const char *value )
{
    xmlNodePtr top, node;

    if( !ct->doc ) return;

    top = xmlDocGetRootElement( ct->doc );
    if( !top ) {
        return;
    }

    node = find_option( top->xmlChildrenNode, name );
    if( !node ) {
        node = xmlNewTextChild( top, 0, BAD_CAST "option", 0 );
        xmlNewProp( node, BAD_CAST "name", BAD_CAST name );
        xmlNewProp( node, BAD_CAST "value", BAD_CAST value );
    } else {
        xmlSetProp( node, BAD_CAST "value", BAD_CAST value );
    }

    xmlKeepBlanksDefault( 0 );
    xmlSaveFormatFile( ct->config_filename, ct->doc, 1 );
}

int config_key_to_command( config_t *ct, int key )
{
    if( key ) {
        if( ct->keymap[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ] ) {
            return ct->keymap[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ];
        }

        if( isalnum( key & 0x1ff ) ) {
            return TVTIME_CHANNEL_CHAR;
        }
    }
        
    return TVTIME_NOCOMMAND;
}

int config_command_to_key( config_t *ct, int command )
{
    int i;

    for( i = 0; i < 8 * MAX_KEYSYMS; i++ ) {
        if( ct->keymap[ i ] == command ) return i;
    }

    return 0;
}

int config_button_to_command( config_t *ct, int button )
{
    if( button < 0 || button >= MAX_BUTTONS ) {
        return 0;
    } else {
        return ct->buttonmap[ button ];
    }
}

int config_get_num_modes( config_t *ct )
{
    return ct->nummodes;
}

config_t *config_get_mode_info( config_t *ct, int mode )
{
    tvtime_modelist_t *cur = ct->modelist;

    if( !cur ) {
        /* No modes. */
        return 0;
    }

    while( mode && cur->next ) {
        cur = cur->next;
        mode--;
    }

    return &(cur->settings);
}

int config_get_verbose( config_t *ct )
{
    return ct->verbose;
}

int config_get_send_fields( config_t *ct )
{
    return ct->send_fields;
}

const char *config_get_output_driver( config_t *ct )
{
    return ct->output_driver;
}

int config_get_debug( config_t *ct )
{
    return ct->debug;
}

int config_get_outputheight( config_t *ct )
{
    return ct->outputheight;
}

int config_get_useposition( config_t *ct )
{
    return ct->useposition;
}

int config_get_output_x( config_t *ct )
{
    return ct->x;
}

int config_get_output_y( config_t *ct )
{
    return ct->x;
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

const char *config_get_deinterlace_method( config_t *ct )
{
    return ct->deinterlace_method;
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

unsigned int config_get_channel_text_rgb( config_t *ct )
{
    return ct->channel_text_rgb;
}

unsigned int config_get_other_text_rgb( config_t *ct )
{
    return ct->other_text_rgb;
}

uid_t config_get_uid( config_t *ct )
{
    return ct->uid;
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

double config_get_overscan( config_t *ct )
{
    return ct->overscan;
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

int config_get_slave_mode( config_t *ct )
{
    return ct->slave_mode;
}

const char *config_get_mixer_device( config_t *ct )
{
    return ct->mixerdev;
}

