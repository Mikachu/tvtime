/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>.
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include <ctype.h>
#include "station.h"
#include "mixer.h"
#include "commands.h"
#include "console.h"

/* Number of frames to wait for next channel digit. */
#define CHANNEL_DELAY 100

/* Number of frames to wait until trying stereo mode. */
#define CHANNEL_STEREO_DELAY 30

typedef struct {
    char name[MAX_CMD_NAMELEN];
    int command;
} Cmd_Names;

static Cmd_Names cmd_table[] = {

    { "AUTO_ADJUST_PICT", TVTIME_AUTO_ADJUST_PICT },

    { "BRIGHTNESS_DOWN", TVTIME_BRIGHTNESS_DOWN },
    { "BRIGHTNESS_UP", TVTIME_BRIGHTNESS_UP },

    { "CHANNEL_1", TVTIME_CHANNEL_1 },
    { "CHANNEL_2", TVTIME_CHANNEL_2 },
    { "CHANNEL_3", TVTIME_CHANNEL_3 },
    { "CHANNEL_4", TVTIME_CHANNEL_4 },
    { "CHANNEL_5", TVTIME_CHANNEL_5 },
    { "CHANNEL_6", TVTIME_CHANNEL_6 },
    { "CHANNEL_7", TVTIME_CHANNEL_7 },
    { "CHANNEL_8", TVTIME_CHANNEL_8 },
    { "CHANNEL_9", TVTIME_CHANNEL_9 },
    { "CHANNEL_0", TVTIME_CHANNEL_0 },
    { "CHANNEL_DOWN", TVTIME_CHANNEL_DOWN },
    { "CHANNEL_PREV", TVTIME_CHANNEL_PREV },
    { "CHANNEL_RENUMBER", TVTIME_CHANNEL_RENUMBER },
    { "CHANNEL_SCAN", TVTIME_CHANNEL_SCAN },
    { "CHANNEL_SKIP", TVTIME_CHANNEL_SKIP },
    { "CHANNEL_UP", TVTIME_CHANNEL_UP },

    { "COLOUR_DOWN", TVTIME_COLOUR_DOWN },
    { "COLOUR_UP", TVTIME_COLOUR_UP },

    { "COLOR_DOWN", TVTIME_COLOUR_DOWN },
    { "COLOR_UP", TVTIME_COLOUR_UP },

    { "CONTRAST_DOWN", TVTIME_CONTRAST_DOWN },
    { "CONTRAST_UP", TVTIME_CONTRAST_UP },

    { "DISPLAY_INFO", TVTIME_DISPLAY_INFO },

    { "ENTER", TVTIME_ENTER },

    { "FINETUNE_DOWN", TVTIME_FINETUNE_DOWN },
    { "FINETUNE_UP", TVTIME_FINETUNE_UP },

    { "HUE_DOWN", TVTIME_HUE_DOWN },
    { "HUE_UP", TVTIME_HUE_UP },

    { "LUMA_UP", TVTIME_LUMA_UP },
    { "LUMA_DOWN", TVTIME_LUMA_DOWN },

    { "MIXER_MUTE", TVTIME_MIXER_MUTE },
    { "MIXER_UP", TVTIME_MIXER_UP },
    { "MIXER_DOWN", TVTIME_MIXER_DOWN },

    { "OVERSCAN_DOWN", TVTIME_OVERSCAN_DOWN },
    { "OVERSCAN_UP", TVTIME_OVERSCAN_UP },

    { "SCREENSHOT", TVTIME_SCREENSHOT },
    { "SCROLL_CONSOLE_DOWN", TVTIME_SCROLL_CONSOLE_DOWN },
    { "SCROLL_CONSOLE_UP", TVTIME_SCROLL_CONSOLE_UP },

    { "SHOW_STATS", TVTIME_SHOW_STATS },

    { "TOGGLE_ASPECT", TVTIME_TOGGLE_ASPECT },
    { "TOGGLE_AUDIO_MODE", TVTIME_TOGGLE_AUDIO_MODE },
    { "TOGGLE_BARS", TVTIME_TOGGLE_BARS },
    { "TOGGLE_CC", TVTIME_TOGGLE_CC },
    { "TOGGLE_CONSOLE", TVTIME_TOGGLE_CONSOLE },
    { "TOGGLE_CREDITS", TVTIME_TOGGLE_CREDITS },
    { "TOGGLE_DEINTERLACER", TVTIME_TOGGLE_DEINTERLACER },
    { "TOGGLE_FULLSCREEN", TVTIME_TOGGLE_FULLSCREEN },
    { "TOGGLE_FRAMERATE", TVTIME_TOGGLE_FRAMERATE },
    { "TOGGLE_INPUT", TVTIME_TOGGLE_INPUT },
    { "TOGGLE_LUMA_CORRECTION", TVTIME_TOGGLE_LUMA_CORRECTION },
    { "TOGGLE_MODE", TVTIME_TOGGLE_MODE },
    { "TOGGLE_NTSC_CABLE_MODE", TVTIME_TOGGLE_NTSC_CABLE_MODE },
    { "TOGGLE_PAUSE", TVTIME_TOGGLE_PAUSE },
    { "TOGGLE_PULLDOWN_DETECTION", TVTIME_TOGGLE_PULLDOWN_DETECTION },

    { "QUIT", TVTIME_QUIT },
};

