/**
 * Copyright (C) 2001, 2002, 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef VIDEOINPUT_H_INCLUDED
#define VIDEOINPUT_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This object represents a video4linux capture device.  Frames are
 * returned as packed Y'CbCr image maps with 4:2:2 encoding.  The scheme
 * used is also known as 'YUY2'.  Currently we only deal in frames and
 * assume that they are interlaced, and we only deal with sources that
 * can provide full frame height: 480 scanlines for NTSC, 576 for PAL.
 *
 * Note: it's arguable that we should try and get 486 scanlines for
 * NTSC.  However, since the bt8x8 only gives 480 scanlines, and this is
 * standard sampling for standards like MPEG-2 etc, I'll stick with 480
 * for now.  As long as I get an interlaced frame, everything is happy.
 */
typedef struct videoinput_s videoinput_t;

/**
 * Possible TV norms we support.
 */
#define VIDEOINPUT_NTSC    0
#define VIDEOINPUT_PAL     1
#define VIDEOINPUT_SECAM   2
#define VIDEOINPUT_PAL_NC  3
#define VIDEOINPUT_PAL_M   4
#define VIDEOINPUT_PAL_N   5
#define VIDEOINPUT_NTSC_JP 6
#define VIDEOINPUT_PAL_60  7

/** 
 * Possible tuner signal states.
 */
#define TUNER_STATE_HAS_SIGNAL      0
#define TUNER_STATE_SIGNAL_DETECTED 1
#define TUNER_STATE_SIGNAL_LOST     2
#define TUNER_STATE_NO_SIGNAL       3

/**
 * Possible audio modes.
 */
#define VIDEOINPUT_MONO    1
#define VIDEOINPUT_STEREO  2

/**
 * Returns a text version of the norm.
 */
const char *videoinput_get_norm_name( int norm );

/**
 * Returns the integer version of the norm from its name.
 */
int videoinput_get_norm_number( const char *name );

/**
 * Returns a text version of the audio mode.
 */
const char *videoinput_get_audio_mode_name( int mode );

/**
 * Create a new input device from the given device name and which input
 * number (cable, composite1, composite2, etc) to use.
 *
 * The capwidth provided is how many samples to ask for per scanline.
 * The height will always be full frame of 480 or 576.
 *
 * The verbose flag indicates we should print to stderr when things go
 * bad, and maybe for some helpful information messages.
 */
videoinput_t *videoinput_new( const char *v4l_device, int capwidth,
                              int norm, int verbose );

/**
 * Shut down the capture device.
 */
void videoinput_delete( videoinput_t *vidin );

/**
 * Returns the width of the input images.
 */
int videoinput_get_width( videoinput_t *vidin );

/**
 * Returns the height of the input images.
 */
int videoinput_get_height( videoinput_t *vidin );

/**
 * Returns the current TV norm.
 */
int videoinput_get_norm( videoinput_t *vidin );

/**
 * Returns true if this input is a BT8x8-based card.
 */
int videoinput_is_bttv( videoinput_t *vidin );

/**
 * Returns a pointer to the next image buffer.  Also returns the last field
 * number captured.
 */
uint8_t *videoinput_next_frame( videoinput_t *vidin, int *frameid );

/**
 * Signal to the videoinput device that we're done reading the last frame, to
 * allow the hardware to use the buffer.
 */
void videoinput_free_frame( videoinput_t *vidin, int frameid );

int videoinput_get_numframes( videoinput_t *vidin );

/**
 * Returns true if this input has a tuner.
 */
int videoinput_has_tuner( videoinput_t *vidin );

/**
 * Controls...
 */
int videoinput_get_hue( videoinput_t *vidin );
void videoinput_set_hue( videoinput_t *vidin, int newhue );
void videoinput_set_hue_relative( videoinput_t *vidin, int offset );
int videoinput_get_brightness( videoinput_t *vidin );
void videoinput_set_brightness( videoinput_t *vidin, int newbright );
void videoinput_set_brightness_relative( videoinput_t *vidin, int offset );
int videoinput_get_contrast( videoinput_t *vidin );
void videoinput_set_contrast( videoinput_t *vidin, int newcont );
void videoinput_set_contrast_relative( videoinput_t *vidin, int offset );
int videoinput_get_colour( videoinput_t *vidin );
void videoinput_set_colour( videoinput_t *vidin, int newcolour );
void videoinput_set_colour_relative( videoinput_t *vidin, int offset );

void videoinput_reset_default_settings( videoinput_t *vidin );

/**
 * Sets the frequency to tune in to. (ie. the station to watch)
 * Since frequencies.c specifies all freqs in KHz, we take that as input.
 * If the card expects frequency in MHz, this function handles that.
 */
void videoinput_set_tuner_freq( videoinput_t *vidin, int freqKHz );

/**
 * Returns the currently tuned frequency in KHz
 */
int videoinput_get_tuner_freq( videoinput_t *vidin );

/**
 * Returns 1 if the frequency tuned to is a decent picture
 */
int videoinput_freq_present( videoinput_t *vidin );

/**
 * Returns how many inputs are available.
 */
int videoinput_get_num_inputs( videoinput_t *vidin );

/**
 * Returns the current input number.
 */
int videoinput_get_input_num( videoinput_t *vidin );

/**
 * Returns the name of the current input.
 */
const char *videoinput_get_input_name( videoinput_t *vidin );

/**
 * Returns the current audio mode.
 */
int videoinput_get_audio_mode( videoinput_t *vidin );

/**
 * Sets the current audio mode.
 */
void videoinput_set_audio_mode( videoinput_t *vidin, int mode );

/**
 * Mute the mixer if mute != 0
 */
void videoinput_mute( videoinput_t *vidin, int mute );

/**
 * Returns whether the volume is muted.
 */
int videoinput_get_muted( videoinput_t *vidin );

/**
 * Sets the current input.
 */
void videoinput_set_input_num( videoinput_t *vidin, int inputnum );

/**
 * Returns the current tuner state.
 */
int videoinput_check_for_signal( videoinput_t *vidin, int check_freq_present );

/**
 * Switches to the next 'compatible' norm.
 */
void videoinput_switch_to_next_compatible_norm( videoinput_t *vidin );

/**
 * Switches to a specific norm.
 */
void videoinput_switch_to_compatible_norm( videoinput_t *vidin, int norm );

#ifdef __cplusplus
};
#endif
#endif /* VIDEOINPUT_H_INCLUDED */
