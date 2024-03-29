/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 * Copyright (c) 2002 Doug Bell <drbell@users.sourceforge.net>.
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
#include <unistd.h>
#include "osdtools.h"
#include "speedy.h"
#include "vbiscreen.h"

#define ROLL_2      6
#define ROLL_3      7
#define ROLL_4      8
#define POP_UP      9
#define PAINT_ON    10


#define NUM_LINES  15
#define ROWS       15
#define COLS       32
#define FONT_SIZE  20

struct vbiscreen_s
{
    osd_font_t *font;
    osd_string_t *line[ ROWS ];

    char buffers[ ROWS * COLS * 2 ];
    char text[ 2 * ROWS * COLS ];
    char hiddenbuf[ COLS ];
    char paintbuf[ ROWS * COLS ];

    unsigned int fgcolour;
    unsigned int bgcolour;
    int bg_luma, bg_cb, bg_cr;

    int frame_width;  
    int frame_height;

    int x, y; /* where to draw the CC screen. */
    int width, height;  /* the size box we have to draw in */
    int rowheight, charwidth;
    
    int curx, cury; /* cursor position */
    int rows, cols; /* 32 cols 15 rows */
    int captions, style; /* CC (1) or Text (0), RU2 RU3 RU4 POP_UP PAINT_ON */
    int first_line; /* where to start drawing */
    int curbuffer;
    int top_of_screen; /* a pointer into line[] */
    int indent;
    int got_eoc;
    int scroll;

    char *fontfile;
    int fontsize;
    int verbose;
};

vbiscreen_t *vbiscreen_new( int video_width, int video_height, 
                            double pixel_aspect, int verbose )
{
    int i=0, fontsize = FONT_SIZE;
    vbiscreen_t *vs = malloc( sizeof( vbiscreen_t ) );

    if( !vs ) {
        return 0;
    }

    vs->verbose = verbose;
    vs->x = ( video_width * 10 ) / 100;
    vs->y = 0;
    vs->frame_width = video_width;
    vs->frame_height = video_height;
    vs->curx = 0;
    vs->cury = 0;
    vs->fgcolour = 0xFFFFFFFFU; /* white */
    vs->bgcolour = 0xFF000000U; /* black */
    vs->bg_luma = 16;
    vs->bg_cb = 128;
    vs->bg_cr = 128;
    vs->rows = ROWS;
    vs->cols = COLS;
    vs->fontfile = "FreeMonoBold.ttf";
    vs->fontsize = fontsize;
    vs->width = video_width;
    vs->height = video_height;
    vs->first_line = 0;
    vs->captions = 0;
    vs->style = 0;
    vs->curbuffer = 0;
    vs->top_of_screen = 0;
    vs->indent = 0;
    memset( vs->buffers, 0, 2 * COLS * ROWS );
    memset( vs->hiddenbuf, 0, COLS );
    memset( vs->paintbuf, 0, ROWS * COLS );
    vs->scroll = 0;
    vs->font = 0;

    vs->font = osd_font_new( vs->fontfile, fontsize, pixel_aspect );
    if( !vs->font ) {
        vbiscreen_delete( vs );
        return 0;
    }

    vs->line[0] = osd_string_new( vs->font );
    if( !vs->line[0] ) {
        fprintf( stderr, "vbiscreen: Could not find my font: %s\n", 
                 vs->fontfile );
        vbiscreen_delete( vs );
        return 0;
    }

    osd_string_show_border( vs->line[ 0 ], 1 );
    osd_string_show_text( vs->line[ 0 ], "gW", 0 );
    vs->rowheight = osd_string_get_height( vs->line[ 0 ] );
    vs->charwidth = osd_string_get_width( vs->line[ 0 ] );
    osd_string_delete( vs->line[ 0 ] );

    for( i = 0; i < ROWS; i++ ) {
        vs->line[ i ] = osd_string_new( vs->font );
        if( !vs->line[ i ] ) {
            fprintf( stderr, "vbiscreen: Could not allocate a line.\n" );
            vbiscreen_delete( vs );
            return 0;
        }
        osd_string_set_colour_rgb( vs->line[ i ], 
                                   (vs->fgcolour & 0xff0000) >> 16,
                                   (vs->fgcolour & 0xff00) >> 8,
                                   (vs->fgcolour & 0xff) );
        osd_string_show_border( vs->line[ i ], 1 );
        osd_string_show_text( vs->line[ i ], " ", 0 );
    }
    memset( vs->text, 0, 2 * ROWS * COLS );
    return vs;
}

