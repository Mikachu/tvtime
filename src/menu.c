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
#include "osdtools.h"
#include "input.h"
#include "menu.h"

typedef enum MenuScreen_e {
    MENU_MAIN,
    MENU_PICT,
    MENU_AUDIO,
    MENU_CHANNELS,
    MENU_PARENTAL,
    MENU_PVR,
    MENU_SETUP,
    MENU_TVGUIDE,
    MENU_INPUT,
    MENU_PREVIEW,

    MENU_LAST
} MenuScreen;

#define MENU_LINES 8
typedef struct menu_text_s {
    osd_string_t *line;
    int x, y;
} menu_text;

struct menu_s {
    osd_graphic_t *bg;
    int bgx, bgy;

    osd_shape_t *box;
    int box_x, box_y;

    osd_shape_t *circle;
    int circle_x, circle_y;

    MenuScreen menu_screen;
    MenuScreen menu_previous_screen;
    unsigned int menu_state;
    menu_text menu_line[8];

    int frame_width;
    int frame_height;
    double frame_aspect;

    input_t *in;
    config_t *cfg;
};


menu_t *menu_new( input_t *in, config_t *cfg, int width, 
                  int height, double aspect )
{
    int i;
    menu_t *m = (menu_t *)malloc( sizeof( struct menu_s ) );
    if( !m ) return NULL;

    m->in = in;
    m->cfg = cfg;
    m->frame_width = width;
    m->frame_height = height;
    m->frame_aspect = aspect;
    m->bg = osd_graphic_new( DATADIR "/menubg.png" , width, height, aspect, 255 );
    if( !m->bg ) {
        free( m );
        return NULL;
    }
    m->bgx = 10;
    m->bgy = 10;

    m->box = osd_shape_new( OSD_Rect, width, height, width-100, 300, aspect, 255 );
    if( !m->box ) {
        osd_graphic_delete( m->bg );
        free( m );
        return NULL;
    }
    m->box_x = 10;
    m->box_y = 10;


    m->circle = osd_shape_new( OSD_Circle, width, height, 50, 50, aspect, 255 );
    if( !m->circle ) {
        osd_graphic_delete( m->bg );
        osd_shape_delete( m->box );
        free( m );
        return NULL;
    }
    m->circle_x = width-55;
    m->circle_y = 10;

    osd_shape_set_colour( m->box, 16, 128, 128 );
    osd_shape_set_colour( m->circle, 200, 200, 200 );

    for( i=0; i < MENU_LINES; i++ ) {
        m->menu_line[i].line = osd_string_new( DATADIR "/FreeSansBold.ttf", 
                                                20, width, height, aspect);
        m->menu_line[i].x = m->box_x + 5;
        m->menu_line[i].y = m->box_y + i*35 + 10;
    }

    return m;
}

/* Menus */
void menu_main( menu_t *m, int key );
void menu_init( menu_t *m )
{
    if( !m ) return;
    m->menu_screen = MENU_MAIN;
    m->menu_state = 0;
    m->menu_previous_screen = MENU_MAIN;
    
//    osd_graphic_show_graphic( m->bg, 51 );
    osd_shape_show_shape( m->box, 51 );
//    osd_shape_show_shape( m->circle, 51 );

    menu_main( m, 0 );

}

void menu_main( menu_t *m, int key )
{
    int i, start=0;
    if( !m ) return;

    switch( key ) {
    case I_UP:
    case I_DOWN:
        m->menu_state += ( key == I_UP ? -1 : 1 ) + MENU_LAST-1;
        m->menu_state %= MENU_LAST - 1;
        break;

    case I_PGUP:
    case I_PGDN:
        m->menu_state += ( key == I_PGUP ? -4 : 4 ) + MENU_LAST-1;
        m->menu_state %= MENU_LAST - 1;

        break;

    case I_LEFT:
    case I_RIGHT:
        break;

    case I_ENTER:
        m->menu_screen = m->menu_state + 1;
        break;

    default:
        break;
    }

    /* draw osd reflecting current state */
    if( m->menu_state >= (MENU_LINES/2) ) {
        start = m->menu_state - (MENU_LINES/2);
    }
    if( m->menu_state > MENU_LAST-1-(MENU_LINES/2) ) {
        start = MENU_LAST-1 - MENU_LINES;
    }
    for( i=0; i < MENU_LINES; i++ ) {
        if( m->menu_state == start+i ) {
            osd_string_set_colour( m->menu_line[i].line, 220, 12, 155 );
        } else {
            osd_string_set_colour( m->menu_line[i].line, 200, 128, 128 );
        }
        switch( start+i ) {
        case 0:
            osd_string_show_text( m->menu_line[i].line, "Picture Settings", 51);
            break;

        case 1:
            osd_string_show_text( m->menu_line[i].line, "Audio Settings", 51);
            break;

        case 2:
            osd_string_show_text( m->menu_line[i].line, "Channel Setup", 51);
            break;

        case 3:
            osd_string_show_text( m->menu_line[i].line, "Parental Control", 51);
            break;

        case 4:
            osd_string_show_text( m->menu_line[i].line, "Personal Video Recorder", 51);
            break;

        case 5:
            osd_string_show_text( m->menu_line[i].line, "General Setup", 51);
            break;

        case 6:
            osd_string_show_text( m->menu_line[i].line, "TV Guide", 51);
            break;

        case 7:
            osd_string_show_text( m->menu_line[i].line, "Input", 51);
            break;

        case 8:
            osd_string_show_text( m->menu_line[i].line, "Channel Preview", 51);
            break;

        default:
            break;
        }
    }
}

