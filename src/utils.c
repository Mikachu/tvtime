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
 *
 * Contains UTF-8 to unicode conversion from encoding.c in libxml2.

Except where otherwise noted in the source code (trio files, hash.c and list.c)
covered by a similar licence but with different Copyright notices:

 Copyright (C) 1998-2002 Daniel Veillard.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is fur-
nished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
DANIEL VEILLARD BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of Daniel Veillard shall not
be used in advertising or otherwise to promote the sale, use or other deal-
ings in this Software without prior written authorization from him.

 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
# include <locale.h>
# include <langinfo.h>
# include <iconv.h>
#else
# define _(string) string
#endif
#include "tvtimeconf.h"
#include "utils.h"

int file_is_openable_for_read( const char *filename )
{
    int fd;
    fd = open( filename, O_RDONLY );
    if( fd < 0 ) {
        return 0;
    } else {
        close( fd );
        return 1;
    }
}

static char *check_path( const char *path, const char *filename )
{
    char *cur;

    if( asprintf( &cur, "%s/%s", path, filename ) < 0 ) {
        /* Memory not available, so we're not getting anywhere. */
        return 0;
    } else if( !file_is_openable_for_read( cur ) ) {
        free( cur );
        return 0;
    }

    return cur;
}

const char *get_tvtime_paths( void )
{
#ifdef FONTDIR
    return FONTDIR ":" DATADIR ":../data:./data";
#else
    return DATADIR ":../data:./data";
#endif
}

int mkdir_and_force_owner( const char *path, uid_t uid, gid_t gid )
{
    if( mkdir( path, S_IRWXU ) < 0 ) {
        if( errno != EEXIST ) {
            lfprintf( stderr, _("Cannot create %s: %s\n"),
                      path, strerror( errno ) );
        } else {
            /* It exists, make sure it's a directory. */
            DIR *temp_dir = opendir( path );
            if( !path ) {
                lfprintf( stderr, _("Cannot open %s: %s\n"), 
                          path, strerror( errno ) );
            } else {
                closedir( temp_dir );
                return 1;
            }
        }
    } else {
        /* We created the directory, now force it to be owned by the user. */
        if( chown( path, uid, gid ) < 0 ) {
            lfprintf( stderr, _("Cannot change owner of %s: %s.\n"),
                      path, strerror( errno ) );
        } else {
            return 1;
        }
    }

    return 0;
}

char *get_tvtime_fifo_filename( uid_t uid )
{
    /* SUSv3 provides no guarantees to the length of a hostname.  It
     * specifies that HOST_NAME_MAX will give you the maximum number
     * of bytes, however, this is not defined on all platforms.
     */
#ifdef HOST_NAME_MAX
    size_t hostname_size = HOST_NAME_MAX;
#else
#ifdef MAXHOSTNAMELEN
    /* MAXHOSTNAMELEN is widely used on UNIX systems, but was never
     * formalised by POSIX.  Strangely enough, POSIX didn't adopt this
     * symbol in SUSv3.
     */
    size_t hostname_size = MAXHOSTNAMELEN;
#else
    size_t hostname_size = 255;
#endif /* MAXHOSTNAMELEN */
#endif /* HOST_NAME_MAX */
    char *hostname = 0;
    char *hostname_realloc = 0;

    struct passwd *pwuid = 0;
    char *fifodir;
    char *fifo;

    /* Create string for the directory in FIFODIR */
    pwuid = getpwuid( uid );
    if( pwuid ) {
        if( asprintf( &fifodir, FIFODIR "/.TV-%s", pwuid->pw_name ) < 0 ) {
            return 0;
        }
    } else {
        if( asprintf( &fifodir, FIFODIR "/.TV-%u", uid ) < 0 ) {
            return 0;
        }
    }

    /* Try to get a hostname */
    hostname = malloc( hostname_size * sizeof( char ) );
    if (hostname == 0)
        return 0;
    errno = 0;
    while( gethostname( hostname, hostname_size ) < 0 ) {
        if( errno == ENAMETOOLONG ) {
            hostname_size *= 2;
            hostname_realloc = realloc( hostname, hostname_size );
            if( !hostname_realloc ) {
                /* Use a partial hostname. */
                hostname_size /= 2;
                break;
            } else {
                hostname = hostname_realloc;
            }
        } else {
            /* Unknown error.  Put errno in the filename for interest. */
            snprintf( hostname, hostname_size, "unknown%d", errno );
            break;
        }
    }

    /* If we can't use our /tmp directory, put the fifo in $HOME. */
    if( !mkdir_and_force_owner( fifodir, uid, getgid() ) ) {
        if( asprintf( &fifo, "%s/.tvtime/tvtimefifo-%s",
                      getenv( "HOME" ), hostname ) < 0 ) {
            fifo = 0;
        }
    } else {
        if( asprintf( &fifo, "%s/tvtimefifo-%s", fifodir, hostname ) < 0 ) {
            fifo = 0;
        }
    }
    free( hostname );
    free( fifodir );
    return fifo;
}

