/**
 * Copyright (C) 2002, 2003 Doug Bell <drbell@users.sourceforge.net>
 * Copyright (C) 2003 Alexander Belov <asbel@mail.ru>
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
#include <math.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
#else
# define _(string) string
#endif
#include "tvtimeconf.h"
#include "input.h"
#include "station.h"
#include "utils.h"

#define MAX_KEYSYMS 350
#define MAX_BUTTONS 15

struct config_s
{
    char *geometry;
    int verbose;
    int aspect;
    int debug;
    int fullscreen;
    int alwaysontop;
    int priority;
    int ntsc_mode;
    int send_fields;
    int apply_luma_correction;
    double luma_correction;
    int fspos;
    int picsaverestore;
    int brightness;
    int contrast;
    int saturation;
    int hue;
    int invert;
    int cc;
    int mirror;
    int boost;
    int quiet_screenshots;
    int unmute_volume;
    int muted;
    int mute_on_exit;

    int keymap[ 8 * MAX_KEYSYMS ];
    char *keymap_arg[ 8 * MAX_KEYSYMS ];
    int keymapmenu[ 8 * MAX_KEYSYMS ];
    char *keymapmenu_arg[ 8 * MAX_KEYSYMS ];
    int buttonmap[ MAX_BUTTONS ];
    char *buttonmap_arg[ MAX_BUTTONS ];
    int buttonmapmenu[ MAX_BUTTONS ];
    char *buttonmapmenu_arg[ MAX_BUTTONS ];

    int inputwidth;
    int inputnum;
    char *v4ldev;
    char *norm;
    char *freq;
    char *ssdir;
    char *timeformat;
    char *audiomode;
    char *xmltvfile;
    char *xmltvlanguage;
    unsigned int channel_text_rgb;
    unsigned int other_text_rgb;

    uid_t uid;

    char *mixerdev;

    char *deinterlace_method;
    int check_freq_present;

    int use_xds;
    char *vbidev;

    int start_channel;
    int prev_channel;
    int framerate;
    int slave_mode;

    double overscan;

    char *config_filename;
    xmlDocPtr doc;
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

        if( !xmlStrcasecmp( name, BAD_CAST "WindowGeometry" ) ) {
            if( ct->geometry ) free( ct->geometry );
            ct->geometry = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "InputWidth" ) ) {
            ct->inputwidth = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Verbose" ) ) {
            ct->verbose = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "SaveAndRestorePictureSettings" ) ) {
            ct->picsaverestore = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DefaultBrightness" ) ) {
            ct->brightness = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DefaultContrast" ) ) {
            ct->contrast = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DefaultColour" ) ) {
            ct->saturation = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DefaultSaturation" ) ) {
            ct->saturation = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DefaultHue" ) ) {
            ct->hue = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ColourInvert" ) ) {
            ct->invert = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "MirrorInput" ) ) {
            ct->mirror = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "AudioBoost" ) ) {
            ct->boost = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "QuietScreenshots" ) ) {
            ct->quiet_screenshots = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "UnmuteVolume" ) ) {
            ct->unmute_volume = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Muted" ) ) {
            ct->muted = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "MuteOnExit" ) ) {
            ct->mute_on_exit = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ShowCC" ) ) {
            ct->cc = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "DFBSendFields" ) ) {
            ct->send_fields = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "AudioMode" ) ) {
            if( ct->audiomode ) free( ct->audiomode );
            ct->audiomode = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "XMLTVFile" ) ) {
            if( ct->xmltvfile ) free( ct->xmltvfile );
            ct->xmltvfile = expand_user_path( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "XMLTVLanguage" ) ) {
            if( ct->xmltvlanguage ) free( ct->xmltvlanguage );
            ct->xmltvlanguage = strdup( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "FullscreenPosition" ) ) {
            if( tolower( curval[ 0 ] ) == 't' ) {
                ct->fspos = 1;
            } else if( tolower( curval[ 0 ] ) == 'b' ) {
                ct->fspos = 2;
            } else {
                ct->fspos = 0;
            }
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
            if( !isnormal( ct->luma_correction ) ) {
                ct->luma_correction = 1.0;
            }
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

        if( !xmlStrcasecmp( name, BAD_CAST "UseXDS" ) ) {
            ct->use_xds = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "ProcessPriority" ) ) {
            ct->priority = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "Fullscreen" ) ) {
            ct->fullscreen = atoi( curval );
        }

        if( !xmlStrcasecmp( name, BAD_CAST "AlwaysOnTop" ) ) {
            ct->alwaysontop = atoi( curval );
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
            if( !isnormal( ct->overscan ) ) {
                ct->overscan = 0.0;
            }
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
    xmlChar *arg = xmlGetProp( node, BAD_CAST "argument" );

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
                    if( tvtime_is_menu_command( command_id ) ) {
                        ct->keymapmenu[ keycode ] = command_id;
                        if( arg ) ct->keymapmenu_arg[ keycode ] = strdup( (const char *) arg );
                    } else if( command_id != TVTIME_NOCOMMAND ) {
                        ct->keymap[ keycode ] = command_id;
                        if( arg ) ct->keymap_arg[ keycode ] = strdup( (const char *) arg );
                    }
                }
            } else if( !xmlStrcasecmp( binding->name, BAD_CAST "mouse" ) ) {
                xmlChar *button = xmlGetProp( binding, BAD_CAST "button" );
                if( button ) {
                    int id = atoi( (const char *) button );
                    if( (id > 0) && (id < MAX_BUTTONS) ) {
                        int command_id = tvtime_string_to_command( (const char *) command );
                        if( tvtime_is_menu_command( command_id ) ) {
                            ct->buttonmapmenu[ id ] = command_id;
                            if( arg ) ct->buttonmapmenu_arg[ id ] = strdup( (const char *) arg );
                        } else {
                            ct->buttonmap[ id ] = command_id;
                            if( arg ) ct->buttonmap_arg[ id ] = strdup( (const char *) arg );
                        }
                    }
                    xmlFree( button );
                }
            }
            binding = binding->next;
        }

        xmlFree( command );
    }
    if( arg ) xmlFree( arg );
}

static int conf_xml_parse( config_t *ct, char *configFile )
{
    xmlDocPtr doc;
    xmlNodePtr top, node;

    doc = xmlParseFile( configFile );
    if( !doc ) {
        lfprintf( stderr, _("Error parsing configuration file %s.\n"),
                  configFile );
        return 0;
    }

    top = xmlDocGetRootElement( doc );
    if( !top ) {
        lfprintf( stderr, _("No XML root element found in %s.\n"),
                  configFile );
        xmlFreeDoc( doc );
        return 0;
    }

    if( xmlStrcasecmp( top->name, BAD_CAST "tvtime" ) ) {
        lfprintf( stderr,
                  _("%s is not a tvtime configuration file.\n"),
                  configFile );
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

    xmlFreeDoc( doc );
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
            lfputs( _("Config file cannot be parsed. "
                      "Settings will not be saved.\n"), stderr );
            return 0;
        } else {
            /* Config file doesn't exist, create a new one. */
            doc = xmlNewDoc( BAD_CAST "1.0" );
            if( !doc ) {
                lfputs( _("Could not create new config file.\n"), stderr );
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
            lfputs( _("Error creating configuration file.\n"), stderr );
            xmlFreeDoc( doc );
            return 0;
        } else {
            xmlDocSetRootElement( doc, top );
            xmlNewProp( top, BAD_CAST "xmlns",
                        BAD_CAST "http://tvtime.sourceforge.net/DTD/" );
        }
    }

    if( xmlStrcasecmp( top->name, BAD_CAST "tvtime" ) ) {
        lfprintf( stderr, _("%s is not a tvtime configuration file.\n"),
                  config_filename );
        xmlFreeDoc( doc );
        return 0;
    }

    xmlKeepBlanksDefault( 0 );
    xmlSaveFormatFile( config_filename, doc, 1 );
    if( create_file ) {
        if( chown( config_filename, getuid(), getgid() ) < 0 ) {
            lfprintf( stderr, _("Cannot change owner of %s: %s.\n"),
                      config_filename, strerror( errno ) );
        }
    }
    return doc;
}