void blank_screen( vbiscreen_t *vs )
{
    int i;

    for( i = 0; i < ROWS; i++ ) {
        osd_string_show_text( vs->line[ i ], " ", 0 );
    }
}

void clear_screen( vbiscreen_t *vs )
{
    int base, i;

    base = vs->top_of_screen * COLS;
    for( i = 0; i < ROWS * COLS; i++ ) {
        vs->text[ base ] = 0;
        base++;
        base %= 2 * ROWS * COLS;
    }
    blank_screen( vs );
}

void clear_hidden_roll( vbiscreen_t *vs )
{
    memset( vs->hiddenbuf, 0, COLS );
}

void clear_hidden_pop( vbiscreen_t *vs )
{
    memset( vs->buffers + vs->curbuffer * COLS * ROWS , 0, COLS * ROWS );
}

void clear_hidden_paint( vbiscreen_t *vs )
{
    memset( vs->paintbuf , 0, COLS * ROWS );
}

void clear_displayed_pop( vbiscreen_t *vs )
{
    memset( vs->buffers + ( vs->curbuffer ^ 1 ) * COLS * ROWS , 0, COLS * ROWS );
}

void vbiscreen_dump_screen_text( vbiscreen_t *vs )
{
    int i, offset;
    
    offset = vs->top_of_screen * COLS;

    fprintf( stderr, "\n   0123456789abcdefghij012345678901" );
    for( i = 0; i < ROWS * COLS; i++ ) {
        if( !(i % COLS) )
            fprintf( stderr, "\n%.2d ", i / COLS );
        fprintf( stderr, "%c", vs->text[ offset ] ? vs->text[ offset ] : ' ' );
        offset++;
        offset %= 2 * ROWS * COLS;
    }
    fprintf( stderr, "\n   0123456789abcdefghij012345678901\n   " );
    for( i = 0; i < COLS; i++ ) {
        fprintf( stderr, "%c", vs->text[ offset ] ? vs->text[ offset ] : ' ' );
        offset++;
        offset %= 2 * ROWS * COLS;
    }
    fprintf( stderr, "\n   0123456789abcdefghij012345678901\n" );
}

int update_row_x( vbiscreen_t *vs, int row )
{
    char text[ COLS + 1 ];
    int i, j, haschars = 0, base;

    text[ COLS ] = 0;
    base = ( ( vs->top_of_screen + row ) % ( 2 * ROWS ) ) * COLS;
    for( j = 0, i = base; i < base + COLS; i++, j++ ) {
        if( vs->text[ i ] ) {
            text[ j ] = vs->text[ i ];
            haschars = 1;
        } else {
            text[ j ] = ' ';
        }
    }

    osd_string_set_colour_rgb( vs->line[ row ], 
                               ( vs->fgcolour & 0xff0000 ) >> 16,
                               ( vs->fgcolour & 0xff00 ) >> 8,
                               ( vs->fgcolour & 0xff ) );
    if( !haschars )  {
        osd_string_show_text( vs->line[ row ], " ", 0 );
    } else {
        osd_string_show_text( vs->line[ row ], text, 51 );
    }

    return haschars;
}

void update_row( vbiscreen_t *vs )
{
    update_row_x( vs, vs->cury );
    /*vbiscreen_dump_screen_text( vs );*/
}

