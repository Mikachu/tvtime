/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>.
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include "station.h"
#include "mixer.h"
#include "input.h"
#include "commands.h"
#include "console.h"
#include "utils.h"
#include "xmltv.h"

#define NUM_FAVORITES 9
#define MAX_USER_MENUS 64

/* Maximum number of steps to increment sleeptimer. */
#define SLEEPTIMER_NUMSTEPS 13

enum menu_type
{
    MENU_REDIRECT,
    MENU_FAVORITES,
    MENU_USER
};

typedef struct menu_names_s {
    const char *name;
    int menutype;
    const char *dest;
} menu_names_t;

static menu_names_t menu_table[] = {
    { "root", MENU_REDIRECT, "root-tuner" },
    { "picture", MENU_REDIRECT, "picture-tuner" },
    { "input", MENU_REDIRECT, "input-ntsc" },
    { "favorites", MENU_FAVORITES, 0 },
    { "color", MENU_REDIRECT, "colour" }
};

static int tvtime_num_builtin_menus( void )
{
    return ( sizeof( menu_table ) / sizeof( menu_names_t ) );
}

static void set_redirect( const char *menu, const char *dest )
{
    int i;

    for( i = 0; i < tvtime_num_builtin_menus(); i++ ) {
        if( !strcasecmp( menu, menu_table[ i ].name ) ) {
            menu_table[ i ].dest = dest;
            return;
        }
    }
}


static int sleeptimer_function( int step )
{
    if( step < 3 ) {
       return step * 10;
    } else {
       return (step - 2) * 30;
    }
}

struct commands_s {
    config_t *cfg;
    videoinput_t *vidin;
    tvtime_osd_t *osd;
    char next_chan_buffer[ 5 ];
    int frame_counter;
    int digit_counter;
    int quit;
    int inputnum;
    int sleeptimer;
    time_t sleeptimer_start;

    xmltv_t *xmltv;

    int picturemode;
    int brightness;
    int contrast;
    int colour;
    int hue;

    int displayinfo;
    int screenshot;
    int printdebug;
    int showbars;
    int showdeinterlacerinfo;
    int togglefullscreen;
    int toggleaspect;
    int togglealwaysontop;
    int toggledeinterlacer;
    int togglepulldowndetection;
    int togglemode;
    int togglematte;
    int framerate;
    int scan_channels;
    int pause;
    int halfsize;
    int resizewindow;
    int restarttvtime;
    int setdeinterlacer;
    int normset;
    const char *newnorm;
    int newsharpness;
    char deinterlacer[ 128 ];
    int setfreqtable;
    char newfreqtable[ 128 ];
    int checkfreq;

    int delay;

    int change_channel;

    int renumbering;

    int apply_invert;
    int apply_mirror;
    int apply_chroma_kill;

    int apply_luma;
    int update_luma;
    double luma_power;

    double overscan;

    int console_on;
    int scrollconsole;
    console_t *console;

    vbidata_t *vbi;
    int capturemode;

    station_mgr_t *stationmgr;

    int curfavorite;
    int numfavorites;
    int favorites[ NUM_FAVORITES ];

    int menuactive;
    int curmenu;
    int curmenupos;
    int curmenusize;
    menu_t *curusermenu;
    menu_t *menus[ MAX_USER_MENUS ];

    menu_t *root_tuner;
    menu_t *root_notuner;
};

static void menu_set_value( menu_t *menu, int newval, int a, int b, int c )
{
    char string[ 128 ];
    sprintf( string, "%c%c%c  Current: %d", a, b, c, newval );
    menu_set_text( menu, 1, string );
}

static void update_xmltv_channel( commands_t *cmd )
{
    if( cmd->xmltv && cmd->osd ) {
        if( station_get_current_xmltv_id( cmd->stationmgr ) ) {
            xmltv_set_channel( cmd->xmltv, station_get_current_xmltv_id( cmd->stationmgr ) );
        } else {
            xmltv_set_channel( cmd->xmltv, xmltv_lookup_channel( cmd->xmltv, station_get_current_channel_name( cmd->stationmgr ) ) );
        }
    } else if( cmd->osd ) {
        tvtime_osd_show_program_info( cmd->osd, 0, 0, 0, 0 );
    }
}

static void update_xmltv_listings( commands_t *cmd )
{
    if( cmd->xmltv && cmd->osd ) {
        time_t tm = time( 0 );
        struct tm *curtime = localtime( &tm );
        int year = 1900 + curtime->tm_year;
        int month = 1 + curtime->tm_mon;
        int day = curtime->tm_mday;
        int hour = curtime->tm_hour;
        int min = curtime->tm_min;

        if( xmltv_needs_refresh( cmd->xmltv, year, month, day, hour, min ) ) {
            const char *desc;
            char subtitle[ 1024 ];
            char descdata[ 1024 ];
            char *line1 = 0;
            char *line2 = 0;

            xmltv_refresh( cmd->xmltv, year, month, day, hour, min );

            desc = xmltv_get_description( cmd->xmltv );
            if( desc ) {
                snprintf( descdata, sizeof( descdata ), "%s", desc );
                line1 = descdata;
                line2 = 0;

                if( strlen( descdata ) > 45 ) {
                    int desc_i = strlen( descdata ) / 2;

                    /* find nearest white space */
                    while( desc_i ) {
                        if( descdata[ desc_i ] == ' ') break; else desc_i--;
                    }
                    if( desc_i ) {
                        line2 = descdata + desc_i + 1; /* +1 for the space */
                        descdata[ desc_i ] = '\0';
                    }
                }
            }

            if( xmltv_get_sub_title( cmd->xmltv ) ) {
               sprintf( subtitle, "%s - %s", xmltv_get_times( cmd->xmltv ),
                        xmltv_get_sub_title( cmd->xmltv ) );
            } else {
               sprintf( subtitle, "%s", xmltv_get_times( cmd->xmltv ) );
            }

            tvtime_osd_show_program_info( cmd->osd, xmltv_get_title( cmd->xmltv ), subtitle, line1, line2 );
        }
    }
}

static void reset_stations_menu( menu_t *menu, int ntsc, int pal, int secam,
                                 int ntsccable, int active, int signaldetect,
                                 int scanning )
{
    char string[ 128 ];
    int cur;

    /* Start over. */
    menu_reset_num_lines( menu );
    cur = 1;

    if( !ntsc ) {
        sprintf( string, "%c%c%c  Renumber current channel", 0xee, 0x80, 0xaf );
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_CHANNEL_RENUMBER, "" );
        menu_set_right_command( menu, cur, TVTIME_CHANNEL_RENUMBER, "" );
        menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
        cur++;
    }

    if( active ) {
        sprintf( string, "%c%c%c  Current channel active in list", 0xee, 0x80, 0xb7 );
    } else {
        sprintf( string, "%c%c%c  Current channel active in list", 0xee, 0x80, 0xb8 );
    }
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_CHANNEL_SKIP, "" );
    menu_set_right_command( menu, cur, TVTIME_CHANNEL_SKIP, "" );
    menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    cur++;

    if( signaldetect ) {
        if( scanning ) {
            sprintf( string, "%c%c%c  Stop channel scan", 0xee, 0x80, 0xa3 );
        } else {
            sprintf( string, "%c%c%c  Scan channels for signal", 0xee, 0x80, 0xa3 );
        }
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_CHANNEL_SCAN, "" );
        menu_set_right_command( menu, cur, TVTIME_CHANNEL_SCAN, "" );
        menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
        cur++;
    }

    sprintf( string, "%c%c%c  Reset all channels as active", 0xee, 0x80, 0xa7 );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_CHANNEL_ACTIVATE_ALL, "" );
    menu_set_right_command( menu, cur, TVTIME_CHANNEL_ACTIVATE_ALL, "" );
    menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    cur++;

    sprintf( string, "%c%c%c  Finetune current channel", 0xee, 0x80, 0xb0 );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "finetune" );
    menu_set_right_command( menu, cur, TVTIME_SHOW_MENU, "finetune" );
    menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    cur++;

    if( ntsccable ) {
        sprintf( string, "%c%c%c  Change NTSC cable mode", 0xee, 0x80, 0xbf );
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_TOGGLE_NTSC_CABLE_MODE, "" );
        menu_set_right_command( menu, cur, TVTIME_TOGGLE_NTSC_CABLE_MODE, "" );
        menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
        cur++;
    } else if( pal || secam ) {
        if( pal ) {
            sprintf( string, "%c%c%c  Set current channel as SECAM", 0xee, 0x80, 0x80 );
        } else {
            sprintf( string, "%c%c%c  Set current channel as PAL", 0xee, 0x80, 0x80 );
        }
        menu_set_text( menu, cur, string );
        menu_set_enter_command( menu, cur, TVTIME_TOGGLE_PAL_SECAM, "" );
        menu_set_right_command( menu, cur, TVTIME_TOGGLE_PAL_SECAM, "" );
        menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
        cur++;
    }

    sprintf( string, "%c%c%c  Change frequency table", 0xee, 0x80, 0xba );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "frequencies" );
    menu_set_right_command( menu, cur, TVTIME_SHOW_MENU, "frequencies" );
    menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    cur++;

    if( signaldetect ) {
        sprintf( string, "%c%c%c  Disable signal detection", 0xee, 0x80, 0x9d );
    } else {
        sprintf( string, "%c%c%c  Enable signal detection", 0xee, 0x80, 0x9d );
    }
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_TOGGLE_SIGNAL_DETECTION, "" );
    menu_set_right_command( menu, cur, TVTIME_TOGGLE_SIGNAL_DETECTION, "" );
    menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    cur++;

    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, cur, string );
    menu_set_enter_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, cur, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, cur, TVTIME_SHOW_MENU, "root" );
}

static void reinit_tuner( commands_t *cmd )
{
    /* Setup the tuner if available. */
    if( cmd->vbi ) {
        vbidata_reset( cmd->vbi );
        vbidata_capture_mode( cmd->vbi, cmd->capturemode );
    }

    set_redirect( "root", "root-notuner" );
    set_redirect( "picture", "picture-notuner" );

    if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
        int norm;

        set_redirect( "root", "root-tuner" );
        set_redirect( "picture", "picture-tuner" );

        videoinput_set_tuner_freq( cmd->vidin, station_get_current_frequency( cmd->stationmgr )
                                   + ((station_get_current_finetune( cmd->stationmgr ) * 1000)/16) );
        menu_set_value( commands_get_menu( cmd, "finetune" ), station_get_current_finetune( cmd->stationmgr ), 0xee, 0x80, 0xb0 );
        commands_refresh_menu( cmd );

        norm = videoinput_get_norm_number( station_get_current_norm( cmd->stationmgr ) );
        if( norm >= 0 ) {
            videoinput_switch_pal_secam( cmd->vidin, norm );
        }

        if( config_get_save_restore_picture( cmd->cfg ) ) {
            int brightness = station_get_current_brightness( cmd->stationmgr );
            int contrast = station_get_current_contrast( cmd->stationmgr );
            int colour = station_get_current_colour( cmd->stationmgr );
            int hue = station_get_current_hue( cmd->stationmgr );

            if( brightness >= 0 ) {
                videoinput_set_brightness( cmd->vidin, brightness );
            } else if( cmd->brightness >= 0 ) {
                videoinput_set_brightness( cmd->vidin, cmd->brightness );
            }
            if( contrast >= 0 ) {
                videoinput_set_contrast( cmd->vidin, contrast );
            } else if( cmd->contrast >= 0 ) {
                videoinput_set_contrast( cmd->vidin, cmd->contrast );
            }
            if( colour >= 0 ) {
                videoinput_set_colour( cmd->vidin, colour );
            } else if( cmd->colour >= 0 ) {
                videoinput_set_colour( cmd->vidin, cmd->colour );
            }
            if( hue >= 0 ) {
                videoinput_set_hue( cmd->vidin, hue );
            } else if( cmd->hue >= 0 ) {
                videoinput_set_hue( cmd->vidin, cmd->hue );
            }
        }

        if( cmd->osd ) {
            char channel_display[ 20 ];

            snprintf( channel_display, sizeof( channel_display ), "%d",
                      station_get_current_id( cmd->stationmgr ) );
            tvtime_osd_set_norm( cmd->osd, videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            tvtime_osd_set_audio_mode( cmd->osd, videoinput_get_audio_mode_name( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) ) );
            tvtime_osd_set_freq_table( cmd->osd, station_get_current_band( cmd->stationmgr ) );
            tvtime_osd_set_channel_number( cmd->osd, channel_display );
            tvtime_osd_set_channel_name( cmd->osd, station_get_current_channel_name( cmd->stationmgr ) );
            tvtime_osd_set_network_call( cmd->osd, station_get_current_network_call_letters( cmd->stationmgr ) );
            tvtime_osd_set_network_name( cmd->osd, station_get_current_network_name( cmd->stationmgr ) );
            tvtime_osd_set_show_name( cmd->osd, "" );
            tvtime_osd_set_show_rating( cmd->osd, "" );
            tvtime_osd_set_show_start( cmd->osd, "" );
            tvtime_osd_set_show_length( cmd->osd, "" );
            tvtime_osd_show_info( cmd->osd );
            update_xmltv_channel( cmd );

            reset_stations_menu( commands_get_menu( cmd, "stations" ),
                                 (videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC ||
                                  videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC_JP),
                                 videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_PAL,
                                 videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_SECAM,
                                 (!strcasecmp( cmd->newfreqtable, "us-cable" ) ||
                                  !strcasecmp( cmd->newfreqtable, "us-cable100" )),
                                 station_get_current_active( cmd->stationmgr ), cmd->checkfreq,
                                 cmd->scan_channels );
            commands_refresh_menu( cmd );
        }
        cmd->frame_counter = 0;
    } else if( cmd->osd ) {
        tvtime_osd_set_audio_mode( cmd->osd, "" );
        tvtime_osd_set_freq_table( cmd->osd, "" );
        tvtime_osd_set_channel_number( cmd->osd, "" );
        tvtime_osd_set_channel_name( cmd->osd, "" );
        tvtime_osd_set_network_call( cmd->osd, "" );
        tvtime_osd_set_network_name( cmd->osd, "" );
        tvtime_osd_set_show_name( cmd->osd, "" );
        tvtime_osd_set_show_rating( cmd->osd, "" );
        tvtime_osd_set_show_start( cmd->osd, "" );
        tvtime_osd_set_show_length( cmd->osd, "" );
        tvtime_osd_show_program_info( cmd->osd, 0, 0, 0, 0 );
        tvtime_osd_show_info( cmd->osd );
    }

    if( config_get_save_restore_picture( cmd->cfg ) && cmd->vidin && !videoinput_has_tuner( cmd->vidin ) ) {
        if( cmd->brightness >= 0 ) {
            videoinput_set_brightness( cmd->vidin, cmd->brightness );
        }
        if( cmd->contrast >= 0 ) {
            videoinput_set_contrast( cmd->vidin, cmd->contrast );
        }
        if( cmd->colour >= 0 ) {
            videoinput_set_colour( cmd->vidin, cmd->colour );
        }
        if( cmd->hue >= 0 ) {
            videoinput_set_hue( cmd->vidin, cmd->hue );
        }
    }

    if( cmd->vidin ) {
        menu_set_value( commands_get_menu( cmd, "brightness" ), videoinput_get_brightness( cmd->vidin ), 0xe2, 0x98, 0x80 );
        menu_set_value( commands_get_menu( cmd, "contrast" ), videoinput_get_contrast( cmd->vidin ), 0xe2, 0x97, 0x90 );
        menu_set_value( commands_get_menu( cmd, "colour" ), videoinput_get_colour( cmd->vidin ), 0xe2, 0x98, 0xb0 );
        menu_set_value( commands_get_menu( cmd, "hue" ), videoinput_get_hue( cmd->vidin ), 0xe2, 0x97, 0xaf );
    }
}