static void print_copyright( void )
{
    lfputs (_("\n"
              "tvtime is free software, written by Billy Biggs, Doug Bell and many\n"
              "others.  For details and copying conditions, please see our website\n"
              "at http://tvtime.net/\n"
              "\n"
              "tvtime is Copyright (C) 2001, 2002, 2003 by Billy Biggs, Doug Bell,\n"
              "Alexander S. Belov, and Achim Schneider.\n"), stderr );
}
 
static void print_usage( char **argv )
{
    lfprintf( stderr, _("usage: %s [OPTION]...\n\n"), argv[ 0 ] );
    lfputs( _("  -a, --widescreen           16:9 mode.\n"), stderr );
    lfputs( _("  -b, --vbidevice=DEVICE     VBI device (defaults to /dev/vbi0).\n"), stderr );
    lfputs( _("  -c, --channel=CHANNEL      Tune to the specified channel on startup.\n"), stderr );
    lfputs( _("  -d, --device=DEVICE        video4linux device (defaults to /dev/video0).\n"), stderr );
    lfputs( _("  -f, --frequencies=NAME     The frequency table to use for the tuner.\n"
              "                             (defaults to us-cable).\n\n"
              "                             Valid values are:\n"
              "                                 us-cable\n"
              "                                 us-cable100\n"
              "                                 us-broadcast\n"
              "                                 china-broadcast\n"
              "                                 japan-cable\n"
              "                                 japan-broadcast\n"
              "                                 europe\n"
              "                                 australia\n"
              "                                 australia-optus\n"
              "                                 newzealand\n"
              "                                 france\n"
              "                                 russia\n"
              "                                 custom (first run tvtime-scanner)\n\n"), stderr );
    lfputs( _("  -F, --configfile=FILE      Additional config file to load settings from.\n"), stderr );
    lfputs( _("  -h, --help                 Show this help message.\n"), stderr );
    lfputs( _("  -g, --geometry=GEOMETRY    Sets the output window size.\n"), stderr );
    lfputs( _("  -i, --input=INPUTNUM       video4linux input number (defaults to 0).\n"), stderr );
    lfputs( _("  -I, --inputwidth=SAMPLING  Horizontal resolution of input\n"
              "                             (defaults to 720 pixels).\n"), stderr );
    lfputs( _("  -k, --slave                Disables input handling in tvtime (slave mode).\n"), stderr );
    lfputs( _("  -m, --fullscreen           Start tvtime in fullscreen mode.\n"), stderr );
    lfputs( _("  -M, --window               Start tvtime in window mode.\n"), stderr );
    lfputs( _("  -n, --norm=NORM            The norm to use for the input.  tvtime supports:\n"
              "                             NTSC, NTSC-JP, SECAM, PAL, PAL-Nc, PAL-M,\n"
              "                             PAL-N or PAL-60 (defaults to NTSC).\n"), stderr );
    lfputs( _("  -s, --showdrops            Print stats on frame drops (for debugging).\n"), stderr );
    lfputs( _("  -S, --saveoptions          Save command line options to the config file.\n"), stderr );
    lfputs( _("  -t, --xmltv=FILE           Read XMLTV listings from the given file.\n"), stderr );
    lfputs( _("  -l, --xmltvlanguage=LANG   Use XMLTV data in given language, if available.\n"), stderr );
    lfputs( _("  -v, --verbose              Print debugging messages to stderr.\n"), stderr );
    lfputs( _("  -X, --display=DISPLAY      Use the given X display to connect to.\n"), stderr );
    lfputs( _("  -x, --mixer=DEVICE[:CH]    The mixer device and channel to control.\n"
              "                             (defaults to /dev/mixer:line)\n\n"
              "                             Valid channels are:\n"
              "                                 vol, bass, treble, synth, pcm, speaker, line,\n"
              "                                 mic, cd, mix, pcm2, rec, igain, ogain, line1,\n"
              "                                 line2, line3, dig1, dig2, dig3, phin, phout,\n"
              "                                 video, radio, monitor\n"), stderr );
}

