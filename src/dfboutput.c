/**
 * Copyright (C) 2002, 2003 Ville Syrjala <syrjala@sci.fi>
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

#include "config.h"
#include "dfboutput.h"

#ifdef HAVE_DIRECTFB && direcffb_major_version >= 9

#include <stdio.h>
#include <directfb.h>

#include "utils.h"
#include "input.h"

static void *image_data;
static int image_pitch;

static IDirectFB *dfb;
static IDirectFBDisplayLayer *primary;
static IDirectFBDisplayLayer *video;
static IDirectFBWindow *window;
static IDirectFBSurface *wsurface;
static IDirectFBSurface *surface;
static IDirectFBEventBuffer *buffer;

static DFBColor colorkey;

static int input_width, input_height;
static int output_aspect = 0;
static int output_fullscreen = 0;
static int deinterlacing = 1;
static int alwaysontop = 0;
static int fullscreen_pos = 0;

static DFBRectangle *current_rect;
static DFBRectangle fullscreen_rect;
static DFBRectangle window_rect;

static void resize_video( void )
{
     DFBDisplayLayerConfig dlc;

     primary->GetConfiguration( primary, &dlc );

     video->SetScreenLocation( video,
                               (float) current_rect->x / (float) dlc.width,
                               (float) current_rect->y / (float) dlc.height,
                               (float) current_rect->w / (float) dlc.width,
                               (float) current_rect->h / (float) dlc.height );
}

static void resize_window( void )
{
     window->Resize( window, current_rect->w, current_rect->h );
     wsurface->Clear( wsurface, colorkey.r, colorkey.g, colorkey.b, 0xff );
     wsurface->Flip( wsurface, 0, 0 );
}

static void move_window( void )
{
     window->MoveTo( window, current_rect->x, current_rect->y );
}

static void window_alwaysontop( void )
{
     if (alwaysontop) {
          window->SetOptions( window, DWOP_KEEP_STACKING );
          window->SetStackingClass( window, DWSC_UPPER );
          window->RaiseToTop( window );
     } else {
          window->SetOptions( window, DWOP_NONE );
          window->SetStackingClass( window, DWSC_MIDDLE );
     }
}

static void calc_size( DFBRectangle *rect,
                       int width, int height, int aspect )
{
     int widthratio = aspect ? 16 : 4;
     int heightratio = aspect ? 9 : 3;
     if (width < 0 || width * heightratio / widthratio > height)
          width = height * widthratio / heightratio;
     else
          height = width * heightratio / widthratio;

     output_aspect = aspect;

     rect->w = width;
     rect->h = height;
}

static void
calc_fullscreen_size( void )
{
     DFBDisplayLayerConfig dlc;

     primary->GetConfiguration( primary, &dlc );

     calc_size( &fullscreen_rect, dlc.width, dlc.height, output_aspect );
     fullscreen_rect.x = (dlc.width - fullscreen_rect.w) / 2;
     switch (fullscreen_pos) {
          case 0:
               fullscreen_rect.y = (dlc.height - fullscreen_rect.h) / 2;
               break;
          case 1:
               fullscreen_rect.y = 0;
               break;
          case 2:
               fullscreen_rect.y = dlc.height - fullscreen_rect.h;
               break;
     }
}

/* public */

static int dfb_init( int outputheight, int aspect, int verbose )
{
     DFBDisplayLayerConfig dlc;
     DFBWindowDescription wd;

     current_rect = &window_rect;

     calc_size( current_rect, -1, outputheight, aspect );

     DirectFBInit( 0, 0 );
     DirectFBCreate( &dfb );

     /* Get the video layer */
     if (dfb->GetDisplayLayer( dfb, 1, &video )) {
          dfb->Release( dfb );
          return 0;
     }
     if (video->SetCooperativeLevel( video, DLSCL_EXCLUSIVE )) {
          video->Release( video );
          dfb->Release( dfb );
          return 0;
     }

     /* Get the primary layer */
     if (dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &primary )) {
          video->Release( video );
          dfb->Release( dfb );
          return 0;
     }
     if (primary->SetCooperativeLevel( primary, DLSCL_SHARED )) {
          primary->Release( primary );
          video->Release( video );
          dfb->Release( dfb );
          return 0;
     }

     primary->GetConfiguration( primary, &dlc );
     colorkey.r = 0x01;
     colorkey.g = 0x01;
     colorkey.b = 0xfe;
     switch (dlc.pixelformat) {
          case DSPF_ARGB1555:
               colorkey.r <<= 3;
               colorkey.g <<= 3;
               colorkey.b <<= 3;
               break;
          case DSPF_RGB16:
               colorkey.r <<= 3;
               colorkey.g <<= 2;
               colorkey.b <<= 3;
               break;
          default:
               ;
     }
     video->SetDstColorKey( video, colorkey.r, colorkey.g, colorkey.b );

     /* Create a window to house the video layer */
     wd.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT;
     wd.posx = current_rect->x;
     wd.posy = current_rect->y;
     wd.width = current_rect->w;
     wd.height = current_rect->h;
     primary->CreateWindow( primary, &wd, &window );

     window->GetSurface( window, &wsurface );
     wsurface->Clear( wsurface, colorkey.r, colorkey.g, colorkey.b, 0xff );
     wsurface->Flip( wsurface, 0, 0 );
     window->SetOpacity( window, 0xff );

     /* Select events */
     window->DisableEvents( window, DWET_ALL );
     window->EnableEvents( window, DWET_BUTTONDOWN | DWET_KEYDOWN |
                           DWET_WHEEL  | DWET_POSITION | DWET_SIZE );
     window->CreateEventBuffer( window, &buffer );

     return 1;
}



