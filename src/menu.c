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
#include "videoinput.h"
#include "speedy.h"
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

#define MENU_LINES 11
typedef struct menu_text_s {
    osd_string_t *line;
    int x, y;
} menu_text;

struct menu_s {
    MenuScreen menu_screen;
    MenuScreen menu_previous_screen;
    unsigned int menu_state;
    menu_text menu_line[MENU_LINES];
    void *menu_data;

    int bg_luma, bg_cb, bg_cr;

    int visible;
    int x, y, width, height;

    int frame_width;
    int frame_height;
    double frame_aspect;

    input_t *in;
    config_t *cfg;
    videoinput_t *vidin;
    tvtime_osd_t *osd;
};


menu_t *menu_new( input_t *in, config_t *cfg, videoinput_t *vidin, 
                  tvtime_osd_t *osd, int width, 
                  int height, double aspect )
{
    menu_t *m = (menu_t *) malloc( sizeof( menu_t ) );
    int i;
    unsigned int rgb;
    unsigned char oconv[3];

    if( !m ) {
        return 0;
    }

    m->in = in;
    m->cfg = cfg;
    m->vidin = vidin;
    m->osd = osd;
    m->frame_width = width;
    m->frame_height = height;
    m->frame_aspect = aspect;
    m->width = ( width * 80 ) / 100;
    m->height = ( height * 80 ) / 100;
    m->x = ( width * 10 ) / 100;
    m->y = ( height * 10 ) / 100;
    m->visible = 0;
    m->bg_luma = 16;
    m->bg_cb = 128;
    m->bg_cr = 128;
    m->menu_data = NULL;

    if( !cfg ) {
        menu_delete( m );
        return 0;
    }

/** Removed by Doug until we can convert argb to ay'cbcr
    rgb = config_get_menu_bg_rgb( cfg );
    m->bg_luma = (int)oconv[0];
    m->bg_cb = (int)oconv[1];
    m->bg_cr = (int)oconv[2];
*/

    for( i = 0; i < MENU_LINES; i++ ) {
        m->menu_line[ i ].line = osd_string_new( DATADIR "/FreeSansBold.ttf", 
                                                 20, width, height, aspect );
        if( !m->menu_line[ i ].line ) {
            menu_delete( m );
            return 0;
        }
        osd_string_set_colour( m->menu_line[i].line, 200, 128, 128 );
        osd_string_show_text( m->menu_line[ i ].line, "Height", 0 );
        m->menu_line[ i ].x = m->x + ( width * 5 ) / 100;
        m->menu_line[ i ].y = m->y +
                              ( i * osd_string_get_height( m->menu_line[ i ].line ) ) +
                              ( ( height * 5 ) / 100 );
    }

    return m;
}

void menu_delete( menu_t *m )
{
    int i;

    for( i = 0; i < MENU_LINES; i++ ) {
        if( m->menu_line[ i ].line ) {
            osd_string_delete( m->menu_line[ i ].line );
        } else {
            break;
        }
    }

    free( m );
}

/* Menus */
void (*menu_blit_scanline)( menu_t *m, unsigned char *output,
                            int width, int xpos, int scanline );


void menu_main( menu_t *m, int key );
void menu_init( menu_t *m )
{
    if( !m ) return;
    m->menu_screen = MENU_MAIN;
    m->menu_state = 0;
    m->menu_previous_screen = MENU_MAIN;
    m->visible = 1;
    
    menu_main( m, 0 );

}

