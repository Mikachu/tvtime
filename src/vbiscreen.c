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
#define FONT_SIZE  15

struct vbiscreen_s {

    osd_string_t *line[ ROWS ];

    char buffers[ 2 * COLS ];
    char text[ 2 * ROWS * COLS ];

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
                                        vs->frame_width, vs->frame_height,
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

    }
    memset( vs->text, 0, 2 * ROWS * COLS );
    return vs;
}


void vbiscreen_dump_screen_text( vbiscreen_t *vs )
{
    int i, base;
    
    if( !vs ) return;

    fprintf( stderr, "\n--------------------------------" );
    for( i = vs->top_of_screen * COLS; 
         i < vs->top_of_screen * COLS + ROWS * COLS;
         i++ ) {
        if( !(i % COLS) )
            fprintf( stderr, "\n" );
        fprintf( stderr, "%c", vs->text[ i ] ? vs->text[ i ] : ' ' );
    }
    fprintf( stderr, "\n--------------------------------\n" );
}

int update_row_x( vbiscreen_t *vs, int row )
{
    char text[ COLS + 1 ];
    int i, j, haschars = 0, base;

    if( !vs || row > ROWS-1 ) return 0;

    text[ COLS ] = 0;
    base = ( vs->top_of_screen + row ) * COLS ;
    for( j = 0, i = base; i < base + COLS; i++, j++ ) {
        if( vs->text[ i ] ) {
            text[ j ] = vs->text[ i ];
            haschars = 1;
        } else {
            text[ j ] = ' ';
        }
    }
    return haschars;
    osd_string_set_colour_rgb( vs->line[ row ], 
                               ( vs->fgcolour & 0xff0000 ) >> 16,
                               ( vs->fgcolour & 0xff00 ) >> 8,
                               ( vs->fgcolour & 0xff ) );
    if( !haschars ) 
        osd_string_show_text( vs->line[ row ], "", 0 );
    else
        osd_string_show_text( vs->line[ row ], text, 0 );

    return haschars;
}

void update_row( vbiscreen_t *vs )
{
    if( !vs ) return;

    update_row_x( vs, vs->cury );
}

void update_all_rows( vbiscreen_t *vs )
{
    int row = 0, base = 0, haschars;

    if( !vs ) return;
    if( vs->captions && vs->style <= ROLL_4 )
        base = vs->first_line - 1;

    for( row = base; row < ROWS; row++ ) {
        haschars = update_row_x( vs, row );
    }
}

void vbiscreen_delete( vbiscreen_t *vs )
{
    free( vs );
}

void vbiscreen_set_mode( vbiscreen_t *vs, int caption, int style,
                         int indent, int ital, unsigned int colour, int row )
{
    if( !vs ) return;

    vs->fgcolour = colour;

    vs->captions = caption;
    vs->style = style;
    vs->curx = indent ? indent - 1 : 0;
    vs->top_of_screen = 0;

    memset( (char *)( vs->text + vs->top_of_screen * COLS ), 
            0, ROWS * COLS );

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
            vs->first_line = 15 - (style-4-1) - 1;
            vs->cury = 14;
            break;
        case POP_UP:
        case PAINT_ON:
            vs->first_line = row-1;
            vs->cury = row-1;
            break;
        }
    }
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
    vs->text[ ( vs->top_of_screen + vs->cury ) * COLS + vs->curx ] = 0;
}

void vbiscreen_set_current_cell( vbiscreen_t *vs, char text )
{
    if( !vs ) return;
    if( isprint( text ) ) 
        vs->text[ ( vs->top_of_screen + vs->cury ) * COLS + vs->curx ] = text;
    else
        vs->text[ ( vs->top_of_screen + vs->cury ) * COLS + vs->curx ] = ' ';
}

void vbiscreen_delete_to_end( vbiscreen_t *vs )
{
    int i;
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
    if( !vs ) return;
    if( !vs->curx ) return;
    vs->curx--;
    vbiscreen_clear_current_cell( vs );
    update_row( vs );
}

void blank_screen( vbiscreen_t *vs )
{
    int i;
    for( i = 0; i < ROWS; i++ ) {
        osd_string_show_text( vs->line[ i ], "", 0 );
    }
}

void vbiscreen_erase_displayed( vbiscreen_t *vs )
{
    if( !vs ) return;

    memset( (char *)( vs->text + vs->top_of_screen * COLS ), 0, ROWS * COLS );
    memset( (char *)( vs->buffers + vs->curbuffer * COLS ) , 0, COLS );
    update_all_rows( vs );
    blank_screen( vs );
}

void vbiscreen_erase_non_displayed( vbiscreen_t *vs )
{
    if( !vs ) return;
    if( !vs->captions || vs->style != POP_UP ) return;
    memset( (char *)( vs->buffers + !vs->curbuffer * COLS ) , 0, COLS );
}