static int dfb_set_input_size( int inputwidth, int inputheight )
{
     DFBDisplayLayerConfig dlc;
     DFBDisplayLayerConfigFlags failed;

     input_width = inputwidth;
     input_height = inputheight;

     /* Set configuration accroding to input data */
     dlc.flags = DLCONF_BUFFERMODE | DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_OPTIONS;
#ifdef DIRECTFB_HAS_TRIPLE
     dlc.buffermode = DLBM_TRIPLE;
#else
    dlc.buffermode = DLBM_BACKVIDEO;
#endif
     dlc.width = input_width;
     dlc.height = input_height;
     dlc.pixelformat = DSPF_YUY2;
     dlc.options = DLOP_DST_COLORKEY;
     if (deinterlacing)
          dlc.options |= DLOP_DEINTERLACING;
     if (video->TestConfiguration( video, &dlc, &failed ))
          return 0;
     if (video->SetConfiguration( video, &dlc ))
          return 0;

     video->GetSurface( video, &surface );
     surface->Clear( surface, 0, 0, 0, 0xff );
     surface->Flip( surface, 0, 0 );
     surface->Clear( surface, 0, 0, 0, 0xff );
     surface->Flip( surface, 0, 0 );
     surface->Clear( surface, 0, 0, 0, 0xff );

     video->SetOpacity( video, 0xff );

     resize_video();

     return 1;
}



static void dfb_lock_output_buffer( void )
{
     surface->Lock( surface, DSLF_WRITE,
                    &image_data, &image_pitch );
}

static uint8_t *dfb_get_output_buffer( void )
{
     return image_data;
}

static int dfb_get_output_stride( void )
{
     return image_pitch;
}

static int dfb_can_read_from_buffer( void )
{
     return 1;
}

static void dfb_unlock_output_buffer( void )
{
     surface->Unlock( surface );
}



static int dfb_is_exposed( void )
{
     return 1;
}

static int dfb_get_visible_width( void )
{
     return current_rect->w;
}

static int dfb_get_visible_height( void )
{
     return current_rect->h;
}

static int dfb_is_fullscreen( void )
{
     return output_fullscreen;
}

static int dfb_is_alwaysontop( void )
{
    return alwaysontop;
}

static int dfb_is_fullscreen_supported( void )
{
    return 1;
}

static int dfb_is_alwaysontop_supported( void )
{
    return 1;
}

static int dfb_is_interlaced( void )
{
     return deinterlacing;
}

static void dfb_wait_for_sync( int field )
{
     if (deinterlacing)
          surface->SetField( surface, field );
}



static int dfb_show_frame( int x, int y, int width, int height )
{
#ifdef DIRECTFB_HAS_TRIPLE
     surface->Flip( surface, 0, DSFLIP_ONSYNC );
#else
     surface->Flip( surface, 0, 0 );
#endif
     return 1;
}



static int dfb_toggle_aspect( void )
{
     DFBDisplayLayerConfig dlc;

     primary->GetConfiguration( primary, &dlc );

     calc_size( &window_rect, -1, window_rect.h, !output_aspect );
     calc_fullscreen_size();

     move_window();
     resize_window();
     resize_video();

     return output_aspect;
}

static int dfb_toggle_alwaysontop( void )
{
     alwaysontop = !alwaysontop;

     if (!output_fullscreen)
          window_alwaysontop();

     return alwaysontop;
}

static int dfb_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
     output_fullscreen = !output_fullscreen;

     if( output_fullscreen ) {
          current_rect = &fullscreen_rect;

          window->SetOptions( window, DWOP_KEEP_POSITION | DWOP_KEEP_SIZE |
                              DWOP_KEEP_STACKING );
          window->SetStackingClass( window, DWSC_UPPER );
          window->RaiseToTop( window );

          window->GrabPointer( window );
          window->GrabKeyboard( window );
     } else {
          current_rect = &window_rect;

          window->UngrabPointer( window );
          window->UngrabKeyboard( window );

          window_alwaysontop();
     }

     move_window();
     resize_window();
     resize_video();

     return output_fullscreen;
}

static void dfb_resize_window_fullscreen( void )
{
}

static void dfb_set_window_caption( const char *caption )
{
}



static void dfb_set_window_position( int x, int y )
{
     window_rect.x = x;
     window_rect.y = y;

     if (!output_fullscreen) {
          move_window();
          resize_video();
     }
}

