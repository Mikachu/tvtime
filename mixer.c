#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <sys/mman.h>
#include "mixer.h"

static char *mixer_device = "/dev/mixer";
static int mixer_usemaster = 0;
static int levelpercentage = 0;

int mixer_get_volume( void )
{
    int fd, v, cmd, devs;
    int curvol = 0;

    fd = open( mixer_device, O_RDONLY );
    if( fd != -1 ) {

        ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
        if( ( devs & SOUND_MASK_LINE ) && ( mixer_usemaster == 0 ) ) {
            cmd = SOUND_MIXER_READ_LINE;
        } else if( ( devs & SOUND_MASK_VOLUME ) && ( mixer_usemaster==1 ) ) {
            cmd = SOUND_MIXER_READ_VOLUME;
        } else {
            close( fd );
            return curvol;
        }

        ioctl( fd,cmd,&v );
        curvol = ( v & 0xFF00 ) >> 8;
        close( fd );
    }

    return curvol;
}

void mixer_set_volume( int percentdiff )
{
    int fd, v, cmd, devs;

    levelpercentage += percentdiff;
    if( levelpercentage > 100 ) levelpercentage = 100;
    if( levelpercentage < 0 ) levelpercentage = 0;

    fd = open( mixer_device, O_RDONLY );
    if( fd != -1 ) {
        ioctl( fd, SOUND_MIXER_READ_DEVMASK, &devs );
        if( ( devs & SOUND_MASK_LINE ) && ( mixer_usemaster==0 ) ) {
            cmd = SOUND_MIXER_WRITE_LINE;
        } else if( ( devs & SOUND_MASK_VOLUME ) && ( mixer_usemaster==1 ) ) {
            cmd = SOUND_MIXER_WRITE_VOLUME;
        } else {
            close( fd );
            return;
        }

        v = ( levelpercentage << 8 ) | levelpercentage;
        ioctl( fd, cmd, &v );
        close( fd );
    }

    //if( percentdiff ) osd_volume( movietime.osd, levelpercentage );
}