char *get_tvtime_file( const char *filename )
{
    char *cur;

#ifdef FONTDIR
    /* If FONTDIR is defined, we'll look for files there first.
     * This is designed for distributions that already have a standard
     * FreeFont package installed. */
    cur = check_path( FONTDIR, filename );
    if( cur ) return cur;
#endif

    cur = check_path( DATADIR, filename );
    if( cur ) return cur;

    cur = check_path( "../data", filename );
    if( cur ) return cur;

    cur = check_path( "./data", filename );
    if( cur ) return cur;

    return 0;
}

char *expand_user_path( const char *path )
{
    wordexp_t result;
    char *ret = 0;

    /* Expand the string.  */
    switch( wordexp( path, &result, 0 ) ) {
        case 0:  /* Successful.  */
            break;
        case WRDE_NOSPACE:
            /**
             * If the error was `WRDE_NOSPACE',
             * then perhaps part of the result was allocated.
             */
            wordfree( &result );
        default: /* Some other error.  */
            return 0;
    }

    if( asprintf( &ret, "%s", result.we_wordv[ 0 ] ) < 0 ) {
        ret = 0;
    }

    wordfree( &result );
    return ret;
}

/**
 * xmlGetUTF8Char:
 *  utf:  a sequence of UTF-8 encoded bytes
 *  len:  a pointer to @bytes len
 *
 * Read one UTF8 Char from @utf
 * Returns the char value or -1 in case of error and update
 * len with the number of bytes used.
 */
uint32_t utf8_to_unicode( const char *utf, int *len )
{
    unsigned int c;
    
    if (utf == NULL)
   goto error; 
    if (len == NULL)
   goto error; 
    if (*len < 1)
   goto error; 
   
    c = utf[0];
    if (c & 0x80) {
   if (*len < 2)
       goto error;
   if ((utf[1] & 0xc0) != 0x80)
       goto error;
   if ((c & 0xe0) == 0xe0) {
       if (*len < 3) 
      goto error; 
       if ((utf[2] & 0xc0) != 0x80)
      goto error;
       if ((c & 0xf0) == 0xf0) {
      if (*len < 4)
          goto error;
      if ((c & 0xf8) != 0xf0 || (utf[3] & 0xc0) != 0x80)
          goto error;
      *len = 4;
      /* 4-byte code */
      c = (utf[0] & 0x7) << 18;
      c |= (utf[1] & 0x3f) << 12;
      c |= (utf[2] & 0x3f) << 6;
      c |= utf[3] & 0x3f;
       } else {
         /* 3-byte code */
      *len = 3;
      c = (utf[0] & 0xf) << 12;
      c |= (utf[1] & 0x3f) << 6;
      c |= utf[2] & 0x3f;
       }
   } else {
     /* 2-byte code */
       *len = 2;
       c = (utf[0] & 0x1f) << 6;
       c |= utf[1] & 0x3f;
   }
    } else {
   /* 1-byte code */
   *len = 1;
    }
    return c;

error:
    *len = 0;
    return 0;
}

/* Key names. */
typedef struct key_name_s key_name_t;
struct key_name_s
{
    char *name;
    int key;
};

