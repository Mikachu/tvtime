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
#if defined (__SVR4) && defined (__sun)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif
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
#include "xcommon.h"
#include "speedy.h"

static mga_vid_config_t mga_config;
static uint8_t *mga_vid_base;
static int mga_fd;
static int mga_input_width;
static int mga_width;
static int mga_height;
static int mga_stride;
static int mga_frame_size;
static int curframe;
static area_t video_area;
static area_t window_area;

static int mga_init( int outputheight, int aspect, int verbose )
{
    if( !xcommon_open_display( aspect, outputheight, verbose ) ) {
        return 0;
    }
    video_area = xcommon_get_video_area();
    window_area = xcommon_get_window_area();

    mga_fd = open( "/dev/mga_vid", O_RDWR );
    if( mga_fd < 0 ) {
        fprintf( stderr, "mgaoutput: Can't open /dev/mga_vid: %s\n", strerror( errno ) );
        xcommon_close_display();
        return 0;
    }

    mga_config.version = MGA_VID_VERSION;

    return 1;
}

static void mga_reconfigure( void )
{
    mga_config.src_width = mga_input_width;
    mga_config.src_height = mga_height;
    mga_config.dest_width = video_area.width;
    mga_config.dest_height = video_area.height;
    mga_config.x_org = window_area.x + video_area.x;
    mga_config.y_org = window_area.y + video_area.y;

    mga_config.colkey_on = 1;
    mga_config.colkey_red = 255;
    mga_config.colkey_green = 0;
    mga_config.colkey_blue = 255;

    mga_config.format = MGA_VID_FORMAT_YUY2;
    mga_config.frame_size = mga_frame_size;
    mga_config.num_frames = 3;

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

    xcommon_set_colourkey( 255 << 16 | 255 );

    curframe = 0;

    fprintf( stderr, "mgaoutput: MGA %s Backend Scaler with %d MB of RAM.\n",
             mga_config.card_type == MGA_G200 ? "G200" : "G400", mga_config.ram_size );

    ioctl( mga_fd, MGA_VID_ON, 0 );
    mga_vid_base = mmap( 0, mga_frame_size * mga_config.num_frames, PROT_WRITE, MAP_SHARED, mga_fd, 0 );
    memset( mga_vid_base, 0, mga_frame_size );

    xcommon_clear_screen();
    return 1;
}

static void mga_lock_output_buffer( void )
{
}

static uint8_t *mga_get_output_buffer( void )
{
    return mga_vid_base + (curframe*mga_frame_size);
}

static int mga_get_output_stride( void )
{
    return mga_width * 2;
}

static void mga_unlock_output_buffer( void )
{
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
    area_t newvidarea = xcommon_get_video_area();
    area_t newwinarea = xcommon_get_window_area();

    if( height != mga_height ) {
        mga_height = height;
        mga_reconfigure();
    }
    ioctl( mga_fd, MGA_VID_FSEL, &curframe );
    curframe = (curframe + 1) % mga_config.num_frames;

    xcommon_ping_screensaver();
    xcommon_frame_drawn();
    XSync( xcommon_get_display(), False );

    if( newvidarea.x != video_area.x || newvidarea.y != video_area.y ||
            newvidarea.width != video_area.width || newvidarea.height != video_area.height ||
            newwinarea.x != window_area.x || newwinarea.y != window_area.y ||
            newwinarea.width != window_area.width || newwinarea.height != window_area.height ) {
        video_area = newvidarea;
        window_area = newwinarea;
        mga_reconfigure();
    }

    return 1;
}

static int mga_can_read_from_buffer( void )
{
    return 0;
}

static void mga_shutdown( void )
{
    ioctl( mga_fd, MGA_VID_OFF, 0 );
    close( mga_fd );
    xcommon_close_display();
}

static output_api_t mgaoutput =
{
    mga_init,

    mga_set_input_size,

    mga_lock_output_buffer,
    mga_get_output_buffer,
    mga_get_output_stride,
    mga_can_read_from_buffer,
    mga_unlock_output_buffer,

    xcommon_is_exposed, 
    xcommon_get_visible_width, 
    xcommon_get_visible_height, 
    xcommon_is_fullscreen,
    xcommon_is_alwaysontop,

    xcommon_is_fullscreen_supported,
    xcommon_is_alwaysontop_supported,

    mga_is_interlaced,
    mga_wait_for_sync,
    mga_show_frame,

    xcommon_toggle_aspect,
    xcommon_toggle_alwaysontop,
    xcommon_toggle_fullscreen,
    xcommon_resize_window_fullscreen,
    xcommon_set_window_caption,

    xcommon_set_window_position,
    xcommon_set_window_height,
    xcommon_set_fullscreen_position,
    xcommon_set_matte,

    xcommon_poll_events,
    mga_shutdown
};

output_api_t *get_xmga_output( void )
{
    return &mgaoutput;
}