static void dfb_set_window_height( int window_height )
{
     calc_size( &window_rect, -1, window_height, output_aspect );

     if (!output_fullscreen) {
          resize_window();
          resize_video();
     }
}

static void dfb_set_fullscreen_position( int pos )
{
     fullscreen_pos = pos;

     calc_fullscreen_size();

     if (output_fullscreen) {
          move_window();
          resize_video();
     }
}

static void dfb_set_matte( int width, int height )
{
}



static void dfb_poll_events( input_t *in )
{
     DFBWindowEvent event;

     while (buffer->GetEvent( buffer, DFB_EVENT( &event ) ) == DFB_OK) {
          int arg = 0;

          switch( event.type ) {
          case DWET_KEYDOWN:
               if( event.modifiers & DIMM_SHIFT ) arg |= I_SHIFT;
               if( event.modifiers & DIMM_CONTROL ) arg |= I_CTRL;
               if( event.modifiers & DIMM_META ) arg |= I_META;

               switch( event.key_symbol ) {
               case DIKS_CURSOR_UP: arg |= I_UP; break;
               case DIKS_CURSOR_DOWN: arg |= I_DOWN; break;
               case DIKS_CURSOR_RIGHT: arg |= I_RIGHT; break;
               case DIKS_CURSOR_LEFT: arg |= I_LEFT; break;
               case DIKS_INSERT: arg |= I_INSERT; break;
               case DIKS_HOME: arg |= I_HOME; break;
               case DIKS_END: arg |= I_END; break;
               case DIKS_PAGE_UP: arg |= I_PGUP; break;
               case DIKS_PAGE_DOWN: arg |= I_PGDN; break;
               case DIKS_F1: arg |= I_F1; break;
               case DIKS_F2: arg |= I_F2; break;
               case DIKS_F3: arg |= I_F3; break;
               case DIKS_F4: arg |= I_F4; break;
               case DIKS_F5: arg |= I_F5; break;
               case DIKS_F6: arg |= I_F6; break;
               case DIKS_F7: arg |= I_F7; break;
               case DIKS_F8: arg |= I_F8; break;
               case DIKS_F9: arg |= I_F9; break;
               case DIKS_F10: arg |= I_F10; break;
               case DIKS_F11: arg |= I_F11; break;
               case DIKS_F12: arg |= I_F12; break;
               case DIKS_PRINT: arg |= I_PRINT; break;
               case DIKS_MENU: arg |= I_MENU; break;
               default:
                    /* if (DFB_KEY_IS_ASCII(event.key_symbol)) */
                         arg |= event.key_symbol;
                    break;
               }
               input_callback( in, I_KEYDOWN, arg );
               break;
          case DWET_BUTTONDOWN:
               input_callback( in, I_BUTTONPRESS, event.button );
               break;
          case DWET_WHEEL:
               input_callback( in, I_BUTTONPRESS, event.step > 0 ? 4 : 5 );
               break;
          case DWET_POSITION:
               current_rect->x = event.x;
               current_rect->y = event.y;
               resize_video();
               break;
          case DWET_SIZE:
               current_rect->w = event.w;
               current_rect->h = event.h;
               wsurface->Clear( wsurface, colorkey.r, colorkey.g, colorkey.b, 0xff );
               wsurface->Flip( wsurface, 0, 0 );
               resize_video();
               break;
          default:
               break;
          }
     }
}

static void dfb_shutdown( void )
{
     /* Hide the video layer */
     video->SetOpacity( video, 0 );

     buffer->Release( buffer );
     surface->Release( surface );
     wsurface->Release( wsurface );
     window->Release( window );
     video->Release( video );
     primary->Release( primary );
     dfb->Release( dfb );
}

static int dfb_is_overscan_supported( void )
{
    return 0;
}

static output_api_t dfboutput =
{
     dfb_init,

     dfb_set_input_size,

     dfb_lock_output_buffer,
     dfb_get_output_buffer,
     dfb_get_output_stride,
     dfb_can_read_from_buffer,
     dfb_unlock_output_buffer,

     dfb_is_exposed,
     dfb_get_visible_width,
     dfb_get_visible_height,
     dfb_is_fullscreen,
     dfb_is_alwaysontop,

     dfb_is_fullscreen_supported,
     dfb_is_alwaysontop_supported,
     dfb_is_overscan_supported,

     dfb_is_interlaced,
     dfb_wait_for_sync,
     dfb_show_frame,

     dfb_toggle_aspect,
     dfb_toggle_alwaysontop,
     dfb_toggle_fullscreen,
     dfb_resize_window_fullscreen,
     dfb_set_window_caption,

     dfb_set_window_position,
     dfb_set_window_height,
     dfb_set_fullscreen_position,
     dfb_set_matte,

     dfb_poll_events,
     dfb_shutdown
};

output_api_t *get_dfb_output( void )
{
     return &dfboutput;
}

#else

output_api_t *get_dfb_output( void )
{
     return 0;
}

#endif

