#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "osdtools.h"
#include "console.h"
#include "speedy.h"


struct console_line {
    osd_string_t *text;
    int x, y;
};

struct console_s {

    config_t *cfg;

    char *text;
    struct console_line *line;

    int timeout;  /* fade delay */
    
    osd_graphic_t *bgpic;

    unsigned int fgcolour;
    unsigned int bgcolour;
    int bg_luma, bg_cb, bg_cr;

    int frame_width;  
    int frame_height;
    int frame_aspect;

    int x, y; /* where to draw console */
    int width, height;  /* the size box we have to draw in */

    int curx, cury; /* cursor position */
    int rows, cols;

    int stdin, stdout, stderr;

    int visible;

};

console_t *console_new( config_t *cfg, int x, int y, int cols, int rows,
                        int fontsize, int video_width, int video_height, 
                        double video_aspect )
{
    int i=0;
    char *fontfile;
    console_t *con = (console_t *)malloc(sizeof(struct console_s));

    if( !con ) {
        return NULL;
    }

    con->cfg = cfg;
    con->x = x;
    con->y = y;
    con->frame_width = video_width;
    con->frame_height = video_height;
    con->frame_aspect = video_aspect;
    con->curx = 0;
    con->cury = 0;
    con->timeout = 51;
    con->fgcolour = config_get_other_text_rgb( cfg );
    con->bgcolour = 0x000000; /* black */
    con->bg_luma = 16;
    con->bg_cb = 128;
    con->bg_cr = 128;
    con->bgpic = NULL;
    con->stdin = 0;
    con->stdout = 1;
    con->stderr = 2;
    con->visible = 0;
    con->rows = rows;
    con->cols = cols+1; /* +1 for NULL */
    con->text = NULL;
    con->line = NULL;

    con->text = (char *)malloc( con->rows * con->cols ); 
    if( !con->text ) {
        console_delete( con );
        return NULL;
    }

    con->line = (struct console_line *)malloc( rows * sizeof( struct console_line ) );
    if( !con->line ) {
        console_delete( con );
        return NULL;
    }

    fontfile = DATADIR "/FreeSansBold.ttf";

    memset( con->text, 0, con->cols * con->rows );

    con->width = -1;
    con->height = -1;

    for( i=0; i < con->rows; i++ ) {
        con->line[ i ].text = osd_string_new( fontfile, fontsize, 
                                              video_width, 
                                              video_height,
                                              video_aspect );
        if( !con->line[ i ].text ) {
            fontfile = "./FreeSansBold.ttf";

            con->line[ i ].text = osd_string_new( fontfile, fontsize, 
                                                  video_width, 
                                                  video_height,
                                                  video_aspect );
        }

        if( !con->line[ i ].text ) {
            fprintf( stderr, "console: Could not find my font (%s)!\n", 
                     fontfile );
            console_delete( con );
            return NULL;
        }

        osd_string_set_colour_rgb( con->line[ i ].text, 
                                   (con->fgcolour >> 16) & 0xff, 
                                   (con->fgcolour >> 8) & 0xff, 
                                   (con->fgcolour & 0xff) );
        osd_string_show_text( con->line[ i ].text, " ", 0 );

        if( con->height == -1 ) {
            con->height = ( con->rows * 
                            (osd_string_get_height( con->line[ i ].text )
                             + 2) );
        }


        con->line[ i ].x = con->x + ( ( video_width * 1 ) / 100 );
        con->line[ i ].y = 1 + con->y +
            (i*osd_string_get_height( con->line[ i ].text))
            + 2;

    }

    return con;
}


void console_delete( console_t *con )
{
    int i=0;

    if( con->line ) {
        for( i=0; i < con->rows; i++ ) {
            if( con->line[ i ].text )
                osd_string_delete( con->line[ i ].text );
        }
        free( con->line );
    }

    if( con->text ) free( con->text );

    free( con );
}

void update_osd_strings( console_t *con )
{
    int i;
    
    if( !con ) return;
    for( i=0; i < con->rows; i++ ) {
        osd_string_set_colour_rgb( con->line[ i ].text, 
                                   (con->fgcolour >> 16) & 0xff, 
                                   (con->fgcolour >> 8) & 0xff, 
                                   (con->fgcolour & 0xff) );
        osd_string_show_text( con->line[ i ].text, con->text + i*con->cols, 51 );

    }

}

void console_printf( console_t *con, char *format, ... )
{
    va_list ap;
    int n=0, size;
    char *str = NULL;
    char *ptr;
    
    if( !con ) return;
    if( !format ) return;

    size = strlen(format)*3;

    va_start( ap, format );
    for(;;) {
        str = (char *)malloc( size );
        if( !str ) return;
        n = vsnprintf( str, size-1, format, ap );
        if( n == -1 ) {
            free( str );
            str = NULL;
            size *= 2;
        } else {
            break;
        }
    }
    va_end( ap );

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

    if( str ) free( str );

    update_osd_strings( con );
}

void console_gotoxy( console_t *con, int x, int y )
{
    if( !con ) return;
    if( x < 0 ) x *= -1;
    if( y < 0 ) y *= -1;

    con->curx = x % con->cols-1;
    con->cury = y % con->rows;
}

void console_setfg( console_t *con, unsigned int fg )
{
    if( !con ) return;

    con->fgcolour = fg;
}

void console_setbg( console_t *con, unsigned int bg )
{
    if( !con ) return;

    con->bgcolour = bg;
}

void console_toggle_console( console_t *con )
{
    if( !con ) return;
    con->visible = !con->visible;
}

void console_composite_packed422_scanline( console_t *con, 
                                           unsigned char *output,
                                           int width, int xpos, int scanline )
{
    int i;

    if( !con ) return;

    if( con->visible && scanline >= con->y && scanline < con->y + con->height ) {

        blit_colour_packed422_scanline( output + (con->x*2), con->width,
                                        con->bg_luma, con->bg_cb, con->bg_cr );

        for( i = 0; i < con->rows; i++ ) {
            if( osd_string_visible( con->line[i].text ) ) {
                if( scanline >= con->line[i].y &&
                    scanline < con->line[i].y + osd_string_get_height( con->line[i].text ) ) {

                    int startx = con->line[i].x - xpos;
                    int strx = 0;

                    if( startx < 0 ) {
                        strx = -startx;
                        startx = 0;
                    }
                    if( startx < width ) {
                        osd_string_composite_packed422_scanline( con->line[i].text,
                                                                 output + (startx*2),
                                                                 output + (startx*2),
                                                                 osd_string_get_width(con->line[i].text),
                                                                 strx,
                                                                 scanline - con->line[i].y );
                    }
                }
            }
        }
    }
}

