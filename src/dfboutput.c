/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dfboutput.h"

#ifdef HAVE_DIRECTFB
/* directfb includes */
#include <directfb.h>
#define DIRECTFB_VERSION (directfb_major_version*100+directfb_minor_version)*100+directfb_micro_version

/* other things */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <linux/fb.h>

static IDirectFB             *dfb;
static IDirectFBDisplayLayer *crtc2;
static IDirectFBSurface      *c2frame;
static IDirectFBSurface      *current_frame;
static DFBSurfacePixelFormat  frame_format;
static IDirectFBInputDevice  *keyboard;
static IDirectFBEventBuffer  *buffer = NULL;

static unsigned int output_width = 0;
static unsigned int output_height = 0;
static int input_width = 0;
static int input_height = 0;

static int parity = 0;

static void *screen_framebuffer = 0;
static int screen_pitch = 0;

void dfb_lock_output_buffer( void )
{
    current_frame->Lock( current_frame, DSLF_WRITE|DSLF_READ, 
                         &screen_framebuffer, &screen_pitch );
}

void dfb_unlock_output_buffer( void )
{
    current_frame->Unlock( current_frame );
}

unsigned char *dfb_get_output_buffer( void )
{
    return (unsigned char *) screen_framebuffer;
}

int dfb_get_current_output_field( void )
{
    int fieldid;
    crtc2->GetCurrentOutputField( crtc2, &fieldid );
    return fieldid;
}

int dfb_get_output_stride( void )
{
    return screen_pitch;
}

void dfb_shutdown( void )
{
    if( buffer ) {
        buffer->Release( buffer );
    }
    if( keyboard ) {
        keyboard->Release( keyboard );
    }

    if( current_frame ) {
        current_frame->Release( current_frame );
    }
    
    if( c2frame ) {
        c2frame->Release( c2frame );
    }
    if( crtc2 ) {
        crtc2->Release( crtc2 );
    }

    if( dfb ) {
        dfb->Release( dfb );
    }
}

const char *fb_dev_name = "/dev/fb0";

int dfb_setup_temp_buffer()
{

  /* Draw to a temporary surface */
  DFBSurfaceDescription dsc;

  dsc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
  dsc.width       = input_width;
  dsc.height      = input_height;
  dsc.pixelformat = DSPF_YUY2;
  int res;
  
  if ((res = dfb->CreateSurface( dfb, &dsc, &current_frame )) != DFB_OK) {
      fprintf(stderr,"dgboutput: Can't create surfaces - %s!\n",
              DirectFBErrorString( res ) );
      return -1;
  }

  return 0;
}