static void print_config_usage( char **argv )
{
    lfprintf( stderr, _("usage: %s [OPTION]...\n\n"), argv[ 0 ] );
    lfputs( _("  -a, --widescreen           16:9 mode.\n"), stderr );
    lfputs( _("  -b, --vbidevice=DEVICE     VBI device (defaults to /dev/vbi0).\n"), stderr );
    lfputs( _("  -c, --channel=CHANNEL      Tune to the specified channel on startup.\n"), stderr );
    lfputs( _("  -d, --device=DEVICE        video4linux device (defaults to /dev/video0).\n"), stderr );
    lfputs( _("  -f, --frequencies=NAME     The frequency table to use for the tuner.\n"
              "                             (defaults to us-cable).\n\n"
              "                             Valid values are:\n"
              "                                 us-cable\n"
              "                                 us-cable100\n"
              "                                 us-broadcast\n"
              "                                 china-broadcast\n"
              "                                 japan-cable\n"
              "                                 japan-broadcast\n"
              "                                 europe\n"
              "                                 australia\n"
              "                                 australia-optus\n"
              "                                 newzealand\n"
              "                                 france\n"
              "                                 russia\n"
              "                                 custom (first run tvtime-scanner)\n\n"), stderr );
    lfputs( _("  -F, --configfile=FILE      Additional config file to load settings from.\n"), stderr );
    lfputs( _("  -h, --help                 Show this help message.\n"), stderr );
    lfputs( _("  -g, --geometry=GEOMETRY    Sets the output window size.\n"), stderr );
    lfputs( _("  -i, --input=INPUTNUM       video4linux input number (defaults to 0).\n"), stderr );
    lfputs( _("  -I, --inputwidth=SAMPLING  Horizontal resolution of input\n"
              "                             (defaults to 720 pixels).\n"), stderr );
    lfputs( _("  -m, --fullscreen           Start tvtime in fullscreen mode.\n"), stderr );
    lfputs( _("  -M, --window               Start tvtime in window mode.\n"), stderr );
    lfputs( _("  -n, --norm=NORM            The norm to use for the input.  tvtime supports:\n"
              "                             NTSC, NTSC-JP, SECAM, PAL, PAL-Nc, PAL-M,\n"
              "                             PAL-N or PAL-60 (defaults to NTSC).\n"), stderr );
    lfputs( _("  -R, --priority=PRI         Sets the process priority to run tvtime at.\n"), stderr );
    lfputs( _("  -t, --xmltv=FILE           Read XMLTV listings from the given file.\n"), stderr );
    lfputs( _("  -l, --xmltvlanguage=LANG   Use XMLTV data in given language, if available.\n"), stderr );
    lfputs( _("  -x, --mixer=DEVICE[:CH]    The mixer device and channel to control.\n"
              "                             (defaults to /dev/mixer:line)\n\n"
              "                             Valid channels are:\n"
              "                                 vol, bass, treble, synth, pcm, speaker, line,\n"
              "                                 mic, cd, mix, pcm2, rec, igain, ogain, line1,\n"
              "                                 line2, line3, dig1, dig2, dig3, phin, phout,\n"
              "                                 video, radio, monitor\n"), stderr );
}

static void print_scanner_usage( char **argv )
{
    lfprintf( stderr, _("usage: %s [OPTION]...\n\n"), argv[ 0 ]);
    lfputs( _("  -d, --device=DEVICE        video4linux device (defaults to /dev/video0).\n"), stderr );
    lfputs( _("  -F, --configfile=FILE      Additional config file to load settings from.\n"), stderr );
    lfputs( _("  -h, --help                 Show this help message.\n"), stderr );
    lfputs( _("  -i, --input=INPUTNUM       video4linux input number (defaults to 0).\n"), stderr );
    lfputs( _("  -n, --norm=NORM            The norm to use for the input.  tvtime supports:\n"
              "                             NTSC, NTSC-JP, SECAM, PAL, PAL-Nc, PAL-M,\n"
              "                             PAL-N or PAL-60 (defaults to NTSC).\n"), stderr );
}