static key_name_t key_names[] = {
    { "Up", I_UP },
    { "Down", I_DOWN },
    { "Left", I_LEFT },
    { "Right", I_RIGHT },
    { "Insert", I_INSERT },
    { "Home", I_HOME },
    { "End", I_END },
    { "pgup", I_PGUP },
    { "pgdn", I_PGDN },
    { "pg up", I_PGUP },
    { "pg dn", I_PGDN },
    { "PageUp", I_PGUP },
    { "PageDown", I_PGDN },
    { "Page Up", I_PGUP },
    { "Page Down", I_PGDN },
    { "F1", I_F1 },
    { "F2", I_F2 },
    { "F3", I_F3 },
    { "F4", I_F4 },
    { "F5", I_F5 },
    { "F6", I_F6 },
    { "F7", I_F7 },
    { "F8", I_F8 },
    { "F9", I_F9 },
    { "F10", I_F10 },
    { "F11", I_F11 },
    { "F12", I_F12 },
    { "F13", I_F13 },
    { "F14", I_F14 },
    { "F15", I_F15 },
    { "Backspace", I_BACKSPACE },
    { "bs", I_BACKSPACE },
    { "Del", I_BACKSPACE },
    { "Delete", I_BACKSPACE },
    { "Escape", I_ESCAPE },
    { "Esc", I_ESCAPE },
    { "Enter", I_ENTER },
    { "Print", I_PRINT },
    { "Menu", I_MENU },
    { "Tab", '\t' },
    { 0, 0 }
};


int input_string_to_special_key( const char *str )
{
    int count;

    for( count = 0; key_names[ count ].name; count++ ) {
        if( !strcasecmp( str, key_names[ count ].name ) ) {
            return key_names[ count ].key;
        }
    }

    return 0;
}

const char *input_special_key_to_string( int key )
{
    int count;

    for( count = 0; key_names[ count ].name; count++ ) {
        if( key == key_names[ count ].key ) {
            return key_names[ count ].name;
        }
    }

    return 0;
}

typedef struct command_names_s {
    const char *name;
    int command;
} command_names_t;