int tvtime_num_commands( void )
{
    return ( sizeof( cmd_table ) / sizeof( Cmd_Names ) );
}

const char *tvtime_get_command( int pos )
{
    return cmd_table[ pos ].name;
}

int tvtime_get_command_id( int pos )
{
    return cmd_table[ pos ].command;
}

int tvtime_string_to_command( const char *str )
{
    if( str ) {
        int i;

        for( i = 0; i < tvtime_num_commands(); i++ ) {
            if( !strcasecmp( str, tvtime_get_command( i ) ) ) {
                return tvtime_get_command_id( i );
            }
        }
    }

    return TVTIME_NOCOMMAND;
}

const char *tvtime_command_to_string( int command )
{
    int i;

    for( i = 0; i < tvtime_num_commands(); i++ ) {
        if( tvtime_get_command_id( i ) == command ) {
            return tvtime_get_command( i );
        }
    }

    return "ERROR";
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

    int screenshot;
    int printdebug;
    int showbars;
    int togglefullscreen;
    int toggleaspect;
    int toggledeinterlacingmode;
    int togglepulldowndetection;
    int togglemode;
    int framerate;
    int scan_channels;
    int pause;

    int change_channel;

    int renumbering;

    int apply_luma;
    int update_luma;
    double luma_power;

    double overscan;

    int audio_counter;
    
    int menu_on;
    menu_t *menu;

    int console_on;
    int scrollconsole;
    console_t *console;

    vbidata_t *vbi;
    int capturemode;

    station_mgr_t *stationmgr;
};

static void reinit_tuner( commands_t *in )
{
    /* Setup the tuner if available. */
    if( in->vidin && videoinput_has_tuner( in->vidin ) ) {
        /**
         * Set to the current channel, or the first channel in our
         * frequency list.
         */
        //if( !frequencies_find_current_index( in->vidin ) ) {
            /* set to a known frequency */
            //frequencies_choose_first_frequency();
            
        videoinput_set_tuner_freq( in->vidin, station_get_current_frequency( in->stationmgr ) );
        if( in->vbi ) {
            vbidata_reset( in->vbi );
            vbidata_capture_mode( in->vbi, in->capturemode );
        }
        //}

        if( config_get_verbose( in->cfg ) ) {
            fprintf( stderr, "tvtime: Changing to channel %s.\n", station_get_current_channel_name( in->stationmgr ) );
        }

        if( in->osd ) {
            char channel_display[ 20 ];
            sprintf( channel_display, "%d", station_get_current_id( in->stationmgr ) );
            tvtime_osd_set_audio_mode( in->osd, videoinput_audio_mode_name( videoinput_get_audio_mode( in->vidin ) ) );
            tvtime_osd_set_freq_table( in->osd, station_get_current_band( in->stationmgr ) );
            tvtime_osd_set_channel_number( in->osd, channel_display );
            tvtime_osd_set_channel_name( in->osd, station_get_current_channel_name( in->stationmgr ) );
        }
    } else if( in->osd ) {
        tvtime_osd_set_audio_mode( in->osd, "" );
        tvtime_osd_set_freq_table( in->osd, "" );
        tvtime_osd_set_channel_number( in->osd, "" );
        tvtime_osd_set_channel_name( in->osd, "" );
        tvtime_osd_set_network_call( in->osd, "" );
        tvtime_osd_set_network_name( in->osd, "" );
        tvtime_osd_set_show_name( in->osd, "" );
        tvtime_osd_set_show_rating( in->osd, "" );
        tvtime_osd_set_show_start( in->osd, "" );
        tvtime_osd_set_show_length( in->osd, "" );
    }
}

