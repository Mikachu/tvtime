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
#include <time.h>
#include "tvtimeconf.h"
#include "frequencies.h"
#include "mixer.h"
#include "input.h"
#include "menu.h"

/* Number of frames to wait for next channel digit. */
#define CHANNEL_DELAY 100

/* Number of frames to pause during channel change. */
#define CHANNEL_HOLD 2


struct input_s {
    config_t *cfg;
    videoinput_t *vidin;
    tvtime_osd_t *osd;
    video_correction_t *vc;
    char next_chan_buffer[ 5 ];
    int frame_counter;
    int digit_counter;
    int videohold;
    int quit;
    int inputnum;

    int screenshot;
    int printdebug;
    int showbars;
    int showtest;
    int togglefullscreen;
    int toggleaspect;
    int toggledeinterlacingmode;
    
    int togglemenumode;
    menu_t *menu;
};

input_t *input_new( config_t *cfg, videoinput_t *vidin,
                    tvtime_osd_t *osd, video_correction_t *vc )
{
    input_t *in = (input_t *) malloc( sizeof( input_t ) );
    if( !in ) {
        fprintf( stderr, "input: Could not create new input object.\n" );
        return NULL;
    }

    in->cfg = cfg;
    in->vidin = vidin;
    in->osd = osd;
    in->vc = vc;
    in->frame_counter = 0;
    in->digit_counter = 0;
    in->menu = 0;

    in->videohold = 0;
    in->quit = 0;
    in->showbars = 0;
    in->showtest = 0;
    in->printdebug = 0;
    in->screenshot = 0;
    in->togglefullscreen = 0;
    in->toggleaspect = 0;
    in->toggledeinterlacingmode = 0;
    in->togglemenumode = 0;

    return in;
}

void input_delete( input_t *in )
{
    free( in );
}

void input_set_menu( input_t *in, menu_t *m )
{
    in->menu = m;
}

