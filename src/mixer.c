/**
 * Copyright (C) 2002, 2003 Doug Bell <drbell@users.sourceforge.net>
 *
 * Some mixer routines from mplayer, http://mplayer.sourceforge.net.
 * Copyright (C) 2000-2002. by A'rpi/ESP-team & others
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/mman.h>
#include <string.h>
#include "utils.h"
#include "mixer.h"

static char *mixer_device = "/dev/mixer";
static int saved_volume = (50 & 0xFF00) | (50 & 0x00FF);
static int mixer_channel = SOUND_MIXER_LINE;
static int mixer_dev_mask = 1 << SOUND_MIXER_LINE;
static int muted = 0;
static int mutecount = 0;
static int fd = -1;

int mixer_get_volume( void )
{
    int v, cmd, devs;
    int curvol = 0;

    if( fd < 0 ) fd = open( mixer_device, O_RDONLY );
    if( fd != -1 ) {

        ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
        if( devs & mixer_dev_mask ) {
            cmd = MIXER_READ( mixer_channel );
        } else {
            return curvol;
        }

        ioctl( fd, cmd, &v );
        curvol = ( v & 0xFF00 ) >> 8;
    }

    return curvol;
}

int mixer_set_volume( int percentdiff )
{
    int v, cmd, devs, levelpercentage;

    levelpercentage = mixer_get_volume();

    levelpercentage += percentdiff;
    if( levelpercentage > 100 ) levelpercentage = 100;
    if( levelpercentage < 0 ) levelpercentage = 0;

    if( fd < 0 ) fd = open( mixer_device, O_RDONLY );
    if( fd != -1 ) {
        ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
        if( devs & mixer_dev_mask ) {
            cmd = MIXER_WRITE( mixer_channel );
        } else {
            return 0;
        }

        v = ( levelpercentage << 8 ) | levelpercentage;
        ioctl( fd, cmd, &v );
        muted = 0;
        return v;
    }

    return 0;
}

void mixer_mute( int mute )
{
    int v, cmd, devs;

    /**
     * Make sure that if multiple users mute the card,
     * we only honour the last one.
     */
    if( !mute ) mutecount--;
    if( mutecount ) return;

    if( fd < 0 ) fd = open( mixer_device, O_RDONLY );

    if( mute ) {
        mutecount++;
        if( fd != -1 ) {

            /* Save volume */
            ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
            if( devs & mixer_dev_mask ) {
                cmd = MIXER_READ( mixer_channel );
            } else {
                return;
            }

            ioctl( fd,cmd,&v );
            saved_volume = v;

            /* Now set volume to 0 */
            ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
            if( devs & mixer_dev_mask ) {
                cmd = MIXER_WRITE( mixer_channel );
            } else {
                return;
            }

            v = 0;
            ioctl( fd, cmd, &v );

            muted = 1;
            return;
        }
    } else {
        if( fd != -1 ) {
            ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
            if( devs & mixer_dev_mask ) {
                cmd = MIXER_WRITE( mixer_channel );
            } else {
                return;
            }

            v = saved_volume;
            ioctl( fd, cmd, &v );
            muted = 0;
            return;
        }
    }
}

int mixer_ismute( void )
{
    return muted;
}

static char *oss_core_devnames[] = SOUND_DEVICE_NAMES;

void mixer_set_device( const char *devname )
{
    const char *channame;
    int found = 0;
    int i;

    mixer_device = strdup( devname );
    if( !mixer_device ) return;

    i = strcspn( mixer_device, ":" );
    if( i == strlen( mixer_device ) ) {
        channame = "line";
    } else {
        mixer_device[ i ] = 0;
        channame = mixer_device + i + 1;
    }
    if( !file_is_openable_for_read( mixer_device ) ) {
        fprintf( stderr, "mixer: Can't open device %s, "
                 "mixer volume and mute unavailable.\n", mixer_device );
    }

    mixer_channel = SOUND_MIXER_LINE;
    for( i = 0; i < SOUND_MIXER_NRDEVICES; i++ ) {
        if( !strcasecmp( channame, oss_core_devnames[ i ] ) ) {
            mixer_channel = i;
            found = 1;
            break;
        }
    }
    if( !found ) {
        fprintf( stderr, "mixer: No such mixer channel '%s', using channel 'line'.\n", channame );
    }
    mixer_dev_mask = 1 << mixer_channel;
}

void mixer_close_device( void )
{
    if( fd >= 0 ) close( fd );
}

