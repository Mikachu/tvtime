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
//  Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
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
