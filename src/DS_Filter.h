/////////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
//
//  This file is subject to the terms of the GNU General Public License as
//  published by the Free Software Foundation.  A copy of this license is
//  included with this software distribution in the file COPYING.  If you
//  do not have a copy, you may obtain a copy by writing to the Free
//  Software Foundation, Inc., 59 Temple Place, Suite 330 - Boston, MA
//  02111-1307  USA
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details
/////////////////////////////////////////////////////////////////////////////
// Change Log
//
// Date          Developer             Changes
//
// 03 Feb 2001   John Adcock           Initial Version
//
/////////////////////////////////////////////////////////////////////////////
// CVS Log
//
// $Log$
// Revision 1.3  2003/04/30 05:35:37  sfllaw
// 	* tvtime/debian/rules: Try to make FONTDIR work.
// 	* tvtime/src/Makefile.am: Ibid.
// 	* tvtime/src/utils.c: Ibid.
//
// 	* tvtime/src/DS_Filter.h: Fixed copyright notices.
// 	* tvtime/src/Makefile.am: Ibid.
// 	* tvtime/src/bands.h: Ibid.
// 	* tvtime/src/commands.c: Ibid.
// 	* tvtime/src/commands.h: Ibid.
// 	* tvtime/src/configsave.c: Ibid.
// 	* tvtime/src/configsave.h: Ibid.
// 	* tvtime/src/console.c: Ibid.
// 	* tvtime/src/console.h: Ibid.
// 	* tvtime/src/credits.c: Ibid.
// 	* tvtime/src/credits.h: Ibid.
// 	* tvtime/src/deinterlace.c: Ibid.
// 	* tvtime/src/deinterlace.h: Ibid.
// 	* tvtime/src/dfboutput.c: Ibid.
// 	* tvtime/src/dfboutput.h: Ibid.
// 	* tvtime/src/diffcomp.c: Ibid.
// 	* tvtime/src/diffcomp.h: Ibid.
// 	* tvtime/src/dscalerplugin.c: Ibid.
// 	* tvtime/src/dscalerplugin.h: Ibid.
// 	* tvtime/src/expandpng.c: Ibid.
// 	* tvtime/src/fifo.c: Ibid.
// 	* tvtime/src/fifo.h: Ibid.
// 	* tvtime/src/input.c: Ibid.
// 	* tvtime/src/input.h: Ibid.
// 	* tvtime/src/leetft.c: Ibid.
// 	* tvtime/src/leetft.h: Ibid.
// 	* tvtime/src/menu.c: Ibid.
// 	* tvtime/src/menu.h: Ibid.
// 	* tvtime/src/mixer.c: Ibid.
// 	* tvtime/src/mixer.h: Ibid.
// 	* tvtime/src/osdtools.c: Ibid.
// 	* tvtime/src/osdtools.h: Ibid.
// 	* tvtime/src/outputapi.h: Ibid.
// 	* tvtime/src/outputfilter.c: Ibid.
// 	* tvtime/src/outputfilter.h: Ibid.
// 	* tvtime/src/performance.c: Ibid.
// 	* tvtime/src/performance.h: Ibid.
// 	* tvtime/src/pnginput.c: Ibid.
// 	* tvtime/src/pnginput.h: Ibid.
// 	* tvtime/src/pngoutput.c: Ibid.
// 	* tvtime/src/pngoutput.h: Ibid.
// 	* tvtime/src/pulldown.c: Ibid.
// 	* tvtime/src/pulldown.h: Ibid.
// 	* tvtime/src/ree.c: Ibid.
// 	* tvtime/src/ree.h: Ibid.
// 	* tvtime/src/reepktq.c: Ibid.
// 	* tvtime/src/reepktq.h: Ibid.
// 	* tvtime/src/rtctimer.c: Ibid.
// 	* tvtime/src/rtctimer.h: Ibid.
// 	* tvtime/src/rvrmain.c: Ibid.
// 	* tvtime/src/rvrreader.c: Ibid.
// 	* tvtime/src/rvrreader.h: Ibid.
// 	* tvtime/src/scope.c: Ibid.
// 	* tvtime/src/scope.h: Ibid.
// 	* tvtime/src/sdloutput.c: Ibid.
// 	* tvtime/src/sdloutput.h: Ibid.
// 	* tvtime/src/speedtools.h: Ibid.
// 	* tvtime/src/speedy.c: Ibid.
// 	* tvtime/src/speedy.h: Ibid.
// 	* tvtime/src/station.c: Ibid.
// 	* tvtime/src/station.h: Ibid.
// 	* tvtime/src/taglines.h: Ibid.
// 	* tvtime/src/timingtest.c: Ibid.
// 	* tvtime/src/tvtime-command.c: Ibid.
// 	* tvtime/src/tvtime.c: Ibid.
// 	* tvtime/src/tvtimeconf.c: Ibid.
// 	* tvtime/src/tvtimeconf.h: Ibid.
// 	* tvtime/src/tvtimeosd.c: Ibid.
// 	* tvtime/src/tvtimeosd.h: Ibid.
// 	* tvtime/src/utils.c: Ibid.
// 	* tvtime/src/utils.h: Ibid.
// 	* tvtime/src/vbidata.c: Ibid.
// 	* tvtime/src/vbidata.h: Ibid.
// 	* tvtime/src/vbiscreen.c: Ibid.
// 	* tvtime/src/vbiscreen.h: Ibid.
// 	* tvtime/src/vgasync.c: Ibid.
// 	* tvtime/src/vgasync.h: Ibid.
// 	* tvtime/src/videocorrection.c: Ibid.
// 	* tvtime/src/videocorrection.h: Ibid.
// 	* tvtime/src/videofilter.c: Ibid.
// 	* tvtime/src/videofilter.h: Ibid.
// 	* tvtime/src/videoinput.c: Ibid.
// 	* tvtime/src/videoinput.h: Ibid.
// 	* tvtime/src/videotools.c: Ibid.
// 	* tvtime/src/videotools.h: Ibid.
// 	* tvtime/src/vsync.c: Ibid.
// 	* tvtime/src/vsync.h: Ibid.
// 	* tvtime/src/x11tools.c: Ibid.
// 	* tvtime/src/x11tools.h: Ibid.
// 	* tvtime/src/xvoutput.c: Ibid.
// 	* tvtime/src/xvoutput.h: Ibid.
//
// Revision 1.2  2003/01/07 15:25:45  vektor
// Added some missing debian files, and removed ^M's from the DScaler
// header files.
//
// Revision 1.1  2003/01/07 15:18:49  vektor
// Added the required DScaler header files.
//
// Revision 1.8  2002/06/13 12:10:21  adcockj
// Move to new Setings dialog for filers, video deint and advanced settings
//
// Revision 1.7  2001/11/28 16:04:49  adcockj
// Major reorganization of STill support
//
// Revision 1.6  2001/11/26 15:27:17  adcockj
// Changed filter structure
//
// Revision 1.5  2001/11/21 15:21:39  adcockj
// Renamed DEINTERLACE_INFO to TDeinterlaceInfo in line with standards
// Changed TDeinterlaceInfo structure to have history of pictures.
//
// Revision 1.4  2001/07/13 16:15:43  adcockj
// Changed lots of variables to match Coding standards
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __DS_FILTER_H___
#define __DS_FILTER_H___

