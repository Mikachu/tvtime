/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Static functions that are globally useful to tvtime.
 */

/**
 * Returns true if the file is openable for read.
 */
int file_is_openable_for_read( const char *filename );

/**
 * Attempts to create the given directory if it does not already
 * exist.  Returns 0 on a failure to create the directory, or if
 * it is not owned by uid / gid.
 */
int mkdir_and_force_owner( const char *path, uid_t uid, gid_t gid );

/**
 * Returns a string for the location of the given file, checking
 * paths that may be used before make install.  Returns 0 if the
 * file is not found, otherwise returns a new string that must be
 * freed using free().
 */
char *get_tvtime_file( const char *filename );

/**
 * Returns the standard path list for tvtime.
 */
const char *get_tvtime_paths( void );

/**
 * Returns the filename of the tvtime fifo.  This memory must be
 * freed by the caller using free().
 */
char *get_tvtime_fifo_filename( uid_t uid );

/**
 * Expands a pathname using wordexp.  This expands ~/foo
 * and ~username/foo to full paths.
 */
char *expand_user_path( const char *path );

/**
 * Returns a UCS4 (is this right?) character from a UTF-8 buffer.
 */
uint32_t utf8_to_unicode( const char *utf, int *len );

/* Modifiers */
#define I_SHIFT       (1<<16)
#define I_META        (1<<17)
#define I_CTRL        (1<<18)

/* Arrows + Home/End pad */
#define I_UP          273
#define I_DOWN        274
#define I_RIGHT       275
#define I_LEFT        276
#define I_INSERT      277
#define I_HOME        278
#define I_END         279
#define I_PGUP        280
#define I_PGDN        281

/* Function keys */
#define I_F1          282
#define I_F2          283
#define I_F3          284
#define I_F4          285
#define I_F5          286
#define I_F6          287
#define I_F7          288
#define I_F8          289
#define I_F9          290
#define I_F10         291
#define I_F11         292
#define I_F12         293
#define I_F13         294
#define I_F14         295
#define I_F15         296

/* Misc. */
#define I_BACKSPACE   8
#define I_ESCAPE      27
#define I_ENTER       13
#define I_PRINT       316
#define I_MENU        319

int input_string_to_special_key( const char *str );
const char *input_special_key_to_string( int key );

/**
 * Input commands for keymap
 */
