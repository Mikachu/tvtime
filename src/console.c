/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "osdtools.h"
#include "console.h"
#include "speedy.h"

/* stupid glibc bug i guess */
int vfscanf( FILE *stream, const char *format, va_list ap );

typedef struct console_coords_s console_coords_t;

struct console_coords_s
{
    int x;
    int y;
};

struct console_s
{
    osd_font_t *font;
    char *text;
    osd_string_t **line;
    console_coords_t *coords;

    int timeout;  /* fade delay */
    
    osd_graphic_t *bgpic;

    unsigned int fgcolour;
    unsigned int bgcolour;
    int bg_luma, bg_cb, bg_cr;

    int frame_width;  
    int frame_height;

    int x, y; /* where to draw console */
    int width, height;  /* the size box we have to draw in */
    
    int num_lines;

    int curx, cury; /* cursor position */
    int rows, cols;

    FILE *in, *out, *err;

    int visible;
    char *fontfile;
    int fontsize;

    int first_line;
    int dropdown;
    int drop_pos;

    int show_cursor;

    int maxrows;
};

console_t *console_new( int x, int y, int width, int height,
                        int fontsize, int video_width, int video_height, 
                        double pixel_aspect, unsigned int fgcolour )
{
    int i=0, rowheight;
    console_t *con = malloc( sizeof( console_t ) );

    if( !con ) {
        return NULL;
    }

    con->x = x;
    con->y = y;
    con->frame_width = video_width;
    con->frame_height = video_height;
    con->curx = 0;
    con->cury = 0;
    con->timeout = 51;
    con->fgcolour = fgcolour;
    con->bgcolour = 0x000000; /* black */
    con->bg_luma = 16;
    con->bg_cb = 128;
    con->bg_cr = 128;
    con->bgpic = NULL;
    con->in = stdin;
    con->out = stdout;
    con->err = stderr;
    con->visible = 0;
    con->rows = 0;
    con->cols = 0;
    con->text = NULL;
    con->line = NULL;
    con->first_line = 0;
    con->num_lines = 0;
    con->coords = NULL;
    con->show_cursor = 1;
    con->drop_pos = 0;
    con->font = 0;

    /*
    con->text = malloc( con->rows * con->cols ); 
    if( !con->text ) {
        console_delete( con );
        return 0;
    }
    */

    con->line = malloc( 1 * sizeof( osd_string_t * ) );
    if( !con->line ) {
        console_delete( con );
        return 0;
    }

    con->fontfile = "FreeMonoBold.ttf";
    con->fontsize = fontsize;

    con->font = osd_font_new( con->fontfile, fontsize, pixel_aspect );
    if( !con->font ) {
        console_delete( con );
        return 0;
    }

    /*
    memset( con->text, 0, con->cols * con->rows );
    */

    con->width = width;
    con->height = height;

    con->line[0] = osd_string_new( con->font );
    if( !con->line[0] ) {
        fprintf( stderr, "console: Could not create string.\n" );
        console_delete( con );
        return 0;
    }

    osd_string_show_text( con->line[0], "H", 0 );

    rowheight = osd_string_get_height( con->line[0] );
    con->rows = (double)height / (double)rowheight - 0.5;

    osd_string_delete( con->line[0] );
    
    free( con->line );
    con->line = 0;

    con->coords = malloc( con->rows * sizeof( console_coords_t ) );
    if( !con->coords ) {
        console_delete( con );
        return 0;
    }


    for( i = 0; i < con->rows; i++ ) {
        int tmp;

        con->coords[ i ].x = (con->x + video_width / 100) & ~1 ;

        if( i == 0 ) {
            tmp = con->y + ((double)height - (double)rowheight * (double)con->rows) / (double)2;
        } else {
            tmp = con->coords[ i-1 ].y + rowheight;
        }

        con->coords[ i ].y = tmp;
    }

    if( !con->line ) {
        con->line = malloc( sizeof( osd_string_t * ) );
        if( !con->line ) {
            fprintf( stderr, "console: Couldn't add a line.\n" );

            console_delete( con );
            return NULL;
        }
        con->line[0] = osd_string_new( con->font );
        if( !con->line[ 0 ] ) {
            fprintf( stderr, 
                     "console: Could not create new string.\n" );
            console_delete( con );
            return NULL;
        }
        osd_string_set_colour_rgb( con->line[ 0 ], 
                                   (con->fgcolour >> 16) & 0xff, 
                                   (con->fgcolour >> 8) & 0xff, 
                                   (con->fgcolour & 0xff) );

        osd_string_show_text( con->line[ 0 ], "_", 51 );
        con->num_lines++;
    }

    con->maxrows = con->rows;

    return con;
}


