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
#include "frequencies.h"
#include "videoinput.h"
#include "sdloutput.h"
#include "input.h"

static SDL_Surface *screen = 0;
static SDL_Overlay *frame = 0;
static int sdlaspect = 0;
static int outwidth = 0;
static int fs = 0;
static int sdlflags = SDL_HWSURFACE | SDL_RESIZABLE;

unsigned char *sdl_get_output( void )
{
    return frame->pixels[ 0 ];
}

int sdl_init( int width, int height, int outputwidth, int aspect )
{
    SDL_Rect **modes;
    int ret, i;

    aspect = sdlaspect;
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

    /* Create overlay surface. */
    frame = SDL_CreateYUVOverlay( width, height, SDL_YUY2_OVERLAY, screen );
    if( !frame ) {
        fprintf( stderr, "sdloutput: Couldn't create overlay surface.\n" );
        return 0;
    }
    SDL_LockYUVOverlay( frame );

    SDL_WM_SetCaption( "tvtime: ... smooth like that ...", 0 );
    SDL_ShowCursor( 0 );
    SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
    return 1;
}


void sdl_toggle_fullscreen( void )
{
    SDL_WM_ToggleFullScreen( screen );
    fs = !fs;
}

static void sdl_reset_display( void )
{
    SDL_UnlockYUVOverlay( frame );
    if( sdlaspect ) {
        /* Run in 16:9 mode. */
        screen = SDL_SetVideoMode( outwidth, outwidth / 16 * 9, 0, sdlflags );
    } else {
        screen = SDL_SetVideoMode( outwidth, outwidth / 4 * 3, 0, sdlflags );
    }
    SDL_LockYUVOverlay( frame );
}

void sdl_toggle_aspect( void )
{
    sdlaspect = !sdlaspect;
    sdl_reset_display();
}


void sdl_show_frame( void )
{
    SDL_Rect r;

    r.x = 0;
    r.y = 0;
    r.w = screen->w;
    r.h = screen->h;

    SDL_UnlockYUVOverlay( frame );
    SDL_DisplayYUVOverlay( frame, &r );
    SDL_LockYUVOverlay( frame );
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
            sdl_reset_display();
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
    }
}

void sdl_quit( void )
{
    if( fs ) {
        SDL_WM_ToggleFullScreen( screen );
        fs = 0;
    }
    SDL_UnlockYUVOverlay( frame );
    SDL_FreeYUVOverlay( frame );
    SDL_Quit();
}

