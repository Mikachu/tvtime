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
#include "input.h"

struct input_s {
    config_t        *cfg;
    osd_t           *osd;
    videoinput_t    *vidin;
    int frame_counter;
};


/**
 * Input commands.
 */
enum tvtime_commands
{
    TVTIME_NOCOMMAND     = 0,
    TVTIME_QUIT          = (1<<0),
    TVTIME_CHANNEL_UP    = (1<<1),
    TVTIME_CHANNEL_DOWN  = (1<<2),
    TVTIME_LUMA_UP       = (1<<3),
    TVTIME_LUMA_DOWN     = (1<<4),
    TVTIME_MIXER_MUTE    = (1<<5),
    TVTIME_MIXER_UP      = (1<<6),
    TVTIME_MIXER_DOWN    = (1<<7),
    TVTIME_ENTER         = (1<<8),
    TVTIME_CHANNEL_CHAR  = (1<<9),

    TVTIME_HUE_DOWN      = (1<<10),
    TVTIME_HUE_UP        = (1<<11),
    TVTIME_BRIGHT_DOWN   = (1<<12),
    TVTIME_BRIGHT_UP     = (1<<13),
    TVTIME_CONT_DOWN     = (1<<14),
    TVTIME_CONT_UP       = (1<<15),
    TVTIME_COLOUR_DOWN   = (1<<16),
    TVTIME_COLOUR_UP     = (1<<17),

    TVTIME_SHOW_BARS     = (1<<18),
    TVTIME_SHOW_TEST     = (1<<19),
    TVTIME_DEBUG         = (1<<20)
};

int key_to_command( input_t *in, int key )
{
    int i;

    if( !in || !in->cfg ) {
        fprintf( stderr, "input: key_to_command: Invalid input_t given.\n" );
        return TVTIME_NOCOMMAND;
    }

    if( !key ) return TVTIME_NOCOMMAND;

    for( i=0; i < KEYMAP_SIZE; i++ ) {
        if( in->cfg->keymap[i] == key ) return i;
    }

    if( isalnum(key) ) return TVTIME_CHANNEL_CHAR;
    if( key == I_ENTER ) return TVTIME_ENTER;
        
    return TVTIME_NOCOMMAND;
}

input_t *input_new( config_t *cfg, osd_t *osd, videoinput_t *vidin )
{
    input_t *in;

    in = (input_t *)malloc(sizeof(input_t));
    if( !in ) {
        fprintf( stderr, "input: Could not create new input object.\n" );
        return NULL;
    }

    in->cfg = cfg;
    in->osd = osd;
    in->vidin = vidin;
    in->frame_counter = 0;

    return in;
}

void input_delete( input_t *in )
{
}