void vbiscreen_carriage_return( vbiscreen_t *vs )
{
    int i;
    if( !vs ) return;
    if( vs->cury == ROWS-1 ) {
        /* not sure if this is right for text mode */
        /* in text mode, perhaps a CR on last row clears screen and goes
         * to (0,0) */
        int start_row = vs->first_line + vs->top_of_screen;
        if( vs->captions && vs->style > ROLL_4 ) 
            start_row = vs->top_of_screen;
        for( i = start_row * COLS;
             i < start_row * COLS + COLS;
             i++ ) {
            vs->text[ i ] = 0;
        }

        vs->top_of_screen = ( vs->top_of_screen + 1 ) % ( 2 * ROWS );
        update_all_rows( vs );
    }

    /* keep cursor on bottom for rollup */
    if( vs->captions && vs->style <= ROLL_4 )
        vs->cury = ROWS-2;

    vs->cury = (vs->cury + 1) % ROWS;
    vs->curx = 0;
}

void vbiscreen_end_of_caption( vbiscreen_t *vs )
{
    int i, j;

    if( !vs ) return;

    memset( (char *)( vs->buffers + vs->curbuffer * COLS ) , 0, COLS );
    vs->curbuffer = !vs->curbuffer;
    /* draw it now */
    for( j = 0, i = ( vs->top_of_screen + vs->cury ) * COLS + vs->curx;
         i < ( vs->top_of_screen + vs->cury ) * COLS + COLS;
         j++, i++ ) {
        vs->text[ i ] = vs->buffers[ vs->curbuffer * COLS + j ];
    }
    update_row( vs );
}

void vbiscreen_print( vbiscreen_t *vs, char c1, char c2 )
{
    int i;
    
    if( !vs ) return;
    if( vs->captions && vs->style == POP_UP ) {
        /* this all gets displayed at another time */
        for( i = 0; i < COLS; i++ ) {
            if( !vs->buffers[ i + !vs->curbuffer * COLS ] ) {
                if( vs->curx + i == COLS-1 ) {
                    vs->buffers[ i + !vs->curbuffer * COLS ] = c2;
                } else {
                    vs->buffers[ i + !vs->curbuffer * COLS ] = c1;
                    vs->buffers[ i+1 + !vs->curbuffer * COLS ] = c2;
                }
                return;
            }
        }
        /* buffer is full so c2 will be at the end */
        vs->buffers[ COLS + !vs->curbuffer * COLS ] = c2;
    }

    if( !vs->captions || vs->style == PAINT_ON ) {
        vbiscreen_set_current_cell( vs, c1 );
        vs->curx++;
        if( vs->curx == COLS )
            vs->curx--;

        vbiscreen_set_current_cell( vs, c2 );
        vs->curx++;
        if( vs->curx == COLS )
            vs->curx--;

        update_row( vs );

    } else if( vs->captions && vs->style <= ROLL_4 ) {
        /* ROLL-UP */
        if( vs->curx != COLS-1 ) {
            vbiscreen_set_current_cell( vs, c1 );
            vs->curx++;
        } else {
            for( i = ( vs->first_line + vs->top_of_screen ) * COLS;
                 i < ( vs->first_line + vs->top_of_screen ) * COLS + COLS;
                 i++ ) {
                vs->text[ i ] = 0;
            }
            vs->top_of_screen = ( vs->top_of_screen + 1 ) % ( 2 * ROWS );
            vs->curx = 0;
            vbiscreen_set_current_cell( vs, c1 );
            vs->curx = 1;            
        }

        if( vs->curx != COLS-1 && c2 ) {
            vbiscreen_set_current_cell( vs, c2 );
            vs->curx++;
        } else if( c2 ) {
            for( i = ( vs->first_line + vs->top_of_screen ) * COLS;
                 i < ( vs->first_line + vs->top_of_screen ) * COLS + COLS;
                 i++ ) {
                vs->text[ i ] = 0;
            }
            vs->top_of_screen = ( vs->top_of_screen + 1 ) % ( 2 * ROWS );
            vs->curx = 0;
            vbiscreen_set_current_cell( vs, c2 );
            vs->curx = 1;
        }
        update_all_rows( vs );
    }
}


void vbiscreen_composite_packed422_scanline( vbiscreen_t *vs,
                                             unsigned char *output,
                                             int width, int xpos, 
                                             int scanline )
{
    int x=0, y=0, row=0, col=0, index=0;

    return;

    if( !vs ) return;
    if( !output ) return;

    if( scanline >= vs->y && scanline < vs->y + vs->height ) {

        if( !vs->captions )
            blit_colour_packed422_scanline( output + (vs->x*2), vs->width,
                                            vs->bg_luma, vs->bg_cb, 
                                            vs->bg_cr );

        index = vs->top_of_screen * COLS;
        for( row = 0; row < ROWS; row++ ) {
            y = vs->y + row * vs->rowheight + vs->rowheight;                
            for( col = 0; col < COLS; col++ ) {
                x = ( vs->x + col * vs->charwidth + vs->charwidth) & ~1;

                if( osd_string_visible( vs->line[ index ] ) ) {
                    if( scanline >= y &&
                        scanline < y + vs->rowheight ) {

                        int startx;
                        int strx;

                        if( vs->captions )
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
                                vs->line[ index ],
                                output + (startx*2),
                                output + (startx*2),
                                vs->charwidth,
                                strx,
                                scanline - y );
                        }
                    }
                }
                index++;
            }
        }
    }
}