void menu_main( menu_t *m, int key )
{
    int i, start=0;
    if( !m ) return;

    menu_blit_scanline = NULL;

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
        m->menu_state = 0;
        break;

    default:
        break;
    }

    /* draw osd reflecting current state */
    if( MENU_LAST > MENU_LINES && m->menu_state >= (MENU_LINES/2) ) {
        start = m->menu_state - (MENU_LINES/2);
    }
    if( MENU_LAST > MENU_LINES && m->menu_state > MENU_LAST-1-(MENU_LINES/2) ) {
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

struct pict_data {
    osd_databars_t *bright;
    osd_databars_t *cont;
    osd_databars_t *col;
    osd_databars_t *hue;
};

void pict_blit_scanline( menu_t *m, unsigned char *output,
                         int width, int xpos, int scanline );
void menu_pict( menu_t *m, int key )
{
    int i;
    int NUM_ITEMS = 4;

    if( !m ) return;

    menu_blit_scanline = pict_blit_scanline;

    if( m->menu_data == NULL ) {
        m->menu_data = (void *)malloc( sizeof(struct pict_data) );
        ((struct pict_data *)(m->menu_data))->bright = osd_databars_new( (m->width * 80) / 100 );
        ((struct pict_data *)(m->menu_data))->cont = osd_databars_new( (m->width * 80) / 100 );
        ((struct pict_data *)(m->menu_data))->col = osd_databars_new( (m->width * 80) / 100 );
        ((struct pict_data *)(m->menu_data))->hue = osd_databars_new( (m->width * 80) / 100 );
    }

    if( !tvtime_osd_data_bar_visible( m->osd ) ) {
        m->visible=1;
    }

    switch( key ) {
    case I_UP:
    case I_DOWN:
        m->menu_state += ( key == I_UP ? -1 : 1 ) + NUM_ITEMS;
        m->menu_state %= NUM_ITEMS ;

        break;

    case I_PGUP:
    case I_PGDN:
        m->menu_state += ( key == I_PGUP ? -2 : 2 ) + NUM_ITEMS;
        m->menu_state %= NUM_ITEMS ;

        break;

    case I_LEFT:
    case I_RIGHT:
        switch( m->menu_state ) {
        case 0:
            videoinput_set_brightness_relative( 
                m->vidin, 
                (key == I_RIGHT) ? 1 : -1 );
            m->visible = 0;
            if( m->osd ) 
                tvtime_osd_show_data_bar( m->osd, "Bright ", videoinput_get_brightness( m->vidin ) );


            break;

        case 1:
            videoinput_set_colour_relative( 
                m->vidin, 
                (key == I_RIGHT) ? 1 : -1 );
            m->visible = 0;
            if( m->osd )
                tvtime_osd_show_data_bar( m->osd, "Colour ", videoinput_get_colour( m->vidin ) );

            break;

        case 2:
            videoinput_set_contrast_relative( 
                m->vidin, 
                (key == I_RIGHT) ? 1 : -1 );
            m->visible = 0;
            if( m->osd ) 
                tvtime_osd_show_data_bar( m->osd, "Cont   ", videoinput_get_contrast( m->vidin ) );

            break;

        case 3:
            videoinput_set_hue_relative( 
                m->vidin, 
                (key == I_RIGHT) ? 1 : -1 );
            m->visible = 0;
            if( m->osd ) 
                tvtime_osd_show_data_bar( m->osd, "Hue    ", videoinput_get_hue( m->vidin ) );

            break;

        default:
            break;
        }
        break;

    case I_ENTER:
        m->menu_screen = m->menu_state + 1;
        m->menu_state = 0;
        break;

    default:
        break;
    }

    /* draw osd reflecting current state */
    for( i=0; i < MENU_LINES/2; i++ ) {
        if( m->menu_state == i ) {
            osd_string_set_colour( m->menu_line[i*2].line, 220, 12, 155 );
        } else {
            osd_string_set_colour( m->menu_line[i*2].line, 200, 128, 128 );
        }
        switch( i ) {
        case 0:
            osd_string_show_text( m->menu_line[i*2].line, "Brightness", 51);
            if( m->menu_state == i ) {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->bright,
                    255, 220, 12, 155);
            } else {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->bright,
                    255, 200, 128, 128);
            }

            osd_databars_show_bar( 
                ((struct pict_data *)(m->menu_data))->bright, 
                videoinput_get_brightness( m->vidin ), 51);
            osd_string_show_text( m->menu_line[i*2+1].line, "", 0 );
            break;

        case 1:
            osd_string_show_text( m->menu_line[i*2].line, "Colour", 51);
            if( m->menu_state == i ) {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->col,
                    255, 220, 12, 155);
            } else {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->col,
                    255, 200, 128, 128);
            }

            osd_databars_show_bar( 
                ((struct pict_data *)(m->menu_data))->col, 
                videoinput_get_colour( m->vidin ), 51);
            osd_string_show_text( m->menu_line[i*2+1].line, "", 0 );
            break;

        case 2:
            osd_string_show_text( m->menu_line[i*2].line, "Contrast", 51);
            if( m->menu_state == i ) {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->cont,
                    255, 220, 12, 155);
            } else {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->cont,
                    255, 200, 128, 128);
            }

            osd_databars_show_bar( 
                ((struct pict_data *)(m->menu_data))->cont, 
                videoinput_get_contrast( m->vidin ), 51);
            osd_string_show_text( m->menu_line[i*2+1].line, "", 0 );
            break;

        case 3:
            osd_string_show_text( m->menu_line[i*2].line, "Hue", 51);
            if( m->menu_state == i ) {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->hue,
                    255, 220, 12, 155);
            } else {
                osd_databars_set_colour( 
                    ((struct pict_data *)(m->menu_data))->hue,
                    255, 200, 128, 128);
            }

            osd_databars_show_bar( 
                ((struct pict_data *)(m->menu_data))->hue, 
                videoinput_get_hue( m->vidin ), 51);
            osd_string_show_text( m->menu_line[i*2+1].line, "", 0 );
            break;

        default:
            osd_string_show_text( m->menu_line[i*2].line, "", 0);
            osd_string_show_text( m->menu_line[i*2+1].line, "", 0);
            break;
        }
    }
}

