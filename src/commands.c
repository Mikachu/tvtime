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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"
#include <ctype.h>
#include "tvtimeconf.h"
#include "frequencies.h"
#include "mixer.h"
#include "commands.h"
#include "menu.h"
#include "parser.h"
#include "console.h"
#include "vbidata.h"

/* Number of frames to wait for next channel digit. */
#define CHANNEL_DELAY 100

/*
  struct freqinfo_s {
    
  };
*/

struct commands_s {
    config_t *cfg;
    videoinput_t *vidin;
    tvtime_osd_t *osd;
    video_correction_t *vc;
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
    
    int menu_on;
    menu_t *menu;

    int console_on;
    int scrollconsole;
    console_t *console;

    vbidata_t *vbi;
    int capturemode;
};

/**
 * Tuner settings.
 */
static int cur_channel = 0;
static int prev_channel = 0;
static int cur_freq_table = 1;
static int ntsc_cable_mode = NTSC_CABLE_MODE_NOMINAL;
static int finetune_amount = 0;
static void toggle_ntsc_cable_mode( void ) { ntsc_cable_mode = ( ntsc_cable_mode + 1 ) % 3; }

static int get_current_frequency( void )
{
    int base = tvtuner[ cur_channel ].freq[ cur_freq_table ].freq;

    if( !base ) {
        return 0;
    }

    if( cur_freq_table == NTSC_CABLE ) {
        if( ntsc_cable_mode == NTSC_CABLE_MODE_IRC ) {
            base = NTSC_CABLE_IRC( base );
        } else if( ntsc_cable_mode == NTSC_CABLE_MODE_HRC ) {
            base = NTSC_CABLE_HRC( base );
        }
    }

    return base + finetune_amount;
}

static int frequencies_find_current_index( videoinput_t *vidin )
{
    int tunerfreq = videoinput_get_tuner_freq( vidin );
    int closest = -1;
    int closesti = 0;
    int i;

    if( tunerfreq == 0 ) {
        /* Probably no tuner present */
        return 0;
    }

    for( i = 0; i < CHAN_ENTRIES; i++ ) {
        int curfreq = tvtuner[ i ].freq[ cur_freq_table ].freq;

        if( curfreq && ((closest < 0) || (abs( curfreq - tunerfreq ) < abs( closest - tunerfreq ))) ) {
            closest = curfreq;
            closesti = i;
        }
    }
    cur_channel = closesti;

    return 1;
}

static void frequencies_choose_first_frequency( void )
{
    int i;
    cur_channel = 0;

    for( i = 0; i < CHAN_ENTRIES; i++ ) {
        if( tvtuner[ i ].freq[ cur_freq_table ].freq ) {
            cur_channel = i;
            break;
        }
    }
}

static int frequencies_find_named_channel( const char *str )
{
    int i;
    const char *instr;
    
    if( !str ) return 0;

    while( *str == '0' ) {
        str++;
    }
    instr = str;
    if( !(*str) ) {
        instr = "0";
    }

    for( i = 0; i < CHAN_ENTRIES; i++ ) {
        const char *curstr = tvtuner[ i ].name;
        while( *curstr == ' ' ) curstr++;

        if( !strcasecmp( curstr, instr ) ) {
            cur_channel = i;
            return 1;
        }
    }

    return 0;
}

void frequencies_set_tuner_by_name( const char *tuner_name )
{
    int i;
    for( i = 0; i < NUM_FREQ_TABLES; i++ ) {
        if( !strcasecmp( freq_table_names[ i ].short_name, tuner_name ) ||
            !strcasecmp( freq_table_names[ i ].long_name, tuner_name ) ) {
            cur_freq_table = i;
            return;
        }
    }

    /* Default to NTSC_CABLE. */
    cur_freq_table = NTSC_CABLE;
}