enum tvtime_commands
{
    TVTIME_NOCOMMAND,
    TVTIME_QUIT,
    TVTIME_RESTART,
    TVTIME_CHANNEL_INC,
    TVTIME_CHANNEL_DEC,
    TVTIME_CHANNEL_PREV,
    TVTIME_TOGGLE_PAL_DK_AUDIO,
    TVTIME_TOGGLE_CHANNEL_PAL_DK,
    TVTIME_TOGGLE_COLOUR_INVERT,
    TVTIME_TOGGLE_MIRROR,
    TVTIME_TOGGLE_MUTE,
    TVTIME_MIXER_UP,
    TVTIME_MIXER_DOWN,
    TVTIME_MIXER_TOGGLE_MUTE,
    TVTIME_ENTER,
    TVTIME_CHANNEL_CHAR,
    TVTIME_TOGGLE_INPUT,
    TVTIME_HUE_DOWN,
    TVTIME_HUE_UP,
    TVTIME_BRIGHTNESS_DOWN,
    TVTIME_BRIGHTNESS_UP,
    TVTIME_CONTRAST_DOWN,
    TVTIME_CONTRAST_UP,
    TVTIME_SATURATION_DOWN,
    TVTIME_SATURATION_UP,
    TVTIME_FINETUNE_DOWN,
    TVTIME_FINETUNE_UP,
    TVTIME_TOGGLE_BARS,
    TVTIME_SHOW_STATS,
    TVTIME_SLEEP,
    TVTIME_SHOW_DEINTERLACER_INFO,
    TVTIME_TOGGLE_FULLSCREEN,
    TVTIME_TOGGLE_ASPECT,
    TVTIME_SCREENSHOT,
    TVTIME_TOGGLE_DEINTERLACER,
    TVTIME_TOGGLE_PULLDOWN_DETECTION,
    TVTIME_TOGGLE_XMLTV_LANGUAGE,
    TVTIME_SET_AUDIO_BOOST,
    TVTIME_SET_NORM,
    TVTIME_SET_DEINTERLACER,
    TVTIME_SET_FRAMERATE,
    TVTIME_SET_AUDIO_MODE,
    TVTIME_SET_INPUT_WIDTH,
    TVTIME_SET_MATTE,
    TVTIME_SET_FREQUENCY_TABLE,
    TVTIME_SET_FULLSCREEN_POSITION,
    TVTIME_SET_STATION,
    TVTIME_SET_XMLTV_LANGUAGE,
    TVTIME_MENUMODE,
    TVTIME_DISPLAY_INFO,
    TVTIME_TOGGLE_CREDITS,
    TVTIME_SAVE_PICTURE_GLOBAL,
    TVTIME_SAVE_PICTURE_CHANNEL,
    TVTIME_TOGGLE_NTSC_CABLE_MODE,
    TVTIME_AUTO_ADJUST_PICT,
    TVTIME_AUTO_ADJUST_WINDOW,
    TVTIME_CHANNEL_SKIP,
    TVTIME_TOGGLE_CC,
    TVTIME_TOGGLE_SIGNAL_DETECTION,
    TVTIME_PICTURE,
    TVTIME_PICTURE_UP,
    TVTIME_PICTURE_DOWN,
    TVTIME_RUN_COMMAND,
    TVTIME_TOGGLE_FRAMERATE,
    TVTIME_TOGGLE_AUDIO_MODE,
    TVTIME_CHANNEL_ACTIVATE_ALL,
    TVTIME_CHANNEL_SCAN,
    TVTIME_CHANNEL_RENUMBER,
    TVTIME_CHANNEL_SAVE_TUNING,
    TVTIME_OVERSCAN_UP,
    TVTIME_OVERSCAN_DOWN,
    TVTIME_CHANNEL_1,
    TVTIME_CHANNEL_2,
    TVTIME_CHANNEL_3,
    TVTIME_CHANNEL_4,
    TVTIME_CHANNEL_5,
    TVTIME_CHANNEL_6,
    TVTIME_CHANNEL_7,
    TVTIME_CHANNEL_8,
    TVTIME_CHANNEL_9,
    TVTIME_CHANNEL_0,
    TVTIME_TOGGLE_PAUSE,
    TVTIME_TOGGLE_ALWAYSONTOP,
    TVTIME_TOGGLE_MATTE,
    TVTIME_TOGGLE_PAL_SECAM,
    TVTIME_TOGGLE_CHROMA_KILL,
    TVTIME_TOGGLE_XDS,
    TVTIME_TOGGLE_QUIET_SCREENSHOTS,
    TVTIME_DISPLAY_MESSAGE,
    TVTIME_KEY_EVENT,

    TVTIME_UP,
    TVTIME_DOWN,
    TVTIME_LEFT,
    TVTIME_RIGHT,

    /* Everything below here is a menu-mode command. */
    TVTIME_MENU_UP,
    TVTIME_MENU_DOWN,
    TVTIME_MENU_BACK,
    TVTIME_MENU_ENTER,
    TVTIME_MENU_EXIT,
    TVTIME_SHOW_MENU,

    /* This is a 'menu-mode' command, but kinda not. */
    TVTIME_MOUSE_MOVE
};

enum framerate_mode
{
    FRAMERATE_FULL = 0,
    FRAMERATE_HALF_TFF = 1,
    FRAMERATE_HALF_BFF = 2,
    FRAMERATE_MAX = 3
};

int tvtime_string_to_command( const char *str );
const char *tvtime_command_to_string( int command );
int tvtime_num_commands( void );
const char *tvtime_get_command( int pos );
int tvtime_get_command_id( int pos );
int tvtime_is_menu_command( int command );
int tvtime_command_takes_arguments( int command );


void setup_i18n( void );
void setup_utf8( void );

int lfputs( const char *s, FILE *stream );
int lprintf( const char *format, ... );
int lfprintf( FILE *stream, const char *format, ... );

#ifdef __cplusplus
};
#endif
#endif /* UTILS_H_INCLUDED */