config_t *config_new( void )
{
    char *temp_dirname;
    char *base;

    config_t *ct = malloc( sizeof( config_t ) );
    if( !ct ) {
        return 0;
    }

    /* Default settings. */
    ct->geometry = strdup( "0x576" );
    ct->verbose = 0;
    ct->aspect = 0;
    ct->debug = 0;
    ct->fullscreen = 0;
    ct->alwaysontop = 0;
    ct->priority = -19;
    ct->ntsc_mode = 0;
    ct->send_fields = 0;
    ct->apply_luma_correction = 0;
    ct->luma_correction = 1.0;
    ct->fspos = 0;
    ct->picsaverestore = 1;
    ct->brightness = -1;
    ct->contrast = -1;
    ct->saturation = -1;
    ct->hue = -1;
    ct->invert = 0;
    ct->cc = 0;
    ct->mirror = 0;
    ct->boost = -1;
    ct->quiet_screenshots = 0;
    ct->unmute_volume = -1;
    ct->muted = 0;
    ct->mute_on_exit = 1;

    memset( ct->keymap, 0, sizeof( ct->keymap ) );
    memset( ct->keymap_arg, 0, sizeof( ct->keymap_arg ) );
    memset( ct->keymapmenu, 0, sizeof( ct->keymapmenu ) );
    memset( ct->keymapmenu_arg, 0, sizeof( ct->keymapmenu_arg ) );
    memset( ct->buttonmap, 0, sizeof( ct->buttonmap ) );
    memset( ct->buttonmap_arg, 0, sizeof( ct->buttonmap_arg ) );
    memset( ct->buttonmapmenu, 0, sizeof( ct->buttonmapmenu ) );
    memset( ct->buttonmapmenu_arg, 0, sizeof( ct->buttonmapmenu_arg ) );

    ct->inputwidth = 720;
    ct->inputnum = 0;
    ct->v4ldev = strdup( "/dev/video0" );
    ct->norm = strdup( "ntsc" );
    ct->freq = strdup( "us-cable" );
    temp_dirname = getenv( "HOME" );
    if( temp_dirname ) {
        ct->ssdir = strdup( temp_dirname );
    } else {
        ct->ssdir = 0;
    }
    ct->audiomode = strdup( "stereo" );
    ct->xmltvfile = strdup( "none" );
    ct->xmltvlanguage = strdup( "none" );
    ct->timeformat = strdup( "%X" );
    ct->channel_text_rgb = 0xffffff00; /* opaque yellow */
    ct->other_text_rgb = 0xfff5deb3;   /* opaque wheat */

    ct->uid = getuid();

    ct->mixerdev = strdup( "/dev/mixer:line" );

    ct->deinterlace_method = strdup( "GreedyH" );
    ct->check_freq_present = 1;

    ct->use_xds = 0;
    ct->vbidev = strdup( "/dev/vbi0" );

    ct->start_channel = 1;
    ct->prev_channel = 1;
    ct->framerate = FRAMERATE_FULL;
    ct->slave_mode = 0;

    ct->overscan = 0.0;

    ct->config_filename = 0;
    ct->doc = 0;

    /* Default key bindings. */
    ct->keymap[ 0 ] = TVTIME_NOCOMMAND;
    ct->keymap[ I_ESCAPE ] = TVTIME_QUIT;
    ct->keymap[ 'q' ] = TVTIME_QUIT;
    ct->keymap[ I_UP ] = TVTIME_UP;
    ct->keymap[ I_DOWN ] = TVTIME_DOWN;
    ct->keymap[ I_LEFT ] = TVTIME_LEFT;
    ct->keymap[ I_RIGHT ] = TVTIME_RIGHT;
    ct->keymap[ I_BACKSPACE ] = TVTIME_CHANNEL_PREV;
    ct->keymap[ 'c' ] = TVTIME_TOGGLE_CC;
    ct->keymap[ 'm' ] = TVTIME_TOGGLE_MUTE;
    ct->keymap[ '-' ] = TVTIME_MIXER_DOWN;
    ct->keymap[ '+' ] = TVTIME_MIXER_UP;
    ct->keymap[ ' ' ] = TVTIME_AUTO_ADJUST_PICT;
    ct->keymap[ I_ENTER ] = TVTIME_ENTER;
    ct->keymap[ I_F1 ] = TVTIME_SHOW_MENU;
    ct->keymap[ 'h' ] = TVTIME_SHOW_MENU;
    ct->keymap[ '\t' ] = TVTIME_SHOW_MENU;
    ct->keymap[ I_F5 ] = TVTIME_PICTURE;
    ct->keymap[ I_F6 ] = TVTIME_PICTURE_DOWN;
    ct->keymap[ I_F7 ] = TVTIME_PICTURE_UP;
    ct->keymap[ 'r' ] = TVTIME_CHANNEL_RENUMBER;
    ct->keymap[ 'd' ] = TVTIME_SHOW_STATS;
    ct->keymap[ 'f' ] = TVTIME_TOGGLE_FULLSCREEN;
    ct->keymap[ 'i' ] = TVTIME_TOGGLE_INPUT;
    ct->keymap[ 'a' ] = TVTIME_TOGGLE_ASPECT;
    ct->keymap[ 's' ] = TVTIME_SCREENSHOT;
    ct->keymap[ 't' ] = TVTIME_TOGGLE_DEINTERLACER;
    ct->keymap[ 'p' ] = TVTIME_TOGGLE_PULLDOWN_DETECTION;
    ct->keymap[ ',' ] = TVTIME_MIXER_TOGGLE_MUTE;
    ct->keymap[ '=' ] = TVTIME_TOGGLE_FRAMERATE;
    ct->keymap[ I_END ] = TVTIME_TOGGLE_PAUSE;
    ct->keymap[ 'e' ] = TVTIME_TOGGLE_AUDIO_MODE;
    ct->keymap[ '<' ] = TVTIME_OVERSCAN_DOWN;
    ct->keymap[ '>' ] = TVTIME_OVERSCAN_UP;
    ct->keymap[ 'b' ] = TVTIME_TOGGLE_BARS;
    ct->keymap[ '/' ] = TVTIME_AUTO_ADJUST_WINDOW;
    ct->keymap[ I_INSERT ] = TVTIME_TOGGLE_MATTE;
    ct->keymap[ 'v' ] = TVTIME_TOGGLE_ALWAYSONTOP;
    ct->keymap[ '0' ] = TVTIME_CHANNEL_0;
    ct->keymap[ '1' ] = TVTIME_CHANNEL_1;
    ct->keymap[ '2' ] = TVTIME_CHANNEL_2;
    ct->keymap[ '3' ] = TVTIME_CHANNEL_3;
    ct->keymap[ '4' ] = TVTIME_CHANNEL_4;
    ct->keymap[ '5' ] = TVTIME_CHANNEL_5;
    ct->keymap[ '6' ] = TVTIME_CHANNEL_6;
    ct->keymap[ '7' ] = TVTIME_CHANNEL_7;
    ct->keymap[ '8' ] = TVTIME_CHANNEL_8;
    ct->keymap[ '9' ] = TVTIME_CHANNEL_9;

    ct->buttonmap[ 1 ] = TVTIME_DISPLAY_INFO;
    ct->buttonmap[ 2 ] = TVTIME_TOGGLE_MUTE;
    ct->buttonmap[ 3 ] = TVTIME_SHOW_MENU;
    ct->buttonmap[ 4 ] = TVTIME_CHANNEL_INC;
    ct->buttonmap[ 5 ] = TVTIME_CHANNEL_DEC;

    /* Menu keys. */
    ct->keymapmenu[ I_UP ] = TVTIME_MENU_UP;
    ct->keymapmenu[ I_DOWN ] = TVTIME_MENU_DOWN;
    ct->keymapmenu[ I_LEFT ] = TVTIME_MENU_BACK;
    ct->keymapmenu[ I_RIGHT ] = TVTIME_MENU_ENTER;
    ct->keymapmenu[ I_ENTER ] = TVTIME_MENU_ENTER;
    ct->keymapmenu[ I_F1 ] = TVTIME_MENU_EXIT;
    ct->keymapmenu[ '\t' ] = TVTIME_MENU_EXIT;
    ct->keymapmenu[ 'q' ] = TVTIME_MENU_EXIT;
    ct->keymapmenu[ I_ESCAPE ] = TVTIME_MENU_EXIT;
    ct->buttonmapmenu[ 1 ] = TVTIME_MENU_ENTER;
    ct->buttonmapmenu[ 3 ] = TVTIME_MENU_ENTER;
    ct->buttonmapmenu[ 4 ] = TVTIME_MENU_UP;
    ct->buttonmapmenu[ 5 ] = TVTIME_MENU_DOWN;

    /* Make the ~/.tvtime directory every time on startup, to be safe. */
    if( asprintf( &temp_dirname, "%s/.tvtime", getenv( "HOME" ) ) < 0 ) {
        /* FIXME: Clean up ?? */
        return 0;
    }
    mkdir_and_force_owner( temp_dirname, ct->uid, getgid() );
    free( temp_dirname );

    /* First read in global settings. */
    asprintf( &base, "%s/tvtime.xml", CONFDIR );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, _("Reading configuration from %s\n"), base );
        conf_xml_parse( ct, base );
    }
    free( base );

    /* Then read in local settings. */
    asprintf( &base, "%s/.tvtime/tvtime.xml", getenv( "HOME" ) );
    ct->config_filename = strdup( base );
    if( file_is_openable_for_read( base ) ) {
        fprintf( stderr, _("Reading configuration from %s\n"), base );
        conf_xml_parse( ct, base );
    }
    free( base );

    return ct;
}

