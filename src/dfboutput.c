
#include "config.h"
#include "dfboutput.h"

#ifdef HAVE_DIRECTFB

/* directfb includes */
#include <directfb.h>

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
static DFBSurfacePixelFormat  frame_format;
static IDirectFBInputDevice  *keyboard;
static IDirectFBEventBuffer  *buffer = NULL;

static int screen_width = 0;
static int screen_height = 0;
static void *screen_framebuffer = 0;
static int screen_pitch = 0;

void dfb_lock_output_buffer( void )
{
    c2frame->Lock( c2frame, DSLF_WRITE, &screen_framebuffer, &screen_pitch );
}

void dfb_unlock_output_buffer( void )
{
    c2frame->Unlock( c2frame );
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

int dfb_init( int inputwidth, int inputheight, int outputwidth, int aspect )
{
    DFBDisplayLayerConfig dlc;
    DFBDisplayLayerConfigFlags failed;

    DirectFBInit( 0, 0 );

    DirectFBSetOption( "fbdev", fb_dev_name );
    DirectFBSetOption( "no-cursor", "" );
    DirectFBSetOption( "bg-color", "00000000" );
    DirectFBSetOption( "matrox-crtc2", "" );
    DirectFBSetOption( "matrox-tv-standard", (inputheight == 576) ? "pal" : "ntsc" );

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

    dlc.flags      = DLCONF_PIXELFORMAT; // | DLCONF_BUFFERMODE;
    dlc.buffermode = 0; // DLBM_BACKVIDEO;
    dlc.pixelformat = DSPF_YUY2;

    if( crtc2->TestConfiguration( crtc2, &dlc, &failed ) != DFB_OK ) {
        fprintf( stderr, "config tested and failed.\n" );
        dfb_shutdown();
        return 0;
    }

    crtc2->SetConfiguration( crtc2, &dlc );
    crtc2->GetSurface( crtc2, &c2frame );

    c2frame->GetSize( c2frame, &screen_width, &screen_height );
    fprintf( stderr, "DirectFB: Screen is %d x %d\n", screen_width, screen_height );

    if( screen_width < inputwidth ) {
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

void dfb_waitforsync( void )
{
    dfb->WaitForSync( dfb );
}

int dfb_is_interlaced( void )
{
    return 1;
}

void dfb_wait_for_sync( int field )
{
    while( dfb_get_current_output_field() != field ) {
    //do {
        dfb->WaitForSync( dfb );
    //} while( dfb_get_current_output_field() != field );
    }
}

void dfb_show_frame( void )
{
}

int dfb_toggle_aspect( void )
{
    return 0;
}

int dfb_toggle_fullscreen( void )
{
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
                // event.modifiers
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
     * keyboard repeat is faster than key handling and this causes problems during seek
     * temporary workabout. should be solved in the future
     */
    buffer->Reset( buffer );
    return;
}

void dfb_set_window_caption( const char *caption )
{
}

output_api_t dfboutput =
{
    dfb_init,
    dfb_lock_output_buffer,
    dfb_get_output_buffer,
    dfb_get_output_stride,
    dfb_unlock_output_buffer,

    dfb_is_interlaced,
    dfb_wait_for_sync,

    dfb_show_frame,

    dfb_toggle_aspect,
    dfb_toggle_fullscreen,
    dfb_set_window_caption,

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