void input_callback( input_t *in, InputEvent command, int arg )
{
    int tvtime_cmd, verbose;
    int volume;

    if( in->quit ) return;

    verbose = config_get_verbose( in->cfg );

    if( in->togglemenumode ) {
        if( menu_callback( in->menu, command, arg ) ) {
            return;
        }
    }

    switch( command ) {
    case I_QUIT:
        in->quit = 1;
        break;

    case I_KEYDOWN:
         tvtime_cmd = config_key_to_command( in->cfg, arg );

         switch( tvtime_cmd ) {

         case TVTIME_QUIT:
             in->quit = 1;
             break;

         case TVTIME_MENUMODE:
             in->togglemenumode = 1;
             menu_init( in->menu );
             break;

         case TVTIME_DEBUG:
             in->printdebug = 1;
             break;

         case TVTIME_SCREENSHOT:
             in->screenshot = 1;
             break;

         case TVTIME_SHOW_TEST:
             in->showtest = !in->showtest;
             break;

         case TVTIME_SHOW_BARS:
             in->showbars = !in->showbars;
             break;

         case TVTIME_FULLSCREEN:
             in->togglefullscreen = 1;
             break;

         case TVTIME_ASPECT:
             in->toggleaspect = 1;
             break;

         case TVTIME_DEINTERLACINGMODE:
             in->toggledeinterlacingmode = 1;
             break;

         case TVTIME_CHANNEL_CHAR:
             /* decode the input char from commands and capitalize */
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

         case TVTIME_FINETUNE_DOWN:
         case TVTIME_FINETUNE_UP:
             if( !videoinput_has_tuner( in->vidin ) && verbose ) {
                 fprintf( stderr, "tvtime: Can't fine tune channel, "
                          "no tuner available on this input!\n" );
             } else if( videoinput_has_tuner( in->vidin ) ) {
                 videoinput_set_tuner_freq( in->vidin, 
                                            videoinput_get_tuner_freq( in->vidin ) +
                                            config_get_finetune( in->cfg ) +
                                            ( tvtime_cmd == TVTIME_FINETUNE_UP ? ((1000/16)+1) : -(1000/16) ) );
                 if( in->osd ) {
                     char message[ 200 ];
                     sprintf( message, "Tuning: %4.2fMhz.", ((double) videoinput_get_tuner_freq( in->vidin )) / 1000.0 );
                     tvtime_osd_show_message( in->osd, message );
                 }
             }
             break;

         case TVTIME_CHANNEL_UP: 
         case TVTIME_CHANNEL_DOWN:
             if( !videoinput_has_tuner( in->vidin ) ) {
                 if( verbose )
                     fprintf( stderr, 
                              "tvtime: Can't change channel, "
                              "no tuner available on this input!\n" );
             } else {
                 char timestamp[50];
                 time_t tm = time(NULL);
                 int start_index = chanindex;
                 do {
                     chanindex = (chanindex + 
                                  ( (tvtime_cmd == TVTIME_CHANNEL_UP) ? 
                                    1 : -1) + chancount) % chancount;

                     if( chanindex == start_index ) break;

                     videoinput_set_tuner_freq( in->vidin, 
                                                chanlist[ chanindex ].freq +
                                                config_get_finetune( in->cfg ) );
                     in->videohold = CHANNEL_HOLD;
                 } while( !videoinput_freq_present( in->vidin ) );

                 if( verbose ) fprintf( stderr, "tvtime: Changing to "
                                        "channel %s\n", 
                                        chanlist[ chanindex ].name );
                 if( in->osd ) {
                     tvtime_osd_show_channel_number( in->osd,
                                                     chanlist[ chanindex ].name );
                 }
                 strftime( timestamp, 50, config_get_timeformat( in->cfg ), 
                           localtime(&tm) );
                 if( in->osd ) {
                     tvtime_osd_show_channel_info( in->osd, timestamp );
                     tvtime_osd_show_channel_logo( in->osd );
                 }
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
             mixer_toggle_mute();
             break;

         case TVTIME_TV_VIDEO:
             videoinput_set_input_num( in->vidin, ( videoinput_get_input_num( in->vidin ) + 1 ) % videoinput_get_num_inputs( in->vidin ) );
             /* Setup the tuner if available. */
             if( videoinput_has_tuner( in->vidin ) ) {
                 /**
                  * Set to the current channel, or the first channel in our
                  * frequency list.
                  */
                 char timestamp[50];
                 time_t tm = time(NULL);
                 int rc = frequencies_find_current_index( in->vidin );
                 if( rc == -1 ) {
                     /* set to a known frequency */
                     videoinput_set_tuner_freq( in->vidin, chanlist[ chanindex ].freq +
                                                config_get_finetune( in->cfg ) );

                     if( verbose ) fprintf( stderr, 
                                            "tvtime: Changing to channel %s.\n", 
                                            chanlist[ chanindex ].name );
                 } else if( rc > 0 ) {
                     if( verbose ) fprintf( stderr, 
                                            "tvtime: Changing to channel %s.\n",
                                            chanlist[ chanindex ].name );
                 }
                 strftime( timestamp, 50, config_get_timeformat( in->cfg ), 
                           localtime(&tm) );
                 if( in->osd ) {
                     tvtime_osd_show_channel_number( in->osd, chanlist[ chanindex ].name );
                     tvtime_osd_show_channel_info( in->osd, timestamp );
                     tvtime_osd_show_channel_logo( in->osd );
                 }
             }
             if( in->osd ) {
                 tvtime_osd_show_message( in->osd, videoinput_get_input_name( in->vidin ) );
             }
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
                     char timestamp[50];
                     time_t tm = time(NULL);
                     int found;

                     /* this sets chanindex accordingly */
                     found = frequencies_find_named_channel( 
                         in->next_chan_buffer );

                     if( found > -1 ) {
                         videoinput_set_tuner_freq( 
                             in->vidin, 
                             chanlist[ chanindex ].freq +
                             config_get_finetune( in->cfg ) );

                         in->videohold = CHANNEL_HOLD;

                         if( verbose ) {
                             fprintf( stderr, 
                                      "tvtime: Changing to channel %s\n", 
                                      chanlist[ chanindex ].name );
                         }

                         if( in->osd ) {
                             tvtime_osd_show_channel_number( 
                                 in->osd, 
                                 chanlist[ chanindex ].name );
                         }

                         strftime( timestamp, 50, 
                                   config_get_timeformat( in->cfg ), 
                                   localtime(&tm) );

                         if( in->osd ) {
                             tvtime_osd_show_channel_info( in->osd, timestamp );
                             tvtime_osd_show_channel_logo( in->osd );
                         }
                         in->frame_counter = 0;
                     } else {
                         /* no valid channel */
                         in->frame_counter = 0;
                     }
                 }
             }
         }
         break;

    default:
        break;

    }

}

int input_quit( input_t *in )
{
    return in->quit;
}

int input_print_debug( input_t *in )
{
    return in->printdebug;
}

int input_show_bars( input_t *in )
{
    return in->showbars;
}

int input_show_test( input_t *in )
{
    return in->showtest;
}

int input_take_screenshot( input_t *in )
{
    return in->screenshot;
}

int input_videohold( input_t *in )
{
    return in->videohold;
}

int input_toggle_fullscreen( input_t *in )
{
    return in->togglefullscreen;
}

int input_toggle_aspect( input_t *in )
{
    return in->toggleaspect;
}

int input_toggle_deinterlacing_mode( input_t *in )
{
    return in->toggledeinterlacingmode;
}

int input_toggle_menu( input_t *in )
{
    in->togglemenumode = !in->togglemenumode;
    return in->togglemenumode;
}

void input_next_frame( input_t *in )
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
            tvtime_osd_show_channel_number( in->osd, input_text );
        }
    }

    if( in->videohold ) in->videohold--;

    in->printdebug = 0;
    in->screenshot = 0;
    in->togglefullscreen = 0;
    in->toggleaspect = 0;
    in->toggledeinterlacingmode = 0;
}