int config_parse_tvtime_command_line( config_t *ct, int argc, char **argv )
{
    static struct option long_options[] = {
        { "help", 0, 0, 'h' },
        { "verbose", 0, 0, 'v' },
        { "geometry", 1, 0, 'g' },
        { "saveoptions", 0, 0, 'S' },
        { "inputwidth", 1, 0, 'I' },
        { "driver", 1, 0, 'D' },
        { "input", 1, 0, 'i' },
        { "channel", 1, 0, 'c' },
        { "configfile", 1, 0, 'F' },
        { "norm", 1, 0, 'n' },
        { "frequencies", 1, 0, 'f' },
        { "vbidevice", 1, 0, 'b' },
        { "device", 1, 0, 'd' },
        { "mixer", 1, 0, 'x' },
        { "showdrops", 0, 0, 's' },
        { "fullscreen", 0, 0, 'm' },
        { "window", 0, 0, 'M' },
        { "slave", 0, 0, 'k' },
        { "widescreen", 0, 0, 'a' },
        { "xmltv", 1, 0, 't' },
        { "xmltvlanguage", 1, 0, 'l' },
        { "display", 1, 0, 'X' },
        { "version", 0, 0, 'Q' },
        { 0, 0, 0, 0 }
    };
    int option_index = 0;
    int saveoptions = 0;
    int filename_specified = 0;
    char c;

    if( argc ) {
        while( (c = getopt_long( argc, argv, "ahkmMsSvF:r:g:I:d:b:i:c:n:D:f:x:X:t:l:Qg:",
                long_options, &option_index )) != -1 ) {
            switch( c ) {
            case 'a': ct->aspect = 1; break;
            case 'k': ct->slave_mode = 1; break;
            case 'm': ct->fullscreen = 1; break;
            case 'M': ct->fullscreen = 0; break;
            case 's': ct->debug = 1; break;
            case 'S': saveoptions = 1; break;
            case 'v': ct->verbose = 1; break;
            case 't': if( ct->xmltvfile ) { free( ct->xmltvfile ); }
                      ct->xmltvfile = expand_user_path( optarg ); break;
            case 'l': if( ct->xmltvlanguage ) { free( ct->xmltvlanguage ); }
                      ct->xmltvlanguage = strdup( optarg ); break;
            case 'F': if( ct->config_filename ) free( ct->config_filename );
                      filename_specified = 1;
                      ct->config_filename = expand_user_path( optarg );
                      if( ct->config_filename ) {
                          lfprintf
                              ( stderr,
                                _("Reading configuration from %s\n"),
                                ct->config_filename );
                          conf_xml_parse( ct, ct->config_filename );
                      }
                      break;
            case 'x': if( ct->mixerdev ) { free( ct->mixerdev ); }
                      ct->mixerdev = strdup( optarg ); break;
            case 'X': setenv( "DISPLAY", optarg, 1 ); break;
            case 'g': if( ct->geometry ) { free( ct->geometry ); }
                      ct->geometry = strdup( optarg ); break;
            case 'I': ct->inputwidth = atoi( optarg ); break;
            case 'd': free( ct->v4ldev ); ct->v4ldev = strdup( optarg ); break;
            case 'b': free( ct->vbidev ); ct->vbidev = strdup( optarg ); break;
            case 'i': ct->inputnum = atoi( optarg ); break;
            case 'c': ct->prev_channel = ct->start_channel;
                      ct->start_channel = atoi( optarg ); break;
            case 'n': free( ct->norm ); ct->norm = strdup( optarg ); break;
            case 'f': free( ct->freq ); ct->freq = strdup( optarg ); break;
            case 'Q': print_copyright(); return 0;
            default:
                print_usage( argv );
                return 0;
            }
        }
    }

    if( !filename_specified ) {
        char *fifofile = get_tvtime_fifo_filename( config_get_uid( ct ) );
        int fifofd;

        if( !fifofile ) {
            lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
            return 0;
        }

        fifofd = open( fifofile, O_WRONLY | O_NONBLOCK );
        if( fifofd >= 0 ) {
            lfprintf( stderr,
                      _("Cannot run two instances of tvtime with the same configuration.\n") );
            return 0;
        }
        close( fifofd );
    }


    ct->doc = configsave_open( ct->config_filename );

    if( ct->doc && saveoptions ) {
        char tempstring[ 32 ];
        lfputs( _("Saving command line options.\n"), stderr );

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

        config_save( ct, "WindowGeometry", ct->geometry );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->inputwidth );
        config_save( ct, "InputWidth", tempstring );

        config_save( ct, "V4LDevice", ct->v4ldev );
        config_save( ct, "VBIDevice", ct->vbidev );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->inputnum );
        config_save( ct, "V4LInput", tempstring );

        config_save( ct, "Norm", ct->norm );
        config_save( ct, "Frequencies", ct->freq );

        config_save( ct, "MixerDevice", ct->mixerdev );

        config_save( ct, "XMLTVFile", ct->xmltvfile );
        config_save( ct, "XMLTVLanguage", ct->xmltvlanguage );
    }

    return 1;
}