commands_t *commands_new( config_t *cfg, videoinput_t *vidin,
                          station_mgr_t *mgr, tvtime_osd_t *osd )
{
    commands_t *in = (commands_t *) malloc( sizeof( struct commands_s ) );

    if( !in ) {
        return NULL;
    }

    in->cfg = cfg;
    in->vidin = vidin;
    in->osd = osd;
    in->stationmgr = mgr;
    in->frame_counter = 0;
    in->digit_counter = 0;
    in->menu = 0;

    in->quit = 0;
    in->showbars = 0;
    in->printdebug = 0;
    in->screenshot = 0;
    in->togglefullscreen = 0;
    in->toggleaspect = 0;
    in->toggledeinterlacingmode = 0;
    in->togglepulldowndetection = 0;
    in->togglemode = 0;
    in->framerate = FRAMERATE_FULL;
    in->menu_on = 0;
    in->console_on = 0;
    in->scrollconsole = 0;
    in->scan_channels = 0;
    in->pause = 0;
    in->audio_counter = -1;
    in->change_channel = 0;
    in->renumbering = 0;

    in->apply_luma = config_get_apply_luma_correction( cfg );
    in->update_luma = 0;
    in->luma_power = config_get_luma_correction( cfg );

    if( vidin && !videoinput_is_bttv( vidin ) && in->apply_luma ) {
        in->apply_luma = 0;
        fprintf( stderr, "commands: Input isn't from a bt8x8, disabling luma correction.\n" );
    }

    if( in->luma_power < 0.0 || in->luma_power > 10.0 ) {
        fprintf( stderr, "commands: Luma correction value out of range. Using 1.0.\n" );
        in->luma_power = 1.0;
    } else if( config_get_verbose( cfg ) ) {
        fprintf( stderr, "commands: Luma correction value: %.1f\n", in->luma_power );
    }

    in->overscan = config_get_overscan( cfg );
    if( in->overscan > 0.4 ) in->overscan = 0.4; if( in->overscan < 0.0 ) in->overscan = 0.0;

    in->console = 0;
    in->vbi = 0;
    in->capturemode = CAPTURE_OFF;


    /**
     * Set the current channel list.
     */

    reinit_tuner( in );

    return in;
}

void commands_delete( commands_t *in )
{
    free( in );
}

void commands_set_vbidata( commands_t *in, vbidata_t *vbi )
{
    in->vbi = vbi;
}

static void commands_station_change( commands_t *in )
{
    int verbose = config_get_verbose( in->cfg );

    if( !in->vidin || !videoinput_has_tuner( in->vidin ) ) {
        if( verbose ) {
            fprintf( stderr, "tvtime: Can't change channel, "
                     "no tuner available on this input!\n" );
        }
    } else {

        videoinput_set_tuner_freq( in->vidin, station_get_current_frequency( in->stationmgr ) );
        if( in->vbi ) {
            vbidata_reset( in->vbi );
            vbidata_capture_mode( in->vbi, in->capturemode );
        }

        if( verbose ) {
            fprintf( stderr, "tvtime: Changing to channel %s\n", station_get_current_channel_name( in->stationmgr ) );
        }
        videoinput_set_audio_mode( in->vidin, 1 );
        in->audio_counter = CHANNEL_STEREO_DELAY;
        if( in->osd ) {
            char channel_display[ 20 ];
            sprintf( channel_display, "%d", station_get_current_id( in->stationmgr ) );
            tvtime_osd_set_audio_mode( in->osd, videoinput_audio_mode_name( videoinput_get_audio_mode( in->vidin ) ) );
            tvtime_osd_set_channel_number( in->osd, channel_display );
            tvtime_osd_set_channel_name( in->osd, station_get_current_channel_name( in->stationmgr ) );
            tvtime_osd_set_freq_table( in->osd, station_get_current_band( in->stationmgr ) );
            tvtime_osd_show_info( in->osd );
        }
    }
}