void update_all_rows( vbiscreen_t *vs )
{
    int row = 0;

    for( row = 0; row < ROWS; row++ ) {
        update_row_x( vs, row );
    }
    /*vbiscreen_dump_screen_text( vs );*/
}

void vbiscreen_delete( vbiscreen_t *vs )
{
    if( vs->font ) osd_font_delete( vs->font );
    free( vs );
}

void copy_row_to_screen( vbiscreen_t *vs, char *row )
{
    int base, i, j;

    base = ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
    for( j = 0, i = base;
         i < base + COLS;
         j++, i++ ) {
        vs->text[ i ] = row[ j ];
    }
    update_row( vs );
}

void scroll_screen( vbiscreen_t *vs )
{
    int start_row;

    if( !vs->captions || !vs->style || vs->style > ROLL_4 )
        return;
    
    start_row = ( vs->first_line + vs->top_of_screen ) % ( 2 * ROWS );
    if( vs->verbose ) {
        fprintf( stderr, "vbiscreen: Start row: %d, first line %d.\n ",
                 start_row, vs->first_line );
    }

    /* zero out top row */
    memset( (char *)( vs->text + start_row * COLS ), 0, COLS );
    vs->top_of_screen = ( vs->top_of_screen + 1 ) % ( 2 * ROWS );
    vs->curx = vs->indent;
    update_all_rows( vs );
    copy_row_to_screen( vs, vs->hiddenbuf );
    clear_hidden_roll( vs );
    vs->scroll = 26;
}

void vbiscreen_new_caption( vbiscreen_t *vs, int indent, int ital, 
                            unsigned int colour, int row )
{
    if( vs->verbose ) {
        fprintf( stderr, "vbiscreen: Indent: %d, ital: %d, colour: 0x%x, row: %d\n",
                 indent, ital, colour, row );
    }

    if( 0 && vs->captions && vs->style <= ROLL_4 && vs->style ) {
        if( row != vs->cury+1 ) {
            vs->cury = row - 1;
            clear_hidden_roll( vs );
        } else {
/*            scroll_screen( vs );*/
        }
    }

    if( vs->style > ROLL_4 ) {
        vs->cury = ( ( row > 0 ) ? row - 1 : 0 );
    }

    vs->fgcolour = colour;
    vs->indent = indent;
    vs->curx = indent;
}

void vbiscreen_set_mode( vbiscreen_t *vs, int caption, int style )
{
    if( vs->verbose ) {
        fprintf( stderr, "vbiscreen: Caption: %d ", caption );
        switch( style ) {
        case ROLL_2:
            fprintf( stderr, "ROLL 2\n");
            break;
        case ROLL_3:
            fprintf( stderr, "ROLL 3\n" );
            break;
        case ROLL_4:
            fprintf( stderr, "ROLL 4\n" );
            break;
        case POP_UP:
            fprintf( stderr, "POP UP\n" );
            break;
        case PAINT_ON:
            fprintf( stderr, "PAINT ON\n" );
            break;
        default:
            break;
        }
    }
    if( !caption ) {
        /* text mode */
        vs->cury = 0;
    } else {
        /* captioning mode */
        /* styles: ru2 ru3 ru4 pop paint
         */
        if( style != POP_UP && vs->style == POP_UP && !vs->got_eoc ) {
            /* stupid that sometimes they dont send a EOC */
            vbiscreen_end_of_caption( vs );
        }

        switch( style ) {
        case ROLL_2:
        case ROLL_3:
        case ROLL_4:
            if( vs->style == style ) {
                return;
            }
            vs->first_line = ROWS - (style - 4);

            if( vs->verbose ) {
                fprintf( stderr, "vbiscreen: first_line %d\n", vs->first_line );
            }

            vs->cury = ROWS - 1;
            break;
        case POP_UP:
            vs->got_eoc = 0;
            break;
        case PAINT_ON:
            break;
        }
    }

    vs->captions = caption;
    vs->style = style;
}

