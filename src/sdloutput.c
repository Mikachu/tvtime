/**
 * Copyright (C) 2001 Billy Biggs <vektor@dumbterm.net>.
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
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <SDL/SDL.h>
#include <time.h>
#include "frequencies.h"
#include "videoinput.h"
#include "sdloutput.h"
#include "input.h"
#include "taglines.h"

const int resize_frame_delay = 10;

static SDL_Surface *screen = 0;
static SDL_Overlay *frame = 0;
static int sdlaspect = 0;
static int outwidth = 0;
static int outheight = 0;
static int fs = 0;
static int sdlflags = SDL_HWSURFACE | SDL_RESIZABLE;
static int needresize = 0;

static int window_width = 0;
static int window_height = 0;

unsigned char *sdl_get_output( void )
{
    return frame->pixels[ 0 ];
}

static void sdl_reset_display( void )
{
    screen = SDL_SetVideoMode( outwidth, outheight, 0, sdlflags );
}

int sdl_init( int inputwidth, int inputheight, int outputwidth, int aspect )
{
    SDL_Rect **modes;
    const char *tagline;
    int ret, i;

    srand( time( 0 ) );
    tagline = taglines[ rand() % numtaglines ];

    sdlaspect = aspect;
    outwidth = outputwidth;

    /* Initialize SDL. */
    ret = SDL_Init( SDL_INIT_VIDEO );
    if( ret < 0 ) {
        fprintf( stderr, "sdloutput: SDL_Init failed.\n" );
        return 0;
    } 

    if( !outputwidth ) {
        /* Get available fullscreen/hardware modes, choose largest. */
        modes = SDL_ListModes( 0, sdlflags );

        if( modes == (SDL_Rect **) 0 || modes == (SDL_Rect **) -1 ) {
            fprintf( stderr, "sdloutput: No mode information available. "
                             "Assuming width of 800.\n" );
        } else {
            /* Find largest mode. */
            for( i = 0; modes[ i ]; ++i ) {
                if( modes[ i ]->w > outputwidth ) outputwidth = modes[ i ]->w;
            }
        }
    }

    /* Create screen surface. */
    /* Unfortunately, we always assume square pixels for now. */
    if( sdlaspect ) {
        /* Run in 16:9 mode. */
        screen = SDL_SetVideoMode( outputwidth, outputwidth / 16 * 9, 0, sdlflags );
    } else {
        screen = SDL_SetVideoMode( outputwidth, outputwidth / 4 * 3, 0, sdlflags );
    }
    if( !screen ) {
        fprintf( stderr, "sdloutput: Can't open output!\n" );
        return 0;
    }
    outheight = screen->h;

    /* Create overlay surface. */
    frame = SDL_CreateYUVOverlay( inputwidth, inputheight,
                                  SDL_YUY2_OVERLAY, screen );
    if( !frame ) {
        fprintf( stderr, "sdloutput: Couldn't create overlay surface.\n" );
        return 0;
    }
    if( !frame->hw_overlay ) {
        fprintf( stderr, "sdloutput: Can't get a hardware YUY2 overlay surface, giving up.\n" );
        return 0;
    }

    SDL_WM_SetCaption( tagline, 0 );
    SDL_ShowCursor( 0 );
    SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
    return 1;
}

int sdl_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
    if( !fs && fullscreen_width && fullscreen_height ) {
        screen = SDL_SetVideoMode( fullscreen_width, fullscreen_height, 0, sdlflags );
        window_width = outwidth;
        window_height = outheight;
        outwidth = fullscreen_width;
        outheight = fullscreen_height;
    }

    SDL_WM_ToggleFullScreen( screen );

    if( fs ) {
        if( window_width && window_height ) {
            outwidth = window_width;
            outheight = window_height;
        }
        sdl_reset_display();
    }

    fs = !fs;
    return fs;
}

int sdl_toggle_aspect( void )
{
    sdlaspect = !sdlaspect;
    sdl_reset_display();
    return sdlaspect;
}