void commands_handle( commands_t *in, int tvtime_cmd, int arg )
{
    int verbose, volume;

    verbose = config_get_verbose( in->cfg );

    switch( tvtime_cmd ) {
    case TVTIME_QUIT:
        in->quit = 1;
        break;

    case TVTIME_MENUMODE:
        in->menu_on = in->menu_on ^ 1;
        if( in->menu_on )
            menu_init( in->menu );
        break;

    case TVTIME_SHOW_STATS:
        in->printdebug = 1;
        break;

    case TVTIME_SCREENSHOT:
        in->screenshot = 1;
        break;

    case TVTIME_TOGGLE_BARS:
        in->showbars = !in->showbars;
        break;

    case TVTIME_TOGGLE_FULLSCREEN:
        in->togglefullscreen = 1;
        break;

    case TVTIME_SCROLL_CONSOLE_UP:
    case TVTIME_SCROLL_CONSOLE_DOWN:
        if( in->console_on )
            console_scroll_n( in->console, (tvtime_cmd == TVTIME_SCROLL_CONSOLE_UP) ? -1 : 1 );
        break;
            
    case TVTIME_TOGGLE_FRAMERATE:
        in->framerate = (in->framerate + 1) % FRAMERATE_MAX;
        break;

    case TVTIME_TOGGLE_CONSOLE:
        in->console_on = !in->console_on;
        console_toggle_console( in->console );
        break;

    case TVTIME_CHANNEL_SKIP:
        station_set_current_active( in->stationmgr, !station_get_current_active( in->stationmgr ) );
        if( in->osd ) {
            if( station_get_current_active( in->stationmgr ) ) {
                tvtime_osd_show_message( in->osd, "Channel active in list." );
            } else {
                tvtime_osd_show_message( in->osd, "Channel disabled from list." );
            }
        }
        station_writeconfig( in->stationmgr );
    break;
            
    case TVTIME_TOGGLE_ASPECT:
        in->toggleaspect = 1;
        break;

    case TVTIME_CHANNEL_RENUMBER:
        /* If we're scanning and suddenly want to renumber, stop scanning. */
        if( in->scan_channels ) {
            commands_handle( in, TVTIME_CHANNEL_SCAN, 0 );
        }

        /* Accept input of the destination channel. */
        if( in->digit_counter == 0 ) memset( in->next_chan_buffer, 0, 5 );
        in->frame_counter = CHANNEL_DELAY;
        in->renumbering = 1;
        if( in->osd ) {
            char message[ 256 ];
            sprintf( message, "Remapping %d.  Enter new channel number.",
                     station_get_current_id( in->stationmgr ) );
            tvtime_osd_set_hold_message( in->osd, message );
        }
        break;

    case TVTIME_CHANNEL_SCAN:
        in->scan_channels = !in->scan_channels;

        if( in->scan_channels && in->renumbering ) {
            memset( in->next_chan_buffer, 0, 5 );
            in->digit_counter = 0;
            in->frame_counter = 0;
            if( in->osd ) tvtime_osd_set_hold_message( in->osd, "" );
            in->renumbering = 0;
        }

        if( in->osd ) {
            if( in->scan_channels ) {
                tvtime_osd_set_hold_message( in->osd, "Scanning (hit F10 to stop)." );
                tvtime_osd_show_info( in->osd );
            } else {
                tvtime_osd_set_hold_message( in->osd, "" );
                tvtime_osd_show_info( in->osd );
            }
        }
        break;

    case TVTIME_TOGGLE_CC:
        if( in->vbi ) {
            vbidata_capture_mode( in->vbi, in->capturemode ? CAPTURE_OFF : CAPTURE_CC1 );
            if( in->capturemode ) {
                if( in->osd ) tvtime_osd_show_message( in->osd, "Closed Captioning Disabled." );
                in->capturemode = CAPTURE_OFF;
            } else {
                if( in->osd ) tvtime_osd_show_message( in->osd, "Closed Captioning Enabled." );
                in->capturemode = CAPTURE_CC1;
            }
        } else {
            if( in->osd ) tvtime_osd_show_message( in->osd, "No VBI device available for CC decoding." );
        }
        break;

    case TVTIME_DISPLAY_INFO:
        if( in->osd ) {
            tvtime_osd_show_info( in->osd );
        }
        break;

    case TVTIME_TOGGLE_CREDITS:
        if( in->osd ) {
            tvtime_osd_toggle_show_credits( in->osd );
        }
        break;

    case TVTIME_TOGGLE_AUDIO_MODE:
        if( in->vidin ) {
            in->audio_counter = -1;
            videoinput_set_audio_mode( in->vidin, videoinput_get_audio_mode( in->vidin ) << 1 );
            if( in->osd ) {
                tvtime_osd_set_audio_mode( in->osd, videoinput_audio_mode_name( videoinput_get_audio_mode( in->vidin ) ) );
                tvtime_osd_show_info( in->osd );
            }
        }
        break;

    case TVTIME_TOGGLE_DEINTERLACER:
        in->toggledeinterlacingmode = 1;
        break;

    case TVTIME_TOGGLE_PULLDOWN_DETECTION:
        in->togglepulldowndetection = 1;
        break;

    case TVTIME_CHANNEL_CHAR:
        if( in->vidin && videoinput_has_tuner( in->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( in->scan_channels ) {
                commands_handle( in, TVTIME_CHANNEL_SCAN, 0 );
            }

            /* Decode the input char from commands.  */
            if( in->digit_counter == 0 ) memset( in->next_chan_buffer, 0, 5 );
            in->next_chan_buffer[ in->digit_counter ] = arg & 0xFF;
            in->digit_counter++;
            in->digit_counter %= 4;
            in->frame_counter = CHANNEL_DELAY;
        }
        break;

    case TVTIME_TOGGLE_LUMA_CORRECTION:
        in->apply_luma = !in->apply_luma;
        if( in->osd ) {
            if( in->apply_luma ) {
                tvtime_osd_show_message( in->osd, "Luma correction enabled." );
                if( config_get_configsave( in->cfg ) ) {
                    configsave( config_get_configsave( in->cfg ), "ApplyLumaCorrection", "1" );
                }
            } else {
                tvtime_osd_show_message( in->osd, "Luma correction disabled." );
                if( config_get_configsave( in->cfg ) ) {
                    configsave( config_get_configsave( in->cfg ), "ApplyLumaCorrection", "0" );
                }
            }
        }
        break;

    case TVTIME_OVERSCAN_UP:
    case TVTIME_OVERSCAN_DOWN:
        in->overscan = in->overscan + ( (tvtime_cmd == TVTIME_OVERSCAN_UP) ? 0.0025 : -0.0025 );
        if( in->overscan > 0.4 ) in->overscan = 0.4; if( in->overscan < 0.0 ) in->overscan = 0.0;

        if( in->osd ) {
            char message[ 200 ];
            sprintf( message, "Overscan: %.1f%%", in->overscan * 2.0 * 100.0 );
            tvtime_osd_show_message( in->osd, message );
        }
        break;

    case TVTIME_LUMA_UP:
    case TVTIME_LUMA_DOWN:
        if( !in->apply_luma ) {
            fprintf( stderr, "tvtime: Luma correction disabled.  "
                     "Run with -c to use it.\n" );
        } else {
            char message[ 200 ];
            in->luma_power = in->luma_power + ( (tvtime_cmd == TVTIME_LUMA_UP) ? 0.1 : -0.1 );

            in->update_luma = 1;

            if( in->luma_power > 10.0 ) in->luma_power = 10.0;
            if( in->luma_power <  0.0 ) in->luma_power = 0.0;

            if( verbose ) {
                fprintf( stderr, "tvtime: Luma correction value: %.1f\n", in->luma_power );
            }

            sprintf( message, "%.1f", in->luma_power );
            if( config_get_configsave( in->cfg ) ) {
                configsave( config_get_configsave( in->cfg ), "LumaCorrection", message );
            }
            if( in->osd ) {
                sprintf( message, "Luma correction value: %.1f", in->luma_power );
                tvtime_osd_show_message( in->osd, message );
            }
        }
        break;

    case TVTIME_AUTO_ADJUST_PICT:
        if( in->vidin ) {
            videoinput_reset_default_settings( in->vidin );
            if( in->osd ) {
                tvtime_osd_show_message( in->osd, "Picture settings reset to defaults." );
            }
        }
        break;

    case TVTIME_TOGGLE_NTSC_CABLE_MODE:
        if( in->vidin && videoinput_has_tuner( in->vidin ) ) {
            station_toggle_us_cable_mode( in->stationmgr );
            in->change_channel = 1;
        }
        break;

    case TVTIME_FINETUNE_DOWN:
    case TVTIME_FINETUNE_UP:
        if( in->vidin ) {
            if( !videoinput_has_tuner( in->vidin ) && verbose ) {
                fprintf( stderr, "tvtime: Can't fine tune channel, "
                         "no tuner available on this input!\n" );
            } else if( videoinput_has_tuner( in->vidin ) ) {
                videoinput_set_tuner_freq( in->vidin, videoinput_get_tuner_freq( in->vidin ) +
                                           ( tvtime_cmd == TVTIME_FINETUNE_UP ? ((1000/16)+1) : -(1000/16) ) );

                if( in->vbi ) {
                    vbidata_reset( in->vbi );
                    vbidata_capture_mode( in->vbi, in->capturemode );
                }

                if( in->osd ) {
                    char message[ 200 ];
                    sprintf( message, "Tuning: %4.2fMhz.", ((double) videoinput_get_tuner_freq( in->vidin )) / 1000.0 );
                    tvtime_osd_show_message( in->osd, message );
                }
            }
        }
        break;

    case TVTIME_CHANNEL_UP: 
        if( in->vidin && videoinput_has_tuner( in->vidin ) ) {

            /**
             * If we're scanning and the user hits a key, stop scanning.
             * arg will be 0 if the scanner has called us.
             */
            if( in->scan_channels && arg ) {
                commands_handle( in, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_next( in->stationmgr );
            in->change_channel = 1;
        }
        break;
    case TVTIME_CHANNEL_DOWN:
        if( in->vidin && videoinput_has_tuner( in->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( in->scan_channels ) {
                commands_handle( in, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_prev( in->stationmgr );
            in->change_channel = 1;
        }
        break;

    case TVTIME_CHANNEL_PREV:
        if( in->vidin && videoinput_has_tuner( in->vidin ) ) {

            /* If we're scanning and the user hits a key, stop scanning. */
            if( in->scan_channels ) {
                commands_handle( in, TVTIME_CHANNEL_SCAN, 0 );
            }

            station_last( in->stationmgr );
            in->change_channel = 1;
        }
        break;

    case TVTIME_MIXER_UP: 
    case TVTIME_MIXER_DOWN:

        volume = mixer_set_volume( 
            ( (tvtime_cmd == TVTIME_MIXER_UP) ? 1 : -1 ) );

        if( verbose ) {
            fprintf( stderr, "input: volume %d\n", (volume & 0xFF) );
        }
        if( in->osd ) {
            tvtime_osd_show_volume_bar( in->osd, volume & 0xff );
        }
        break;

    case TVTIME_MIXER_MUTE:
        if( in->vidin ) {
            videoinput_mute( in->vidin, !videoinput_get_muted( in->vidin ) );
            if( in->osd ) {
                tvtime_osd_volume_muted( in->osd, videoinput_get_muted( in->vidin ) );
            }
        }
        break;

    case TVTIME_TOGGLE_INPUT:
        if( in->vidin ) {
            in->frame_counter = 0;
            videoinput_set_input_num( in->vidin, ( videoinput_get_input_num( in->vidin ) + 1 ) % videoinput_get_num_inputs( in->vidin ) );
            reinit_tuner( in );

            if( in->osd ) {
                tvtime_osd_set_input( in->osd, videoinput_get_input_name( in->vidin ) );
                tvtime_osd_show_info( in->osd );
            }
        }
        break;

    case TVTIME_HUE_UP:
    case TVTIME_HUE_DOWN:
        if( in->vidin ) {
            videoinput_set_hue_relative( in->vidin, (tvtime_cmd == TVTIME_HUE_UP) ? 1 : -1 );
            if( in->osd ) {
                tvtime_osd_show_data_bar( in->osd, "Hue    ", videoinput_get_hue( in->vidin ) );
            }
        }
        break;

    case TVTIME_BRIGHTNESS_UP: 
    case TVTIME_BRIGHTNESS_DOWN:
        if( in->vidin ) {
            videoinput_set_brightness_relative( in->vidin, (tvtime_cmd == TVTIME_BRIGHTNESS_UP) ? 1 : -1 );
            if( in->osd ) {
                tvtime_osd_show_data_bar( in->osd, "Bright ", videoinput_get_brightness( in->vidin ) );
            }
        }
        break;

    case TVTIME_CONTRAST_UP:
    case TVTIME_CONTRAST_DOWN:
        if( in->vidin ) {
            videoinput_set_contrast_relative( in->vidin, (tvtime_cmd == TVTIME_CONTRAST_UP) ? 1 : -1 );
            if( in->osd ) {
                tvtime_osd_show_data_bar( in->osd, "Cont   ", videoinput_get_contrast( in->vidin ) );
            }
        }
        break;

    case TVTIME_COLOUR_UP:
    case TVTIME_COLOUR_DOWN:
        if( in->vidin ) {
            videoinput_set_colour_relative( in->vidin, (tvtime_cmd == TVTIME_COLOUR_UP) ? 1 : -1 );
            if( in->osd ) {
                tvtime_osd_show_data_bar( in->osd, "Colour ", videoinput_get_colour( in->vidin ) );
            }
        }
        break;

    case TVTIME_ENTER:
        if( in->next_chan_buffer[ 0 ] ) {
            if( in->renumbering ) {
                station_remap( in->stationmgr, atoi( in->next_chan_buffer ) );
                station_writeconfig( in->stationmgr );
                in->renumbering = 0;
                if( in->osd ) tvtime_osd_set_hold_message( in->osd, "" );
            }
            if( station_set( in->stationmgr, atoi( in->next_chan_buffer ) ) ) {
                in->change_channel = 1;
            } else {
                sprintf( in->next_chan_buffer, "%d", station_get_current_id( in->stationmgr ) );
                if( in->osd ) {
                    tvtime_osd_set_channel_number( in->osd, in->next_chan_buffer );
                    tvtime_osd_show_info( in->osd );
                }
            }
            in->frame_counter = 0;
        } else {
            sprintf( in->next_chan_buffer, "%d", station_get_current_id( in->stationmgr ) );
            if( in->osd ) {
                tvtime_osd_set_channel_number( in->osd, in->next_chan_buffer );
                tvtime_osd_show_info( in->osd );
            }
            in->frame_counter = 0;
        }
        break;

    case TVTIME_CHANNEL_1:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 49);
        break;

    case TVTIME_CHANNEL_2:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 50);
        break;

    case TVTIME_CHANNEL_3:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 51);
        break;

    case TVTIME_CHANNEL_4:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 52);
        break;

    case TVTIME_CHANNEL_5:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 53);
        break;

    case TVTIME_CHANNEL_6:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 54);
        break;

    case TVTIME_CHANNEL_7:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 55);
        break;

    case TVTIME_CHANNEL_8:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 56);
        break;

    case TVTIME_CHANNEL_9:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 57);
        break;

    case TVTIME_CHANNEL_0:
        commands_handle(in, TVTIME_CHANNEL_CHAR, 48);
        break;

    case TVTIME_TOGGLE_PAUSE:
        in->pause = !(in->pause);
        if( in->osd ) tvtime_osd_show_message( in->osd, in->pause ? "Paused" : "Resumed" );
        break;

    case TVTIME_TOGGLE_MODE:
        in->togglemode = 1;
        break;
    }
}