void console_delete( console_t *con )
{
    int i = 0;

    if( con->line ) {
        for( i = 0; i < con->num_lines; i++ ) {
            if( con->line[ i ] ) {
                osd_string_delete( con->line[ i ] );
            }
        }
        free( con->line );
    }

    if( con->font ) osd_font_delete( con->font );

    if( con->text ) free( con->text );
    if( con->coords ) free( con->coords );

    if( con->in != stdin ) fclose( con->in );
    if( con->out != stdout ) fclose( con->out );

    free( con );
}

void update_osd_strings( console_t *con )
{
    char *ptr;
    char tmpstr[1024];
    int maxwidth = con->width - ( ( con->frame_width * 1 ) / 100 );
    int needtoscroll = 0;

    if( con->cury <= con->first_line + con->rows ) {
        needtoscroll = 1;
    }

    ptr = con->text + con->curx;
    tmpstr[0] = '\0';

    if( !con->line && con->maxrows ) {
        con->line = malloc( sizeof( osd_string_t * ) );
        if( !con->line ) {
            fprintf( stderr, "console: Couldn't add a line.\n" );

            return;
        }
        con->line[0] = osd_string_new( con->font );
        if( !con->line[ 0 ] ) {
            fprintf( stderr, "console: Could not create new string.\n" );
            return;
        }
        osd_string_set_colour_rgb( con->line[ 0 ], 
                                   (con->fgcolour >> 16) & 0xff, 
                                   (con->fgcolour >> 8) & 0xff, 
                                   (con->fgcolour & 0xff) );

        osd_string_show_text( con->line[ 0 ], " ", 51 );
        con->num_lines++;
    }

    for(;;) {

        if( !*ptr ) break;

        if( *ptr == '\b' ) {
            if( con->curx == ptr - con->text  && ptr != con->text ) {
                /* can't delete past current line */
                *ptr = '\0';
                strcat( con->text, ptr+1 );
                con->cols--;
                continue;
            } else if( ptr == con->text ) {
                con->text[0] = '\0';
                strcat( con->text, ptr+1 );
                con->cols--;
                continue;
            }
            tmpstr[0] = '\0';
            snprintf( tmpstr, ptr - con->text - con->curx, 
                      con->text + con->curx );
            tmpstr[ ptr - con->text - con->curx ] = '\0';
            osd_string_show_text( con->line[ con->cury ], tmpstr, 51 );
            *ptr = '\0';
            *(ptr-1) = '\0';
            strcat( con->text + con->curx, ptr );
            con->cols -= 2;
            continue;
        }

        if( *ptr != '\n' ) {
            char blah[2];
            if( !isprint( *ptr ) ) *ptr = ' ';
            blah[0] = *ptr;
            blah[1] = '\0';
            strcat( tmpstr, blah );
            osd_string_set_colour_rgb( con->line[ con->cury ], 
                                       (con->fgcolour >> 16) & 0xff, 
                                       (con->fgcolour >> 8) & 0xff, 
                                       (con->fgcolour & 0xff) );

            osd_string_show_text( con->line[ con->cury ], tmpstr, 51 );
        }
        
        if( *ptr == '\n' 
            || osd_string_get_width( con->line[ con->cury ] ) > maxwidth ) {
            if( con->line && con->cury < con->maxrows-1 ) {
                osd_string_t **hmm;
                hmm = (osd_string_t **)realloc( (void *)con->line, sizeof(osd_string_t*)*(con->num_lines+1) );
                if( hmm ) {
                    con->num_lines++;
                    con->line = hmm;
                } else {
                    fprintf( stderr, "console: Couldn't add another line.\n" );
                    return;
                }
            }

            con->curx += strlen( tmpstr );
            if( *ptr == '\n' ) con->curx++;
            if( strlen( tmpstr ) != 0 )
                tmpstr[ strlen(tmpstr) ] = '\0';
            else {
                tmpstr[0] = ' ';
                tmpstr[1] = '\0';
            }
            osd_string_set_colour_rgb( con->line[ con->cury ], 
                                       (con->fgcolour >> 16) & 0xff, 
                                       (con->fgcolour >> 8) & 0xff, 
                                       (con->fgcolour & 0xff) );

            osd_string_show_text( con->line[ con->cury ], tmpstr, 51 );
            if( con->line && con->cury >= con->maxrows-1 ) {
                int i;
                osd_string_delete( con->line[ 0 ] );
                for( i = 0; i < con->num_lines-1; i++ ) {
                    con->line[ i ] = con->line[ i + 1 ];
                }
                con->cury--;
            }
            con->cury++;
            tmpstr[0] = '\0';

            con->line[ con->cury ] = osd_string_new( con->font );
            if( !con->line[ con->cury ] ) {
                fprintf( stderr, "console: Could not create new string.\n" );
                con->cury--;
                return;
            }
            osd_string_set_colour_rgb( con->line[ con->cury ], 
                                       (con->fgcolour >> 16) & 0xff, 
                                       (con->fgcolour >> 8) & 0xff, 
                                       (con->fgcolour & 0xff) );

            osd_string_show_text( con->line[ con->cury ], " ", 51 );
        }
        ptr++;
    }

    if( con->show_cursor ) { 
        strcat( tmpstr, "_" );
        osd_string_show_text( con->line[ con->cury ], tmpstr, 51 );
    }
    if( needtoscroll && con->cury >= con->first_line + con->rows ) {
        con->first_line = con->cury - con->rows + 1;
        if( con->first_line < 0 ) {
            con->first_line = 0;
        }
    }
}


