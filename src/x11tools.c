
/**
 * X11 screensaver enable/disable code comes from mplayer's x11_common.c.
 */

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xmd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

/* when do we not have XDPMS? */
#define HAVE_XDPMS

int stop_xscreensaver=1;

static int dpms_disabled=0;
static int timeout_save=0;
static int xscreensaver_was_running=0;
static int kdescreensaver_was_running=0;

#ifdef HAVE_XDPMS
#include <X11/extensions/dpms.h>
#endif

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

    if (xscreensaver_was_running && stop_xscreensaver) {
        system("cd /; xscreensaver -no-splash &");
        xscreensaver_was_running = 0;
    }
    if (kdescreensaver_was_running && stop_xscreensaver) {
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

    // turning off screensaver
    if (stop_xscreensaver && !xscreensaver_was_running)
    {
      xscreensaver_was_running = (system("xscreensaver-command -version 2>/dev/null >/dev/null")==0);
      if (xscreensaver_was_running)
          system("xscreensaver-command -exit 2>/dev/null >/dev/null");    
    }
    if (stop_xscreensaver && !kdescreensaver_was_running)
    {
      kdescreensaver_was_running=(system("dcop kdesktop KScreensaverIface isEnabled 2>/dev/null | sed 's/1/true/g' | grep true 2>/dev/null >/dev/null")==0);
      if (kdescreensaver_was_running)
          system("dcop kdesktop KScreensaverIface enable false 2>/dev/null >/dev/null");
    }
}