static void reset_frequency_menu( menu_t *menu, int norm, const char *tablename )
{
    char string[ 128 ];

    if( norm == VIDEOINPUT_NTSC || norm == VIDEOINPUT_PAL_M || norm == VIDEOINPUT_PAL_NC ) {
        if( !strcasecmp( tablename, "us-cable" ) ) {
            sprintf( string, "%c%c%c  US Cable", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  US Cable", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 1, string );
        menu_set_enter_command( menu, 1, TVTIME_SET_FREQUENCY_TABLE, "us-cable" );
        menu_set_right_command( menu, 1, TVTIME_SET_FREQUENCY_TABLE, "us-cable" );
        menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "us-broadcast" ) ) {
            sprintf( string, "%c%c%c  US Broadcast", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  US Broadcast", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 2, string );
        menu_set_enter_command( menu, 2, TVTIME_SET_FREQUENCY_TABLE, "us-broadcast" );
        menu_set_right_command( menu, 2, TVTIME_SET_FREQUENCY_TABLE, "us-broadcast" );
        menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "us-cable100" ) ) {
            sprintf( string, "%c%c%c  US Cable with channels 100+", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  US Cable with channels 100+", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SET_FREQUENCY_TABLE, "us-cable100" );
        menu_set_right_command( menu, 3, TVTIME_SET_FREQUENCY_TABLE, "us-cable100" );
        menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "stations" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 4, string );
        menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "input" );
        menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "input" );
        menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "input" );
    } else if( norm == VIDEOINPUT_NTSC_JP ) {
        if( !strcasecmp( tablename, "japan-cable" ) ) {
            sprintf( string, "%c%c%c  Japan Cable", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Japan Cable", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 1, string );
        menu_set_enter_command( menu, 1, TVTIME_SET_FREQUENCY_TABLE, "japan-cable" );
        menu_set_right_command( menu, 1, TVTIME_SET_FREQUENCY_TABLE, "japan-cable" );
        menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "japan-broadcast" ) ) {
            sprintf( string, "%c%c%c  Japan Broadcast", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Japan Broadcast", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 2, string );
        menu_set_enter_command( menu, 2, TVTIME_SET_FREQUENCY_TABLE, "japan-broadcast" );
        menu_set_right_command( menu, 2, TVTIME_SET_FREQUENCY_TABLE, "japan-broadcast" );
        menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "stations" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "input" );
        menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "input" );
        menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "input" );
    } else {
        if( !strcasecmp( tablename, "europe" ) ) {
            sprintf( string, "%c%c%c  Europe", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Europe", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 1, string );
        menu_set_enter_command( menu, 1, TVTIME_SET_FREQUENCY_TABLE, "europe" );
        menu_set_right_command( menu, 1, TVTIME_SET_FREQUENCY_TABLE, "europe" );
        menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "russia" ) ) {
            sprintf( string, "%c%c%c  Russia", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Russia", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 2, string );
        menu_set_enter_command( menu, 2, TVTIME_SET_FREQUENCY_TABLE, "russia" );
        menu_set_right_command( menu, 2, TVTIME_SET_FREQUENCY_TABLE, "russia" );
        menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "france" ) ) {
            sprintf( string, "%c%c%c  France", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  France", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SET_FREQUENCY_TABLE, "france" );
        menu_set_right_command( menu, 3, TVTIME_SET_FREQUENCY_TABLE, "france" );
        menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "australia" ) ) {
            sprintf( string, "%c%c%c  Australia", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Australia", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 4, string );
        menu_set_enter_command( menu, 4, TVTIME_SET_FREQUENCY_TABLE, "australia" );
        menu_set_right_command( menu, 4, TVTIME_SET_FREQUENCY_TABLE, "australia" );
        menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "australia-optus" ) ) {
            sprintf( string, "%c%c%c  Australia (Optus)", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Australia (Optus)", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_SET_FREQUENCY_TABLE, "australia-optus" );
        menu_set_right_command( menu, 5, TVTIME_SET_FREQUENCY_TABLE, "australia-optus" );
        menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "newzealand" ) ) {
            sprintf( string, "%c%c%c  New Zealand", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  New Zealand", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 6, string );
        menu_set_enter_command( menu, 6, TVTIME_SET_FREQUENCY_TABLE, "newzealand" );
        menu_set_right_command( menu, 6, TVTIME_SET_FREQUENCY_TABLE, "newzealand" );
        menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "china-broadcast" ) ) {
            sprintf( string, "%c%c%c  China Broadcast", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  China Broadcast", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 7, string );
        menu_set_enter_command( menu, 7, TVTIME_SET_FREQUENCY_TABLE, "china-broadcast" );
        menu_set_right_command( menu, 7, TVTIME_SET_FREQUENCY_TABLE, "china-broadcast" );
        menu_set_left_command( menu, 7, TVTIME_SHOW_MENU, "stations" );
        if( !strcasecmp( tablename, "custom" ) ) {
            sprintf( string, "%c%c%c  Custom (first run tvtime-scanner)", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Custom (first run tvtime-scanner)", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 8, string );
        menu_set_enter_command( menu, 8, TVTIME_SET_FREQUENCY_TABLE, "custom" );
        menu_set_right_command( menu, 8, TVTIME_SET_FREQUENCY_TABLE, "custom" );
        menu_set_left_command( menu, 8, TVTIME_SHOW_MENU, "stations" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 9, string );
        menu_set_enter_command( menu, 9, TVTIME_SHOW_MENU, "input" );
        menu_set_right_command( menu, 9, TVTIME_SHOW_MENU, "input" );
        menu_set_left_command( menu, 9, TVTIME_SHOW_MENU, "input" );
    }
}

static void reset_sharpness_menu( menu_t *menu, int sharpness )
{
    char string[ 128 ];

    sprintf( string, "%c%c%c  Current: %d pixels", 0xee, 0x80, 0x80, sharpness );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "input" );

    if( sharpness <= 180  ) {
        sprintf( string, "%c%c%c  Poor (180 pixels)", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  Poor (180 pixels)", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SET_SHARPNESS, "180" );
    menu_set_right_command( menu, 2, TVTIME_SET_SHARPNESS, "180" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "input" );

    if( sharpness > 180 && sharpness <= 360 ) {
        sprintf( string, "%c%c%c  Low (360 pixels)", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  Low (360 pixels)", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SET_SHARPNESS, "360" );
    menu_set_right_command( menu, 3, TVTIME_SET_SHARPNESS, "360" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "input" );

    if( sharpness > 360 && sharpness <= 576 ) {
        sprintf( string, "%c%c%c  Moderate (576 pixels)", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  Moderate (576 pixels)", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SET_SHARPNESS, "576" );
    menu_set_right_command( menu, 4, TVTIME_SET_SHARPNESS, "576" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "input" );

    if( sharpness > 576 && sharpness <= 720 ) {
        sprintf( string, "%c%c%c  Standard (720 pixels)", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  Standard (720 pixels)", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SET_SHARPNESS, "720" );
    menu_set_right_command( menu, 5, TVTIME_SET_SHARPNESS, "720" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "input" );

    if( sharpness > 720 ) {
        sprintf( string, "%c%c%c  High (768 pixels)", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  High (768 pixels)", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 6, string );
    menu_set_enter_command( menu, 6, TVTIME_SET_SHARPNESS, "768" );
    menu_set_right_command( menu, 6, TVTIME_SET_SHARPNESS, "768" );
    menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "input" );

    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 7, string );
    menu_set_enter_command( menu, 7, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 7, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 7, TVTIME_SHOW_MENU, "input" );
}

static void reset_norm_menu( menu_t *menu, const char *norm )
{
    char string[ 128 ];

    if( !strcasecmp( norm, "ntsc" ) ) {
        sprintf( string, "%c%c%c  NTSC", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  NTSC", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SET_NORM, "ntsc" );
    menu_set_right_command( menu, 1, TVTIME_SET_NORM, "ntsc" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "pal" ) ) {
        sprintf( string, "%c%c%c  PAL", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  PAL", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SET_NORM, "pal" );
    menu_set_right_command( menu, 2, TVTIME_SET_NORM, "pal" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "secam" ) ) {
        sprintf( string, "%c%c%c  SECAM", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  SECAM", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SET_NORM, "secam" );
    menu_set_right_command( menu, 3, TVTIME_SET_NORM, "secam" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "pal-nc" ) ) {
        sprintf( string, "%c%c%c  PAL-Nc", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  PAL-Nc", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SET_NORM, "pal-nc" );
    menu_set_right_command( menu, 4, TVTIME_SET_NORM, "pal-nc" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "pal-m" ) ) {
        sprintf( string, "%c%c%c  PAL-M", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  PAL-M", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SET_NORM, "pal-m" );
    menu_set_right_command( menu, 5, TVTIME_SET_NORM, "pal-m" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "pal-n" ) ) {
        sprintf( string, "%c%c%c  PAL-N", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  PAL-N", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 6, string );
    menu_set_enter_command( menu, 6, TVTIME_SET_NORM, "pal-n" );
    menu_set_right_command( menu, 6, TVTIME_SET_NORM, "pal-n" );
    menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "ntsc-jp" ) ) {
        sprintf( string, "%c%c%c  NTSC-JP", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  NTSC-JP", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 7, string );
    menu_set_enter_command( menu, 7, TVTIME_SET_NORM, "ntsc-jp" );
    menu_set_right_command( menu, 7, TVTIME_SET_NORM, "ntsc-jp" );
    menu_set_left_command( menu, 7, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( norm, "pal-60" ) ) {
        sprintf( string, "%c%c%c  PAL-60", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  PAL-60", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 8, string );
    menu_set_enter_command( menu, 8, TVTIME_SET_NORM, "pal-60" );
    menu_set_right_command( menu, 8, TVTIME_SET_NORM, "pal-60" );
    menu_set_left_command( menu, 8, TVTIME_SHOW_MENU, "input" );
}

static void reset_audio_mode_menu( menu_t *menu, int ntsc, int curmode )
{
    char string[ 128 ];

    if( curmode == VIDEOINPUT_MONO ) {
        sprintf( string, "%c%c%c  Mono", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  Mono", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SET_AUDIO_MODE, "mono" );
    menu_set_right_command( menu, 1, TVTIME_SET_AUDIO_MODE, "mono" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "input" );

    if( curmode == VIDEOINPUT_STEREO ) {
        sprintf( string, "%c%c%c  Stereo", 0xee, 0x80, 0xa5 );
    } else {
        sprintf( string, "%c%c%c  Stereo", 0xee, 0x80, 0xa4 );
    }
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SET_AUDIO_MODE, "stereo" );
    menu_set_right_command( menu, 2, TVTIME_SET_AUDIO_MODE, "stereo" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "input" );

    if( ntsc ) {
        if( curmode == VIDEOINPUT_LANG1 || curmode == VIDEOINPUT_LANG2 ) {
            sprintf( string, "%c%c%c  SAP", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  SAP", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SET_AUDIO_MODE, "sap" );
        menu_set_right_command( menu, 3, TVTIME_SET_AUDIO_MODE, "sap" );
        menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "input" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 4, string );
        menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "input" );
        menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "input" );
        menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "input" );
    } else {
        if( curmode == VIDEOINPUT_LANG1 ) {
            sprintf( string, "%c%c%c  Primary Language", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Primary Language", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 3, string );
        menu_set_enter_command( menu, 3, TVTIME_SET_AUDIO_MODE, "lang1" );
        menu_set_right_command( menu, 3, TVTIME_SET_AUDIO_MODE, "lang1" );
        menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "input" );
        if( curmode == VIDEOINPUT_LANG2 ) {
            sprintf( string, "%c%c%c  Secondary Language", 0xee, 0x80, 0xa5 );
        } else {
            sprintf( string, "%c%c%c  Secondary Language", 0xee, 0x80, 0xa4 );
        }
        menu_set_text( menu, 4, string );
        menu_set_enter_command( menu, 4, TVTIME_SET_AUDIO_MODE, "lang2" );
        menu_set_right_command( menu, 4, TVTIME_SET_AUDIO_MODE, "lang2" );
        menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "input" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_SHOW_MENU, "input" );
        menu_set_right_command( menu, 5, TVTIME_SHOW_MENU, "input" );
        menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "input" );
    }
}

static void reset_overscan_menu( menu_t *menu, double overscan )
{
    char string[ 128 ];

    sprintf( string, "%c%c%c  Current: %.1f%%", 0xee, 0x80, 0x80, overscan * 2.0 * 100.0 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "output" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "output" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "output" );
    sprintf( string, "%c%c%c  Decrease", 0xee, 0x80, 0xa9 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_OVERSCAN_DOWN, "" );
    menu_set_right_command( menu, 2, TVTIME_OVERSCAN_DOWN, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "output" );
    sprintf( string, "%c%c%c  Increase", 0xee, 0x80, 0xa8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_OVERSCAN_UP, "" );
    menu_set_right_command( menu, 3, TVTIME_OVERSCAN_UP, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "output" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "output" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "output" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "output" );
}

commands_t *commands_new( config_t *cfg, videoinput_t *vidin,
                          station_mgr_t *mgr, tvtime_osd_t *osd,
                          int fieldtime )
{
    commands_t *cmd = malloc( sizeof( struct commands_s ) );
    char string[ 128 ];
    menu_t *menu;

    if( !cmd ) {
        return 0;
    }

    /* Number of frames to wait for next channel digit. */
    cmd->delay = 1000000 / fieldtime;

    cmd->cfg = cfg;
    cmd->vidin = vidin;
    cmd->osd = osd;
    cmd->stationmgr = mgr;
    cmd->frame_counter = 0;
    cmd->digit_counter = 0;

    cmd->displayinfo = 0;
    cmd->checkfreq = config_get_check_freq_present( cfg );

    cmd->sleeptimer = 0;
    cmd->sleeptimer_start = 0;

    cmd->quit = 0;
    cmd->showbars = 0;
    cmd->picturemode = 3;
    cmd->showdeinterlacerinfo = 0;
    cmd->printdebug = 0;
    cmd->screenshot = 0;
    cmd->togglefullscreen = 0;
    cmd->toggleaspect = 0;
    cmd->togglealwaysontop = 0;
    cmd->toggledeinterlacer = 0;
    cmd->togglepulldowndetection = 0;
    cmd->togglemode = 0;
    cmd->togglematte = 0;
    cmd->framerate = FRAMERATE_FULL;
    cmd->console_on = 0;
    cmd->scrollconsole = 0;
    cmd->scan_channels = 0;
    cmd->pause = 0;
    cmd->halfsize = 0;
    cmd->change_channel = 0;
    cmd->renumbering = 0;
    cmd->resizewindow = 0;
    cmd->restarttvtime = 0;
    cmd->setdeinterlacer = 0;
    cmd->normset = 0;
    cmd->newsharpness = 0;
    cmd->setfreqtable = 0;
    cmd->brightness = config_get_global_brightness( cfg );
    cmd->contrast = config_get_global_contrast( cfg );
    cmd->colour = config_get_global_colour( cfg );
    cmd->hue = config_get_global_hue( cfg );

    cmd->apply_invert = 0;
    cmd->apply_mirror = 0;
    cmd->apply_chroma_kill = 0;

    cmd->apply_luma = config_get_apply_luma_correction( cfg );
    cmd->update_luma = 0;
    cmd->luma_power = config_get_luma_correction( cfg );

    snprintf( cmd->newfreqtable, sizeof( cmd->newfreqtable ), "%s", config_get_v4l_freq( cfg ) );

    if( config_get_xmltv_file( cfg ) && strcasecmp( config_get_xmltv_file( cfg ), "none" ) ) {
        cmd->xmltv = xmltv_new( config_get_xmltv_file( cfg ) );
    } else {
        cmd->xmltv = 0;
    }

    if( vidin && !videoinput_is_bttv( vidin ) && cmd->apply_luma ) {
        cmd->apply_luma = 0;
        fprintf( stderr, "commands: Input isn't from a bt8x8, disabling luma correction.\n" );
    }

    if( vidin && videoinput_get_norm( vidin ) != VIDEOINPUT_NTSC &&
                 videoinput_get_norm( vidin ) != VIDEOINPUT_NTSC_JP ) {
        set_redirect( "input", "input-pal" );
    }

    if( vidin && (videoinput_get_norm( vidin ) == VIDEOINPUT_PAL ||
                  videoinput_get_norm( vidin ) == VIDEOINPUT_SECAM) ) {
        if( cmd->checkfreq ) {
            set_redirect( "stations", "stations-palsecam-signal" );
        } else {
            set_redirect( "stations", "stations-palsecam-nosignal" );
        }
    } else if( vidin && videoinput_get_norm( vidin ) == VIDEOINPUT_NTSC &&
               (!strcasecmp( config_get_v4l_freq( cfg ), "us-cable" ) ||
                !strcasecmp( config_get_v4l_freq( cfg ), "us-cable100" )) ) {
        if( cmd->checkfreq ) {
            set_redirect( "stations", "stations-ntsccable-signal" );
        } else {
            set_redirect( "stations", "stations-ntsccable-nosignal" );
        }
    }

    if( cmd->luma_power < 0.0 || cmd->luma_power > 10.0 ) {
        fprintf( stderr, "commands: Luma correction value out of range. Using 1.0.\n" );
        cmd->luma_power = 1.0;
    }

    cmd->overscan = config_get_overscan( cfg );
    if( cmd->overscan > 0.4 ) cmd->overscan = 0.4; if( cmd->overscan < 0.0 ) cmd->overscan = 0.0;

    cmd->console = 0;
    cmd->vbi = 0;
    cmd->capturemode = CAPTURE_OFF;

    cmd->numfavorites = 0;
    cmd->curfavorite = 0;

    cmd->menuactive = 0;
    cmd->curmenu = MENU_FAVORITES;
    cmd->curmenupos = 0;
    cmd->curusermenu = 0;
    memset( cmd->menus, 0, sizeof( cmd->menus ) );

    menu = menu_new( "root-tuner" );
    menu_set_text( menu, 0, "Setup" );
    sprintf( string, "%c%c%c  Station management", 0xee, 0x80, 0x9d );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_left_command( menu, 1, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Input configuration", 0xee, 0x80, 0x9b );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 2, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Picture settings", 0xee, 0x80, 0x9e );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 3, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Video processing", 0xee, 0x80, 0x9f );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, 4, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Output configuration", 0xee, 0x80, 0xb9 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SHOW_MENU, "output" );
    menu_set_right_command( menu, 5, TVTIME_SHOW_MENU, "output" );
    menu_set_left_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Exit menu", 0xee, 0x80, 0xa0 );
    menu_set_text( menu, 6, string );
    menu_set_enter_command( menu, 6, TVTIME_MENU_EXIT, 0 );
    menu_set_right_command( menu, 6, TVTIME_MENU_EXIT, 0 );
    menu_set_left_command( menu, 6, TVTIME_MENU_EXIT, 0 );
    commands_add_menu( cmd, menu );

    menu = menu_new( "root-notuner" );
    menu_set_text( menu, 0, "Setup" );
    sprintf( string, "%c%c%c  Input configuration", 0xee, 0x80, 0x9b );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 1, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Picture settings", 0xee, 0x80, 0x9e );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 2, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Video processing", 0xee, 0x80, 0x9f );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, 3, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Output configuration", 0xee, 0x80, 0xb9 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "output" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "output" );
    menu_set_left_command( menu, 4, TVTIME_MENU_EXIT, 0 );
    sprintf( string, "%c%c%c  Exit menu", 0xee, 0x80, 0xa0 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    menu_set_right_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    menu_set_left_command( menu, 5, TVTIME_MENU_EXIT, 0 );
    commands_add_menu( cmd, menu );


    menu = menu_new( "stations" );
    menu_set_text( menu, 0, "Setup - Station management" );
    commands_add_menu( cmd, menu );
    if( vidin ) {
        reset_stations_menu( commands_get_menu( cmd, "stations" ),
                             (videoinput_get_norm( vidin ) == VIDEOINPUT_NTSC ||
                              videoinput_get_norm( vidin ) == VIDEOINPUT_NTSC_JP),
                             videoinput_get_norm( vidin ) == VIDEOINPUT_PAL,
                             videoinput_get_norm( vidin ) == VIDEOINPUT_SECAM,
                             (!strcasecmp( cmd->newfreqtable, "us-cable" ) ||
                              !strcasecmp( cmd->newfreqtable, "us-cable100" )),
                             station_get_current_active( cmd->stationmgr ), cmd->checkfreq,
                             cmd->scan_channels );
    }

    menu = menu_new( "frequencies" );
    menu_set_text( menu, 0, "Setup - Station management - Frequency table" );
    commands_add_menu( cmd, menu );
    if( vidin ) {
        reset_frequency_menu( commands_get_menu( cmd, "frequencies" ),
                              videoinput_get_norm( vidin ), config_get_v4l_freq( cfg ) );
    }

    menu = menu_new( "finetune" );
    menu_set_text( menu, 0, "Setup - Station management - Finetune" );
    sprintf( string, "%c%c%c  Current: ---", 0xee, 0x80, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "stations" );
    menu_set_default_cursor( menu, 1 );
    sprintf( string, "%c%c%c  Decrease", 0xee, 0x80, 0xa9 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_FINETUNE_DOWN, "" );
    menu_set_right_command( menu, 2, TVTIME_FINETUNE_DOWN, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "stations" );
    sprintf( string, "%c%c%c  Increase", 0xee, 0x80, 0xa8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_FINETUNE_UP, "" );
    menu_set_right_command( menu, 3, TVTIME_FINETUNE_UP, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "stations" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "stations" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "stations" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "stations" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "input-ntsc" );
    menu_set_text( menu, 0, "Setup - Input configuration" );
    if( vidin ) {
        sprintf( string, "%c%c%c  Change video source: %s", 0xee, 0x80, 0xbb,
                 videoinput_get_input_name( vidin ) );
    } else {
        sprintf( string, "%c%c%c  Change video source", 0xee, 0x80, 0xbb );
    }
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_TOGGLE_INPUT, "" );
    menu_set_right_command( menu, 1, TVTIME_TOGGLE_INPUT, "" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Preferred audio mode", 0xee, 0x80, 0x9d );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "audiomode" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "audiomode" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Television standard", 0xee, 0x80, 0xad );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "norm" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "norm" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Sharpness", 0xee, 0x80, 0xa7 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "sharpness" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "sharpness" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Toggle closed captions", 0xee, 0x80, 0xb6 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_TOGGLE_CC, "" );
    menu_set_right_command( menu, 5, TVTIME_TOGGLE_CC, "" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Restart with new settings", 0xee, 0x80, 0x9c );
    menu_set_text( menu, 6, string );
    menu_set_enter_command( menu, 6, TVTIME_RESTART, "" );
    menu_set_right_command( menu, 6, TVTIME_RESTART, "" );
    menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 7, string );
    menu_set_enter_command( menu, 7, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 7, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 7, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "input-pal" );
    menu_set_text( menu, 0, "Setup - Input configuration" );
    if( vidin ) {
        sprintf( string, "%c%c%c  Change video source: %s", 0xee, 0x80, 0xbb,
                 videoinput_get_input_name( vidin ) );
    } else {
        sprintf( string, "%c%c%c  Change video source", 0xee, 0x80, 0xbb );
    }
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_TOGGLE_INPUT, "" );
    menu_set_right_command( menu, 1, TVTIME_TOGGLE_INPUT, "" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Preferred audio mode", 0xee, 0x80, 0x9d );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "audiomode" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "audiomode" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Television standard", 0xee, 0x80, 0xad );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "norm" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "norm" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Sharpness", 0xee, 0x80, 0xa7 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "sharpness" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "sharpness" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Restart with new settings", 0xee, 0x80, 0x9c );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_RESTART, "" );
    menu_set_right_command( menu, 5, TVTIME_RESTART, "" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 6, string );
    menu_set_enter_command( menu, 6, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 6, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "sharpness" );
    menu_set_text( menu, 0, "Setup - Input configuration - Sharpness" );
    menu_set_default_cursor( menu, 1 );
    commands_add_menu( cmd, menu );
    reset_sharpness_menu( commands_get_menu( cmd, "sharpness" ), config_get_inputwidth( cfg ) );

    menu = menu_new( "audiomode" );
    menu_set_text( menu, 0, "Setup - Input configuration - Preferred audio mode" );
    commands_add_menu( cmd, menu );
    if( cmd->vidin ) {
        reset_audio_mode_menu( commands_get_menu( cmd, "audiomode" ),
                               videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC,
                               videoinput_get_audio_mode( cmd->vidin ) );
    } else {
        reset_audio_mode_menu( commands_get_menu( cmd, "audiomode" ), 0, 0 );
    }

    menu = menu_new( "norm" );
    menu_set_text( menu, 0, "Setup - Input configuration - Television standard" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 9, string );
    menu_set_enter_command( menu, 9, TVTIME_SHOW_MENU, "input" );
    menu_set_right_command( menu, 9, TVTIME_SHOW_MENU, "input" );
    menu_set_left_command( menu, 9, TVTIME_SHOW_MENU, "input" );

    if( !strcasecmp( config_get_v4l_norm( cfg ), "pal" ) ) {
        cmd->newnorm = "PAL";
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "secam" ) ) {
        cmd->newnorm = "SECAM";
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-nc" ) ) {
        cmd->newnorm = "PAL-Nc";
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-m" ) ) {
        cmd->newnorm = "PAL-M";
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-n" ) ) {
        cmd->newnorm = "PAL-N";
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "ntsc-jp" ) ) {
        cmd->newnorm = "NTSC-JP";
    } else if( !strcasecmp( config_get_v4l_norm( cfg ), "pal-60" ) ) {
        cmd->newnorm = "PAL-60";
    } else {
        cmd->newnorm = "NTSC";
    }
    reset_norm_menu( menu, cmd->newnorm );
    commands_add_menu( cmd, menu );

    menu = menu_new( "output" );
    menu_set_text( menu, 0, "Setup - Output configuration" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "overscan" );
    menu_set_text( menu, 0, "Setup - Output configuration - Overscan" );
    commands_add_menu( cmd, menu );
    reset_overscan_menu( commands_get_menu( cmd, "overscan" ), cmd->overscan );

    menu = menu_new( "processing" );
    menu_set_text( menu, 0, "Setup - Video processing" );
    sprintf( string, "%c%c%c  Deinterlacer configuration", 0xee, 0x80, 0xa1 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "deinterlacer" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "deinterlacer" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Current deinterlacer description", 0xee, 0x80, 0xab );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "deintdescription" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "deintdescription" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Attempted framerate", 0xee, 0x80, 0xac );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "framerate" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "framerate" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Input filters", 0xee, 0x80, 0xae );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "filters" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "filters" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    menu_set_right_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "deinterlacer" );
    menu_set_text( menu, 0, "Setup - Video processing - Deinterlacer configuration" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "deintdescription" );
    menu_set_text( menu, 0, "Setup - Video processing - Deinterlacer description" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "framerate" );
    menu_set_text( menu, 0, "Setup - Video processing - Attempted framerate" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "filters" );
    menu_set_text( menu, 0, "Setup - Video processing - Input filters" );
    if( cmd->apply_luma ) {
        sprintf( string, "%c%c%c  BT8x8 luma correction", 0xee, 0x80, 0xb7 );
    } else {
        sprintf( string, "%c%c%c  BT8x8 luma correction", 0xee, 0x80, 0xb8 );
    }
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_TOGGLE_LUMA_CORRECTION, "" );
    menu_set_right_command( menu, 1, TVTIME_TOGGLE_LUMA_CORRECTION, "" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "processing" );
    sprintf( string, "%c%c%c  Colour invert", 0xee, 0x80, 0xb8 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_TOGGLE_COLOUR_INVERT, "" );
    menu_set_right_command( menu, 2, TVTIME_TOGGLE_COLOUR_INVERT, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "processing" );
    sprintf( string, "%c%c%c  Mirror", 0xee, 0x80, 0xb8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_TOGGLE_MIRROR, "" );
    menu_set_right_command( menu, 3, TVTIME_TOGGLE_MIRROR, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "processing" );
    sprintf( string, "%c%c%c  Chroma killer", 0xee, 0x80, 0xb8 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_TOGGLE_CHROMA_KILL, "" );
    menu_set_right_command( menu, 4, TVTIME_TOGGLE_CHROMA_KILL, "" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "processing" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 5, string );
    menu_set_enter_command( menu, 5, TVTIME_SHOW_MENU, "processing" );
    menu_set_right_command( menu, 5, TVTIME_SHOW_MENU, "processing" );
    menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "processing" );
    commands_add_menu( cmd, menu );

    menu = menu_new( "picture-notuner" );
    menu_set_text( menu, 0, "Setup - Picture" );
    sprintf( string, "%c%c%c  Brightness", 0xe2, 0x98, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "brightness" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "brightness" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Contrast", 0xe2, 0x97, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "contrast" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "contrast" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Colour", 0xe2, 0x98, 0xB0 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "colour" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "colour" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Hue", 0xe2, 0x97, 0xaf );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "hue" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "hue" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    if( config_get_save_restore_picture( cfg ) ) {
        sprintf( string, "%c%c%c  Save current settings as defaults", 0xee, 0x80, 0xbd );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_SAVE_PICTURE_GLOBAL, "" );
        menu_set_right_command( menu, 5, TVTIME_SAVE_PICTURE_GLOBAL, "" );
        menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Reset to global defaults", 0xee, 0x80, 0xbe );
        menu_set_text( menu, 6, string );
        menu_set_enter_command( menu, 6, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_right_command( menu, 6, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 7, string );
        menu_set_enter_command( menu, 7, TVTIME_SHOW_MENU, "root" );
        menu_set_right_command( menu, 7, TVTIME_SHOW_MENU, "root" );
        menu_set_left_command( menu, 7, TVTIME_SHOW_MENU, "root" );
        commands_add_menu( cmd, menu );
    } else {
        sprintf( string, "%c%c%c  Reset to global defaults", 0xee, 0x80, 0xbe );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_right_command( menu, 5, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 6, string );
        menu_set_enter_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        menu_set_right_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        commands_add_menu( cmd, menu );
    }

    menu = menu_new( "picture-tuner" );
    menu_set_text( menu, 0, "Setup - Picture" );
    sprintf( string, "%c%c%c  Brightness", 0xe2, 0x98, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "brightness" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "brightness" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Contrast", 0xe2, 0x97, 0x90 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_SHOW_MENU, "contrast" );
    menu_set_right_command( menu, 2, TVTIME_SHOW_MENU, "contrast" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Colour", 0xe2, 0x98, 0xB0 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_SHOW_MENU, "colour" );
    menu_set_right_command( menu, 3, TVTIME_SHOW_MENU, "colour" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "root" );
    sprintf( string, "%c%c%c  Hue", 0xe2, 0x97, 0xaf );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "hue" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "hue" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "root" );
    if( config_get_save_restore_picture( cfg ) ) {
        sprintf( string, "%c%c%c  Save current settings as global defaults", 0xee, 0x80, 0xbd );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_SAVE_PICTURE_GLOBAL, "" );
        menu_set_right_command( menu, 5, TVTIME_SAVE_PICTURE_GLOBAL, "" );
        menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Save current settings as channel defaults", 0xee, 0x80, 0xbc );
        menu_set_text( menu, 6, string );
        menu_set_enter_command( menu, 6, TVTIME_SAVE_PICTURE_CHANNEL, "" );
        menu_set_right_command( menu, 6, TVTIME_SAVE_PICTURE_CHANNEL, "" );
        menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Reset to global defaults", 0xee, 0x80, 0xbe );
        menu_set_text( menu, 7, string );
        menu_set_enter_command( menu, 7, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_right_command( menu, 7, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_left_command( menu, 7, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 8, string );
        menu_set_enter_command( menu, 8, TVTIME_SHOW_MENU, "root" );
        menu_set_right_command( menu, 8, TVTIME_SHOW_MENU, "root" );
        menu_set_left_command( menu, 8, TVTIME_SHOW_MENU, "root" );
        commands_add_menu( cmd, menu );
    } else {
        sprintf( string, "%c%c%c  Reset to global defaults", 0xee, 0x80, 0xbe );
        menu_set_text( menu, 5, string );
        menu_set_enter_command( menu, 5, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_right_command( menu, 5, TVTIME_AUTO_ADJUST_PICT, "" );
        menu_set_left_command( menu, 5, TVTIME_SHOW_MENU, "root" );
        sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
        menu_set_text( menu, 6, string );
        menu_set_enter_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        menu_set_right_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        menu_set_left_command( menu, 6, TVTIME_SHOW_MENU, "root" );
        commands_add_menu( cmd, menu );
    }


    menu = menu_new( "brightness" );
    menu_set_text( menu, 0, "Setup - Picture - Brightness" );
    sprintf( string, "%c%c%c  Current: ---", 0xee, 0x80, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_default_cursor( menu, 1 );
    sprintf( string, "%c%c%c  Decrease", 0xee, 0x80, 0xa9 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_BRIGHTNESS_DOWN, "" );
    menu_set_right_command( menu, 2, TVTIME_BRIGHTNESS_DOWN, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Increase", 0xee, 0x80, 0xa8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_BRIGHTNESS_UP, "" );
    menu_set_right_command( menu, 3, TVTIME_BRIGHTNESS_UP, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );
    if( vidin ) {
        menu_set_value( commands_get_menu( cmd, "brightness" ), videoinput_get_brightness( cmd->vidin ), 0xe2, 0x98, 0x80 );
    }

    menu = menu_new( "contrast" );
    menu_set_text( menu, 0, "Setup - Picture - Contrast" );
    sprintf( string, "%c%c%c  Current: ---", 0xee, 0x80, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_default_cursor( menu, 1 );
    sprintf( string, "%c%c%c  Decrease", 0xee, 0x80, 0xa9 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_CONTRAST_DOWN, "" );
    menu_set_right_command( menu, 2, TVTIME_CONTRAST_DOWN, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Increase", 0xee, 0x80, 0xa8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_CONTRAST_UP, "" );
    menu_set_right_command( menu, 3, TVTIME_CONTRAST_UP, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );
    if( vidin ) {
        menu_set_value( commands_get_menu( cmd, "contrast" ), videoinput_get_contrast( cmd->vidin ), 0xe2, 0x97, 0x90 );
    }

    menu = menu_new( "colour" );
    menu_set_text( menu, 0, "Setup - Picture - Colour" );
    sprintf( string, "%c%c%c  Current: ---", 0xee, 0x80, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_default_cursor( menu, 1 );
    sprintf( string, "%c%c%c  Decrease", 0xee, 0x80, 0xa9 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_COLOUR_DOWN, "" );
    menu_set_right_command( menu, 2, TVTIME_COLOUR_DOWN, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Increase", 0xee, 0x80, 0xa8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_COLOUR_UP, "" );
    menu_set_right_command( menu, 3, TVTIME_COLOUR_UP, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );
    if( vidin ) {
        menu_set_value( commands_get_menu( cmd, "colour" ), videoinput_get_colour( cmd->vidin ), 0xe2, 0x98, 0xb0 );
    }

    menu = menu_new( "hue" );
    menu_set_text( menu, 0, "Setup - Picture - Hue" );
    sprintf( string, "%c%c%c  Current: ---", 0xee, 0x80, 0x80 );
    menu_set_text( menu, 1, string );
    menu_set_enter_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 1, TVTIME_SHOW_MENU, "picture" );
    menu_set_default_cursor( menu, 1 );
    sprintf( string, "%c%c%c  Decrease", 0xee, 0x80, 0xa9 );
    menu_set_text( menu, 2, string );
    menu_set_enter_command( menu, 2, TVTIME_HUE_DOWN, "" );
    menu_set_right_command( menu, 2, TVTIME_HUE_DOWN, "" );
    menu_set_left_command( menu, 2, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Increase", 0xee, 0x80, 0xa8 );
    menu_set_text( menu, 3, string );
    menu_set_enter_command( menu, 3, TVTIME_HUE_UP, "" );
    menu_set_right_command( menu, 3, TVTIME_HUE_UP, "" );
    menu_set_left_command( menu, 3, TVTIME_SHOW_MENU, "picture" );
    sprintf( string, "%c%c%c  Back", 0xe2, 0x86, 0x90 );
    menu_set_text( menu, 4, string );
    menu_set_enter_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_right_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    menu_set_left_command( menu, 4, TVTIME_SHOW_MENU, "picture" );
    commands_add_menu( cmd, menu );
    if( vidin ) {
        menu_set_value( commands_get_menu( cmd, "hue" ), videoinput_get_hue( cmd->vidin ), 0xe2, 0x97, 0xaf );
    }

    reinit_tuner( cmd );

    return cmd;
}

void commands_delete( commands_t *cmd )
{
    int i;

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( cmd->menus[ i ] ) {
            menu_delete( cmd->menus[ i ] );
        }
    }
    free( cmd );
}

static void add_to_favorites( commands_t *cmd, int pos )
{
    int i;

    for( i = 0; i < NUM_FAVORITES; i++ ) {
        if( cmd->favorites[ i ] == pos ) return;
    }
    cmd->favorites[ cmd->curfavorite ] = pos;
    cmd->curfavorite = (cmd->curfavorite + 1) % NUM_FAVORITES;
    if( cmd->numfavorites < NUM_FAVORITES ) {
        cmd->numfavorites++;
    }
}

static void osd_list_audio_modes( tvtime_osd_t *osd, int ntsc, int curmode )
{
    tvtime_osd_list_set_lines( osd, ntsc ? 4 : 5 );
    tvtime_osd_list_set_text( osd, 0, "Preferred audio channel" );
    tvtime_osd_list_set_text( osd, 1, "Mono" );
    tvtime_osd_list_set_text( osd, 2, "Stereo" );
    tvtime_osd_list_set_text( osd, 3, ntsc ? "SAP" : "Primary Language" );
    if( !ntsc ) tvtime_osd_list_set_text( osd, 4, "Secondary Language" );
    if( curmode == VIDEOINPUT_MONO ) {
        tvtime_osd_list_set_hilight( osd, 1 );
    } else if( curmode == VIDEOINPUT_STEREO ) {
        tvtime_osd_list_set_hilight( osd, 2 );
    } else if( curmode == VIDEOINPUT_LANG1 || (ntsc && curmode == VIDEOINPUT_LANG2) ) {
        tvtime_osd_list_set_hilight( osd, 3 );
    } else if( curmode == VIDEOINPUT_LANG2 ) {
        tvtime_osd_list_set_hilight( osd, 4 );
    }
    tvtime_osd_show_list( osd, 1 );
}

/**
 * Hardcoded menus.
 */

static void menu_off( commands_t *cmd )
{
    tvtime_osd_list_hold( cmd->osd, 0 );
    tvtime_osd_show_list( cmd->osd, 0 );
    cmd->menuactive = 0;
}

static void menu_enter( commands_t *cmd )
{
    if( cmd->curmenu == MENU_FAVORITES ) {
        if( cmd->curmenupos == cmd->numfavorites ) {
            add_to_favorites( cmd, station_get_current_id( cmd->stationmgr ) );
        } else {
            if( cmd->curmenupos < cmd->numfavorites ) {
                station_set( cmd->stationmgr, cmd->favorites[ cmd->curmenupos ] );
                cmd->change_channel = 1;
            }
        }
        menu_off( cmd );
    } else if( cmd->curmenu == MENU_USER ) {
        int command = menu_get_enter_command( cmd->curusermenu, cmd->curmenupos + 1 );
        const char *argument = menu_get_enter_argument( cmd->curusermenu, cmd->curmenupos + 1 );

        /* I check for MENU_ENTER just to avoid a malicious infinite loop. */
        if( command != TVTIME_MENU_ENTER ) {
            commands_handle( cmd, command, argument );
        }
    }
}

static void menu_left( commands_t *cmd )
{
    if( cmd->curmenu == MENU_FAVORITES ) {
        commands_handle( cmd, TVTIME_SHOW_MENU, "stations" );
    } else if( cmd->curmenu == MENU_USER ) {
        int command = menu_get_left_command( cmd->curusermenu, cmd->curmenupos + 1 );
        const char *argument = menu_get_left_argument( cmd->curusermenu, cmd->curmenupos + 1 );

        /* I check for MENU_ENTER just to avoid a malicious infinite loop. */
        if( command != TVTIME_MENU_ENTER ) {
            commands_handle( cmd, command, argument );
        }
    }
}

static void menu_right( commands_t *cmd )
{
    if( cmd->curmenu == MENU_FAVORITES ) {
        menu_enter( cmd );
    } else if( cmd->curmenu == MENU_USER ) {
        int command = menu_get_right_command( cmd->curusermenu, cmd->curmenupos + 1 );
        const char *argument = menu_get_right_argument( cmd->curusermenu, cmd->curmenupos + 1 );

        /* I check for MENU_ENTER just to avoid a malicious infinite loop. */
        if( command != TVTIME_MENU_ENTER ) {
            commands_handle( cmd, command, argument );
        }
    }
}

static void display_current_menu( commands_t *cmd )
{
    int i;

    if( cmd->curmenu == MENU_FAVORITES ) {
        char string[ 128 ];
        tvtime_osd_list_set_lines( cmd->osd, cmd->numfavorites + 3 );
        tvtime_osd_list_set_text( cmd->osd, 0, "Favorites" );
        for( i = 0; i < cmd->numfavorites; i++ ) {
            char text[ 32 ];
            sprintf( text, "%d", cmd->favorites[ i ] );
            tvtime_osd_list_set_text( cmd->osd, i + 1, text );
        }
        sprintf( string, "%c%c%c  Add current station", 0xee, 0x80, 0xa8 );
        tvtime_osd_list_set_text( cmd->osd, cmd->numfavorites + 1, string );
        sprintf( string, "%c%c%c  Exit", 0xee, 0x80, 0xa0 );
        tvtime_osd_list_set_text( cmd->osd, cmd->numfavorites + 2, string );
        cmd->curmenusize = cmd->numfavorites + 2;
    } else if( cmd->curmenu == MENU_USER && cmd->curusermenu ) {
        tvtime_osd_list_set_lines( cmd->osd, menu_get_num_lines( cmd->curusermenu ) );
        for( i = 0; i < menu_get_num_lines( cmd->curusermenu ); i++ ) {
            tvtime_osd_list_set_text( cmd->osd, i, menu_get_text( cmd->curusermenu, i ) );
        }
        cmd->curmenusize = menu_get_num_lines( cmd->curusermenu ) - 1;
    }

    tvtime_osd_list_set_hilight( cmd->osd, cmd->curmenupos + 1 );
    tvtime_osd_list_hold( cmd->osd, 1 );
    tvtime_osd_show_list( cmd->osd, 1 );
}

menu_t *commands_get_menu( commands_t *cmd, const char *menuname )
{
    int i;

    for( i = 0; i < tvtime_num_builtin_menus(); i++ ) {
        if( !strcasecmp( menu_table[ i ].name, menuname ) ) {
            if( menu_table[ i ].menutype == MENU_REDIRECT ) {
                return commands_get_menu( cmd, menu_table[ i ].dest );
            } else {
                return 0;
            }
        }
    }

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( !cmd->menus[ i ] ) break;

        if( !strcasecmp( menuname, menu_get_name( cmd->menus[ i ] ) ) ) {
            return cmd->menus[ i ];
            cmd->curusermenu = cmd->menus[ i ];
            break;
        }
    }

    return 0;
}

static int set_menu( commands_t *cmd, const char *menuname )
{
    int i;

    if( !menuname || !*menuname ) {
        return 0;
    }

    cmd->menuactive = 1;
    cmd->curusermenu = 0;

    for( i = 0; i < tvtime_num_builtin_menus(); i++ ) {
        if( !strcasecmp( menu_table[ i ].name, menuname ) ) {
            if( menu_table[ i ].menutype == MENU_REDIRECT ) {
                return set_menu( cmd, menu_table[ i ].dest );
            } else {
                cmd->curmenu = menu_table[ i ].menutype;
                cmd->curmenupos = 0;
                return 1;
            }
        }
    }

    cmd->curmenu = MENU_USER;
    cmd->curusermenu = 0;

    if( menuname && *menuname ) {
        for( i = 0; i < MAX_USER_MENUS; i++ ) {
            if( !cmd->menus[ i ] ) {
                break;
            }

            if( !strcasecmp( menuname, menu_get_name( cmd->menus[ i ] ) ) ) {
                cmd->curusermenu = cmd->menus[ i ];
                break;
            }
        }
    }

    if( cmd->curusermenu ) {
        cmd->curmenupos = menu_get_cursor( cmd->curusermenu );
        cmd->curmenusize = menu_get_num_lines( cmd->curusermenu ) - 1;
        return 1;
    }

    cmd->menuactive = 0;
    return 0;
}

void commands_refresh_menu( commands_t *cmd )
{
    if( cmd->menuactive ) {
        if( cmd->curmenu == MENU_USER ) {
            const char *curname = menu_get_name( cmd->curusermenu );
            int i;

            for( i = 0; i < tvtime_num_builtin_menus(); i++ ) {
                if( !strncasecmp( menu_table[ i ].name, curname, strlen( menu_table[ i ].name ) ) ) {
                    if( menu_table[ i ].menutype == MENU_REDIRECT ) {
                        set_menu( cmd, menu_table[ i ].name );
                        break;
                    }
                }
            }
        }
        display_current_menu( cmd );
    }
}

void commands_add_menu( commands_t *cmd, menu_t *menu )
{
    int i;

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( cmd->menus[ i ] && !strcasecmp( menu_get_name( cmd->menus[ i ] ), menu_get_name( menu ) ) ) {
            fprintf( stderr, "commands: Adding duplicate menu %s!\n",
                     menu_get_name( menu ) );
        }
        if( !cmd->menus[ i ] ) {
            cmd->menus[ i ] = menu;
            return;
        }
    }
}

void commands_set_station_mgr( commands_t *cmd, station_mgr_t *mgr )
{
    cmd->stationmgr = mgr;
    reinit_tuner( cmd );
}

void commands_clear_menu_positions( commands_t *cmd )
{
    int i;

    for( i = 0; i < MAX_USER_MENUS; i++ ) {
        if( cmd->menus[ i ] ) {
            menu_set_cursor( cmd->menus[ i ], menu_get_default_cursor( cmd->menus[ i ] ) );
        } else {
            return;
        }
    }
}

int commands_in_menu( commands_t *cmd )
{
    return cmd->menuactive;
}

void commands_handle( commands_t *cmd, int tvtime_cmd, const char *arg )
{
    time_t now;
    int volume;
    int key;

    if( tvtime_cmd == TVTIME_NOCOMMAND ) return;

    if( cmd->menuactive && tvtime_is_menu_command( tvtime_cmd ) ) {
        int x, y, line;
        switch( tvtime_cmd ) {
        case TVTIME_MENU_EXIT:
            menu_off( cmd );
            break;
        case TVTIME_MENU_UP:
            cmd->curmenupos = (cmd->curmenupos + cmd->curmenusize - 1) % (cmd->curmenusize);
            if( cmd->curusermenu ) menu_set_cursor( cmd->curusermenu, cmd->curmenupos );
            display_current_menu( cmd );
            break;
        case TVTIME_MENU_DOWN:
            cmd->curmenupos = (cmd->curmenupos + 1) % (cmd->curmenusize);
            if( cmd->curusermenu ) menu_set_cursor( cmd->curusermenu, cmd->curmenupos );
            display_current_menu( cmd );
            break;
        case TVTIME_MENU_LEFT: menu_left( cmd ); break;
        case TVTIME_MENU_RIGHT: menu_right( cmd ); break;
        case TVTIME_MENU_ENTER: menu_enter( cmd ); break;
        case TVTIME_SHOW_MENU:
            if( set_menu( cmd, arg ) ) {
                display_current_menu( cmd );
            } else {
                menu_off( cmd );
            }
            break;
        case TVTIME_MOUSE_MOVE:
            sscanf( arg, "%d %d", &x, &y );
            if( cmd->halfsize ) y *= 2;
            line = tvtime_osd_list_get_line_pos( cmd->osd, y );
            if( line > 0 ) {
                cmd->curmenupos = (line - 1);
                if( cmd->curusermenu ) menu_set_cursor( cmd->curusermenu, cmd->curmenupos );
                display_current_menu( cmd );
            }
            break;
        }
        return;
    }

    switch( tvtime_cmd ) {
    case TVTIME_QUIT:
        cmd->quit = 1;
        break;

    case TVTIME_SHOW_MENU:
        if( cmd->osd ) {
            commands_clear_menu_positions( cmd );
            if( set_menu( cmd, arg ) || set_menu( cmd, "root" ) ) {
                display_current_menu( cmd );
            }
        }
        break;

    case TVTIME_KEY_EVENT:
        key = input_string_to_special_key( arg );
        if( !key ) key = arg[ 0 ];
        tvtime_cmd = config_key_to_command( cmd->cfg, key );
        if( tvtime_cmd != TVTIME_KEY_EVENT ) {
            commands_handle( cmd, tvtime_cmd, arg );
        }
        break;

    case TVTIME_SLEEP:
        time( &now );
        /**
         * increment sleeptimer by SLEEPTIMER_STEP if user hits
         * button within 5 seconds else turn it off
         */
        if( cmd->sleeptimer_start && (now > (cmd->sleeptimer_start + 5)) ) {
            cmd->sleeptimer = 0;
            cmd->sleeptimer_start = 0;
        } else if( cmd->sleeptimer > SLEEPTIMER_NUMSTEPS ) {
            cmd->sleeptimer = 0;
            cmd->sleeptimer_start = 0;
        } else {
            cmd->sleeptimer++;
            cmd->sleeptimer_start = now;
        }

        if( cmd->osd ) {
            char message[ 25 ];

            if( cmd->sleeptimer ) {
                sprintf( message, "Sleep in %d minutes.",
                         sleeptimer_function( cmd->sleeptimer ) );
            } else {
                sprintf( message, "Sleep off." );
            }

            tvtime_osd_show_message( cmd->osd, message );
        }
        break;

    case TVTIME_SHOW_DEINTERLACER_INFO:
        cmd->showdeinterlacerinfo = 1;
        break;

    case TVTIME_SHOW_STATS:
        cmd->printdebug = 1;
        break;

    case TVTIME_RESTART:
        cmd->restarttvtime = 1;
        break;

    case TVTIME_SCREENSHOT:
        cmd->screenshot = 1;
        break;

    case TVTIME_TOGGLE_BARS:
        cmd->showbars = !cmd->showbars;
        break;

    case TVTIME_TOGGLE_FULLSCREEN:
        cmd->togglefullscreen = 1;
        break;

    case TVTIME_SCROLL_CONSOLE_UP:
    case TVTIME_SCROLL_CONSOLE_DOWN:
        if( cmd->console_on )
            console_scroll_n( cmd->console, (tvtime_cmd == TVTIME_SCROLL_CONSOLE_UP) ? -1 : 1 );
        break;
            
    case TVTIME_TOGGLE_FRAMERATE:
        cmd->framerate = (cmd->framerate + 1) % FRAMERATE_MAX;
        break;

    case TVTIME_TOGGLE_CONSOLE:
        cmd->console_on = !cmd->console_on;
        console_toggle_console( cmd->console );
        break;

    case TVTIME_CHANNEL_SKIP:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            station_set_current_active( cmd->stationmgr, !station_get_current_active( cmd->stationmgr ) );
            if( cmd->osd ) {
                if( station_get_current_active( cmd->stationmgr ) ) {
                    tvtime_osd_show_message( cmd->osd, "Channel active in list." );
                } else {
                    tvtime_osd_show_message( cmd->osd, "Channel disabled from list." );
                }
            }
            reset_stations_menu( commands_get_menu( cmd, "stations" ),
                                 (videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC ||
                                  videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC_JP),
                                 videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_PAL,
                                 videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_SECAM,
                                 (!strcasecmp( cmd->newfreqtable, "us-cable" ) ||
                                  !strcasecmp( cmd->newfreqtable, "us-cable100" )),
                                 station_get_current_active( cmd->stationmgr ), cmd->checkfreq,
                                 cmd->scan_channels );
            commands_refresh_menu( cmd );
            station_writeconfig( cmd->stationmgr );
        }
        break;

    case TVTIME_SET_DEINTERLACER:
        cmd->setdeinterlacer = 1;
        snprintf( cmd->deinterlacer, sizeof( cmd->deinterlacer ), "%s", arg );
        break;

    case TVTIME_SET_FREQUENCY_TABLE:
        cmd->setfreqtable = 1;
        snprintf( cmd->newfreqtable, sizeof( cmd->newfreqtable ), "%s", arg );
        if( cmd->vidin ) {
            reset_frequency_menu( commands_get_menu( cmd, "frequencies" ),
                                  videoinput_get_norm( cmd->vidin ), cmd->newfreqtable );
            commands_refresh_menu( cmd );
        }
        break;

    case TVTIME_SET_FRAMERATE:
        if( !strcasecmp( arg, "full" ) ) {
            cmd->framerate = FRAMERATE_FULL;
        } else if( !strcasecmp( arg, "top" ) ) {
            cmd->framerate = FRAMERATE_HALF_TFF;
        } else {
            cmd->framerate = FRAMERATE_HALF_BFF;
        }
        if( cmd->osd ) {
            char message[ 128 ];
            sprintf( message, "Framerate set at %s.",
                     cmd->framerate == FRAMERATE_FULL ? "full" :
                     (cmd->framerate == FRAMERATE_HALF_TFF ? "half (top fields processed)" : "half (bottom fields processed)") );
            tvtime_osd_show_message( cmd->osd, message );
        }
        break;

    case TVTIME_SET_SHARPNESS:
        cmd->newsharpness = atoi( arg );
        if( cmd->osd ) {
            menu_t *sharpmenu = commands_get_menu( cmd, "sharpness" );
            char message[ 128 ];
            reset_sharpness_menu( sharpmenu, cmd->newsharpness );
            commands_refresh_menu( cmd );
            sprintf( message, "Sharpness will be %d on restart.",
                     cmd->newsharpness );
            tvtime_osd_show_message( cmd->osd, message );
        }
        break;

    case TVTIME_SET_NORM:
        if( !arg || !*arg ) {
            cmd->newnorm = "NTSC";
        } else if( !strcasecmp( arg, "pal" ) ) {
            cmd->newnorm = "PAL";
        } else if( !strcasecmp( arg, "secam" ) ) {
            cmd->newnorm = "SECAM";
        } else if( !strcasecmp( arg, "pal-nc" ) ) {
            cmd->newnorm = "PAL-Nc";
        } else if( !strcasecmp( arg, "pal-m" ) ) {
            cmd->newnorm = "PAL-M";
        } else if( !strcasecmp( arg, "pal-n" ) ) {
            cmd->newnorm = "PAL-N";
        } else if( !strcasecmp( arg, "ntsc-jp" ) ) {
            cmd->newnorm = "NTSC-JP";
        } else if( !strcasecmp( arg, "pal-60" ) ) {
            cmd->newnorm = "PAL-60";
        } else {
            cmd->newnorm = "NTSC";
        }
        cmd->normset = 1;

        if( cmd->osd ) {
            menu_t *normmenu = commands_get_menu( cmd, "norm" );
            char message[ 128 ];
            reset_norm_menu( normmenu, cmd->newnorm );
            commands_refresh_menu( cmd );
            sprintf( message, "Television standard will be %s on restart.",
                     cmd->newnorm );
            tvtime_osd_show_message( cmd->osd, message );
        }
        break;
            
    case TVTIME_TOGGLE_ASPECT:
        cmd->toggleaspect = 1;
        break;

    case TVTIME_TOGGLE_ALWAYSONTOP:
        cmd->togglealwaysontop = 1;
        break;

    case TVTIME_CHANNEL_SAVE_TUNING:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            char freq[ 32 ];
            int pos;

            snprintf( freq, sizeof( freq ), "%f", ((double) videoinput_get_tuner_freq( cmd->vidin )) / 1000.0 );
            pos = station_add( cmd->stationmgr, 0, "Custom", freq, 0 );
            station_writeconfig( cmd->stationmgr );
            station_set( cmd->stationmgr, pos );
            cmd->change_channel = 1;
        }
        break;

    case TVTIME_CHANNEL_ACTIVATE_ALL:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "All channels re-activated." );
            station_activate_all_channels( cmd->stationmgr );
            station_writeconfig( cmd->stationmgr );
        }
        break;

    case TVTIME_CHANNEL_RENUMBER:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            /* If we're scanning and suddenly want to renumber, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            /* Accept input of the destination channel. */
            if( cmd->digit_counter == 0 ) memset( cmd->next_chan_buffer, 0, 5 );
            cmd->frame_counter = cmd->delay;
            cmd->renumbering = 1;
            if( cmd->osd ) {
                char message[ 256 ];
                snprintf( message, sizeof( message ),
                          "Remapping %d.  Enter new channel number.",
                          station_get_current_id( cmd->stationmgr ) );
                tvtime_osd_set_hold_message( cmd->osd, message );
            }
        }
        break;

    case TVTIME_CHANNEL_SCAN:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            if( !config_get_check_freq_present( cmd->cfg ) ) {
                if( cmd->osd ) {
                    tvtime_osd_show_message( cmd->osd, "Scanner unavailable: Signal checking disabled." );
                }
            } else {
                cmd->scan_channels = !cmd->scan_channels;

                if( cmd->scan_channels && cmd->renumbering ) {
                    memset( cmd->next_chan_buffer, 0, 5 );
                    cmd->digit_counter = 0;
                    cmd->frame_counter = 0;
                    if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
                    cmd->renumbering = 0;
                }

                if( cmd->osd ) {
                    reset_stations_menu( commands_get_menu( cmd, "stations" ),
                                         (videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC ||
                                          videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC_JP),
                                         videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_PAL,
                                         videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_SECAM,
                                         (!strcasecmp( cmd->newfreqtable, "us-cable" ) ||
                                          !strcasecmp( cmd->newfreqtable, "us-cable100" )),
                                         station_get_current_active( cmd->stationmgr ), cmd->checkfreq,
                                         cmd->scan_channels );
                    commands_refresh_menu( cmd );

                    if( cmd->scan_channels ) {
                        tvtime_osd_set_hold_message( cmd->osd, "Scanning for active channels." );
                    } else {
                        /* Nuke the hold message, and make sure we show nothing (hack). */
                        tvtime_osd_set_hold_message( cmd->osd, "" );
                        tvtime_osd_show_message( cmd->osd, " " );
                    }
                    tvtime_osd_show_info( cmd->osd );
                }
            }
        }
        break;

    case TVTIME_TOGGLE_CC:
        if( cmd->vbi ) {
            vbidata_capture_mode( cmd->vbi, cmd->capturemode ? CAPTURE_OFF : CAPTURE_CC1 );
            if( cmd->capturemode ) {
                if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "Closed Captioning Disabled." );
                cmd->capturemode = CAPTURE_OFF;
            } else {
                if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "Closed Captioning Enabled." );
                cmd->capturemode = CAPTURE_CC1;
            }
        } else {
            if( cmd->osd ) tvtime_osd_show_message( cmd->osd, "No VBI device configured for CC decoding." );
        }
        break;

    case TVTIME_TOGGLE_PAL_SECAM:
        videoinput_toggle_pal_secam( cmd->vidin );
        if( videoinput_has_tuner( cmd->vidin ) ) {
            station_set_current_norm( cmd->stationmgr, videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            station_writeconfig( cmd->stationmgr );
        }
        if( cmd->osd ) {
            char message[ 128 ];
            tvtime_osd_set_norm( cmd->osd, videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            tvtime_osd_show_info( cmd->osd );
            sprintf( message, "Colour decoding for this channel set to %s.", videoinput_get_norm_name( videoinput_get_norm( cmd->vidin ) ) );
            tvtime_osd_show_message( cmd->osd, message );
        }
        break;

    case TVTIME_DISPLAY_MESSAGE:
        if( cmd->osd && arg ) tvtime_osd_show_message( cmd->osd, arg );
        break;

    case TVTIME_DISPLAY_INFO:
        cmd->displayinfo = !cmd->displayinfo;
        if( cmd->osd ) {
            if( cmd->displayinfo ) {
                tvtime_osd_hold( cmd->osd, 1 );
                tvtime_osd_show_info( cmd->osd );
            } else {
                tvtime_osd_hold( cmd->osd, 0 );
                tvtime_osd_clear( cmd->osd );
            }
        }
        break;

    case TVTIME_RUN_COMMAND:
        if( arg ) {
            char commandline[ 256 ];
            sprintf( commandline, "%s &", arg );
            if( cmd->osd ) {
                char message[ 256 ];
                sprintf( message, "Running: %s", arg );
                tvtime_osd_show_message( cmd->osd, message );
            }
            system( commandline );
        }
        break;

    case TVTIME_TOGGLE_CREDITS:
        break;

    case TVTIME_SET_AUDIO_MODE:
        if( cmd->vidin ) {
            if( !strcasecmp( arg, "mono" ) ) {
                videoinput_set_audio_mode( cmd->vidin, VIDEOINPUT_MONO );
            } else if( !strcasecmp( arg, "stereo" ) ) {
                videoinput_set_audio_mode( cmd->vidin, VIDEOINPUT_STEREO );
            } else if( !strcasecmp( arg, "sap" ) || !strcasecmp( arg, "lang1" ) ) {
                videoinput_set_audio_mode( cmd->vidin, VIDEOINPUT_LANG1 );
            } else {
                videoinput_set_audio_mode( cmd->vidin, VIDEOINPUT_LANG2 );
            }
            if( cmd->osd ) {
                if( !cmd->menuactive ) {
                    osd_list_audio_modes( cmd->osd, videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC,
                                          videoinput_get_audio_mode( cmd->vidin ) );
                }
                tvtime_osd_set_audio_mode( cmd->osd, videoinput_get_audio_mode_name( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) ) );
                tvtime_osd_show_info( cmd->osd );
                reset_audio_mode_menu( commands_get_menu( cmd, "audiomode" ),
                                       videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC,
                                       videoinput_get_audio_mode( cmd->vidin ) );
                commands_refresh_menu( cmd );
            }
        }
        break;

    case TVTIME_TOGGLE_AUDIO_MODE:
        if( cmd->vidin ) {
            videoinput_set_audio_mode( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) << 1 );
            if( cmd->osd ) {
                osd_list_audio_modes( cmd->osd, videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC,
                                      videoinput_get_audio_mode( cmd->vidin ) );
                tvtime_osd_set_audio_mode( cmd->osd, videoinput_get_audio_mode_name( cmd->vidin, videoinput_get_audio_mode( cmd->vidin ) ) );
                tvtime_osd_show_info( cmd->osd );
                reset_audio_mode_menu( commands_get_menu( cmd, "audiomode" ),
                                       videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC,
                                       videoinput_get_audio_mode( cmd->vidin ) );
                commands_refresh_menu( cmd );
            }
        }
        break;

    case TVTIME_TOGGLE_DEINTERLACER:
        cmd->toggledeinterlacer = 1;
        break;

    case TVTIME_TOGGLE_PULLDOWN_DETECTION:
        cmd->togglepulldowndetection = 1;
        break;

    case TVTIME_TOGGLE_SIGNAL_DETECTION:
        cmd->checkfreq = !cmd->checkfreq;
        if( !cmd->checkfreq && cmd->scan_channels ) {
            commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
        }
        if( cmd->osd ) {
            reset_stations_menu( commands_get_menu( cmd, "stations" ),
                                 (videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC ||
                                  videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_NTSC_JP),
                                 videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_PAL,
                                 videoinput_get_norm( cmd->vidin ) == VIDEOINPUT_SECAM,
                                 (!strcasecmp( cmd->newfreqtable, "us-cable" ) ||
                                  !strcasecmp( cmd->newfreqtable, "us-cable100" )),
                                 station_get_current_active( cmd->stationmgr ), cmd->checkfreq,
                                 cmd->scan_channels );
            if( cmd->checkfreq ) {
                tvtime_osd_show_message( cmd->osd, "Signal detection enabled." );
            } else {
                tvtime_osd_show_message( cmd->osd, "Signal detection disabled." );
            }
            commands_refresh_menu( cmd );
        }
        break;

    case TVTIME_CHANNEL_CHAR:
        if( arg && isdigit( arg[ 0 ] ) && cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            /* Decode the input char from commands.  */
            if( cmd->digit_counter == 0 ) memset( cmd->next_chan_buffer, 0, 5 );
            cmd->next_chan_buffer[ cmd->digit_counter ] = arg[ 0 ];
            cmd->digit_counter++;
            cmd->frame_counter = cmd->delay;

            /**
             * Send an enter command if we type more
             * digits than there are channels.
             */
            if( cmd->digit_counter > 0 && (station_get_max_position( cmd->stationmgr ) < 10) ) {
                commands_handle( cmd, TVTIME_ENTER, 0 );
            } else if( cmd->digit_counter > 1 && (station_get_max_position( cmd->stationmgr ) < 100) ) {
                commands_handle( cmd, TVTIME_ENTER, 0 );
            } else if( cmd->digit_counter > 2 ) {
                commands_handle( cmd, TVTIME_ENTER, 0 );
            }
        }
        break;

    case TVTIME_TOGGLE_COLOUR_INVERT:
        cmd->apply_invert = !cmd->apply_invert;
        if( cmd->osd ) {
            menu_t *filtermenu = commands_get_menu( cmd, "filters" );
            char string[ 128 ];

            if( cmd->apply_invert ) {
                sprintf( string, "%c%c%c  Colour invert", 0xee, 0x80, 0xb7 );
            } else {
                sprintf( string, "%c%c%c  Colour invert", 0xee, 0x80, 0xb8 );
            }
            menu_set_text( filtermenu, 2, string );
            commands_refresh_menu( cmd );

            if( cmd->apply_invert ) {
                tvtime_osd_show_message( cmd->osd, "Colour invert enabled." );
            } else {
                tvtime_osd_show_message( cmd->osd, "Colour invert disabled." );
            }
        }
        break;

    case TVTIME_TOGGLE_MIRROR:
        cmd->apply_mirror = !cmd->apply_mirror;
        if( cmd->osd ) {
            menu_t *filtermenu = commands_get_menu( cmd, "filters" );
            char string[ 128 ];

            if( cmd->apply_mirror ) {
                sprintf( string, "%c%c%c  Mirror", 0xee, 0x80, 0xb7 );
            } else {
                sprintf( string, "%c%c%c  Mirror", 0xee, 0x80, 0xb8 );
            }
            menu_set_text( filtermenu, 3, string );
            commands_refresh_menu( cmd );

            if( cmd->apply_mirror ) {
                tvtime_osd_show_message( cmd->osd, "Mirror enabled." );
            } else {
                tvtime_osd_show_message( cmd->osd, "Mirror disabled." );
            }
        }
        break;

    case TVTIME_TOGGLE_CHROMA_KILL:
        cmd->apply_chroma_kill = !cmd->apply_chroma_kill;
        if( cmd->osd ) {
            menu_t *filtermenu = commands_get_menu( cmd, "filters" );
            char string[ 128 ];

            if( cmd->apply_chroma_kill ) {
                sprintf( string, "%c%c%c  Chroma killer", 0xee, 0x80, 0xb7 );
            } else {
                sprintf( string, "%c%c%c  Chroma killer", 0xee, 0x80, 0xb8 );
            }
            menu_set_text( filtermenu, 4, string );
            commands_refresh_menu( cmd );

            if( cmd->apply_chroma_kill ) {
                tvtime_osd_show_message( cmd->osd, "Chroma kill enabled." );
            } else {
                tvtime_osd_show_message( cmd->osd, "Chroma kill disabled." );
            }
        }
        break;

    case TVTIME_TOGGLE_LUMA_CORRECTION:
        cmd->apply_luma = !cmd->apply_luma;
        if( cmd->osd ) {
            menu_t *filtermenu = commands_get_menu( cmd, "filters" );
            char string[ 128 ];

            if( cmd->apply_luma ) {
                sprintf( string, "%c%c%c  BT8x8 luma correction", 0xee, 0x80, 0xb7 );
            } else {
                sprintf( string, "%c%c%c  BT8x8 luma correction", 0xee, 0x80, 0xb8 );
            }
            menu_set_text( filtermenu, 1, string );
            commands_refresh_menu( cmd );

            if( cmd->apply_luma ) {
                tvtime_osd_show_message( cmd->osd, "Luma correction enabled." );
                config_save( cmd->cfg, "ApplyLumaCorrection", "1" );
            } else {
                tvtime_osd_show_message( cmd->osd, "Luma correction disabled." );
                config_save( cmd->cfg, "ApplyLumaCorrection", "0" );
            }
        }
        break;

    case TVTIME_OVERSCAN_UP:
    case TVTIME_OVERSCAN_DOWN:
        cmd->overscan = cmd->overscan + ( (tvtime_cmd == TVTIME_OVERSCAN_UP) ? 0.0025 : -0.0025 );
        if( cmd->overscan > 0.4 ) cmd->overscan = 0.4; if( cmd->overscan < 0.0 ) cmd->overscan = 0.0;

        if( cmd->osd ) {
            char message[ 200 ];
            snprintf( message, sizeof( message ), "Overscan: %.1f%%",
                      cmd->overscan * 2.0 * 100.0 );
            tvtime_osd_show_message( cmd->osd, message );
            reset_overscan_menu( commands_get_menu( cmd, "overscan" ), cmd->overscan );
            commands_refresh_menu( cmd );
        }
        break;

    case TVTIME_LUMA_UP:
    case TVTIME_LUMA_DOWN:
        if( !cmd->apply_luma ) {
            fprintf( stderr, "tvtime: Luma correction disabled.  "
                     "Run with -c to use it.\n" );
        } else {
            char message[ 200 ];
            cmd->luma_power = cmd->luma_power + ( (tvtime_cmd == TVTIME_LUMA_UP) ? 0.1 : -0.1 );

            cmd->update_luma = 1;

            if( cmd->luma_power > 10.0 ) cmd->luma_power = 10.0;
            if( cmd->luma_power <  0.0 ) cmd->luma_power = 0.0;

            snprintf( message, sizeof( message ), "%.1f", cmd->luma_power );
            config_save( cmd->cfg, "LumaCorrection", message );
            if( cmd->osd ) {
                snprintf( message, sizeof( message ), "Luma correction value: %.1f",
                          cmd->luma_power );
                tvtime_osd_show_message( cmd->osd, message );
            }
        }
        break;

    case TVTIME_AUTO_ADJUST_PICT:
        if( cmd->vidin ) {
            if( cmd->brightness >= 0 ) {
                videoinput_set_brightness( cmd->vidin, cmd->brightness );
            } else {
                videoinput_set_brightness( cmd->vidin, 50 );
            }
            if( cmd->contrast >= 0 ) {
                videoinput_set_contrast( cmd->vidin, cmd->contrast );
            } else {
                videoinput_set_contrast( cmd->vidin, 50 );
            }
            if( cmd->colour >= 0 ) {
                videoinput_set_colour( cmd->vidin, cmd->colour );
            } else {
                videoinput_set_colour( cmd->vidin, 50 );
            }
            if( cmd->hue >= 0 ) {
                videoinput_set_hue( cmd->vidin, cmd->hue );
            } else {
                videoinput_set_hue( cmd->vidin, 50 );
            }
            if( cmd->osd ) {
                tvtime_osd_show_message( cmd->osd, "Picture settings reset to defaults." );
                menu_set_value( commands_get_menu( cmd, "brightness" ), videoinput_get_brightness( cmd->vidin ), 0xe2, 0x98, 0x80 );
                menu_set_value( commands_get_menu( cmd, "contrast" ), videoinput_get_contrast( cmd->vidin ), 0xe2, 0x97, 0x90 );
                menu_set_value( commands_get_menu( cmd, "colour" ), videoinput_get_colour( cmd->vidin ), 0xe2, 0x98, 0xb0 );
                menu_set_value( commands_get_menu( cmd, "hue" ), videoinput_get_hue( cmd->vidin ), 0xe2, 0x97, 0xaf );
            }
        }
        break;

    case TVTIME_AUTO_ADJUST_WINDOW:
        cmd->resizewindow = 1;
        break;

    case TVTIME_TOGGLE_NTSC_CABLE_MODE:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            station_toggle_us_cable_mode( cmd->stationmgr );
            cmd->change_channel = 1;
        }
        break;

    case TVTIME_FINETUNE_DOWN:
    case TVTIME_FINETUNE_UP:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {
            station_set_current_finetune( cmd->stationmgr, station_get_current_finetune( cmd->stationmgr )
                                          + (tvtime_cmd == TVTIME_FINETUNE_UP ? 1 : -1) );
            videoinput_set_tuner_freq( cmd->vidin, station_get_current_frequency( cmd->stationmgr )
                                       + ((station_get_current_finetune( cmd->stationmgr ) * 1000)/16) );

            if( cmd->vbi ) {
                vbidata_reset( cmd->vbi );
                vbidata_capture_mode( cmd->vbi, cmd->capturemode );
            }

            if( cmd->osd ) {
                menu_set_value( commands_get_menu( cmd, "finetune" ), station_get_current_finetune( cmd->stationmgr ), 0xee, 0x80, 0xb0 );
                commands_refresh_menu( cmd );
                tvtime_osd_show_data_bar_centered( cmd->osd, "Finetune",
                                                   station_get_current_finetune( cmd->stationmgr ) );
            }
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_CHANNEL_INC: 
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /**
             * If we're scanning and the user hits a key, stop scanning.
             * arg will be 0 if the scanner has called us.
             */
            if( cmd->scan_channels && arg ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_inc( cmd->stationmgr );
            cmd->change_channel = 1;
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;
    case TVTIME_CHANNEL_DEC:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_dec( cmd->stationmgr );
            cmd->change_channel = 1;
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_CHANNEL_PREV:
        if( cmd->vidin && videoinput_has_tuner( cmd->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_prev( cmd->stationmgr );
            cmd->change_channel = 1;
        } else if( cmd->osd ) {
            tvtime_osd_show_info( cmd->osd );
        }
        break;

    case TVTIME_MIXER_TOGGLE_MUTE:
        mixer_mute( !mixer_ismute() );

        if( cmd->osd ) {
            tvtime_osd_show_data_bar( cmd->osd, "Volume", (mixer_get_volume()) & 0xff );
        }
        break;

    case TVTIME_MIXER_UP:
    case TVTIME_MIXER_DOWN:

        /* If the user hits the volume control, drop us out of mute mode. */
        if( cmd->vidin && videoinput_get_muted( cmd->vidin ) ) {
            commands_handle( cmd, TVTIME_TOGGLE_MUTE, 0 );
        }
        volume = mixer_set_volume( ( (tvtime_cmd == TVTIME_MIXER_UP) ? 1 : -1 ) );

        if( cmd->osd ) {
            tvtime_osd_show_data_bar( cmd->osd, "Volume", volume & 0xff );
        }
        break;

    case TVTIME_TOGGLE_MUTE:
        if( cmd->vidin ) {
            videoinput_mute( cmd->vidin, !videoinput_get_muted( cmd->vidin ) );
            if( cmd->osd ) {
                tvtime_osd_volume_muted( cmd->osd, videoinput_get_muted( cmd->vidin ) );
            }
        }
        break;

    case TVTIME_TOGGLE_INPUT:
        if( cmd->vidin ) {
            cmd->frame_counter = 0;

            if( cmd->renumbering ) {
                memset( cmd->next_chan_buffer, 0, sizeof( cmd->next_chan_buffer ) );
                commands_handle( cmd, TVTIME_ENTER, 0 );
            }

            if( cmd->scan_channels ) {
                commands_handle( cmd, TVTIME_CHANNEL_SCAN, 0 );
            }

            videoinput_set_input_num( cmd->vidin, ( videoinput_get_input_num( cmd->vidin ) + 1 ) % videoinput_get_num_inputs( cmd->vidin ) );
            reinit_tuner( cmd );

            if( cmd->osd ) {
                char string[ 128 ];
                sprintf( string, "%c%c%c  Change video source: %s", 0xee, 0x80, 0xbb,
                         videoinput_get_input_name( cmd->vidin ) );
                menu_set_text( commands_get_menu( cmd, "input" ), 1, string );
                commands_refresh_menu( cmd );
                tvtime_osd_set_input( cmd->osd, videoinput_get_input_name( cmd->vidin ) );
                tvtime_osd_show_info( cmd->osd );
            }
        }
        break;

    case TVTIME_HUE_UP:
    case TVTIME_HUE_DOWN:
        if( cmd->vidin ) {
            videoinput_set_hue_relative( cmd->vidin, (tvtime_cmd == TVTIME_HUE_UP) ? 1 : -1 );
            if( cmd->osd ) {
                int hue = videoinput_get_hue( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Hue", hue );
                menu_set_value( commands_get_menu( cmd, "hue" ), hue, 0xe2, 0x97, 0xaf );
                commands_refresh_menu( cmd );
            }
        }
        break;

    case TVTIME_BRIGHTNESS_UP: 
    case TVTIME_BRIGHTNESS_DOWN:
        if( cmd->vidin ) {
            videoinput_set_brightness_relative( cmd->vidin, (tvtime_cmd == TVTIME_BRIGHTNESS_UP) ? 1 : -1 );
            if( cmd->osd ) {
                int brightness = videoinput_get_brightness( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Brightness", brightness );
                menu_set_value( commands_get_menu( cmd, "brightness" ), brightness, 0xe2, 0x98, 0x80 );
                commands_refresh_menu( cmd );
            }
        }
        break;

    case TVTIME_CONTRAST_UP:
    case TVTIME_CONTRAST_DOWN:
        if( cmd->vidin ) {
            videoinput_set_contrast_relative( cmd->vidin, (tvtime_cmd == TVTIME_CONTRAST_UP) ? 1 : -1 );
            if( cmd->osd ) {
                int contrast = videoinput_get_contrast( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Contrast", contrast );
                menu_set_value( commands_get_menu( cmd, "contrast" ), contrast, 0xe2, 0x97, 0x90 );
                commands_refresh_menu( cmd );
            }
        }
        break;

    case TVTIME_COLOUR_UP:
    case TVTIME_COLOUR_DOWN:
        if( cmd->vidin ) {
            videoinput_set_colour_relative( cmd->vidin, (tvtime_cmd == TVTIME_COLOUR_UP) ? 1 : -1 );
            if( cmd->osd ) {
                int colour = videoinput_get_colour( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Colour", colour );
                menu_set_value( commands_get_menu( cmd, "colour" ), colour, 0xe2, 0x98, 0xb0 );
                commands_refresh_menu( cmd );
            }
        }
        break;

    case TVTIME_PICTURE:
        cmd->picturemode = (cmd->picturemode + 1) % 4;
        if( cmd->osd && cmd->vidin ) {
            if( cmd->picturemode == 0 ) {
                int cur = videoinput_get_brightness( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Brightness", cur );
            } else if( cmd->picturemode == 1 ) {
                int cur = videoinput_get_contrast( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Contrast", cur );
            } else if( cmd->picturemode == 2 ) {
                int cur = videoinput_get_colour( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Colour", cur );
            } else if( cmd->picturemode == 3 ) {
                int cur = videoinput_get_hue( cmd->vidin );
                tvtime_osd_show_data_bar( cmd->osd, "Hue", cur );
            }
        }
        break;

    case TVTIME_PICTURE_UP:
        if( cmd->picturemode == 0 ) {
            commands_handle( cmd, TVTIME_BRIGHTNESS_UP, "" );
        } else if( cmd->picturemode == 1 ) {
            commands_handle( cmd, TVTIME_CONTRAST_UP, "" );
        } else if( cmd->picturemode == 2 ) {
            commands_handle( cmd, TVTIME_COLOUR_UP, "" );
        } else if( cmd->picturemode == 3 ) {
            commands_handle( cmd, TVTIME_HUE_UP, "" );
        }
        break;

    case TVTIME_PICTURE_DOWN:
        if( cmd->picturemode == 0 ) {
            commands_handle( cmd, TVTIME_BRIGHTNESS_DOWN, "" );
        } else if( cmd->picturemode == 1 ) {
            commands_handle( cmd, TVTIME_CONTRAST_DOWN, "" );
        } else if( cmd->picturemode == 2 ) {
            commands_handle( cmd, TVTIME_COLOUR_DOWN, "" );
        } else if( cmd->picturemode == 3 ) {
            commands_handle( cmd, TVTIME_HUE_DOWN, "" );
        }
        break;

    case TVTIME_SAVE_PICTURE_GLOBAL:
        if( cmd->vidin && config_get_save_restore_picture( cmd->cfg ) ) {
            cmd->brightness = videoinput_get_brightness( cmd->vidin );
            cmd->contrast = videoinput_get_contrast( cmd->vidin );
            cmd->colour = videoinput_get_colour( cmd->vidin );
            cmd->hue = videoinput_get_hue( cmd->vidin );
            if( cmd->osd ) {
                char message[ 128 ];
                sprintf( message, "Saved current picture settings as global defaults.\n" );
                tvtime_osd_show_message( cmd->osd, message );
            }
        }
        break;
    case TVTIME_SAVE_PICTURE_CHANNEL:
        if( cmd->stationmgr && cmd->vidin && config_get_save_restore_picture( cmd->cfg ) ) {
            station_set_current_brightness( cmd->stationmgr, videoinput_get_brightness( cmd->vidin ) );
            station_set_current_contrast( cmd->stationmgr, videoinput_get_contrast( cmd->vidin ) );
            station_set_current_colour( cmd->stationmgr, videoinput_get_colour( cmd->vidin ) );
            station_set_current_hue( cmd->stationmgr, videoinput_get_hue( cmd->vidin ) );
            if( cmd->osd ) {
                char message[ 128 ];
                sprintf( message, "Saved current picture settings on channel %d.\n",
                         station_get_current_id( cmd->stationmgr ) );
                tvtime_osd_show_message( cmd->osd, message );
            }
        }
        break;

    case TVTIME_ENTER:
        if( cmd->next_chan_buffer[ 0 ] ) {
            if( cmd->renumbering ) {
                station_remap( cmd->stationmgr, atoi( cmd->next_chan_buffer ) );
                station_writeconfig( cmd->stationmgr );
                cmd->renumbering = 0;
                if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
            }
            if( station_set( cmd->stationmgr, atoi( cmd->next_chan_buffer ) ) ) {
                cmd->change_channel = 1;
            } else {
                snprintf( cmd->next_chan_buffer, sizeof( cmd->next_chan_buffer ),
                          "%d", station_get_current_id( cmd->stationmgr ) );
                if( cmd->osd ) {
                    tvtime_osd_set_channel_number( cmd->osd, cmd->next_chan_buffer );
                    tvtime_osd_show_info( cmd->osd );
                }
            }
        } else {
            if( cmd->renumbering ) {
                cmd->renumbering = 0;
                if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
            }
            snprintf( cmd->next_chan_buffer, sizeof( cmd->next_chan_buffer ),
                      "%d", station_get_current_id( cmd->stationmgr ) );
            if( cmd->osd ) {
                tvtime_osd_set_channel_number( cmd->osd, cmd->next_chan_buffer );
                tvtime_osd_show_info( cmd->osd );
            }
        }
        cmd->frame_counter = 0;
        break;

    case TVTIME_CHANNEL_1:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "1" );
        break;

    case TVTIME_CHANNEL_2:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "2" );
        break;

    case TVTIME_CHANNEL_3:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "3" );
        break;

    case TVTIME_CHANNEL_4:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "4" );
        break;

    case TVTIME_CHANNEL_5:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "5" );
        break;

    case TVTIME_CHANNEL_6:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "6" );
        break;

    case TVTIME_CHANNEL_7:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "7" );
        break;

    case TVTIME_CHANNEL_8:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "8" );
        break;

    case TVTIME_CHANNEL_9:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "9" );
        break;

    case TVTIME_CHANNEL_0:
        commands_handle( cmd, TVTIME_CHANNEL_CHAR, "0" );
        break;

    case TVTIME_TOGGLE_PAUSE:
        cmd->pause = !(cmd->pause);
        if( cmd->osd ) tvtime_osd_show_message( cmd->osd, cmd->pause ? "Paused" : "Resumed" );
        break;

    case TVTIME_TOGGLE_MATTE:
        cmd->togglematte = 1;
        break;

    case TVTIME_TOGGLE_MODE:
        cmd->togglemode = 1;
        break;
    }
}

void commands_next_frame( commands_t *cmd )
{
    /* Decrement the frame counter if user is typing digits */
    if( cmd->frame_counter > 0 ) {
        cmd->frame_counter--;
        if( cmd->frame_counter == 0 ) {
            /* Switch to the next channel if the countdown expires. */
            commands_handle( cmd, TVTIME_ENTER, 0 );
        }
    }

    if( cmd->frame_counter == 0 ) {
        memset( cmd->next_chan_buffer, 0, 5 );
        cmd->digit_counter = 0;
        if( cmd->renumbering ) {
            if( cmd->osd ) tvtime_osd_set_hold_message( cmd->osd, "" );
            cmd->renumbering = 0;
        }
    }

    if( cmd->frame_counter > 0 && !(cmd->frame_counter % 5)) {
        char input_text[6];

        strcpy( input_text, cmd->next_chan_buffer );
        if( !(cmd->frame_counter % 10) ) {
            strcat( input_text, "_" );
        } else {
            strcat( input_text, " " );
        }
        if( cmd->osd ) {
            tvtime_osd_set_channel_number( cmd->osd, input_text );
            tvtime_osd_show_info( cmd->osd );
        }
    }

    if( cmd->change_channel ) {
        reinit_tuner( cmd );
        cmd->change_channel = 0;
    }

    update_xmltv_listings( cmd );
    if( cmd->vbi ) {
        if( *(vbidata_get_network_name( cmd->vbi )) ) {
            /* If the network name has changed, save it to the config file. */
            if( strcmp( station_get_current_network_name( cmd->stationmgr ),
                        vbidata_get_network_name( cmd->vbi ) ) ) {
                station_set_current_network_name( cmd->stationmgr,
                                                  vbidata_get_network_name( cmd->vbi ) );
                station_writeconfig( cmd->stationmgr );
            }

            if( cmd->osd ) {
                tvtime_osd_set_network_name( cmd->osd, station_get_current_network_name( cmd->stationmgr ) );
            }
        }

        if( *(vbidata_get_network_call_letters( cmd->vbi )) ) {
            /* If the call letters have changed, save them to the config file. */
            if( strcmp( station_get_current_network_call_letters( cmd->stationmgr ),
                        vbidata_get_network_call_letters( cmd->vbi ) ) ) {
                station_set_current_network_call_letters( cmd->stationmgr,
                                                          vbidata_get_network_call_letters( cmd->vbi ) );
                station_writeconfig( cmd->stationmgr );
            }

            if( cmd->osd ) {
                tvtime_osd_set_network_call( cmd->osd, station_get_current_network_call_letters( cmd->stationmgr ) );
            }
        }

        if( cmd->osd ) {
            tvtime_osd_set_show_name( cmd->osd, vbidata_get_program_name( cmd->vbi ) );
            tvtime_osd_set_show_rating( cmd->osd, vbidata_get_program_rating( cmd->vbi ) );
            tvtime_osd_set_show_start( cmd->osd, vbidata_get_program_start_time( cmd->vbi ) );
            tvtime_osd_set_show_length( cmd->osd, vbidata_get_program_length( cmd->vbi ) );
        }
    }

    cmd->printdebug = 0;
    cmd->showdeinterlacerinfo = 0;
    cmd->screenshot = 0;
    cmd->togglefullscreen = 0;
    cmd->toggleaspect = 0;
    cmd->togglealwaysontop = 0;
    cmd->toggledeinterlacer = 0;
    cmd->togglepulldowndetection = 0;
    cmd->togglemode = 0;
    cmd->togglematte = 0;
    cmd->update_luma = 0;
    cmd->resizewindow = 0;
    cmd->setdeinterlacer = 0;
    cmd->setfreqtable = 0;
}

int commands_quit( commands_t *cmd )
{
    return cmd->quit;
}

int commands_print_debug( commands_t *cmd )
{
    return cmd->printdebug;
}

int commands_show_bars( commands_t *cmd )
{
    return cmd->showbars;
}

int commands_take_screenshot( commands_t *cmd )
{
    return cmd->screenshot;
}

int commands_toggle_fullscreen( commands_t *cmd )
{
    return cmd->togglefullscreen;
}

int commands_get_framerate( commands_t *cmd )
{
    return cmd->framerate;
}

int commands_toggle_aspect( commands_t *cmd )
{
    return cmd->toggleaspect;
}

int commands_toggle_alwaysontop( commands_t *cmd )
{
    return cmd->togglealwaysontop;
}

int commands_toggle_deinterlacer( commands_t *cmd )
{
    return cmd->toggledeinterlacer;
}

int commands_toggle_pulldown_detection( commands_t *cmd )
{
    return cmd->togglepulldowndetection;
}

int commands_toggle_mode( commands_t *cmd )
{
    return cmd->togglemode;
}

int commands_toggle_matte( commands_t *cmd )
{
    return cmd->togglematte;
}

void commands_set_console( commands_t *cmd, console_t *con )
{
    cmd->console = con;
}

void commands_set_vbidata( commands_t *cmd, vbidata_t *vbi )
{
    cmd->vbi = vbi;
}

int commands_console_on( commands_t *cmd )
{
    return cmd->console_on;
}

int commands_scan_channels( commands_t *cmd )
{
    return cmd->scan_channels;
}

int commands_pause( commands_t *cmd )
{
    return cmd->pause;
}

int commands_apply_colour_invert( commands_t *cmd )
{
    return cmd->apply_invert;
}

int commands_apply_mirror( commands_t *cmd )
{
    return cmd->apply_mirror;
}

int commands_apply_chroma_kill( commands_t *cmd )
{
    return cmd->apply_chroma_kill;
}

int commands_apply_luma_correction( commands_t *cmd )
{
    return cmd->apply_luma;
}

int commands_update_luma_power( commands_t *cmd )
{
    return cmd->update_luma;
}

double commands_get_luma_power( commands_t *cmd )
{
    return cmd->luma_power;
}

double commands_get_overscan( commands_t *cmd )
{
    return cmd->overscan;
}

int commands_resize_window( commands_t *cmd )
{
    return cmd->resizewindow;
}

void commands_set_framerate( commands_t *cmd, int framerate )
{
    cmd->framerate = framerate % FRAMERATE_MAX;
}

int commands_show_deinterlacer_info( commands_t *cmd )
{
    return cmd->showdeinterlacerinfo;
}

int commands_restart_tvtime( commands_t *cmd )
{
    return cmd->restarttvtime;
}

const char *commands_get_new_norm( commands_t *cmd )
{
    if( cmd->normset ) {
        return cmd->newnorm;
    } else {
        return 0;
    }
}

int commands_set_deinterlacer( commands_t *cmd )
{
    return cmd->setdeinterlacer;
}

const char *commands_get_new_deinterlacer( commands_t *cmd )
{
    return cmd->deinterlacer;
}

int commands_menu_active( commands_t *cmd )
{
    return cmd->menuactive;
}

void commands_set_half_size( commands_t *cmd, int halfsize )
{
    cmd->halfsize = halfsize;
}

int commands_get_new_sharpness( commands_t *cmd )
{
    return cmd->newsharpness;
}

int commands_get_global_brightness( commands_t *cmd )
{
    return cmd->brightness;
}

int commands_get_global_contrast( commands_t *cmd )
{
    return cmd->contrast;
}

int commands_get_global_colour( commands_t *cmd )
{
    return cmd->colour;
}

int commands_get_global_hue( commands_t *cmd )
{
    return cmd->hue;
}

int commands_set_freq_table( commands_t *cmd )
{
    return cmd->setfreqtable;
}

const char *commands_get_new_freq_table( commands_t *cmd )
{
    return cmd->newfreqtable;
}

int commands_check_freq_present( commands_t *cmd )
{
    return cmd->checkfreq;
}

int commands_sleeptimer( commands_t *cmd )
{
    return cmd->sleeptimer;
}

int commands_sleeptimer_do_shutdown( commands_t *cmd )
{
    time_t now;

    time( &now );

    return (now >= ((sleeptimer_function( cmd->sleeptimer ) * 60) + cmd->sleeptimer_start));
}