void console_printf( console_t *con, char *format, ... )
{
    va_list ap;
    int n=0, size;
    char *str = NULL;
    
    size = strlen( format ) * 3;

    va_start( ap, format );
    str = malloc( size );
    if( !str ) return;
    n = vsnprintf( str, size-1, format, ap );
    if( n > size ) {
        free( str );
        size = n+1;
        str = malloc( size );
        if( !str ) return;
        n = vsnprintf( str, size, format, ap );
        if ( n > size ) {
            free( str );
            return;
        }
    }
    va_end( ap );

#if 0
    ptr = str;
    while( ptr && *ptr ) {
        int newline;

        newline=0;

        if( con->cury == con->rows ) {
            /* scroll everything up */
            int j=0, k=0;

            con->cury--;
            con->curx = 0;
            
            for( j=0; j < con->rows-1; j++ ) {
                for( k=0; k < con->cols-1; k++ ) {
                    con->text[ k + j*con->cols ] = con->text[ k + (j+1)*con->cols ];
                }
                con->text[ con->cols-1 + j*con->cols ] = '\0';
            }

            /* clear last line */
            for( j=0; j < con->cols-1; j++ ) {
                con->text[ j + (con->rows-1)*con->cols ] = ' ';
            }
            con->text[ con->cols-1 + (con->rows-1)*con->cols ] = '\0';
        }

        switch( *ptr ) {
        case '\n':
            newline = 1;
            break;

        default:
            if( !isprint( *ptr ) ) {
                *ptr = ' ';
            }
            break;
        }

        if( !newline ) {
            con->text[ con->curx + con->cury * con->cols ] = *ptr;
        } else {
            con->text[ con->curx + con->cury * con->cols ] = '\0';
            con->cury++;
            con->curx = 0;
            ptr++;
            continue;
        }
        ptr++;
        if( con->curx < con->cols-2 ) {
            con->curx++;
        } else {
            con->cury++;
            con->curx = 0;
            con->text[ con->cols-1 + (con->cury-1)*con->cols ] = '\0';
        }

    }

    con->text[ con->curx + con->cury * con->cols ] = '\0';
#endif

    if( con->text ) {
        char *hmm;
        hmm = realloc( (void *)con->text, con->cols + size - 1 );
        if( hmm ) {
            con->text = hmm;
            strcat( con->text, str );
            con->cols += size - 1;
        } else {
            if( str )
                free( str );
            return;
        }
    } else {
        con->cols = size;
        con->text = str;
    }

    update_osd_strings( con );
}