static command_names_t command_table[] = {

    { "AUTO_ADJUST_PICT", TVTIME_AUTO_ADJUST_PICT },
    { "AUTO_ADJUST_WINDOW", TVTIME_AUTO_ADJUST_WINDOW },

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

    { "CHANNEL_ACTIVATE_ALL", TVTIME_CHANNEL_ACTIVATE_ALL },
    { "CHANNEL_DEC", TVTIME_CHANNEL_DEC },
    { "CHANNEL_DOWN", TVTIME_CHANNEL_DEC },
    { "CHANNEL_INC", TVTIME_CHANNEL_INC },
    { "CHANNEL_JUMP", TVTIME_CHANNEL_PREV },
    { "CHANNEL_PREV", TVTIME_CHANNEL_PREV },
    { "CHANNEL_RENUMBER", TVTIME_CHANNEL_RENUMBER },
    { "CHANNEL_SAVE_TUNING", TVTIME_CHANNEL_SAVE_TUNING },
    { "CHANNEL_SCAN", TVTIME_CHANNEL_SCAN },
    { "CHANNEL_SKIP", TVTIME_CHANNEL_SKIP },
    { "CHANNEL_UP", TVTIME_CHANNEL_INC },

    { "COLOR_DOWN", TVTIME_SATURATION_DOWN },
    { "COLOR_UP", TVTIME_SATURATION_UP },

    { "COLOUR_DOWN", TVTIME_SATURATION_DOWN },
    { "COLOUR_UP", TVTIME_SATURATION_UP },

    { "CONTRAST_DOWN", TVTIME_CONTRAST_DOWN },
    { "CONTRAST_UP", TVTIME_CONTRAST_UP },

    { "DISPLAY_INFO", TVTIME_DISPLAY_INFO },
    { "DISPLAY_MESSAGE", TVTIME_DISPLAY_MESSAGE },

    { "DOWN", TVTIME_DOWN },

    { "ENTER", TVTIME_ENTER },

    { "FINETUNE_DOWN", TVTIME_FINETUNE_DOWN },
    { "FINETUNE_UP", TVTIME_FINETUNE_UP },

    { "HUE_DOWN", TVTIME_HUE_DOWN },
    { "HUE_UP", TVTIME_HUE_UP },

    { "KEY_EVENT", TVTIME_KEY_EVENT },

    { "LEFT", TVTIME_LEFT },

    { "MENU_DOWN", TVTIME_MENU_DOWN },
    { "MENU_ENTER", TVTIME_MENU_ENTER },
    { "MENU_EXIT", TVTIME_MENU_EXIT },
    { "MENU_BACK", TVTIME_MENU_BACK },
    { "MENU_UP", TVTIME_MENU_UP },

    { "MIXER_DOWN", TVTIME_MIXER_DOWN },
    { "MIXER_TOGGLE_MUTE", TVTIME_MIXER_TOGGLE_MUTE },
    { "MIXER_UP", TVTIME_MIXER_UP },

    { "NOOP", TVTIME_NOCOMMAND },

    { "OVERSCAN_DOWN", TVTIME_OVERSCAN_DOWN },
    { "OVERSCAN_UP", TVTIME_OVERSCAN_UP },

    { "PICTURE", TVTIME_PICTURE },
    { "PICTURE_UP", TVTIME_PICTURE_UP },
    { "PICTURE_DOWN", TVTIME_PICTURE_DOWN },

    { "RESTART", TVTIME_RESTART },
    { "RIGHT", TVTIME_RIGHT },
    { "RUN_COMMAND", TVTIME_RUN_COMMAND },

    { "SATURATION_DOWN", TVTIME_SATURATION_DOWN },
    { "SATURATION_UP", TVTIME_SATURATION_UP },

    { "SAVE_PICTURE_GLOBAL", TVTIME_SAVE_PICTURE_GLOBAL },
    { "SAVE_PICTURE_CHANNEL", TVTIME_SAVE_PICTURE_CHANNEL },

    { "SCREENSHOT", TVTIME_SCREENSHOT },

    { "SET_AUDIO_BOOST", TVTIME_SET_AUDIO_BOOST },
    { "SET_AUDIO_MODE", TVTIME_SET_AUDIO_MODE },
    { "SET_DEINTERLACER", TVTIME_SET_DEINTERLACER },
    { "SET_FRAMERATE", TVTIME_SET_FRAMERATE },
    { "SET_FREQUENCY_TABLE", TVTIME_SET_FREQUENCY_TABLE },
    { "SET_FULLSCREEN_POSITION", TVTIME_SET_FULLSCREEN_POSITION },
    { "SET_INPUT_WIDTH", TVTIME_SET_INPUT_WIDTH },
    { "SET_MATTE", TVTIME_SET_MATTE },
    { "SET_NORM", TVTIME_SET_NORM },
    { "SET_STATION", TVTIME_SET_STATION },
    { "SET_XMLTV_LANGUAGE", TVTIME_SET_XMLTV_LANGUAGE },

    { "SHOW_DEINTERLACER_INFO", TVTIME_SHOW_DEINTERLACER_INFO },
    { "SHOW_MENU", TVTIME_SHOW_MENU },
    { "SHOW_STATS", TVTIME_SHOW_STATS },

    { "SLEEP", TVTIME_SLEEP },

    { "TOGGLE_ALWAYSONTOP", TVTIME_TOGGLE_ALWAYSONTOP },
    { "TOGGLE_ASPECT", TVTIME_TOGGLE_ASPECT },
    { "TOGGLE_AUDIO_MODE", TVTIME_TOGGLE_AUDIO_MODE },
    { "TOGGLE_BARS", TVTIME_TOGGLE_BARS },
    { "TOGGLE_CC", TVTIME_TOGGLE_CC },
    { "TOGGLE_CHANNEL_PAL_DK", TVTIME_TOGGLE_CHANNEL_PAL_DK },
    { "TOGGLE_CHROMA_KILL", TVTIME_TOGGLE_CHROMA_KILL },
    { "TOGGLE_COLOR_INVERT", TVTIME_TOGGLE_COLOUR_INVERT },
    { "TOGGLE_COLOUR_INVERT", TVTIME_TOGGLE_COLOUR_INVERT },
    { "TOGGLE_DEINTERLACER", TVTIME_TOGGLE_DEINTERLACER },
    { "TOGGLE_FULLSCREEN", TVTIME_TOGGLE_FULLSCREEN },
    { "TOGGLE_FRAMERATE", TVTIME_TOGGLE_FRAMERATE },
    { "TOGGLE_INPUT", TVTIME_TOGGLE_INPUT },
    { "TOGGLE_MATTE", TVTIME_TOGGLE_MATTE },
    { "TOGGLE_MIRROR", TVTIME_TOGGLE_MIRROR },
    { "TOGGLE_MUTE", TVTIME_TOGGLE_MUTE },
    { "TOGGLE_NTSC_CABLE_MODE", TVTIME_TOGGLE_NTSC_CABLE_MODE },
    { "TOGGLE_PAL_DK_AUDIO", TVTIME_TOGGLE_PAL_DK_AUDIO },
    { "TOGGLE_PAL_SECAM", TVTIME_TOGGLE_PAL_SECAM },
    { "TOGGLE_PAUSE", TVTIME_TOGGLE_PAUSE },
    { "TOGGLE_PULLDOWN_DETECTION", TVTIME_TOGGLE_PULLDOWN_DETECTION },
    { "TOGGLE_QUIET_SCREENSHOTS", TVTIME_TOGGLE_QUIET_SCREENSHOTS },
    { "TOGGLE_SIGNAL_DETECTION", TVTIME_TOGGLE_SIGNAL_DETECTION },
    { "TOGGLE_XDS", TVTIME_TOGGLE_XDS },
    { "TOGGLE_XMLTV_LANGUAGE", TVTIME_TOGGLE_XMLTV_LANGUAGE },

    { "UP", TVTIME_UP },

    { "QUIT", TVTIME_QUIT },
};

