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
#define FONT_SIZE  45

struct vbiscreen_s {

    osd_string_t *line[ ROWS ];

    char buffers[ COLS ];
    char text[ 2 * ROWS * COLS ];
    char *disp_buf;

    unsigned int fgcolour;
    unsigned int bgcolour;
    int bg_luma, bg_cb, bg_cr;

    int frame_width;  
    int frame_height;
    int frame_aspect;

    int x, y; /* where to draw console */
    int width, height;  /* the size box we have to draw in */
    int rowheight, charwidth;
    
    int curx, cury; /* cursor position */
    int rows, cols; /* 32 cols 15 rows */
    int captions, style; /* CC (1) or Text (0), RU2 RU3 RU4 POP_UP PAINT_ON */
    int first_line; /* where to start drawing */
    int curbuffer;
    int top_of_screen; /* a pointer into line[] */
    int indent;

    char *fontfile;
    int fontsize;
};

vbiscreen_t *vbiscreen_new( int video_width, int video_height, 
                            double video_aspect )
{
    int i=0, fontsize = FONT_SIZE;
    vbiscreen_t *vs = (vbiscreen_t *)malloc(sizeof(struct vbiscreen_s));

    if( !vs ) {
        return NULL;
    }

    vs->x = 0;
    vs->y = 0;
    vs->frame_width = video_width;
    vs->frame_height = video_height;
    vs->frame_aspect = video_aspect;
    vs->curx = 0;
    vs->cury = 0;
    vs->fgcolour = 0xFFFFFFFFU; /* white */
    vs->bgcolour = 0xFF000000U; /* black */
    vs->bg_luma = 16;
    vs->bg_cb = 128;
    vs->bg_cr = 128;
    vs->rows = ROWS;
    vs->cols = COLS;
    vs->fontfile = DATADIR "/FreeMonoBold.ttf";
    vs->fontsize = fontsize;
    vs->width = video_width;
    vs->height = video_height;
    vs->first_line = 0;
    vs->captions = 0;
    vs->style = 0;
    vs->curbuffer = 0;
    vs->top_of_screen = 0;
    vs->indent = 0;
    vs->disp_buf = 0;
    memset( vs->buffers, 0, 2 * COLS );

    vs->line[0] = osd_string_new( vs->fontfile, fontsize, video_width, 
                                  video_height,
                                  video_aspect );

    if( !vs->line[0] ) {
        vs->fontfile = "./FreeMonoBold.ttf";

        vs->line[0] = osd_string_new( vs->fontfile, fontsize, 
                                       video_width, 
                                       video_height,
                                       video_aspect );
    }

    if( !vs->line[0] ) {
        fprintf( stderr, "vbiscreen: Could not find my font (%s)!\n", 
                 vs->fontfile );
        vbiscreen_delete( vs );
        return NULL;
    }

    osd_string_show_text( vs->line[ 0 ], "W", 0 );
    vs->rowheight = osd_string_get_height( vs->line[ 0 ] );
    vs->charwidth = osd_string_get_width( vs->line[ 0 ] );
    osd_string_delete( vs->line[ 0 ] );

    for( i = 0; i < ROWS; i++ ) {
        vs->line[ i ] = osd_string_new( vs->fontfile, fontsize,
                                        video_width, video_height,
                                        video_aspect );
        if( !vs->line[ i ] ) {
            fprintf( stderr, "vbiscreen: Could not allocate a line.\n" );
            vbiscreen_delete( vs );
            return NULL;
        }
        osd_string_set_colour_rgb( vs->line[ i ], 
                                   (vs->fgcolour & 0xff0000) >> 16,
                                   (vs->fgcolour & 0xff00) >> 8,
                                   (vs->fgcolour & 0xff) );
        osd_string_show_text( vs->line[ i ], " ", 0 );
    }
    memset( vs->text, 0, 2 * ROWS * COLS );
    return vs;
}


void vbiscreen_dump_screen_text( vbiscreen_t *vs )
{
    int i, offset;
    
    if( !vs ) return;
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

    if( !vs ) return 0;

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
    if( !haschars ) 
        osd_string_show_text( vs->line[ row ], " ", 0 );
    else
        osd_string_show_text( vs->line[ row ], text, 51 );

    return haschars;
}

void update_row( vbiscreen_t *vs )
{
    if( !vs ) return;

    update_row_x( vs, vs->cury );
    //vbiscreen_dump_screen_text( vs );
}

void update_all_rows( vbiscreen_t *vs )
{
    int row = 0;

    if( !vs ) return;

    for( row = 0; row < ROWS; row++ ) {
        update_row_x( vs, row );
    }
    //vbiscreen_dump_screen_text( vs );
}

void vbiscreen_delete( vbiscreen_t *vs )
{
    free( vs );
}