int config_parse_tvtime_config_command_line( config_t *ct, int argc, char **argv )
{
    static struct option long_options[] = {
        { "help", 0, 0, 'h' },
        { "geometry", 1, 0, 'g' },
        { "inputwidth", 1, 0, 'I' },
        { "driver", 1, 0, 'D' },
        { "input", 2, 0, 'i' },
        { "channel", 1, 0, 'c' },
        { "configfile", 1, 0, 'F' },
        { "norm", 2, 0, 'n' },
        { "frequencies", 2, 0, 'f' },
        { "vbidevice", 2, 0, 'b' },
        { "device", 2, 0, 'd' },
        { "mixer", 1, 0, 'x' },
        { "fullscreen", 0, 0, 'm' },
        { "window", 0, 0, 'M' },
        { "widescreen", 0, 0, 'a' },
        { "xmltv", 2, 0, 't' },
        { "xmltvlanguage", 2, 0, 'l' },
        { "priority", 2, 0, 'R' },
        { 0, 0, 0, 0 }
    };
    int option_index = 0;
    int filename_specified = 0;
    char c;

    if( argc == 1 ) {
        print_config_usage( argv );
        return 0;
    }

    while( (c = getopt_long( argc, argv, "ahmMF:g:I:d:b:i:c:n:D:f:x:t:l:R:",
            long_options, &option_index )) != -1 ) {
        switch( c ) {
        case 'a': ct->aspect = 1; break;
        case 'm': ct->fullscreen = 1; break;
        case 'M': ct->fullscreen = 0; break;
        case 'F': if( ct->config_filename ) free( ct->config_filename );
                  filename_specified = 1;
                  ct->config_filename = expand_user_path( optarg );
                  if( ct->config_filename ) {
                      lfprintf( stderr,
                                _("Reading configuration from %s\n"),
                                ct->config_filename );
                      conf_xml_parse( ct, ct->config_filename );
                  }
                  break;
        case 'x': if( ct->mixerdev ) { free( ct->mixerdev ); }
                  ct->mixerdev = strdup( optarg ); break;
        case 'g': if( ct->geometry ) { free( ct->geometry ); }
                  ct->geometry = strdup( optarg ); break;
        case 'I': ct->inputwidth = atoi( optarg ); break;
        case 'd': if( !optarg ) {
                      fprintf( stdout, "V4LDevice:%s\n",
                               config_get_v4l_device( ct ) );
                  } else {
                      free( ct->v4ldev );
                      ct->v4ldev = strdup( optarg );
                  }
                  break;
        case 'b': if( !optarg ) {
                      fprintf( stdout, "VBIDevice:%s\n",
                               config_get_vbi_device( ct ) );
                  } else {
                      free( ct->vbidev );
                      ct->vbidev = strdup( optarg );
                  }
                  break;
        case 'i': if( !optarg ) {
                      fprintf( stdout, "V4LInput:%d\n",
                               config_get_inputnum( ct ) );
                  } else {
                      ct->inputnum = atoi( optarg );
                  }
                  break;
        case 'c': ct->prev_channel = ct->start_channel;
                  ct->start_channel = atoi( optarg ); break;
        case 't': if( !optarg ) {
                      fprintf( stdout, "XMLTVFile:%s\n",
                               config_get_xmltv_file( ct ) );
                  } else {
                      if( ct->xmltvfile ) free( ct->xmltvfile );
                      ct->xmltvfile = expand_user_path( optarg );
                  }
                  break;
        case 'l': if( !optarg ) {
                      fprintf( stdout, "XMLTVLanguage:%s\n",
                               config_get_xmltv_language( ct ) );
                  } else {
                      if( ct->xmltvlanguage ) free( ct->xmltvlanguage );
                      ct->xmltvlanguage = strdup( optarg );
                  }
                  break;
        case 'n': if( !optarg ) {
                      fprintf( stdout, "Norm:%s\n", config_get_v4l_norm( ct ) );
                  } else {
                      free( ct->norm );
                      ct->norm = strdup( optarg );
                  }
                  break;
        case 'f': if( !optarg ) {
                      fprintf( stdout, "Frequencies:%s\n",
                               config_get_v4l_freq( ct ) );
                  } else {
                      free( ct->freq );
                      ct->freq = strdup( optarg );
                  }
                  break;
        case 'R': if( !optarg ) {
                      fprintf( stdout, "Priority:%d\n",
                               config_get_priority( ct ) );
                  } else {
                      ct->priority = atoi( optarg );
                  }
                  break;
        default:
            print_config_usage( argv );
            return 0;
        }
    }

    if( !filename_specified ) {
        char *fifofile = get_tvtime_fifo_filename( config_get_uid( ct ) );
        int fifofd;

        if( !fifofile ) {
            lfprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
            return 0;
        }

        fifofd = open( fifofile, O_WRONLY | O_NONBLOCK );
        if( fifofd >= 0 ) {
            lfprintf( stderr,
                      _("Cannot update configuration while tvtime running.\n") );
            return 0;
        }
        close( fifofd );
    }

    ct->doc = configsave_open( ct->config_filename );

    if( ct->doc ) {
        char tempstring[ 32 ];

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

        config_save( ct, "WindowGeometry", ct->geometry );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->inputwidth );
        config_save( ct, "InputWidth", tempstring );

        config_save( ct, "V4LDevice", ct->v4ldev );

        config_save( ct, "VBIDevice", ct->vbidev );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->inputnum );
        config_save( ct, "V4LInput", tempstring );

        config_save( ct, "Norm", ct->norm );
        config_save( ct, "Frequencies", ct->freq );

        config_save( ct, "MixerDevice", ct->mixerdev );

        config_save( ct, "XMLTVFile", ct->xmltvfile );
        config_save( ct, "XMLTVLanguage", ct->xmltvlanguage );

        snprintf( tempstring, sizeof( tempstring ), "%d", ct->priority );
        config_save( ct, "ProcessPriority", tempstring );
    }

    return 1;
}