void input_callback( input_t* in, InputEvent command, int arg )
{
    int commands;
    switch( command ) {
    case I_KEYDOWN:

        commands = key_to_command( key );

        if( commands & TVTIME_CHANNEL_CHAR ) {
            /* decode the input char from commands and capitalize */
            next_chan_buffer[ digit_counter ] = (char)((commands & 0xFF) ^ 0x20);
            digit_counter++;
            digit_counter %= 4;
            frame_counter = CHANNEL_DELAY;
        } else {
            if( commands & TVTIME_QUIT ) {
                break;
            }
            if( commands & TVTIME_DEBUG ) {
                printdebug = 1;
            }
            if( commands & TVTIME_SHOW_BARS ) {
                if( !showtest ) {
                    showtest = 2;
                } else {
                    showtest = 0;
                }
            }
            if( commands & TVTIME_SHOW_TEST ) {
                if( !showtest ) {
                    showtest = 1;
                } else {
                    showtest = 0;
                }
            }
            if( commands & TVTIME_LUMA_UP || commands & TVTIME_LUMA_DOWN ) {
                if( !config_get_apply_luma_correction( ct ) ) {
                    fprintf( stderr, "tvtime: Luma correction disabled.  "
                                     "Run with -c to use it.\n" );
                } else {
                    config_set_luma_correction( ct, config_get_luma_correction(ct) + ( (commands & TVTIME_LUMA_UP) ? 0.1 : -0.1 ));
                    if( config_get_luma_correction( ct ) > 10.0 ) 
                        config_set_luma_correction( ct , 10.0);
                    if( config_get_luma_correction( ct ) <  0.0 ) 
                        config_set_luma_correction( ct, 0.0 );
                    if( verbose ) fprintf( stderr, "tvtime: Luma "
                                           "correction value: %.1f\n", 
                                           config_get_luma_correction( ct ) );
                    video_correction_set_luma_power( vc, 
                                                     config_get_luma_correction( ct ) );
                }
            }
            if( commands & TVTIME_CHANNEL_UP || commands & TVTIME_CHANNEL_DOWN ) {
                if( !videoinput_has_tuner( vidin ) ) {
                    if( verbose )
                        fprintf( stderr, "tvtime: Can't change channel, "
                                 "no tuner present!\n" );
                } else {
                    int start_index = chanindex;
                    do {
                        chanindex = (chanindex + ( (commands & TVTIME_CHANNEL_UP) ? 1 : -1) + chancount) % chancount;

                        if( chanindex == start_index ) break;

                        videoinput_set_tuner_freq( vidin, chanlist[ chanindex ].freq );
                        videohold = CHANNEL_HOLD;
                    } while( !videoinput_freq_present( vidin ) );
                    if( verbose ) fprintf( stderr, "tvtime: Changing to "
                                           "channel %s\n", 
                                           chanlist[ chanindex ].name );
                    osd_string_show_text( channel_number, 
                                          chanlist[ chanindex ].name, 80 );
                }
            }
            if( commands & TVTIME_MIXER_UP || commands & TVTIME_MIXER_DOWN ) {
                char bar[108];
                volume = mixer_set_volume( ( (commands & TVTIME_MIXER_UP) ? 1 : -1 ) );
                if( verbose )
                    fprintf( stderr, "tvtime: volume %d\n", (volume & 0xFF) );

                memset( bar, 0, 108 );
                strcpy( bar, "Volume " );
                memset( bar+7, '|', volume & 0xFF );
                osd_string_show_text( volume_bar, bar, 80 );
            }
            if( commands & TVTIME_MIXER_MUTE ) {
                mixer_toggle_mute();
            }
            if( commands & TVTIME_HUE_UP || commands & TVTIME_HUE_DOWN ) {
                char bar[108];
                videoinput_set_hue_relative( vidin, (commands & TVTIME_HUE_UP) ? 1 : -1 );
                memset( bar, 0, 108 );
                strcpy( bar, "Hue    " );
                memset( bar+7, '|', videoinput_get_hue( vidin ) );
                osd_string_show_text( volume_bar, bar, 80 );
            }
            if( commands & TVTIME_BRIGHT_UP || commands & TVTIME_BRIGHT_DOWN ) {
                char bar[108];
                videoinput_set_brightness_relative( vidin, (commands & TVTIME_BRIGHT_UP) ? 1 : -1 );
                memset( bar, 0, 108 );
                strcpy( bar, "Bright " );
                memset( bar+7, '|', videoinput_get_brightness( vidin ) );
                osd_string_show_text( volume_bar, bar, 80 );
            }
            if( commands & TVTIME_CONT_UP || commands & TVTIME_CONT_DOWN ) {
                char bar[108];
                videoinput_set_contrast_relative( vidin, (commands & TVTIME_CONT_UP) ? 1 : -1 );
                memset( bar, 0, 108 );
                strcpy( bar, "Cont   " );
                memset( bar+7, '|', videoinput_get_contrast( vidin ) );
                osd_string_show_text( volume_bar, bar, 80 );
            }
            if( commands & TVTIME_COLOUR_UP || commands & TVTIME_COLOUR_DOWN ) {
                char bar[108];
                videoinput_set_colour_relative( vidin, (commands & TVTIME_COLOUR_UP) ? 1 : -1 );
                memset( bar, 0, 108 );
                strcpy( bar, "Colour " );
                memset( bar+7, '|', videoinput_get_colour( vidin ) );
                osd_string_show_text( volume_bar, bar, 80 );
            }
            if( commands & TVTIME_DIGIT ) {
                char digit = '0';

                if( commands & TVTIME_KP0 ) { digit = '0'; }
                if( commands & TVTIME_KP1 ) { digit = '1'; }
                if( commands & TVTIME_KP2 ) { digit = '2'; }
                if( commands & TVTIME_KP3 ) { digit = '3'; }
                if( commands & TVTIME_KP4 ) { digit = '4'; }
                if( commands & TVTIME_KP5 ) { digit = '5'; }
                if( commands & TVTIME_KP6 ) { digit = '6'; }
                if( commands & TVTIME_KP7 ) { digit = '7'; }
                if( commands & TVTIME_KP8 ) { digit = '8'; }
                if( commands & TVTIME_KP9 ) { digit = '9'; }

                next_chan_buffer[ digit_counter ] = digit;
                digit_counter++;
                digit_counter %= 4;
                frame_counter = CHANNEL_DELAY;
            }
            if( commands & TVTIME_KP_ENTER ) {
                if( frame_counter ) {
                    if( *next_chan_buffer ) {
                        int found;

                        /* this sets chanindex accordingly */
                        found = frequencies_find_named_channel( next_chan_buffer );
                        if( found > -1 ) {
                            videoinput_set_tuner_freq( vidin, 
                                                       chanlist[ chanindex ].freq );
                            videohold = CHANNEL_HOLD;

                            if( verbose ) 
                                fprintf( stderr, 
                                         "tvtime: Changing to channel %s\n", 
                                         chanlist[ chanindex ].name );

                            osd_string_show_text( channel_number, 
                                                  chanlist[ chanindex ].name, 80 );
                            frame_counter = 0;
                        } else {
                            /* no valid channel */
                            frame_counter = 0;
                        }
                    }
                }
            }
        } /* DON'T PROCESS commands PAST HERE */

        /* Increment the frame counter if user is typing digits */
        if( frame_counter > 0 ) frame_counter--;
        if( frame_counter == 0 ) {
            memset( (void*)next_chan_buffer, 0, 5 );
            digit_counter = 0;
        }

        if( frame_counter > 0 && !(frame_counter % 5)) {
            char input_text[6];

            strcpy( input_text, next_chan_buffer );
            if( !(frame_counter % 10) )
                strcat( input_text, "_" );
            osd_string_show_text( channel_number, 
                                  input_text, CHANNEL_DELAY );
        }


        break;

    default:
        break;

    }

}

void input_next_frame( input_t *in )
{
}