void scroll_screen( vbiscreen_t *vs )
{
    int start_row;

    if( !vs || !vs->captions || !vs->style || vs->style > ROLL_4 )
        return;

    start_row = ( vs->first_line + vs->top_of_screen ) % ( 2 * ROWS );
    fprintf ( stderr, "start row : %d first line %d\n ", start_row, vs->first_line );
    /* zero out top row */
    memset( (char *)( vs->text + start_row * COLS ), 0, COLS );

    vs->top_of_screen = ( vs->top_of_screen + 1 ) % ( 2 * ROWS );
    vs->curx = 0;
    update_all_rows( vs );

}

void vbiscreen_new_caption( vbiscreen_t *vs, int indent, int ital, 
                            unsigned int colour, int row )
{
    int i, j, base;
    if( !vs ) return;

    vs->fgcolour = colour;
    vs->indent = indent;
    vs->cury = row ? row - 1 : vs->cury;
    vs->curx = 0;

    if( vs->captions && vs->style == POP_UP ) {
        base = ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
        for( j = 0, i = base + vs->curx + vs->indent;
             i < base + COLS;
             j++, i++ ) {
            vs->text[ i ] = vs->buffers[ j ];
            fprintf( stderr, "%c", vs->buffers[ j ] );
        }
        fprintf( stderr, "\n" );
        vs->disp_buf = vs->text + base;

        update_row( vs );
        memset( vs->buffers, 0, COLS );
    }

    if( vs->captions && vs->style <= ROLL_4 && vs->style )
        scroll_screen( vs );



}

void vbiscreen_set_mode( vbiscreen_t *vs, int caption, int style )
{
    int base, i, j;
    if( !vs ) return;
    fprintf( stderr, "in set mode\n");

    if( vs->style != style ) {
        base = vs->top_of_screen * COLS;
        for( i = 0; i < ROWS * COLS; i++ ) {
            vs->text[ base ] = 0;
            base++;
            base %= 2 * ROWS * COLS;
        }
    }


    if( vs->captions && vs->style == POP_UP ) {
        base = ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
        for( j = 0, i = base + vs->curx + vs->indent;
             i < base + COLS;
             j++, i++ ) {
            vs->text[ i ] = vs->buffers[ j ];
            fprintf( stderr, "%c", vs->buffers[ j ] );
        }
        fprintf( stderr, "\n" );
        vs->disp_buf = vs->text + base;

        update_row( vs );
        memset( vs->buffers, 0, COLS );
    }


    fprintf( stderr, "Caption: %d ", caption );
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
    if( !caption ) {
        /* text mode */
        vs->cury = 0;
    } else {
        /* captioning mode */
        /* styles: ru2 ru3 ru4 pop paint
         */
        switch( style ) {
        case ROLL_2:
        case ROLL_3:
        case ROLL_4:
            /*
            base = vs->top_of_screen * COLS;
            for( i = 0; i < ROWS * COLS; i++ ) {
                vs->text[ base ] = 0;
                base++;
                base %= 2 * ROWS * COLS;
            }
            */

            /* scroll */
            if( vs->captions && vs->style <= ROLL_4 && vs->style )
                scroll_screen( vs );

            vs->first_line = ROWS - (style - 4);
            fprintf( stderr, "first_line %d\n", vs->first_line );
            vs->cury = ROWS; /* this is offscreen */
            break;
        case POP_UP:
            break;
        case PAINT_ON:
            vs->disp_buf = vs->text + 
                ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
            break;
        }
    }

    vs->captions = caption;
    vs->style = style;
}

void vbiscreen_tab( vbiscreen_t *vs, int cols )
{
    if( !vs ) return;
    if( cols < 0 || cols > 3 ) return;
    vs->curx += cols;
}

void vbiscreen_set_colour( vbiscreen_t *vs, unsigned int col )
{
    if( !vs ) return;
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
    if( !vs ) return;
    base = ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
    if( isprint( text ) ) 
        vs->text[ base + vs->curx + vs->indent ] = text;
    else
        vs->text[ base + vs->curx + vs->indent ] = ' ';
//    fprintf( stderr, "setting %d,%d = %c\n", vs->curx + vs->indent, 
    //            vs->cury, vs->text[ base + vs->curx + vs->indent ] );

}

void vbiscreen_delete_to_end( vbiscreen_t *vs )
{
    int i;
    fprintf( stderr, "in del to end\n");
    if( !vs ) return;
    for( i = vs->curx; i < COLS; i++ ) {
        vbiscreen_clear_current_cell( vs );
        vs->curx++;
    }
    vs->curx = COLS-1; /* is this right ? */
    update_row( vs );
}

void vbiscreen_backspace( vbiscreen_t *vs )
{
    fprintf( stderr, "in backspace\n");
    if( !vs ) return;
    if( !vs->curx ) return;
    vs->curx--;
    vbiscreen_clear_current_cell( vs );
    update_row( vs );
}

void blank_screen( vbiscreen_t *vs )
{
    int i;
    fprintf( stderr, "in blank\n");
    for( i = 0; i < ROWS; i++ ) {
        osd_string_show_text( vs->line[ i ], "", 0 );
    }
}

