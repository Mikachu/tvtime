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
#include <SDL/SDL.h>
#include "frequencies.h"
#include "videoinput.h"
#include "sdloutput.h"

static SDL_Surface *screen = 0;
static SDL_Overlay *frame = 0;
static int fs = 0;

unsigned char *sdl_get_output( void )
{
    return frame->pixels[ 0 ];
}

int sdl_init( int width, int height, int outputwidth, int aspect )
{
    SDL_Rect **modes;
    int ret, i;

    /* Initialize SDL. */
    ret = SDL_Init( SDL_INIT_VIDEO );
    if( ret < 0 ) {
        fprintf( stderr, "sdloutput: SDL_Init failed.\n" );
        return 0;
    } 

    if( !outputwidth ) {
        /* Get available fullscreen/hardware modes, choose largest. */
        modes = SDL_ListModes( 0, SDL_FULLSCREEN | SDL_HWSURFACE );

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
    if( aspect ) {
        /* Run in 16:9 mode. */
        screen = SDL_SetVideoMode( outputwidth, outputwidth / 16 * 9, 0, 0 );
    } else {
        screen = SDL_SetVideoMode( outputwidth, outputwidth / 4 * 3, 0, 0 );
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

int sdl_poll_events( void )
{
    SDL_Event event;
    int curcommand = 0;

    SDL_PumpEvents();
    if( SDL_PeepEvents( &event, 1, SDL_PEEKEVENT, SDL_KEYDOWNMASK ) > 0 ) {
        if( event.key.keysym.mod & KMOD_SHIFT ) {
            SDL_PollEvent( &event );
            curcommand |= TVTIME_CHANNEL_CHAR;

            curcommand = (curcommand & 0xFFFF0000) | (event.key.keysym.sym & 0xFF);            
            return curcommand;
        }
    }

    while( SDL_PollEvent( &event ) ) {

        if( event.type == SDL_QUIT ) {
            curcommand |= TVTIME_QUIT;
        }

        if( event.type == SDL_KEYDOWN ) {
            switch ( event.key.keysym.sym ) {

            /* Escape and Q are our quit keys. */
            case SDLK_ESCAPE:
            case SDLK_q:
                curcommand |= TVTIME_QUIT;
                break;

            /* f toggles fullscreen. */
            case SDLK_f:
                (void) SDL_WM_ToggleFullScreen( screen );
                fs = !fs;
                break;
            
            /* k decreases the amount of luma correction */
            case SDLK_k:
                curcommand |= TVTIME_LUMA_DOWN;
                break;

            /* l increases the amount of gamma correction */
            case SDLK_l:
                curcommand |= TVTIME_LUMA_UP;
                break;

            /* down arrows make the channel decrease */
            case SDLK_DOWN:
                curcommand |= TVTIME_CHANNEL_DOWN;
                break;

            /* up arrows make the channel increase */
            case SDLK_UP:
                curcommand |= TVTIME_CHANNEL_UP;
                break;

            case SDLK_m:
                curcommand |= TVTIME_MIXER_MUTE;
                break;

            case SDLK_KP_PLUS:
                curcommand |= TVTIME_MIXER_UP;
                break;

            case SDLK_KP_MINUS:
                curcommand |= TVTIME_MIXER_DOWN;
                break;

            case SDLK_KP0:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP0;
                break;

            case SDLK_KP1:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP1;
                break;

            case SDLK_KP2:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP2;
                break;

            case SDLK_KP3:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP3;
                break;

            case SDLK_KP4:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP4;
                break;

            case SDLK_KP5:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP5;
                break;

            case SDLK_KP6:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP6;
                break;

            case SDLK_KP7:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP7;
                break;

            case SDLK_KP8:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP8;
                break;

            case SDLK_KP9:
                curcommand |= TVTIME_DIGIT;
                curcommand |= TVTIME_KP9;
                break;

            case SDLK_d:
                curcommand |= TVTIME_DEBUG;
                break;

            case SDLK_KP_ENTER:
                curcommand |= TVTIME_KP_ENTER;
                break;

            /* Picture controls */
            case SDLK_F1:
                curcommand |= TVTIME_HUE_DOWN;
                break;
            case SDLK_F2:
                curcommand |= TVTIME_HUE_UP;
                break;
            case SDLK_F3:
                curcommand |= TVTIME_BRIGHT_DOWN;
                break;
            case SDLK_F4:
                curcommand |= TVTIME_BRIGHT_UP;
                break;
            case SDLK_F5:
                curcommand |= TVTIME_CONT_DOWN;
                break;
            case SDLK_F6:
                curcommand |= TVTIME_CONT_UP;
                break;
            case SDLK_F7:
                curcommand |= TVTIME_COLOUR_DOWN;
                break;
            case SDLK_F8:
                curcommand |= TVTIME_COLOUR_UP;
                break;
            case SDLK_F12:
                curcommand |= TVTIME_SHOW_TEST;
                break;

            default:
                break;
            }
            
            /* just handle one digit at a time */
            if( curcommand & TVTIME_DIGIT ) {
                break;
            }
        }
    }

    if( curcommand & TVTIME_QUIT ) {
        if( fs ) {
            SDL_WM_ToggleFullScreen( screen );
            fs = 0;
        }
        SDL_UnlockYUVOverlay( frame );
        SDL_FreeYUVOverlay( frame );
        SDL_Quit();
    }

    return curcommand;
}