void frequencies_list_disabled_freqs( )
{
    int i;

    
    for( i = 0; i < CHAN_ENTRIES; i++ ) {
        if( tvtuner[ i ].freq[ cur_freq_table ].freq
            && !tvtuner[ i ].freq[ cur_freq_table ].enabled ) {
            fprintf( stderr, "channel = %s %s 0\n", freq_table_names[ cur_freq_table ].short_name, tvtuner[ i ].name );
        }
    }
}

void frequencies_disable_all( )
{
    int i;

    for( i = 0; i < CHAN_ENTRIES; i++ ) {
        if( tvtuner[ i ].freq[ cur_freq_table ].freq ) {
            tvtuner[ i ].freq[ cur_freq_table ].enabled = 0;
        }
    }
}

void frequencies_disable_freqs( config_t *ct )
{
    const char *instr;
    const char *tmp;
    char *acopy = NULL, *str;
    parser_file_t *pf = config_get_parsed_file( ct );
    int i=1, j=0, k=0;
    char table_name[255], chan_name[6];
    int enabled, ret, the_freq_table=-1, the_channel=-1;

    for(;;) {
        tmp = parser_get( pf, "channel", i++ );
        if( !tmp ) break;
        if( acopy ) free( acopy );
        acopy = strdup( tmp );

        ret = sscanf( acopy, " %254s %5s %d ", table_name, chan_name, &enabled );
        if( ret != 3 ) {
            fprintf( stderr, "Ignoring: ret = %d, channel = %s\n", ret, acopy);
            continue;
        }

        the_freq_table = -1;
        for( j = 0; j < NUM_FREQ_TABLES; j++ ) {
            if( !strcasecmp( freq_table_names[ j ].short_name, table_name ) ) {
                the_freq_table = j;
                break;
            }
        }

        str = chan_name;
        
        while( *str == '0' ) {
            str++;
        }
        instr = str;
        if( !(*str) ) {
            instr = "0";
        }

        the_channel = -1;
        for( k = 0; k < CHAN_ENTRIES; k++ ) {
            const char *curstr = tvtuner[ k ].name;
            while( *curstr == ' ' ) curstr++;

            if( !strcasecmp( curstr, instr ) ) {
                the_channel = k;
            }
        }

        if( the_channel > -1 && the_freq_table > -1 )
            tvtuner[ the_channel ].freq[ the_freq_table ].enabled = enabled;
    }
}

static void reinit_tuner( commands_t *in )
{
    /* Setup the tuner if available. */
    if( videoinput_has_tuner( in->vidin ) ) {
        /**
         * Set to the current channel, or the first channel in our
         * frequency list.
         */
        if( !frequencies_find_current_index( in->vidin ) ) {
            /* set to a known frequency */
            frequencies_choose_first_frequency();
            videoinput_set_tuner_freq( in->vidin, get_current_frequency() );
            if( in->vbi ) {
                vbidata_reset( in->vbi );
                vbidata_capture_mode( in->vbi, in->capturemode );
            }
        }

        if( config_get_verbose( in->cfg ) ) {
            fprintf( stderr, "tvtime: Changing to channel %s.\n", 
                     tvtuner[ cur_channel ].name );
        }

        if( in->osd ) {
            if( cur_freq_table == NTSC_CABLE && ntsc_cable_mode != NTSC_CABLE_MODE_NOMINAL ) {
                char tablename[ 200 ];
                sprintf( tablename, "%s [%s]", freq_table_names[ cur_freq_table ].long_name,
                         ntsc_cable_mode == NTSC_CABLE_MODE_IRC ? "IRC" : "HRC" );
                tvtime_osd_set_freq_table( in->osd, tablename );
            } else {
                tvtime_osd_set_freq_table( in->osd, freq_table_names[ cur_freq_table ].long_name );
            }
            tvtime_osd_set_channel_number( in->osd, tvtuner[ cur_channel ].name );
            tvtime_osd_show_info( in->osd );
        }
    } else if( in->osd ) {
        tvtime_osd_set_freq_table( in->osd, "" );
        tvtime_osd_set_channel_number( in->osd, "" );
        tvtime_osd_show_info( in->osd );
    }
}