void vbiscreen_tab( vbiscreen_t *vs, int cols )
{
    if( cols < 0 || cols > 3 ) return;
    vs->curx += cols;
    if( vs->curx > 31 ) vs->curx = 31;
}

void vbiscreen_set_colour( vbiscreen_t *vs, unsigned int col )
{
    vs->fgcolour = col;
}

void vbiscreen_clear_current_cell( vbiscreen_t *vs )
{
    vs->text[ ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS 
              + vs->curx + vs->indent ] = 0;
}

void vbiscreen_set_current_cell( vbiscreen_t *vs, char text )
{
    int base;
    base = ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
    if( isprint( text ) ) 
        vs->text[ base + vs->curx + vs->indent ] = text;
    else
        vs->text[ base + vs->curx + vs->indent ] = ' ';
}

void vbiscreen_delete_to_end( vbiscreen_t *vs )
{
    int i;
    if( vs->verbose ) fprintf( stderr, "vbiscreen: Delete to end.\n");
    for( i = vs->curx; i < COLS; i++ ) {
        vbiscreen_clear_current_cell( vs );
        vs->curx++;
    }
    vs->curx = COLS-1; /* is this right ? */
    if( vs->captions && vs->style && vs->style != POP_UP )
        update_row( vs );
}

void vbiscreen_backspace( vbiscreen_t *vs )
{
    if( vs->verbose ) fprintf( stderr, "vbiscreen: Backspace.\n");
    if( !vs->curx ) return;
    vs->curx--;
    vbiscreen_clear_current_cell( vs );
    update_row( vs );
}

void vbiscreen_erase_displayed( vbiscreen_t *vs )
{
    if( vs->verbose ) fprintf( stderr, "vbiscreen: Erase displayed.\n");

    if( vs->captions && vs->style && vs->style <= ROLL_4 ) {
        clear_hidden_roll( vs );
    }

    clear_displayed_pop( vs );
    clear_screen( vs );
}

void vbiscreen_erase_non_displayed( vbiscreen_t *vs )
{
    if( vs->verbose ) fprintf( stderr, "vbiscreen: Erase non-displayed.\n");

    if( vs->captions && vs->style == POP_UP ) {
        memset( vs->buffers + vs->curbuffer * COLS * ROWS + vs->cury * COLS, 0, COLS );
/*        clear_hidden_pop( vs );*/
    } else if( vs->captions && vs->style && vs->style <= ROLL_4 ) {
        clear_hidden_roll( vs );
    }
}

void vbiscreen_carriage_return( vbiscreen_t *vs )
{
    if( vs->verbose ) fprintf( stderr, "vbiscreen: Carriage return.\n");
    if( vs->style != POP_UP) {
        /* not sure if this is right for text mode */
        /* in text mode, perhaps a CR on last row clears screen and goes
         * to (0,0) */
        scroll_screen( vs );
    }

    /* keep cursor on bottom for rollup */
    if( vs->captions && vs->style && vs->style <= ROLL_4 )
        vs->cury--;

    vs->cury++;
    vs->curx = 0;
}

void copy_buf_to_screen( vbiscreen_t *vs, char *buf )
{
    int base, i, j;

    base = vs->top_of_screen * COLS;
    for( j = 0, i = 0; i < ROWS * COLS; i++, j++ ) {
        vs->text[ base ] = buf[ j ];
        base++;
        base %= 2 * ROWS * COLS;
    }
    update_all_rows( vs );
}

void vbiscreen_end_of_caption( vbiscreen_t *vs )
{
    if( vs->verbose ) fprintf( stderr, "vbiscreen: End of caption.\n");

    if( vs->style == PAINT_ON ) {
        copy_buf_to_screen( vs, vs->paintbuf );
        clear_hidden_paint( vs );
    } else if( vs->style == POP_UP ) {
        copy_buf_to_screen( vs, vs->buffers + vs->curbuffer * COLS * ROWS );
        vs->curbuffer ^= 1;
    }

    /* to be safe? */
    vs->curx = 0;
    vs->cury = ROWS - 1;
    vs->got_eoc = 1;
}

