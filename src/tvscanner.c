
/*

        for (i = 0; i < chancount; i++) {
            fprintf(stderr,"%-4s (%6.2f MHz): ",chanlist[i].name,
                    (float)chanlist[i].freq/1000);
            do_va_cmd(2,"setchannel",chanlist[i].name);
            usleep(200000); // 0.2 sec
            if (0 == drv->is_tuned(h_drv)) {
                fprintf(stderr,"no station\n");
                continue;
            }
            name = get_vbi_name(vbi);
            fprintf(stderr, "%s\n", name ? name : "???");
            if (NULL == name) {
                sprintf(dummy,"unknown (%s)",chanlist[i].name);
                name = dummy;
            }
            fprintf(conf,"[%s]\nchannel = %s\n\n",name,chanlist[i].name);
            fflush(conf);
        }
*/

#include <stdio.h>
#include "bands.h"
#include "videoinput.h"

int main( int argc, char **argv )
{
    videoinput_t *vidin = 0;
    int i;

    vidin = videoinput_new( "/dev/video", 0, VIDEOINPUT_NTSC, 1 );
    if( !vidin ) {
        fprintf( stderr, "tvtime: Can't open video input, "
                         "maybe try a different device?\n" );
        return 1;
    }

    for( i = 0; i < 20; i++ ) {
        int tuner_state;
        tuner_state = videoinput_check_for_signal( vidin, 1 );
    }

    return 0;
}