commands_t *commands_new( config_t *cfg, videoinput_t *vidin,
                          tvtime_osd_t *osd, video_correction_t *vc )
{
    commands_t *in = (commands_t *) malloc( sizeof( struct commands_s ) );

    if( !in ) {
        return NULL;
    }

    in->cfg = cfg;
    in->vidin = vidin;
    in->osd = osd;
    in->vc = vc;
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
    in->menu_on = 0;
    in->console_on = 0;
    in->scrollconsole = 0;

    in->console = 0;
    in->vbi = 0;
    in->capturemode = CAPTURE_OFF;


    /**
     * Set the current channel list.
     */
    frequencies_set_tuner_by_name( config_get_v4l_freq( in->cfg ) );
    finetune_amount = config_get_finetune( in->cfg );

    reinit_tuner( in );

    frequencies_disable_freqs( cfg );

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


static void commands_channel_change_relative( commands_t *in, int offset )
{
    int verbose = config_get_verbose( in->cfg );

    if( !videoinput_has_tuner( in->vidin ) ) {
        if( verbose )
            fprintf( stderr, "tvtime: Can't change channel, "
                     "no tuner available on this input!\n" );
    } else {
        prev_channel = cur_channel;
		for(;;) {
            cur_channel = (cur_channel + offset + CHAN_ENTRIES) % CHAN_ENTRIES;
            if( tvtuner[ cur_channel ].freq[ cur_freq_table ].freq 
                && tvtuner[ cur_channel ].freq[ cur_freq_table ].enabled ) 
                break;
        }

        
        videoinput_set_tuner_freq( in->vidin, get_current_frequency() );
        if( in->vbi ) {
            vbidata_reset( in->vbi );
            vbidata_capture_mode( in->vbi, in->capturemode );
        }

        if( verbose ) {
            fprintf( stderr, "tvtime: Changing to channel %s\n", 
                     tvtuner[ cur_channel ].name );
        }
        if( in->osd ) {
            tvtime_osd_set_channel_number( in->osd, tvtuner[ cur_channel ].name );
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

    case TVTIME_DEBUG:
        in->printdebug = 1;
        break;

    case TVTIME_SCREENSHOT:
        in->screenshot = 1;
        break;

    case TVTIME_SHOW_BARS:
        in->showbars = !in->showbars;
        break;

    case TVTIME_FULLSCREEN:
        in->togglefullscreen = 1;
        break;

    case TVTIME_SCROLL_CONSOLE_UP:
    case TVTIME_SCROLL_CONSOLE_DOWN:
        if( in->console_on )
            console_scroll_n( in->console, (tvtime_cmd == TVTIME_SCROLL_CONSOLE_UP) ? -1 : 1 );
        break;
            

    case TVTIME_TOGGLE_CONSOLE:
        in->console_on = !in->console_on;
        console_toggle_console( in->console );
        break;

    case TVTIME_SKIP_CHANNEL:
        tvtuner[ cur_channel ].freq[ cur_freq_table ].enabled = !tvtuner[ cur_channel ].freq[ cur_freq_table ].enabled;
        fprintf( stderr, "channel = %s %s %d\n", 
                 freq_table_names[ cur_freq_table ].short_name, 
                 tvtuner[ cur_channel ].name, 
                 tvtuner[ cur_channel ].freq[ cur_freq_table ].enabled );
        break;
            
    case TVTIME_ASPECT:
        in->toggleaspect = 1;
        break;

    case TVTIME_TOGGLE_CC:
        vbidata_capture_mode( in->vbi, in->capturemode ? CAPTURE_OFF : CAPTURE_CC1 );
        if( in->capturemode )
            in->capturemode = CAPTURE_OFF;
        else
            in->capturemode = CAPTURE_CC1;
        break;

    case TVTIME_DISPLAY_INFO:
        tvtime_osd_show_info( in->osd );
        break;

    case TVTIME_SHOW_CREDITS:
        tvtime_osd_toggle_show_credits( in->osd );
        break;

    case TVTIME_DEINTERLACINGMODE:
        in->toggledeinterlacingmode = 1;
        break;

    case TVTIME_CHANNEL_CHAR:
        /* decode the input char from commands  */
        if( in->digit_counter == 0 ) memset( in->next_chan_buffer, 0, 5 );
        in->next_chan_buffer[ in->digit_counter ] = arg & 0xFF;
        in->digit_counter++;
        in->digit_counter %= 4;
        in->frame_counter = CHANNEL_DELAY;
        break;

    case TVTIME_LUMA_CORRECTION_TOGGLE:
        config_set_apply_luma_correction( in->cfg, !config_get_apply_luma_correction( in->cfg ) );
        if( in->osd ) {
            if( config_get_apply_luma_correction( in->cfg ) ) {
                tvtime_osd_show_message( in->osd, "Luma correction enabled." );
            } else {
                tvtime_osd_show_message( in->osd, "Luma correction disabled." );
            }
        }
        break;

    case TVTIME_LUMA_UP:
    case TVTIME_LUMA_DOWN:
        if( !config_get_apply_luma_correction( in->cfg ) ) {
            fprintf( stderr, "tvtime: Luma correction disabled.  "
                     "Run with -c to use it.\n" );
        } else {
            config_set_luma_correction( 
                in->cfg, 
                config_get_luma_correction( in->cfg ) + 
                ( (tvtime_cmd == TVTIME_LUMA_UP) ? 0.1 : -0.1 ));

            if( config_get_luma_correction( in->cfg ) > 10.0 ) 
                config_set_luma_correction( in->cfg , 10.0);

            if( config_get_luma_correction( in->cfg ) <  0.0 ) 
                config_set_luma_correction( in->cfg, 0.0 );

            if( verbose ) fprintf( stderr, "tvtime: Luma "
                                   "correction value: %.1f\n", 
                                   config_get_luma_correction( in->cfg ));

            video_correction_set_luma_power( in->vc,
                                             config_get_luma_correction( in->cfg ) );

            if( in->osd ) {
                char message[ 200 ];
                sprintf( message, "Luma correction value: %.1f", 
                         config_get_luma_correction( in->cfg ) );
                tvtime_osd_show_message( in->osd, message );
            }
        }
        break;

    case TVTIME_FREQLIST_DOWN:
    case TVTIME_FREQLIST_UP:
        if( videoinput_has_tuner( in->vidin ) ) {
            cur_freq_table = ( cur_freq_table + ( tvtime_cmd == TVTIME_FREQLIST_UP ? 1 : -1 ) + NUM_FREQ_TABLES ) % NUM_FREQ_TABLES;
            reinit_tuner( in );
        }
        break;

    case TVTIME_AUTO_ADJUST_PICT:
        videoinput_reset_default_settings( in->vidin );
        break;

    case TVTIME_TOGGLE_NTSC_CABLE_MODE:
        if( videoinput_has_tuner( in->vidin ) ) {
            toggle_ntsc_cable_mode();
            reinit_tuner( in );
        }
        break;

    case TVTIME_FINETUNE_DOWN:
    case TVTIME_FINETUNE_UP:
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
        break;

    case TVTIME_CHANNEL_UP: 
    case TVTIME_CHANNEL_DOWN:
        commands_channel_change_relative( in, (tvtime_cmd == TVTIME_CHANNEL_UP) ?  1 : -1 );
        break;

    case TVTIME_CHANNEL_PREV:
        commands_channel_change_relative( in, prev_channel ? (prev_channel - cur_channel) : 0 );
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
        videoinput_mute( in->vidin, !videoinput_get_muted( in->vidin ) );
        tvtime_osd_volume_muted( in->osd, videoinput_get_muted( in->vidin ) );
        break;

    case TVTIME_TV_VIDEO:
        videoinput_set_input_num( in->vidin, ( videoinput_get_input_num( in->vidin ) + 1 ) % videoinput_get_num_inputs( in->vidin ) );
        tvtime_osd_set_input( in->osd, videoinput_get_input_name( in->vidin ) );
        reinit_tuner( in );
        break;

    case TVTIME_HUE_UP:
    case TVTIME_HUE_DOWN:
        videoinput_set_hue_relative( 
            in->vidin, 
            (tvtime_cmd == TVTIME_HUE_UP) ? 1 : -1 );
        if( in->osd ) {
            tvtime_osd_show_data_bar( in->osd, "Hue    ", videoinput_get_hue( in->vidin ) );
        }
        break;

    case TVTIME_BRIGHT_UP: 
    case TVTIME_BRIGHT_DOWN:
        videoinput_set_brightness_relative( 
            in->vidin, 
            (tvtime_cmd == TVTIME_BRIGHT_UP) ? 1 : -1 );
        if( in->osd ) {
            tvtime_osd_show_data_bar( in->osd, "Bright ", videoinput_get_brightness( in->vidin ) );
        }
        break;

    case TVTIME_CONT_UP:
    case TVTIME_CONT_DOWN:
        videoinput_set_contrast_relative( 
            in->vidin, 
            (tvtime_cmd == TVTIME_CONT_UP) ? 1 : -1 );
        if( in->osd ) {
            tvtime_osd_show_data_bar( in->osd, "Cont   ", videoinput_get_contrast( in->vidin ) );
        }

        break;

    case TVTIME_COLOUR_UP:
    case TVTIME_COLOUR_DOWN:
        videoinput_set_colour_relative( 
            in->vidin, 
            (tvtime_cmd == TVTIME_COLOUR_UP) ? 1 : -1 );
        if( in->osd ) {
            tvtime_osd_show_data_bar( in->osd, "Colour ", videoinput_get_colour( in->vidin ) );
        }
        break;

    case TVTIME_ENTER:
        if( in->frame_counter ) {
            if( *in->next_chan_buffer ) {
                prev_channel = cur_channel;
                /* this sets the current channel accordingly */
                if( frequencies_find_named_channel( in->next_chan_buffer ) ) {
                    /* go to the next valid channel instead */
                    for(;;) {
                        if( tvtuner[ cur_channel ].freq[ cur_freq_table ].freq ) break;
                        cur_channel = (cur_channel + 1 + CHAN_ENTRIES) % CHAN_ENTRIES;
                    }
                    videoinput_set_tuner_freq( in->vidin, get_current_frequency() );
                    if( in->vbi ) {
                        vbidata_reset( in->vbi );
                        vbidata_capture_mode( in->vbi, in->capturemode );
                    }


                    if( verbose ) {
                        fprintf( stderr, "tvtime: Changing to channel %s\n", 
                                 tvtuner[ cur_channel ].name );
                    }

                    if( in->osd ) {
                        tvtime_osd_set_channel_number( in->osd, tvtuner[ cur_channel ].name );
                        tvtime_osd_show_info( in->osd );
                    }
                    in->frame_counter = 0;
                } else {
                    /* No valid channel found. */
                    if( in->osd ) {
                        tvtime_osd_set_channel_number( in->osd, tvtuner[ cur_channel ].name );
                        tvtime_osd_show_info( in->osd );
                    }
                    in->frame_counter = 0;
                }
            }
        }
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

int commands_toggle_aspect( commands_t *in )
{
    return in->toggleaspect;
}

int commands_toggle_deinterlacing_mode( commands_t *in )
{
    return in->toggledeinterlacingmode;
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

    in->printdebug = 0;
    in->screenshot = 0;
    in->togglefullscreen = 0;
    in->toggleaspect = 0;
    in->toggledeinterlacingmode = 0;
}


void commands_set_console( commands_t *in, console_t *con ) {
    if( !in ) return;
    in->console = con;
}
void commands_set_menu( commands_t *in, menu_t *m ) {
    if( !in ) return;
    in->menu = m;
}