void vbiscreen_erase_displayed( vbiscreen_t *vs )
{
    int base , i;
    fprintf( stderr, "in erase disp\n");
    if( !vs ) return;

    if( vs->disp_buf && vs->style == POP_UP )
        memset( vs->disp_buf, 0, COLS );
    else if( vs->style && vs->style <= ROLL_4 ) {
        base = vs->top_of_screen * COLS;
        for( i = 0; i < ROWS * COLS; i++ ) {
            vs->text[ base ] = 0;
            base++;
            base %= 2 * ROWS * COLS;
        }
        
    }

    update_all_rows( vs );
}

void vbiscreen_erase_non_displayed( vbiscreen_t *vs )
{
    fprintf( stderr, "in erase non disp\n");
    if( !vs ) return;
    if( !vs->captions || vs->style < POP_UP ) return;
    memset( vs->buffers, 0, COLS );
}

void vbiscreen_carriage_return( vbiscreen_t *vs )
{
    if( !vs ) return;
    fprintf( stderr, "in CR\n");
    if( vs->cury == ROWS ) {
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

void vbiscreen_end_of_caption( vbiscreen_t *vs )
{
    int i, j, base;

    if( !vs ) return;
    fprintf( stderr, "in end of caption\n");
    /* draw it now */
    if( vs->captions && vs->style && vs->style <= ROLL_4 ) {
        base = vs->top_of_screen * COLS;
        for( i = 0; i < ROWS * COLS; i++ ) {
            vs->text[ base ] = 0;
            base++;
            base %= 2 * ROWS * COLS;
        }
        return;
    }

    base = ( ( vs->top_of_screen + vs->cury ) % ( 2 * ROWS ) ) * COLS;
    for( j = 0, i = base + vs->curx + vs->indent;
         i < base + COLS;
         j++, i++ ) {
        vs->text[ i ] = vs->buffers[ j ];
        fprintf( stderr, "%c", vs->buffers[ j ] );
    }
    fprintf( stderr, "\n" );
    vs->disp_buf = vs->text + base;

    update_row( vs );
    memset( vs->buffers, 0, COLS );
    vs->style = 0;
    vs->indent = 0;
}

void vbiscreen_print( vbiscreen_t *vs, char c1, char c2 )
{
    int i;
    if( !vs ) return;
    fprintf( stderr, "in print\n");
    if( vs->captions && vs->style == POP_UP ) {
        /* this all gets displayed at another time */
        for( i = 0; i < COLS; i++ ) {
            if( !vs->buffers[ i ] ) {
                if( i == COLS-1 ) {
                    vs->buffers[ i ] = c2;
                } else {
                    vs->buffers[ i ] = c1;
                    vs->buffers[ i+1 ] = c2;
                }
                return;
            }
            /* buffer is full so c2 will be at the end */
            vs->buffers[ COLS-1 ] = c2;
            return;
        }
    }

    if( vs->captions && vs->style && vs->style != POP_UP ) {
        /* ROLL-UP or PAINT ON */
        if( vs->curx != COLS-1 ) {
            vbiscreen_set_current_cell( vs, c1 );
            vs->curx++;
        } else if( vs->style != PAINT_ON ){
//            scroll_screen( vs );
            vs->curx = 0;
            vbiscreen_set_current_cell( vs, c1 );
            vs->curx = 1;            
        }

        if( vs->curx != COLS-1 && c2 ) {
            vbiscreen_set_current_cell( vs, c2 );
            vs->curx++;
            if( vs->curx == COLS-1 && vs->style != PAINT_ON ) {
//                scroll_screen( vs );
                vs->curx = 0;
            }
        } else if( c2 && vs->style != PAINT_ON ) {
//            scroll_screen( vs );
            vs->curx = 0;
            vbiscreen_set_current_cell( vs, c2 );
            vs->curx = 1;
        } else if( c2 && vs->style == PAINT_ON ) {
            vbiscreen_set_current_cell( vs, c2 );
        }
        
        if( vs->style == PAINT_ON ) {
            update_row( vs );
        }
    }
}


void vbiscreen_composite_packed422_scanline( vbiscreen_t *vs,
                                             unsigned char *output,
                                             int width, int xpos, 
                                             int scanline )
{
    int x=0, y=0, row=0, col=0, index=0;

    if( !vs ) return;
    if( !output ) return;

    if( scanline >= vs->y && scanline < vs->y + vs->height ) {

        if( 0 && !vs->captions )
            blit_colour_packed422_scanline( output + (vs->x*2), vs->width,
                                            vs->bg_luma, vs->bg_cb, 
                                            vs->bg_cr );

        index = vs->top_of_screen * COLS;
        x = ( vs->x + vs->charwidth) & ~1;
        for( row = 0; row < ROWS; row++ ) {
            y = vs->y + row * vs->rowheight + vs->rowheight;                

            if( osd_string_visible( vs->line[ row ] ) ) {
                if( scanline >= y &&
                    scanline < y + vs->rowheight ) {

                    int startx;
                    int strx;

                    if( 0 && vs->captions )
                        blit_colour_packed422_scanline( 
                            output + (x*2), 
                            vs->charwidth,
                            vs->bg_luma, 
                            vs->bg_cb, 
                            vs->bg_cr );

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

