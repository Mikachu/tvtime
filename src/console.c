#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "osdtools.h"
#include "console.h"
#include "speedy.h"

struct console_line_coords {
    int x, y;
};

struct console_s {

    config_t *cfg;

    char *text;
    osd_string_t **line;
    struct console_line_coords *coords;

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
    
    int num_lines;

    int curx, cury; /* cursor position */
    int rows, cols;

    int stdin, stdout, stderr;

    int visible;
    char *fontfile;
    int fontsize;

    int first_line;
    int dropdown;
    int drop_pos;

};

console_t *console_new( config_t *cfg, int x, int y, int width, int height,
                        int fontsize, int video_width, int video_height, 
                        double video_aspect )
{
    int i=0, rowheight;
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
    con->rows = 0;
    con->cols = 0;
    con->text = NULL;
    con->line = NULL;
    con->first_line = 0;
    con->num_lines = 0;
    con->coords = NULL;

#if 0
    con->text = (char *)malloc( con->rows * con->cols ); 
    if( !con->text ) {
        console_delete( con );
        return NULL;
    }
#endif

    con->line = (osd_string_t **)malloc( 1 * sizeof( osd_string_t * ) );
    if( !con->line ) {
        console_delete( con );
        return NULL;
    }

    con->fontfile = DATADIR "/FreeSansBold.ttf";
    con->fontsize = fontsize;

#if 0
    memset( con->text, 0, con->cols * con->rows );
#endif

    con->width = width;
    con->height = height;

    con->line[0] = osd_string_new( con->fontfile, fontsize, video_width, 
                                   video_height,
                                   video_aspect );

    if( !con->line[0] ) {
        con->fontfile = "./FreeSansBold.ttf";

        con->line[0] = osd_string_new( con->fontfile, fontsize, 
                                       video_width, 
                                       video_height,
                                       video_aspect );
    }

    if( !con->line[0] ) {
        fprintf( stderr, "console: Could not find my font (%s)!\n", 
                 con->fontfile );
        console_delete( con );
        return NULL;
    }

    osd_string_show_text( con->line[0], "H", 0 );

    rowheight = osd_string_get_height( con->line[0] );
    con->rows = (double)height / (double)rowheight - 0.5;

    osd_string_delete( con->line[0] );
    
    free( con->line );
    con->line = NULL;

    con->coords = (struct console_line_coords *)malloc( con->rows * sizeof( struct console_line_coords ) );
    if( !con->coords ) {
        console_delete( con );
        return NULL;
    }


    for( i=0; i < con->rows; i++ ) {
        int tmp;

        con->coords[ i ].x = con->x + video_width / 100 ;

        if( i == 0 ) 
            tmp = con->y + ((double)height - (double)rowheight * (double)con->rows) / (double)2;
        else 
            tmp = con->coords[ i-1 ].y + rowheight;

        con->coords[ i ].y = tmp;
    }

    return con;
}


void console_delete( console_t *con )
{
    int i=0;

    if( con->line ) {
        for( i=0; i < con->num_lines; i++ ) {
            if( con->line[ i ] )
                osd_string_delete( con->line[ i ] );
        }
        free( con->line );
    }

    if( con->text ) free( con->text );
    if( con->coords ) free( con->coords );

    free( con );
}

void update_osd_strings( console_t *con )
{
    char *ptr;
    char tmpstr[1024];
    int maxwidth = con->width - ( ( con->frame_width * 1 ) / 100 );
    
    if( !con ) return;
    ptr = con->text + con->curx;
    tmpstr[0] = '\0';

    if( !con->line ) {
        con->line = (osd_string_t**)malloc(sizeof(osd_string_t*));
        if( !con->line ) {
            fprintf(stderr, 
                    "console: Couldn't add a line.\n" );

            return;
        }
        con->line[0] = osd_string_new( con->fontfile, 
                                       con->fontsize, 
                                       con->frame_width, 
                                       con->frame_height,
                                       con->frame_aspect );
        if( !con->line[ 0 ] ) {
            fprintf( stderr, 
                     "console: Could not create new string.\n" );
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
            if( con->line ) {
                osd_string_t **hmm;
                hmm = (osd_string_t **)realloc( (void *)con->line, sizeof(osd_string_t*)*(con->num_lines+1) );
                if( hmm ) {
                    con->num_lines++;
                    con->line = hmm;
                } else {
                    fprintf(stderr, 
                            "console: Couldn't add another line.\n" );
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
            con->cury++;
            tmpstr[0] = '\0';
            con->line[ con->cury ] = osd_string_new( con->fontfile, 
                                                     con->fontsize, 
                                                     con->frame_width, 
                                                     con->frame_height,
                                                     con->frame_aspect );
            if( !con->line[ con->cury ] ) {
                fprintf( stderr, 
                         "console: Could not create new string.\n" );
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

    if( str ) {
        char *hmm;
        hmm = realloc( (void *)str, strlen(str)+1 );
        if( hmm ) {
            size = strlen(str)+1;
            str = hmm;
        }
    }

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
    return; /*NOWARN*/
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

void console_scroll_n( console_t *con, int n )
{
    if( !con ) return;

    if( n > 0) {
        con->first_line = (con->num_lines-1 > con->first_line + n) ? con->first_line + n : con->num_lines - 1;
    } else {
        con->first_line = (0 > con->first_line + n) ? 0 : con->first_line + n;
    }
}

void console_toggle_console( console_t *con )
{
    if( !con ) return;
    con->visible = !con->visible;
    if( con->visible ) {
        con->dropdown = 1;
        con->drop_pos = con->y;
    }
}

void console_composite_packed422_scanline( console_t *con, 
                                           unsigned char *output,
                                           int width, int xpos, int scanline )
{
    int i;

    if( !con ) return;

    if( scanline <= con->drop_pos &&
        con->visible && scanline >= con->y && 
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
    if( scanline == con->frame_height-1 ) con->drop_pos+=4;
}