void sdl_show_frame( void )
{
    SDL_Rect r;
    int curwidth;
    int curheight;
    int widthratio = sdlaspect ? 16 : 4;
    int heightratio = sdlaspect ? 9 : 3;

    if( needresize == 1 ) {
        sdl_reset_display();
    }
    if( needresize ) needresize--;

    curwidth = outwidth;
    curheight = ( outwidth / widthratio ) * heightratio;
    if( curheight > outheight ) {
        curheight = outheight;
        curwidth = ( outheight / heightratio ) * widthratio;
    }

    r.x = ( outwidth - curwidth ) / 2;
    r.y = ( outheight - curheight ) / 2;
    r.w = curwidth;
    r.h = curheight;

    SDL_DisplayYUVOverlay( frame, &r );
}

void sdl_poll_events( input_t *in )
{
    SDL_Event event;
    int curcommand = 0, arg = 0;
    SDLMod mods;

    while( SDL_PollEvent( &event ) ) {

        if( event.type == SDL_QUIT ) {
            curcommand = I_QUIT;
            input_callback( in, curcommand, arg );
        }

        if( event.type == SDL_VIDEORESIZE ) {
            outwidth = event.resize.w;
            outheight = event.resize.h;
            needresize = resize_frame_delay;
        }

        if( event.type == SDL_KEYDOWN ) {
            curcommand = I_KEYDOWN;

            mods = event.key.keysym.mod;
            if( mods & KMOD_SHIFT ) arg |= I_SHIFT;
            if( mods & KMOD_CTRL ) arg |= I_CTRL;
            if( mods & (KMOD_META | KMOD_ALT) ) arg |= I_META;

            switch ( event.key.keysym.sym ) {

            case SDLK_KP_PLUS:     arg |= '+'; break;
            case SDLK_KP_MINUS:    arg |= '-'; break;
            case SDLK_KP_DIVIDE:   arg |= '/'; break;
            case SDLK_KP_PERIOD:   arg |= '.'; break;
            case SDLK_KP_MULTIPLY: arg |= '*'; break;
            case SDLK_KP_EQUALS:   arg |= '='; break;

            case SDLK_KP0: arg |= '0'; break;
            case SDLK_KP1: arg |= '1'; break;
            case SDLK_KP2: arg |= '2'; break;
            case SDLK_KP3: arg |= '3'; break;
            case SDLK_KP4: arg |= '4'; break;
            case SDLK_KP5: arg |= '5'; break;
            case SDLK_KP6: arg |= '6'; break;
            case SDLK_KP7: arg |= '7'; break;
            case SDLK_KP8: arg |= '8'; break;
            case SDLK_KP9: arg |= '9'; break;

            case SDLK_KP_ENTER:
                arg |= I_ENTER;
                break;

            default:
                if( (mods & KMOD_SHIFT) || (mods & KMOD_CAPS) ) {
                    arg |= toupper( event.key.keysym.sym );
                } else {
                    arg |= event.key.keysym.sym;
                }
                break;
            }

            input_callback( in, curcommand, arg );
        }

        if( event.type == SDL_MOUSEBUTTONDOWN ) {

            input_callback( in, I_BUTTONPRESS, event.button.button );

        }
    }
}

void sdl_quit( void )
{
    if( fs ) {
        SDL_WM_ToggleFullScreen( screen );
        fs = 0;
    }
    SDL_FreeYUVOverlay( frame );
    SDL_Quit();
}

int sdl_get_stride( void )
{
    return frame->w * 2;
}

int sdl_is_interlaced( void )
{
    return 0;
}

void sdl_wait_for_sync( int field )
{
}

void sdl_lock_output( void )
{
    SDL_LockYUVOverlay( frame );
}

void sdl_unlock_output( void )
{
    SDL_UnlockYUVOverlay( frame );
}
void sdl_set_window_caption( const char *caption )
{
}

static output_api_t sdloutput =
{
    sdl_init,

    sdl_lock_output,
    sdl_get_output,
    sdl_get_stride,
    sdl_unlock_output,

    sdl_is_interlaced,
    sdl_wait_for_sync,
    sdl_show_frame,

    sdl_toggle_aspect,
    sdl_toggle_fullscreen,
    sdl_set_window_caption,

    sdl_poll_events,
    sdl_quit
};

output_api_t *get_sdl_output( void )
{
    return &sdloutput;
}