int dfb_init( int outputheight, int aspect, int verbose )
{
    DFBDisplayLayerConfig dlc;
    DFBDisplayLayerConfigFlags failed;

    if (verbose)
	fprintf(stderr,"Using directfb version:%d\n",DIRECTFB_VERSION);
    if (DIRECTFB_VERSION < 918)
	fprintf(stderr,"\n*** WARNING: You are using a DirectFB version less than 0.9.18\n"
		       "*** this may lead to less than optimal output\n");

    DirectFBInit( 0, 0 );

    DirectFBSetOption( "fbdev", fb_dev_name );
    DirectFBSetOption( "no-cursor", "" );
    DirectFBSetOption( "bg-color", "00000000" );
    DirectFBSetOption( "matrox-crtc2", "" );
    DirectFBSetOption( "matrox-tv-standard", (outputheight == 576) ? "pal" : "ntsc" );

    DirectFBCreate( &dfb );
    dfb->GetDisplayLayer( dfb, 2, &crtc2 );
    if( !crtc2 ) {
        fprintf( stderr, "damnit! failure, exiting\n" );
        dfb_shutdown();
        return 0;
    }
    crtc2->SetCooperativeLevel( crtc2, DLSCL_EXCLUSIVE );
    dfb->GetInputDevice( dfb, DIDID_KEYBOARD, &keyboard );

    keyboard->CreateEventBuffer( keyboard, &buffer );
    buffer->Reset( buffer );

    dlc.flags      = DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE | DLCONF_OPTIONS;

#ifdef DIRECTFB_HAS_TRIPLE
    if (verbose)
	fprintf(stderr,"Using triple buffering\n");
    dlc.buffermode = DLBM_TRIPLE;/*DLBM_BACKVIDEO;DLBM_FRONTONLY;*/
#else
    if (verbose)
	fprintf(stderr,"Using double buffering\n");
    dlc.buffermode = DLBM_BACKVIDEO;
#endif
    
    dlc.pixelformat = DSPF_YUY2; 
    dlc.options = 0;
    
#ifdef DIRECTFB_HAS_FLICKER_FILTERING
    dlc.options = DLOP_FLICKER_FILTERING;
#endif

#ifdef DIRECTFB_HAS_FIELD_PARITY
    dlc.options |=  DLOP_FIELD_PARITY;
#endif

    if( crtc2->TestConfiguration( crtc2, &dlc, &failed ) != DFB_OK ) {
        fprintf( stderr, "config tested and failed.\n" );
        dfb_shutdown();
        return 0;
    }
    crtc2->SetConfiguration( crtc2, &dlc );

    if (DIRECTFB_VERSION > 917)
    {
	parity = 0;
    }
    else
    {
	parity = 1;
    }

    if (verbose)
	fprintf(stderr,"Using initial field parity:%d\n",parity);
#ifdef DIRECTFB_HAS_FIELD_PARITY
    crtc2->SetFieldParity( crtc2, parity );
#endif
    crtc2->GetSurface( crtc2, &c2frame );

    c2frame->SetBlittingFlags( c2frame, DSBLIT_NOFX );
    c2frame->GetSize( c2frame, &output_width, &output_height );
   
    fprintf( stderr, "DirectFB: Screen is %d x %d\n", 
             output_width, output_height );

    /* Make sure we clear all buffers */
    c2frame->Clear( c2frame, 0, 0, 0, 0xff );
    c2frame->Flip( c2frame, NULL, 0 );
    c2frame->Clear( c2frame, 0, 0, 0, 0xff );
    c2frame->Flip( c2frame, NULL, 0 );
    c2frame->Clear( c2frame, 0, 0, 0, 0xff );

    if( output_width < input_width ) {
        fprintf( stderr, "dfboutput: Screen width not big enough!\n" );
        return 0;
    }

    c2frame->GetPixelFormat( c2frame, &frame_format );

    fprintf( stderr, "DirectFB: Frame format = " );
    switch (frame_format) {
    case DSPF_ARGB:
         fprintf( stderr, "ARGB\n" );
         break;
    case DSPF_RGB32:
         fprintf( stderr, "RGB32\n" );
         break;
    case DSPF_RGB24:
         fprintf( stderr, "RGB24\n" );
         break;
    case DSPF_RGB16:
         fprintf( stderr, "RGB16\n" );
         break;
    case DSPF_RGB15:
         fprintf( stderr, "RGB15\n" );
         break;
    case DSPF_YUY2:
         fprintf( stderr, "YUY2\n" );
         break;
    case DSPF_UYVY:
         fprintf( stderr, "UYVY\n" );
         break;
    case DSPF_YV12:
         fprintf( stderr, "YV12\n" );
         break;
    case DSPF_I420:
         fprintf( stderr, "I420\n" );
         break;
    default:
         fprintf( stderr, "unknown\n" );
    }

    crtc2->SetOpacity( crtc2, 0xff );
    return 1;
}

int dfb_is_interlaced( void )
{
    return 1;
}

void dfb_wait_for_sync( int field )
{
    while( dfb_get_current_output_field() != field ) {
    /*do { */
        crtc2->WaitForSync( crtc2 );
    /*} while( dfb_get_current_output_field() != field ); */
    }
}

int dfb_show_frame( int x, int y, int width, int height )
{

    IDirectFBSurface *blitsrc = current_frame;
    /* Allow for input versus output height here later FIX */
    /* c2frame->StretchBlit( c2frame, blitsrc, NULL, NULL); */
    c2frame->Blit( c2frame, blitsrc, NULL, 0,0);
    
#ifdef DIRECTFB_HAS_TRIPLE
    c2frame->Flip( c2frame, NULL,0);
#else
    c2frame->Flip( c2frame, NULL,DSFLIP_WAITFORSYNC );
#endif
    
    return 1;
}

int dfb_toggle_aspect( void )
{
    /* Not supported yet */
    return 0;
}

int dfb_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
    /* Doesn't make sense for tv-output */
    return 1;
}