void console_gotoxy( console_t *con, int x, int y )
{
    return;

    if( x < 0 ) x *= -1;
    if( y < 0 ) y *= -1;

    con->curx = x % con->cols-1;
    con->cury = y % con->rows;
}

void console_setfg( console_t *con, unsigned int fg )
{
    con->fgcolour = fg;
}

void console_setbg( console_t *con, unsigned int bg )
{
    con->bgcolour = bg;
}

void console_scroll_n( console_t *con, int n )
{
    if( n > 0) {
        con->first_line = (con->num_lines-1 > con->first_line + n) ? con->first_line + n : con->num_lines - 1;
    } else {
        con->first_line = (0 > con->first_line + n) ? 0 : con->first_line + n;
    }
}

void console_toggle_console( console_t *con )
{
    con->visible = !con->visible;

    if( con->visible ) {
        con->dropdown = 1;
        con->drop_pos = con->y;
    }
}

void console_pipe_printf( console_t *con, char * format, ... )
{
    va_list ap;
    int ret;
    
    va_start( ap, format );
    ret = vfprintf( con->out, format, ap );
    va_end( ap );
    fflush( con->out );
}

int console_scanf( console_t *con, char *format, ... )
{
    va_list ap;
    fd_set rfds;
    struct timeval tv;
    int ret = 0;

    FD_ZERO( &rfds );
    FD_SET( fileno( con->in ), &rfds );
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    va_start( ap, format );
    if( select( fileno( con->in ) + 1, &rfds, NULL, NULL, &tv ) > 0 ) {
        ret = vfscanf( con->in, format, ap );
    }
    va_end( ap );

    return ret;
}

void console_setup_pipe( console_t *con, const char *pipename )
{
    int fd;

    if( ( fd = open( pipename, O_WRONLY | O_NONBLOCK ) ) == -1 ) {
        perror("console: Error opening pipe");
        return;
    }

    con->out = fdopen(fd, "a");
    if( con->out == NULL ) {
        perror("console: Error fdopen'ing pipe");
        con->out = stdout;
        close( fd );
    }
}

void console_composite_packed422_scanline( console_t *con, 
                                           uint8_t *output,
                                           int width, int xpos, int scanline )
{
    int i;

    if( scanline <= con->drop_pos && con->visible && scanline >= con->y && 
        scanline < con->y + con->height ) {

        blit_colour_packed422_scanline( output + (con->x*2), con->width,
                                        con->bg_luma, con->bg_cb, con->bg_cr );

        for( i = 0; i < con->rows; i++ ) {
            if( con->first_line + i >= con->num_lines ) break;
            if( osd_string_visible( con->line[ con->first_line + i ] ) ) {
                if( scanline >= con->coords[i].y &&
                    scanline < con->coords[i].y + 
                    osd_string_get_height( con->line[ con->first_line + i ] ) ) {
                    int startx = con->coords[i].x - xpos;
                    int strx = 0;

                    if( startx < 0 ) {
                        strx = -startx;
                        startx = 0;
                    }
                    if( startx < width ) {
                        osd_string_composite_packed422_scanline( con->line[ con->first_line + i ],
                                                                 output + (startx*2),
                                                                 output + (startx*2),
                                                                 osd_string_get_width(con->line[ con->first_line + i ]),
                                                                 strx,
                                                                 scanline - con->coords[i].y );
                    }
                }
            }
        }
    }

    if( scanline == con->frame_height-1 ) {
        con->drop_pos+=4;
    }
}

