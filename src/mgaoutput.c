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

static mga_vid_config_t mga_config;
static uint8_t *mga_vid_base;
static int mga_fd;
static int mga_width;
static int mga_height;
static int mga_stride;
static int mga_frame_size;
static int curframe;

int mga_init( int outputheight, int aspect, int verbose )
{
    mga_fd = open( "/dev/mga_vid", O_RDWR );
    if( mga_fd < 0 ) {
        return 0;
    }

    mga_config.version = MGA_VID_VERSION;

    return 1;
}

void mga_set_input_size( int inputwidth, int inputheight )
{
    mga_width = (inputwidth + 31) & ~31;
    mga_stride = mga_width * 2;
    mga_height = inputheight;
    mga_frame_size = mga_stride * mga_height;

    mga_config.src_width = inputwidth;
    mga_config.src_height = inputheight;
    mga_config.dest_width = 1024;
    mga_config.dest_height = 768;
    mga_config.x_org = 0;
    mga_config.y_org = 0;
    mga_config.colkey_on = 0;
    mga_config.format = MGA_VID_FORMAT_YUY2;
    mga_config.frame_size = mga_frame_size;
    mga_config.num_frames = 2;

    curframe = 0;

    if( ioctl( mga_fd, MGA_VID_CONFIG, &mga_config ) ) {
        fprintf( stderr, "mgaoutput: Error in config ioctl: %s\n", strerror( errno ) );
    }

    fprintf( stderr, "mgaoutput: MGA %s Backend Scaler with %d MB of RAM.\n",
             mga_config.card_type == MGA_G200 ? "G200" : "G400", mga_config.ram_size );

    ioctl( mga_fd, MGA_VID_ON, 0 );
    mga_vid_base = mmap( 0, mga_frame_size * mga_config.num_frames, PROT_WRITE, MAP_SHARED, mga_fd, 0 );
    fprintf( stderr, "mgaoutput: Base address = %8p.\n", mga_vid_base );
    memset( mga_vid_base, 0, mga_frame_size );
}

void mga_lock_output_buffer( void )
{
}

uint8_t *mga_get_output_buffer( void )
{
    return mga_vid_base + (curframe*mga_frame_size);
}

int mga_get_output_stride( void )
{
    return mga_width * 2;
}

void mga_unlock_output_buffer( void )
{
}

int mga_is_exposed( void )
{
    return 1;
}

int mga_get_visible_width( void )
{
    return 0;
}

int mga_get_visible_height( void )
{
    return 0;
}

int mga_is_fullscreen( void )
{
    return 1;
}

int mga_is_interlaced( void )
{
    return 0;
}

void mga_wait_for_sync( int field )
{
}

int mga_show_frame( int x, int y, int width, int height )
{
    int id = curframe;
    ioctl( mga_fd, MGA_VID_FSEL, &id );
    id = !id;
    return 1;
}

int mga_toggle_aspect( void )
{
    return 0;
}

int mga_toggle_fullscreen( int fullscreen_width, int fullscreen_height )
{
    return 1;
}

void mga_set_window_caption( const char *caption )
{
}

void mga_set_window_height( int window_height )
{
}

void mga_poll_events( input_t *in )
{
}

void mga_shutdown( void )
{
    ioctl( mga_fd, MGA_VID_OFF, 0 );
    close( mga_fd );
}

output_api_t mgaoutput =
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

