/**
 * Copyright (C) 2000-2002. by A'rpi/ESP-team & others
 * Copyright (C) 2003 Billy Biggs <vektor@dumbterm.net>.
 *
 * X11 screensaver enable/disable code comes from mplayer's x11_common.c.
 *   http://www.mplayerhq.hu/
 *
 * This file is subject to the terms of the GNU General Public License as
 * published by the Free Software Foundation.  A copy of this license is
 * included with this software distribution in the file COPYING.  If you
 * do not have a copy, you may obtain a copy by writing to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <signal.h>

#include <sys/resource.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

/* when do we not have XDPMS? */
#define HAVE_XDPMS

int stop_xscreensaver = 1;
int stop_kscreensaver = 0;

static int dpms_disabled=0;
static int timeout_save=0;

/* The child's PID */
static pid_t ping_xscreensaver_child = 0;
/* The number of seconds between pings */
static int ping_xscreensaver_sleep = 60;
/* Exit values */
#define NO_XSCREENSAVER  2
#define BAD_XSCREENSAVER 3

static int kdescreensaver_was_running=0;

#ifdef HAVE_XDPMS
#include <X11/extensions/dpms.h>
#endif

/**
 * Handle SIGCHLD signals, which tell us that xscreensaver-command
 * doesn't work. 
 */
void sigchld_handler( int signum )
{
    if( signum == SIGCHLD ) {
        ping_xscreensaver_child = 0;
        stop_xscreensaver = 0;
        signal( signum, SIG_DFL );
    }
}


void saver_on(Display *mDisplay) {

#ifdef HAVE_XDPMS
    int nothing;
    if (dpms_disabled)
    {
        if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
        {
            if (!DPMSEnable(mDisplay)) {  // restoring power saving settings
                fprintf( stderr, "x11tools: DPMS not available?\n" );
            } else {
                // DPMS does not seem to be enabled unless we call DPMSInfo
                BOOL onoff;
                CARD16 state;
                DPMSForceLevel(mDisplay, DPMSModeOn);
                DPMSInfo(mDisplay, &state, &onoff);
                if (onoff) {
                    fprintf( stderr, "x11tools: Successfully enabled DPMS.\n" );
                } else {
                    fprintf( stderr, "x11tools: Could not enable DPMS.\n" );
                }
            }
        }
        dpms_disabled=0;
    }
#endif

    if (timeout_save)
    {
        int dummy, interval, prefer_blank, allow_exp;
        XGetScreenSaver(mDisplay, &dummy, &interval, &prefer_blank, &allow_exp);
        XSetScreenSaver(mDisplay, timeout_save, interval, prefer_blank, allow_exp);
        XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
        timeout_save=0;
    }

    /* Re-enable xscreensaver.  This isn't performed if the
     * ping_xscreensaver loop has been killed before. */
    if( ping_xscreensaver_child && stop_xscreensaver ) {
        signal( SIGCHLD, SIG_DFL );
        kill( ping_xscreensaver_child, SIGHUP );
        ping_xscreensaver_child = 0;
    }

    if (kdescreensaver_was_running && stop_kscreensaver) {
        system("dcop kdesktop KScreensaverIface enable true 2>/dev/null >/dev/null");
        kdescreensaver_was_running = 0;
    }


}

void saver_off(Display *mDisplay) {

    int interval, prefer_blank, allow_exp;
#ifdef HAVE_XDPMS
    int nothing;

    if (DPMSQueryExtension(mDisplay, &nothing, &nothing))
    {
        BOOL onoff;
        CARD16 state;
        DPMSInfo(mDisplay, &state, &onoff);
        if (onoff)
        {
           Status stat;
            //mp_msg(MSGT_VO,MSGL_INFO,"Disabling DPMS\n");
            dpms_disabled=1;
            stat = DPMSDisable(mDisplay);  // monitor powersave off
            //mp_msg(MSGT_VO,MSGL_V,"DPMSDisable stat: %d\n", stat);
        }
    }
#endif
    if (!timeout_save) {
        XGetScreenSaver(mDisplay, &timeout_save, &interval, &prefer_blank, &allow_exp);
        if (timeout_save)
            XSetScreenSaver(mDisplay, 0, interval, prefer_blank, allow_exp);
    }

    /* Disabling xscreensaver by simulating activity. 
     * http://www.jwz.org/xscreensaver/faq.html#dvd */
    if( stop_xscreensaver && !ping_xscreensaver_child ) {
        struct sigaction sigchld_act;

        /* Set a handler for the child */
        sigchld_act.sa_handler = sigchld_handler;
        sigemptyset( &sigchld_act.sa_mask );
        sigchld_act.sa_flags = SA_NOCLDSTOP;
        sigaction( SIGCHLD, &sigchld_act, NULL );

        errno = 0;
        ping_xscreensaver_child = fork();
        if( !ping_xscreensaver_child ) {
            int result;
            /* We are the child, and we will ping xscreensaver every minute,
             * to keep it from activating on us. */

            /* We don't trap for any close() errors because we don't care
             * about data lossage. */
            close( STDIN_FILENO );
            close( STDOUT_FILENO );
            close( STDERR_FILENO );

            /* We need to drop privileges to the normal user, in case we are
             * still setuid. */
#ifdef _POSIX_SAVED_IDS
            if( seteuid( getuid() ) == -1 ) {
#else
            if( setreuid( -1, getuid() ) == -1 ) {
#endif
                fprintf( stderr, 
                        "x11tools: Unknown problems dropping root access: %s\n",
                        strerror( errno ) );
            }

            /* Now we should re-nice ourselves so we don't get scheduled too
             * often. */
            errno = 0;
            result = getpriority( PRIO_PROCESS, 0 );
            if( errno || result < 0 ) {
                /* Ensure that we are at least 0 nice. */
                setpriority( PRIO_PROCESS, 0, 0 );
            }

            /* Ping xscreensaver once every ping_xscreensaver_sleep 
             * seconds. */
            for( ;; ) {
                /* create a new process that will be _replaced_ by 
                 * xscreensaver-command */
                errno = 0;
                result = fork();

                if ( !result ) {
                    /* Child process: exec xscreensaver-command */
                    /* xscreensaver-command must be written _twice_ as 
                     * execlp starts its parameters with arg[0], not arg[1] */
                    errno = 0;
                    result = execlp( "xscreensaver-command", 
                                     "xscreensaver-command",
                                     "-deactivate", 
                                     NULL );
                    if( errno ) {
                        exit( NO_XSCREENSAVER );
                    }
                } else if ( result > 0 ) {
                    /* Parent spinning process: wait for xscreensaver-command */
                    wait( &result );
                    if ( result > 0 ) {
                        /* xscreensaver-command exited nonzero status, or 
                         * the child couldn't exec xscreensaver-command */
                        exit( result );
                    }
                } else {
                    /* Could not create child to run xscreensaver-command */
                    exit( NO_XSCREENSAVER );
                }

                sleep( ping_xscreensaver_sleep );
            }
        } else if( ping_xscreensaver_child < 0 ) {
            fprintf( stderr, "x11tools: Cannot fork a new process.\n" );
        }
    }

    if (stop_kscreensaver && !kdescreensaver_was_running)
    {
      kdescreensaver_was_running=(system("dcop kdesktop KScreensaverIface isEnabled 2>/dev/null | sed 's/1/true/g' | grep true 2>/dev/null >/dev/null")==0);
      if (kdescreensaver_was_running)
          system("dcop kdesktop KScreensaverIface enable false 2>/dev/null >/dev/null");
    }
}