void vbiscreen_print( vbiscreen_t *vs, char c1, char c2 )
{
    if( vs->verbose ) fprintf( stderr, "vbiscreen: Print (%d, %d)[%c %c]\n", vs->curx, vs->cury, c1, c2);
    if( vs->captions && vs->style == POP_UP ) {
        /* this all gets displayed at another time */
        if( vs->curx != COLS-1 ) {
            *(vs->buffers + vs->curx + vs->curbuffer * ROWS * COLS + vs->cury * COLS ) = c1;
            vs->curx++;
        }

        if( vs->curx != COLS-1 && c2 ) {
            *(vs->buffers + vs->curx + vs->curbuffer * ROWS * COLS + vs->cury * COLS ) = c2;
            vs->curx++;
        } else if( c2 ) {
            *(vs->buffers + vs->curx + vs->curbuffer * ROWS * COLS + vs->cury * COLS ) = c2;
        }
    }

    if( vs->captions && vs->style == PAINT_ON ) {
        if( vs->curx != COLS-1 ) {
            vs->paintbuf[ vs->curx + vs->cury * COLS ] = c1;
            vs->curx++;
        }

        if( vs->curx != COLS-1 && c2 ) {
            vs->paintbuf[ vs->curx + vs->cury * COLS ] = c2;
            vs->curx++;
        } else if( c2 ) {
            vs->paintbuf[ vs->curx + vs->cury * COLS ] = c2;
        }
    }

    if( vs->captions && vs->style && vs->style <= ROLL_4 ) {
        if( vs->curx != COLS-1 ) {
            vs->hiddenbuf[ vs->curx ] = c1;
            vs->curx++;
        } else {
            vs->hiddenbuf[ vs->curx ] = c1;
        }

        if( vs->curx != COLS-1 && c2 ) {
            vs->hiddenbuf[ vs->curx ] = c2;
            vs->curx++;
        } else if( c2 ) {
            vs->hiddenbuf[ vs->curx ] = c2;
        }
    }
}

void vbiscreen_reset( vbiscreen_t *vs )
{
    clear_screen( vs );
    clear_hidden_pop( vs );
    clear_displayed_pop( vs );
    clear_hidden_roll( vs );
    vs->captions = 0;
    vs->style = 0;
}

int vbiscreen_active_on_scanline( vbiscreen_t *vs, int scanline )
{
    if( scanline >= vs->y && scanline < vs->y + vs->height ) {
        return 1;
    }
    return 0;
}

void vbiscreen_composite_packed422_scanline( vbiscreen_t *vs,
                                             uint8_t *output,
                                             int width, int xpos, 
                                             int scanline )
{
    int x = 0, y = 0, row = 0, index = 0;

    if( scanline >= vs->y && scanline < vs->y + vs->height ) {

        index = vs->top_of_screen * COLS;
        x = ( vs->x + vs->charwidth) & ~1;
        for( row = 0; row < ROWS; row++ ) {
            y = vs->y + row * vs->rowheight + vs->rowheight;
            if( osd_string_visible( vs->line[ row ] ) ) {
                if( scanline >= y && scanline < y + vs->rowheight ) {

                    int startx;
                    int strx;

                    startx = x - xpos;
                    strx = 0;                       

                    if( startx < 0 ) {
                        strx = -startx;
                        startx = 0;
                    }


                    if( startx < width ) {
                        osd_string_composite_packed422_scanline( 
                            vs->line[ row ],
                            output + (startx*2),
                            output + (startx*2),
                            width - startx,
                            strx,
                            scanline - y );
                    }
                }
                index++;
            }
        }
    }
}