int tvtime_num_commands( void )
{
    return ( sizeof( command_table ) / sizeof( command_names_t ) );
}

const char *tvtime_get_command( int pos )
{
    return command_table[ pos ].name;
}

int tvtime_get_command_id( int pos )
{
    return command_table[ pos ].command;
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

int tvtime_is_menu_command( int command )
{
    return (command >= TVTIME_MENU_UP);
}

int tvtime_command_takes_arguments( int command )
{
    return (command == TVTIME_DISPLAY_MESSAGE || command == TVTIME_SCREENSHOT ||
            command == TVTIME_KEY_EVENT || command == TVTIME_SET_DEINTERLACER ||
            command == TVTIME_SHOW_MENU || command == TVTIME_SET_FRAMERATE ||
            command == TVTIME_SET_AUDIO_MODE || command == TVTIME_SET_INPUT_WIDTH ||
            command == TVTIME_SET_MATTE || command == TVTIME_SET_FREQUENCY_TABLE ||
            command == TVTIME_RUN_COMMAND || command == TVTIME_SET_FULLSCREEN_POSITION ||
            command == TVTIME_MIXER_UP || command == TVTIME_MIXER_DOWN ||
            command == TVTIME_SET_AUDIO_BOOST || command == TVTIME_SET_STATION ||
            command == TVTIME_SET_XMLTV_LANGUAGE);
}

#ifdef ENABLE_NLS
static iconv_t cd = 0;
#endif

void setup_i18n( void )
{
#ifdef ENABLE_NLS
    setlocale( LC_ALL, "" );
    bindtextdomain( "tvtime", LOCALEDIR );
    textdomain( "tvtime" );
#endif
}

void setup_utf8( void )
{
#ifdef ENABLE_NLS
    char *mycodeset = nl_langinfo( CODESET );

    /**
     * Setup conversion descriptor if user's console is non-UTF-8. Otherwise
     * we can just leave cd as NULL and allow lprintf/lputs & co to short-
     * circuit.
     */
    if( strcmp( mycodeset, "UTF-8" ) ) {
        char *codeset;
        char *errfmt;

        cd = iconv_open( mycodeset, "UTF-8" );
        if( cd == (iconv_t) (-1) ) {
            /**
             * This error message is displayed using fprintf, NOT LFPRINTF, 
             * since we have not yet called bind_textdomain_codeset, so
             * gettext is still returning in the user's locale charset.
             */
            fprintf( stderr, _("Failed to initialize UTF-8 to %s converter: "
                     "iconv_open failed (%s).\n"),
                     mycodeset, strerror( errno ) );
            cd = 0; /* (iconv_t) (-1) is retarded. */
        }

        /**
         * We're calling gettext in advance here, in case
         * bind_textdomain_codeset leaves gettext in an
         * undefined state.
         */
        errfmt = _("\n"
        "    Failed to enter UTF-8 mode using bind_textdomain_codeset()\n"
        "    (returned %s.)  This may cause messages\n"
        "    to be displayed incorrectly!  Please report this bug at\n"
        "    %s.\n\n");

        codeset = bind_textdomain_codeset( "tvtime", "UTF-8" );
        if( !codeset || strcmp( codeset, "UTF-8" ) ) {
            /**
             * We do not die here, since if the user has an improperly set up
             * locale but still wants to display messages in English, this will
             * allow the program to function. If this happens, it's probably a
             * bug, and we'll want to hear about it.
             *
             * Also, we're using fprintf() here and not lfprintf() since we
             * called gettext() prior to bind_textdomain_codeset().
             */
            fprintf( stderr, errfmt, codeset, PACKAGE_BUGREPORT );
        }
    }
#endif
}

int lfputs( const char *s, FILE *stream )
{
#ifdef ENABLE_NLS
    /* iconv wants a char ** even though the argument is not modified. */
    char *inbuf = (char *)s;
    size_t inbytesleft = strlen( s );
    static char outbufstart[BUFSIZ];
    char *outbuf = outbufstart;
    size_t outbytesleft = BUFSIZ;
    int ret;
    int nonreversible = 0;
    
    /* conversion not neccecary. save our time. */
    if( !cd ) {
        return fputs( s, stream );
    }
    
    while( inbytesleft > 0 ) {
        ret = iconv( cd, &inbuf, &inbytesleft, &outbuf, &outbytesleft );
        if( ret > 0 )
            nonreversible += ret;
        else if( ret < 0 ) {
            switch( errno ) {
            case E2BIG: /* Insufficient room at *outbuf */
                /* Recoverable, and indeed expected! We just
                   flush the buffers and keep on going. */
                /* Flush the output buffer */
                fwrite( outbufstart, sizeof( *outbuf ),
                        BUFSIZ - outbytesleft, stream );
                /* ... and rewind it */
                outbuf = outbufstart;
                outbytesleft = BUFSIZ;
                break;
            case EILSEQ: /* Invalid multibyte sequence */
            case EINVAL: /* Incomplete multibyte sequence */
                /* Flush the output buffer */
                fwrite( outbufstart, sizeof( *outbuf ),
                        BUFSIZ - outbytesleft, stream );
                /* ... and rewind it */
                outbuf = outbufstart;
                outbytesleft = BUFSIZ;

                /* To my knowledge, we cannot recover here,
                   we must truncate the string. */
                fputs( "[Truncated: Illegal sequence]\n", stream );
                return EOF;
            }
        }
    }
    /* inbytesleft == 0 */
    /* Flush the output buffer */
    fwrite( outbufstart, sizeof( *outbuf ), BUFSIZ - outbytesleft, stream );
    /* ... and rewind it */
    outbuf = outbufstart;
    outbytesleft = BUFSIZ;

    return nonreversible;
#else /* no ENABLE_NLS */
    return fputs( s, stream );
#endif
}

static int lvfprintf( FILE *stream, const char *format, va_list ap )
{
#ifdef ENABLE_NLS
    static char str[4096];
    int ret = -1;
    
    /* conversion not neccecary. save our time. */
    if( !cd ) {
        return vfprintf( stream, format, ap );
    }
    
    ret = vsnprintf( str, sizeof str, format, ap );
    
    if( lfputs( str, stream ) == EOF ) {
        ret = -1;
    }

    /* else, ret remains as the return value from vsnprintf () */
    return ret;
#else /* no ENABLE_NLS */
    return vfprintf( stream, format, ap );
#endif
}

static int lvprintf( const char *format, va_list ap )
{
    return lvfprintf( stdout, format, ap );
}

int lprintf( const char *format, ... )
{
    int ret;
    va_list ap;
    va_start( ap, format );
    ret = lvprintf( format, ap );
    va_end( ap );
    return ret;
}

int lfprintf( FILE *stream, const char *format, ... )
{
    int ret;
    va_list ap;
    va_start( ap, format );
    ret = lvfprintf( stream, format, ap );
    va_end( ap );
    return ret;
}