int config_parse_tvtime_scanner_command_line( config_t *ct, int argc,
                                              char **argv )
{
    static struct option long_options[] = {
        { "help", 0, 0, 'h' },
        { "height", 1, 0, 'H' },
        { "input", 1, 0, 'i' },
        { "configfile", 1, 0, 'F' },
        { "norm", 1, 0, 'n' },
        { "device", 1, 0, 'd' },
        { 0, 0, 0, 0 }
    };
    int option_index = 0;
    char c;

    while( (c = getopt_long( argc, argv, "hF:d:i:n:",
            long_options, &option_index )) != -1 ) {
        switch( c ) {
        case 'F': if( ct->config_filename ) free( ct->config_filename );
                  ct->config_filename = expand_user_path( optarg );
                  if( ct->config_filename ) {
                      lfprintf( stderr,
                                _("Reading configuration from %s\n"),
                                ct->config_filename );
                      conf_xml_parse( ct, ct->config_filename );
                  }
                  break;
        case 'd': free( ct->v4ldev ); ct->v4ldev = strdup( optarg ); break;
        case 'i': ct->inputnum = atoi( optarg ); break;
        case 'n': free( ct->norm ); ct->norm = strdup( optarg ); break;
        default:
            print_scanner_usage( argv );
            return 0;
        }
    }

    return 1;
}