//#include "DS_Control.h"
#include "DS_ApiCommon.h"

// filter functions return the aspect ratio change 1000 = 1.0
typedef long (__cdecl FILTER_FUNC)(TDeinterlaceInfo* Info);
typedef void (__cdecl FILTERPLUGINSTART)(void);
typedef void (__cdecl FILTERPLUGINEXIT)(void);

// list of supported plugin versions
#define FILTER_VERSION_1 1
#define FILTER_VERSION_2 2
#define FILTER_VERSION_3 3

// The current version
#define FILTER_CURRENT_VERSION FILTER_VERSION_3

typedef struct
{
    // should be set up as sizeof(FILTER_METHOD)
    // used to test that the program is using the same
    // header as the plug-in
    size_t SizeOfStructure;
    // may be used in the future when backwards combatability may
    // be required
    // set to FILTER_CURRENT_VERSION
    long FilterStructureVersion;
    // Set to DEINTERLACE_INFO_CURRENT_VERSION
    long InfoStructureVersion;
    // What to display when selected
    char* szName;
    // What to put in the Menu (NULL to use szName)
    char* szMenuName;
    // Are we active Initially FALSE
    BOOL bActive;
    // Do we get called on Input
    BOOL bOnInput;
    // Pointer to Algorithm function (cannot be NULL)
    FILTER_FUNC* pfnAlgorithm;
    // id of menu to display status
    int MenuId;
    // Always run - do we run if there has been an overrun
    BOOL bAlwaysRun;
    // call this if plugin needs to do anything before it is used
    FILTERPLUGINSTART* pfnPluginStart;
    // call this if plugin needs to deallocate anything
    FILTERPLUGINEXIT* pfnPluginExit;
    // Used to save the module Handle
    HMODULE hModule;
    // number of settings
    long nSettings;
    // pointer to start of Settings[nSettings]
    SETTING* pSettings;
    // the offset used by the external settings API
    long nSettingsOffset;
    // can we cope with interlaced material
    // is only checked for input filters
    // outpuit filters are assumed not to care
    BOOL CanDoInterlaced;
    // How many pictures of history do we need to run
    int HistoryRequired;
    // Help ID
    // needs to be in \help\helpids.h
    int HelpID;
} FILTER_METHOD;


// Call this function to get plug-in Info
typedef FILTER_METHOD* (__cdecl GETFILTERPLUGININFO)(long CpuFeatureFlags);

#endif