void menu_pict( menu_t *m, int key )
{
}

void menu_audio( menu_t *m, int key )
{
}

void menu_channels( menu_t *m, int key )
{
}

void menu_parental( menu_t *m, int key )
{
}

void menu_pvr( menu_t *m, int key )
{
}

void menu_setup( menu_t *m, int key )
{
}

void menu_tvguide( menu_t *m, int key )
{
}

void menu_input( menu_t *m, int key )
{
}

void menu_preview( menu_t *m, int key )
{
}

int menu_callback( menu_t *m, InputEvent command, int arg )
{
    int tvtime_cmd, verbose, fixed_arg = 0, i;

    if( !m ) return 0;

    verbose = config_get_verbose( m->cfg );

    switch( command ) {
    case I_KEYDOWN:

        fixed_arg = arg & 0xFFFF;

        switch( fixed_arg ) {

        case I_ENTER:
        case I_UP:
        case I_DOWN:
        case I_LEFT:
        case I_RIGHT:   
        case I_PGUP:
        case I_PGDN:
        case I_ESCAPE:
            if( fixed_arg == I_ESCAPE ) {
                MenuScreen tmp = m->menu_previous_screen;
                m->menu_previous_screen = MENU_MAIN;
                m->menu_screen = tmp;
                m->menu_state = 0;
            }

            switch( m->menu_screen ) {
            case MENU_MAIN:
                menu_main( m, fixed_arg );
                break;

            case MENU_PICT:
                menu_pict( m, fixed_arg );
                break;

            case MENU_AUDIO:
                menu_audio( m, fixed_arg );
                break;

            case MENU_CHANNELS:
                menu_channels( m, fixed_arg );
                break;

            case MENU_PARENTAL:
                menu_parental( m, fixed_arg );
                break;

            case MENU_PVR:
                menu_pvr( m, fixed_arg );
                break;

            case MENU_SETUP:
                menu_setup( m, fixed_arg );
                break;

            case MENU_TVGUIDE:
                menu_tvguide( m, fixed_arg );
                break;

            case MENU_INPUT:
                menu_input( m, fixed_arg );
                break;

            case MENU_PREVIEW:
                menu_preview( m, fixed_arg );
                break;

            default:
                break;
            }
            return 1;
            break;

        default:
            tvtime_cmd = config_key_to_command( m->cfg, arg );

            switch( tvtime_cmd ) {

            case TVTIME_MENUMODE:
                input_toggle_menu( m->in );
                m->menu_screen = MENU_MAIN;
                /* now remove OSD */
//                osd_graphic_set_timeout( m->bg, 0 );
                osd_shape_set_timeout( m->box, 0 );
//                osd_shape_set_timeout( m->circle, 0 );
                for( i=0; i < MENU_LINES; i++ ) {
                    osd_string_show_text(m->menu_line[i].line, "", 0);
                }
                return 1;
                break;

            default:
                /* cause input_callback to handle this */
                return 0;
                break;
            }
            break;
        }

    default:
        /* cause input_callback to handle this */
        return 0;
        break;
    }

    return 1;
}

/*
void menu_composite_packed422( menu_t *m, unsigned char *output,
                               int width, int height, int stride )
{
    if( osd_graphic_visible( m->bg ) ) {
        osd_graphic_composite_packed422( m->bg, 
                                         output,
                                         width, height, stride,
                                         m->bgx, m->bgy );
    }

    if( osd_shape_visible( m->box ) ) {
        osd_shape_composite_packed422( m->box, 
                                       output,
                                       width, height, stride,
                                       m->box_x, m->box_y );
    }

    if( osd_shape_visible( m->circle ) ) {
        osd_shape_composite_packed422( m->circle, 
                                       output,
                                       width, height, stride,
                                       m->circle_x, m->circle_y );
    }
}
*/

void menu_composite_packed422_scanline( menu_t *m, unsigned char *output,
                                        int width, int xpos, int scanline )
{

    int i;

    if( osd_shape_visible( m->box ) ) {
        int start = m->box_y;
        int end = start + 300;

        if( scanline >= start && scanline < end ) {
            int startx = m->box_x - xpos;
            int strx = 0;

            if( startx < 0 ) {
                strx = -startx;
                startx = 0;
            }
            if( startx < width ) {
                osd_shape_composite_packed422_scanline( m->box,
                                                        output + (startx*2),
                                                        output + (startx*2),
                                                        width - startx,
                                                        strx,
                                                        scanline - m->box_y );
            }
        }
    }

    for( i=0; i < MENU_LINES; i++ ) {
        if( osd_string_visible( m->menu_line[i].line ) ) {
            if( scanline >= m->menu_line[i].y &&
                scanline < m->menu_line[i].y + osd_string_get_height( m->menu_line[i].line ) ) {

                int startx = m->menu_line[i].x - xpos;
                int strx = 0;
                if( startx < 0 ) {
                    strx = -startx;
                    startx = 0;
                }
                if( startx < width ) {
                    osd_string_composite_packed422_scanline( m->menu_line[i].line,
                                                             output + (startx*2),
                                                             output + (startx*2),
                                                             width - startx,
                                                             strx,
                                                             scanline - m->menu_line[i].y );
                }
            }
        }
    }

}