void config_free_data( config_t *ct )
{
    if( ct->doc ) xmlFreeDoc( ct->doc );
    if( ct->geometry ) free( ct->geometry );
    if( ct->v4ldev ) free( ct->v4ldev );
    if( ct->norm ) free( ct->norm );
    if( ct->freq ) free( ct->freq );
    if( ct->ssdir ) free( ct->ssdir );
    if( ct->audiomode ) free( ct->audiomode );
    if( ct->xmltvfile ) free( ct->xmltvfile );
    if( ct->xmltvlanguage ) free( ct->xmltvlanguage );
    if( ct->timeformat ) free( ct->timeformat );
    if( ct->mixerdev ) free( ct->mixerdev );
    if( ct->vbidev ) free( ct->vbidev );
    if( ct->config_filename ) free( ct->config_filename );
    if( ct->deinterlace_method ) free( ct->deinterlace_method );
}

void config_delete( config_t *ct )
{
    int i;

    for( i = 0; i < 8 * MAX_KEYSYMS; i++ ) {
        if( ct->keymap_arg[ i ] ) free( ct->keymap_arg[ i ] );
        if( ct->keymapmenu_arg[ i ] ) free( ct->keymapmenu_arg[ i ] );
    }

    for( i = 0; i < MAX_BUTTONS; i++ ) {
        if( ct->buttonmap_arg[ i ] ) free( ct->buttonmap_arg[ i ] );
        if( ct->buttonmapmenu_arg[ i ] ) free( ct->buttonmapmenu_arg[ i ] );
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

const char *config_key_to_command_argument( config_t *ct, int key )
{
    if( key ) {
        if( ct->keymap_arg[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ] ) {
            return ct->keymap_arg[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ];
        }
    }
    return 0;
}

int config_key_to_menu_command( config_t *ct, int key )
{
    if( key ) {
        int cmd;

        if( ct->keymapmenu[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ] ) {
            return ct->keymapmenu[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ];
        }

        /* Fall back to standard commands. */
        cmd = config_key_to_command( ct, key );
        if( cmd != TVTIME_NOCOMMAND ) {
            return cmd;
        }

        if( isalnum( key & 0x1ff ) ) {
            return TVTIME_CHANNEL_CHAR;
        }
    }
        
    return TVTIME_NOCOMMAND;
}

const char *config_key_to_menu_command_argument( config_t *ct, int key )
{
    if( key ) {
        if( ct->keymapmenu_arg[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ] ) {
            return ct->keymapmenu_arg[ MAX_KEYSYMS*((key & 0x70000)>>16) + (key & 0x1ff) ];
        }

        return config_key_to_command_argument( ct, key );
    }

    return 0;
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

const char *config_button_to_command_argument( config_t *ct, int button )
{
    if( button < 0 || button >= MAX_BUTTONS ) {
        return 0;
    } else {
        return ct->buttonmap_arg[ button ];
    }
}

int config_button_to_menu_command( config_t *ct, int button )
{
    if( button < 0 || button >= MAX_BUTTONS ) {
        return 0;
    } else {
        if( ct->buttonmapmenu[ button ] ) {
            return ct->buttonmapmenu[ button ];
        } else {
            return ct->buttonmap[ button ];
        }
    }
}

const char *config_button_to_menu_command_argument( config_t *ct, int button )
{
    if( button < 0 || button >= MAX_BUTTONS ) {
        return 0;
    } else {
        if( ct->buttonmapmenu_arg[ button ] ) {
            return ct->buttonmapmenu_arg[ button ];
        } else {
            return ct->buttonmap_arg[ button ];
        }
    }
}

int config_get_verbose( config_t *ct )
{
    return ct->verbose;
}

int config_get_send_fields( config_t *ct )
{
    return ct->send_fields;
}

int config_get_debug( config_t *ct )
{
    return ct->debug;
}

const char *config_get_geometry( config_t *ct )
{
    return ct->geometry;
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

int config_get_alwaysontop( config_t *ct )
{
    return ct->alwaysontop;
}

int config_get_priority( config_t *ct )
{
    return ct->priority;
}

const char *config_get_v4l_freq( config_t *ct )
{
    return ct->freq;
}

const char *config_get_vbi_device( config_t *ct )
{
    return ct->vbidev;
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

int config_get_usexds( config_t *ct )
{
    return ct->use_xds;
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

int config_get_fullscreen_position( config_t *ct )
{
    return ct->fspos;
}

int config_get_save_restore_picture( config_t *ct )
{
    return ct->picsaverestore;
}

int config_get_global_brightness( config_t *ct )
{
    return ct->brightness;
}

int config_get_global_contrast( config_t *ct )
{
    return ct->contrast;
}

int config_get_global_saturation( config_t *ct )
{
    return ct->saturation;
}

int config_get_global_hue( config_t *ct )
{
    return ct->hue;
}

const char *config_get_audio_mode( config_t *ct )
{
    return ct->audiomode;
}

const char *config_get_xmltv_file( config_t *ct )
{
    return ct->xmltvfile;
}

const char *config_get_xmltv_language( config_t *ct )
{
    return ct->xmltvlanguage;
}

int config_get_invert( config_t *ct )
{
    return ct->invert;
}

int config_get_cc( config_t *ct )
{
    return ct->cc;
}

int config_get_mirror( config_t *ct )
{
    return ct->mirror;
}

int config_get_audio_boost( config_t *ct )
{
    return ct->boost;
}

int config_get_quiet_screenshots( config_t *ct )
{
    return ct->quiet_screenshots;
}

int config_get_unmute_volume( config_t *ct )
{
    return ct->unmute_volume;
}

int config_get_muted( config_t *ct )
{
    return ct->muted;
}

int config_get_mute_on_exit( config_t *ct )
{
    return ct->mute_on_exit;
}

