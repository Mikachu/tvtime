/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "mga_vid.h"
#include "mgaoutput.h"
#include "speedy.h"

static mga_vid_config_t mga_config;
static uint8_t *mga_vid_base;
static uint8_t *backbuffer;
static int mga_fd;
static int mga_input_width;
static int mga_width;
static int mga_height;
static int mga_stride;
static int mga_frame_size;
static int mga_outheight;
static int curframe;

static int mga_init( int outputheight, int aspect, int verbose )
{
    mga_fd = open( "/dev/mga_vid", O_RDWR );
    if( mga_fd < 0 ) {
        return 0;
    }

    mga_config.version = MGA_VID_VERSION;
    mga_outheight = outputheight;

    return 1;
}

static void mga_reconfigure( void )
{
    mga_config.src_width = mga_input_width;
    mga_config.src_height = mga_height;
    mga_config.x_org = 0;
    mga_config.y_org = 0;

    /* Assume square pixels. */
    mga_config.dest_height = mga_outheight;
    mga_config.dest_width = (mga_outheight * 4) / 3;

    mga_config.colkey_on = 0;
    mga_config.format = MGA_VID_FORMAT_YUY2;
    mga_config.frame_size = mga_frame_size;
    mga_config.num_frames = 2;

    if( ioctl( mga_fd, MGA_VID_CONFIG, &mga_config ) ) {
        fprintf( stderr, "mgaoutput: Error in config ioctl: %s\n", strerror( errno ) );
    }
}

static int mga_set_input_size( int inputwidth, int inputheight )
{
    mga_width = (inputwidth + 31) & ~31;
    mga_stride = mga_width * 2;
    mga_height = inputheight;
    mga_frame_size = mga_stride * mga_height;
    mga_input_width = inputwidth;

    mga_reconfigure();

    curframe = 0;

    fprintf( stderr, "mgaoutput: MGA %s Backend Scaler with %d MB of RAM.\n",
             mga_config.card_type == MGA_G200 ? "G200" : "G400", mga_config.ram_size );

    ioctl( mga_fd, MGA_VID_ON, 0 );
    mga_vid_base = mmap( 0, mga_frame_size * mga_config.num_frames, PROT_WRITE, MAP_SHARED, mga_fd, 0 );
    memset( mga_vid_base, 0, mga_frame_size );
    backbuffer = malloc( mga_frame_size * mga_config.num_frames );

    return 1;
}

static void mga_lock_output_buffer( void )
{
}

static uint8_t *mga_get_output_buffer( void )
{
    return backbuffer;
    // return mga_vid_base + (curframe*mga_frame_size);
}

static int mga_get_output_stride( void )
{
    return mga_width * 2;
}

static void mga_unlock_output_buffer( void )
{
}

static int mga_is_exposed( void )
{
    return 1;
}

static int mga_get_visible_width( void )
{
    return 0;
}

static int mga_get_visible_height( void )
{
    return 0;
}

static int mga_is_fullscreen( void )
{
    return 1;
}

static int mga_is_interlaced( void )
{
    return 0;
}

static void mga_wait_for_sync( int field )
{
}

static int mga_show_frame( int x, int y, int width, int height )
{
    // static int foobar = 0;
    uint8_t *base = mga_vid_base + (curframe*mga_frame_size);
    int i;

    for( i = 0; i < mga_height; i++ ) {
        // blit_colour_packed422_scanline( base + (i * mga_get_output_stride()),
        //                                 mga_input_width, foobar, 128, 128 );
        blit_packed422_scanline( base + (i * mga_stride), backbuffer + (i * mga_stride), width );
    }
    // foobar = (foobar + 1) % 256;

    ioctl( mga_fd, MGA_VID_FSEL, &curframe );
    curframe = (curframe + 1) % mga_config.num_frames;
    return 1;
}

static int mga_toggle_aspect( void )
{
    return 0;
}

static int mga_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
    return 1;
}

static void mga_set_window_caption( const char *caption )
{
}

static void mga_set_window_height( int window_height )
{
}

static void mga_poll_events( input_t *in )
{
}

static void mga_shutdown( void )
{
    ioctl( mga_fd, MGA_VID_OFF, 0 );
    close( mga_fd );
}

static output_api_t mgaoutput =
{
    mga_init,

    mga_set_input_size,

    mga_lock_output_buffer,
    mga_get_output_buffer,
    mga_get_output_stride,
    mga_unlock_output_buffer,

    mga_is_exposed, 
    mga_get_visible_width, 
    mga_get_visible_height, 
    mga_is_fullscreen,

    mga_is_interlaced,
    mga_wait_for_sync,
    mga_show_frame,

    mga_toggle_aspect,
    mga_toggle_fullscreen,
    mga_set_window_caption,

    mga_set_window_height,

    mga_poll_events,
    mga_shutdown
};


output_api_t *get_mga_output( void )
{
    return &mgaoutput;
}