void pict_blit_scanline( menu_t *m, unsigned char *output,
                         int width, int xpos, int scanline )
{

    if( scanline >= m->menu_line[7].y && scanline < (m->menu_line[7].y + 10) ) {
        osd_databars_composite_packed422_scanline( 
            ((struct pict_data *)(m->menu_data))->hue, 
            output + m->menu_line[7].x*2, output + m->menu_line[7].x*2,
            (m->width * 80) / 100);
    }

    if( scanline >= m->menu_line[3].y && scanline < (m->menu_line[3].y + 10) ) {
        osd_databars_composite_packed422_scanline( 
            ((struct pict_data *)(m->menu_data))->col, 
            output + m->menu_line[3].x*2, output + m->menu_line[3].x*2,
            (m->width * 80) / 100);
    }

    if( scanline >= m->menu_line[5].y && scanline < (m->menu_line[5].y + 10) ) {
        osd_databars_composite_packed422_scanline( 
            ((struct pict_data *)(m->menu_data))->cont, 
            output + m->menu_line[5].x*2, output + m->menu_line[5].x*2,
            (m->width * 80) / 100);
    }

    if( scanline >= m->menu_line[1].y && scanline < (m->menu_line[1].y + 10) ) {
        osd_databars_composite_packed422_scanline( 
            ((struct pict_data *)(m->menu_data))->bright, 
            output + m->menu_line[1].x*2, output + m->menu_line[1].x*2,
            (m->width * 80) / 100);
    }

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

void menu_refresh( menu_t *m ) {
    switch( m->menu_screen ) {
    case MENU_MAIN:
        menu_main( m, 0 );
        break;

    case MENU_PICT:
        menu_pict( m, 0 );
        break;

    case MENU_AUDIO:
        menu_audio( m, 0 );
        break;

    case MENU_CHANNELS:
        menu_channels( m, 0 );
        break;

    case MENU_PARENTAL:
        menu_parental( m, 0 );
        break;

    case MENU_PVR:
        menu_pvr( m, 0 );
        break;

    case MENU_SETUP:
        menu_setup( m, 0 );
        break;

    case MENU_TVGUIDE:
        menu_tvguide( m, 0 );
        break;

    case MENU_INPUT:
        menu_input( m, 0 );
        break;

    case MENU_PREVIEW:
        menu_preview( m, 0 );
        break;

    default:
        break;
    }

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
                if( m->menu_screen == MENU_MAIN ) {
                    input_toggle_menu( m->in );
                    m->menu_screen = MENU_MAIN;
                    /* now remove OSD */
                    m->visible = 0;
                    for( i=0; i < MENU_LINES; i++ ) {
                        osd_string_show_text(m->menu_line[i].line, "", 0);
                    }
                    return 1;
                }
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
                m->visible = 0;
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

void menu_composite_packed422_scanline( menu_t *m, unsigned char *output,
                                        int width, int xpos, int scanline )
{
    int i;

    if( !m ) return;

    if( m->visible && scanline >= m->y && scanline < m->y + m->height ) {

        blit_colour_packed422_scanline( output + (m->x*2), m->width,
                                        m->bg_luma, m->bg_cb, m->bg_cr );

        if( menu_blit_scanline ) 
            menu_blit_scanline( m, output, width, xpos, scanline );

        for( i = 0; i < MENU_LINES; i++ ) {
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
}