int commands_quit( commands_t *in )
{
    return in->quit;
}

int commands_print_debug( commands_t *in )
{
    return in->printdebug;
}

int commands_show_bars( commands_t *in )
{
    return in->showbars;
}

int commands_take_screenshot( commands_t *in )
{
    return in->screenshot;
}

int commands_toggle_fullscreen( commands_t *in )
{
    return in->togglefullscreen;
}

int commands_get_framerate( commands_t *in )
{
    return in->framerate;
}

int commands_toggle_aspect( commands_t *in )
{
    return in->toggleaspect;
}

int commands_toggle_deinterlacing_mode( commands_t *in )
{
    return in->toggledeinterlacingmode;
}

int commands_toggle_pulldown_detection( commands_t *in )
{
    return in->togglepulldowndetection;
}

int commands_toggle_mode( commands_t *in )
{
    return in->togglemode;
}

int commands_toggle_menu( commands_t *in )
{
    in->menu_on = !in->menu_on;
    return in->menu_on;
}

void commands_next_frame( commands_t *in )
{
    /* Decrement the frame counter if user is typing digits */
    if( in->frame_counter > 0 ) in->frame_counter--;

    if( in->frame_counter == 0 ) {
        memset( in->next_chan_buffer, 0, 5 );
        in->digit_counter = 0;
        if( in->renumbering ) {
            if( in->osd ) tvtime_osd_set_hold_message( in->osd, "" );
            in->renumbering = 0;
        }
    }

    /* Decrement the stereo wait counter. */
    if( in->audio_counter > 0 ) in->audio_counter--;

    if( in->audio_counter == 0 ) {
        videoinput_set_audio_mode( in->vidin, 2 );
        if( in->osd ) {
            tvtime_osd_set_audio_mode( in->osd, videoinput_audio_mode_name( videoinput_get_audio_mode( in->vidin ) ) );
            tvtime_osd_show_info( in->osd );
        }
        in->audio_counter = -1;
    }

    if( in->frame_counter > 0 && !(in->frame_counter % 5)) {
        char input_text[6];

        strcpy( input_text, in->next_chan_buffer );
        if( !(in->frame_counter % 10) )
            strcat( input_text, "_" );
        if( in->osd ) {
            tvtime_osd_set_channel_number( in->osd, input_text );
            tvtime_osd_show_info( in->osd );
        }
    }

    if( in->change_channel ) {
        commands_station_change( in );
        in->change_channel = 0;
    }

    in->printdebug = 0;
    in->screenshot = 0;
    in->togglefullscreen = 0;
    in->toggleaspect = 0;
    in->toggledeinterlacingmode = 0;
    in->togglepulldowndetection = 0;
    in->togglemode = 0;
    in->update_luma = 0;
}


void commands_set_console( commands_t *in, console_t *con )
{
    in->console = con;
}

void commands_set_menu( commands_t *in, menu_t *m )
{
    in->menu = m;
}

int commands_console_on( commands_t *in )
{
    return in->console_on;
}

int commands_menu_on( commands_t *in )
{
    return in->menu_on;
}

int commands_scan_channels( commands_t *in )
{
    return in->scan_channels;
}

int commands_pause( commands_t *in )
{
    return in->pause;
}

int commands_apply_luma_correction( commands_t *in )
{
    return in->apply_luma;
}

int commands_update_luma_power( commands_t *in )
{
    return in->update_luma;
}

double commands_get_luma_power( commands_t *in )
{
    return in->luma_power;
}

double commands_get_overscan( commands_t *in )
{
    return in->overscan;
}

void commands_set_framerate( commands_t *in, int framerate )
{
    in->framerate = framerate % FRAMERATE_MAX;
}