void dfb_poll_events( input_t *in )
{
    DFBInputEvent event;
    int curcommand = 0, arg = 0;

    if( buffer->GetEvent( buffer, DFB_EVENT( &event ) ) == DFB_OK ) {
        if( event.type == DIET_KEYPRESS ) {
            char cur;

            curcommand = I_KEYDOWN;

            if( event.flags & DIEF_MODIFIERS ) {
                /* event.modifiers */
            }

            switch( event.key_id ) {
                case DIKI_ESCAPE:     arg |= I_ESCAPE; break;
                case DIKI_KP_PLUS:    arg |= '+'; break;
                case DIKI_KP_MINUS:   arg |= '-'; break;
                case DIKI_KP_DIV:     arg |= '/'; break;
                case DIKI_KP_DECIMAL: arg |= '.'; break;
                case DIKI_KP_MULT:    arg |= '*'; break;
                case DIKI_KP_EQUAL:   arg |= '='; break;

                case DIKI_KP_0:  arg |= '0'; break;
                case DIKI_KP_1:  arg |= '1'; break;
                case DIKI_KP_2:  arg |= '2'; break;
                case DIKI_KP_3:  arg |= '3'; break;
                case DIKI_KP_4:  arg |= '4'; break;
                case DIKI_KP_5:  arg |= '5'; break;
                case DIKI_KP_6:  arg |= '6'; break;
                case DIKI_KP_7:  arg |= '7'; break;
                case DIKI_KP_8:  arg |= '8'; break;
                case DIKI_KP_9:  arg |= '9'; break;

                case DIKI_KP_ENTER:  arg |= I_ENTER; break;

                case DIKI_F1:  arg |= I_F1; break;
                case DIKI_F2:  arg |= I_F2; break;
                case DIKI_F3:  arg |= I_F3; break;
                case DIKI_F4:  arg |= I_F4; break;
                case DIKI_F5:  arg |= I_F5; break;
                case DIKI_F6:  arg |= I_F6; break;
                case DIKI_F7:  arg |= I_F7; break;
                case DIKI_F8:  arg |= I_F8; break;
                case DIKI_F9:  arg |= I_F9; break;
                case DIKI_F10: arg |= I_F10; break;
                case DIKI_F11: arg |= I_F11; break;
                case DIKI_F12: arg |= I_F12; break;

                case DIKI_LEFT:  arg |= I_LEFT; break;
                case DIKI_UP:    arg |= I_UP; break;
                case DIKI_DOWN:  arg |= I_DOWN; break;
                case DIKI_RIGHT: arg |= I_RIGHT; break;


                case DIKI_INSERT: 
#ifdef DIRECTFB_HAS_FIELD_PARITY
		    if (parity == 0) parity = 1; else parity = 0;
		    fprintf(stderr,"Setting parity to:%d\n",parity);
		    crtc2->SetFieldParity( crtc2, parity );
#endif
                    break;
                default:
                    cur = 'a' + ( event.key_id - DIKI_A );
                    arg |= cur;
                    break;
            }
            input_callback( in, curcommand, arg );
        }
    }

    /*
     * empty buffer, because of repeating
     * keyboard repeat is faster than key handling and this 
     * causes problems during seek
     * temporary workabout. should be solved in the future
     */
    buffer->Reset( buffer );
    return;
}

void dfb_set_window_caption( const char *caption )
{
    /* Doesn't make sense for tv-out */
}

int dfb_is_exposed( void ) 
{ 
    /* Always exposed for tv-out */
    return 1; 
} 
 
int dfb_get_visible_width( void ) 
{ 
    return output_width; 
} 
 
int dfb_get_visible_height( void ) 
{ 
    return output_height; 
} 

int dfb_set_input_size( int inputwidth, int inputheight )
{
    int need_input_resize = (current_frame == NULL ||
                            (input_width != inputwidth) || (input_height != inputheight));

    input_width = inputwidth;
    input_height = inputheight;
    fprintf(stderr,"Set input size: %dx%d\n",inputwidth,inputheight);
    if( need_input_resize ) {
        dfb_setup_temp_buffer();
    }
    return 1;
}

int dfb_is_fullscreen( void )
{
    /* Tv-out is always full screen */
    return 1;
}

void dfb_set_window_height( int window_height )
{
    /* Dfb Tv-out does not support height modifications */
}

int dfb_can_read_from_buffer( void )
{
    return 0;
}

output_api_t dfboutput =
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

    dfb_is_interlaced,
    dfb_wait_for_sync,
    dfb_show_frame,

    dfb_toggle_aspect,
    dfb_toggle_fullscreen,
    dfb_set_window_caption,

    dfb_set_window_height,

    dfb_poll_events,
    dfb_shutdown
};

output_api_t *get_dfb_output( void )
{
    return &dfboutput;
}

#else /* no DirectFB support */

output_api_t *get_dfb_output( void )
{
    return 0;
}

#endif

