

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>
#include "vidmode.h"

/* Why aren't these in the damn header??? */
/* Mode flags -- ignore flags not in V_FLAG_MASK */
#define V_FLAG_MASK     0x1FF;
#define V_PHSYNC        0x001 
#define V_NHSYNC        0x002
#define V_PVSYNC        0x004
#define V_NVSYNC        0x008
#define V_INTERLACE     0x010 
#define V_DBLSCAN       0x020
#define V_CSYNC         0x040
#define V_PCSYNC        0x080
#define V_NCSYNC        0x100

struct vidmode_s
{
    Display *display;
    int screen;
};

vidmode_t *vidmode_new( void )
{
    vidmode_t *vm = (vidmode_t *) malloc( sizeof( vidmode_t ) );
    if( !vm ) return 0;

    vm->display = XOpenDisplay( getenv( "DISPLAY" ) );
    if( !vm->display ) {
        free( vm );
        return 0;
    }
    vm->screen = DefaultScreen( vm->display );

    return vm;
}

void vidmode_delete( vidmode_t *vm )
{
    XCloseDisplay( vm->display );
    free( vm );
}

void vidmode_print_modeline( vidmode_t *vm )
{
    XF86VidModeMonitor monitor;
    XF86VidModeModeLine mode_line;
    int dot_clock;
    int flags, numclocks, maxclock;
    int *clocks;
    int i;

    if( !XF86VidModeGetModeLine( vm->display, vm->screen,
                                 &dot_clock, &mode_line ) ) {
        fprintf( stderr, "vidmode: Can't get modeline.\n" );
        return;
    }

    fprintf( stderr, "vidmode: dot clock  %d\n", dot_clock );
    fprintf( stderr, "vidmode: hdisplay   %d\n", mode_line.hdisplay );
    fprintf( stderr, "vidmode: hsyncstart %d\n", mode_line.hsyncstart );
    fprintf( stderr, "vidmode: hsyncend   %d\n", mode_line.hsyncend );
    fprintf( stderr, "vidmode: htotal     %d\n", mode_line.htotal );
    fprintf( stderr, "vidmode: hskew      %d\n", mode_line.hskew );
    fprintf( stderr, "vidmode: vdisplay   %d\n", mode_line.vdisplay );
    fprintf( stderr, "vidmode: vsyncstart %d\n", mode_line.vsyncstart );
    fprintf( stderr, "vidmode: vsyncend   %d\n", mode_line.vsyncend );
    fprintf( stderr, "vidmode: vtotal     %d\n", mode_line.vtotal );
    fprintf( stderr, "vidmode: flags      %x\n", mode_line.flags );
    fprintf( stderr, "vidmode: privsize   %d\n", mode_line.privsize );
    fprintf( stderr, "vidmode: refresh    %5.2fhz.\n",
             (double) ( (double) dot_clock * 1000.0 ) /
             (double) ( mode_line.htotal * mode_line.vtotal ) );

    XF86VidModeGetDotClocks( vm->display, vm->screen, &flags,
                             &numclocks, &maxclock, &clocks );

    fprintf( stderr, "vidmode: Dotclock flags: %x, numclocks: %d, maxclock: %d\n",
             flags, numclocks, maxclock );
    for( i = 0; i < numclocks; i++ ) {
        fprintf( stderr, "vidmode: \tclock %d: %d\n", i, clocks[ i ] );
    }

    XF86VidModeGetMonitor( vm->display, vm->screen, &monitor );
    fprintf( stderr, "vidmode: hsync hi %f lo %f\n",
             monitor.hsync->hi, monitor.hsync->lo );
    fprintf( stderr, "vidmode: vsync hi %f lo %f\n",
             monitor.vsync->hi, monitor.vsync->lo );
}

