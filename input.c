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
#include "input.h"
#include "config.h"
#include "videotools.h"
#include "videoinput.h"
#include "frequencies.h"
#include "mixer.h"

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
    in->videohold = 0;

    return in;
}

void input_delete( input_t *in )
{
    free( in );
}

void input_callback( input_t *in, InputEvent command, int arg )
{
    int tvtime_cmd, verbose;
    int volume;

    verbose = config_get_verbose( in->cfg );

    fprintf( stderr, "input: command = %o  arg = %d\n", command, arg );
    switch( command ) {
    case I_QUIT:
        break;

    case I_KEYDOWN:
         tvtime_cmd = config_key_to_command( in->cfg, arg );
         fprintf( stderr, "input: command becomes %d\n", tvtime_cmd );

         switch( tvtime_cmd ) {

         case TVTIME_CHANNEL_CHAR:
             /* decode the input char from commands and capitalize */
             in->next_chan_buffer[ in->digit_counter ] = arg & 0xFF;
             in->digit_counter++;
             in->digit_counter %= 4;
             in->frame_counter = CHANNEL_DELAY;
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
             }
             break;

         case TVTIME_CHANNEL_UP: 
         case TVTIME_CHANNEL_DOWN:
             if( !videoinput_has_tuner( in->vidin ) ) {
                 if( verbose )
                     fprintf( stderr, 
                              "tvtime: Can't change channel, "
                              "no tuner present!\n" );
             } else {
                 int start_index = chanindex;
                 do {
                     chanindex = (chanindex + 
                                  ( (tvtime_cmd == TVTIME_CHANNEL_UP) ? 
                                    1 : -1) + chancount) % chancount;

                     if( chanindex == start_index ) break;

                     videoinput_set_tuner_freq( in->vidin, 
                                                chanlist[ chanindex ].freq );
                     in->videohold = CHANNEL_HOLD;
                 } while( !videoinput_freq_present( in->vidin ) );

                 if( verbose ) fprintf( stderr, "tvtime: Changing to "
                                        "channel %s\n", 
                                        chanlist[ chanindex ].name );
/*
                 osd_string_show_text( channel_number, 
                                       chanlist[ chanindex ].name, 80 );
*/
             }
             break;

         case TVTIME_MIXER_UP: 
         case TVTIME_MIXER_DOWN:

             volume = mixer_set_volume( 
                 ( (tvtime_cmd == TVTIME_MIXER_UP) ? 1 : -1 ) );

             if( verbose )
                 fprintf( stderr, "input: volume %d\n", (volume & 0xFF) );
/*
             show_osd_bars( in, "Volume ", bar, volume & 0xFF );
*/
             break;

         case TVTIME_MIXER_MUTE:
             mixer_toggle_mute();
             break;

         case TVTIME_HUE_UP:
         case TVTIME_HUE_DOWN:
             videoinput_set_hue_relative( 
                 in->vidin, 
                 (tvtime_cmd == TVTIME_HUE_UP) ? 1 : -1 );
/*
             show_osd_bars( in, "Hue    ", bar, 
                            videoinput_get_hue( in->vidin ) );
*/
             break;

         case TVTIME_BRIGHT_UP: 
         case TVTIME_BRIGHT_DOWN:
             videoinput_set_brightness_relative( 
                 in->vidin, 
                 (tvtime_cmd == TVTIME_BRIGHT_UP) ? 1 : -1 );
/*
             show_osd_bars( in, "Bright ", bar, 
                            videoinput_get_brightness( in->vidin ) );
*/
             break;

         case TVTIME_CONT_UP:
         case TVTIME_CONT_DOWN:
             videoinput_set_contrast_relative( 
                 in->vidin, 
                 (tvtime_cmd == TVTIME_CONT_UP) ? 1 : -1 );

/*
             show_osd_bars( in, "Cont   ", bar, 
                            videoinput_get_contrast( in->vidin ) );
*/
             break;

         case TVTIME_COLOUR_UP:
         case TVTIME_COLOUR_DOWN:
             videoinput_set_colour_relative( 
                 in->vidin, 
                 (tvtime_cmd == TVTIME_COLOUR_UP) ? 1 : -1 );

/*
             show_osd_bars( in, "Colour ", bar, 
                            videoinput_get_colour( in->vidin ) );
*/
             break;

         case TVTIME_ENTER:
             if( in->frame_counter ) {
                 if( *in->next_chan_buffer ) {
                     int found;

                     /* this sets chanindex accordingly */
                     found = frequencies_find_named_channel( 
                         in->next_chan_buffer );

                     if( found > -1 ) {
                         videoinput_set_tuner_freq( 
                             in->vidin, 
                             chanlist[ chanindex ].freq );

                         in->videohold = CHANNEL_HOLD;

                         if( verbose ) 
                             fprintf( stderr, 
                                      "tvtime: Changing to channel %s\n", 
                                      chanlist[ chanindex ].name );

/*
                         osd_string_show_text( in->channel_number, 
                                               chanlist[ chanindex ].name, 
                                               80 );
*/
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

void input_next_frame( input_t *in )
{
    /* Decrement the frame counter if user is typing digits */
    if( in->frame_counter > 0 ) in->frame_counter--;

    if( in->frame_counter == 0 ) {
        memset( (void*)in->next_chan_buffer, 0, 5 );
        in->digit_counter = 0;
    }

    if( in->frame_counter > 0 && !(in->frame_counter % 5)) {
        char input_text[6];

        strcpy( input_text, in->next_chan_buffer );
        if( !(in->frame_counter % 10) )
            strcat( input_text, "_" );
/*
        osd_string_show_text( in->channel_number, 
                              input_text, CHANNEL_DELAY );
*/
    }
}

void input_dec_videohold( input_t *in ) 
{
    if( in ) in->videohold--;
}

int input_get_videohold( input_t *in )
{
    if( in ) return in->videohold;
    return 0;
}

