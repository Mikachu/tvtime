// -*- c++ -*-
/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 Tom Barry  All rights reserved.
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
//
//  Also, this program is "Philanthropy-Ware".  That is, if you like it and 
//  feel the need to reward or inspire the author then please feel free (but
//  not obligated) to consider joining or donating to the Electronic Frontier
//  Foundation. This will help keep cyber space free of barbed wire and bullsh*t.  
//  See www.eff.org for details
/////////////////////////////////////////////////////////////////////////////

// Qt 3.1.1 compatibility
#ifndef Q_INT64
#define Q_INT64 long long
#endif

#include "tomsmocompmacros.h"

#define TopFirst IsOdd

#if defined(IS_SSE)
#define SEFUNC(x) Search_Effort_SSE_##x()
#elif defined(IS_3DNOW)
#define SEFUNC(x) Search_Effort_3DNOW_##x()
#else
#define SEFUNC(x) Search_Effort_MMX_##x()
#endif

static void FUNCT_NAME(TDeinterlaceInfo* pInfo)
{
	pMyMemcpy = pInfo->pMemcpy;

    src_pitch = pInfo->InputPitch;
    dst_pitch = pInfo->OverlayPitch;
    rowsize = pInfo->LineLength;
	FldHeight = pInfo->FieldHeight;
       
    pCopySrc = pInfo->PictureHistory[1]->pData;
    pCopySrcP = pInfo->PictureHistory[3]->pData;
    pWeaveSrc = pInfo->PictureHistory[0]->pData;  
    pWeaveSrcP = pInfo->PictureHistory[2]->pData;
	IsOdd =	pInfo->PictureHistory[0]->Flags & PICTURE_INTERLACED_ODD;

#ifdef IS_SSE2
// SSE2 support temporarily deleted

#endif

    if(IsOdd) {
		// if we have an odd field we copy an even field and weave an odd field
		pCopyDest = pInfo->Overlay;
		pWeaveDest = pInfo->Overlay + dst_pitch;
	} else {
		// if we have an ever field we copy an odd field and weave an even field
		pCopyDest = pInfo->Overlay + dst_pitch;
		pWeaveDest = pInfo->Overlay;
	}
	// copy 1st and last weave lines 
	Fieldcopy(pWeaveDest, pCopySrc, rowsize,		
              1, dst_pitch*2, src_pitch);
	Fieldcopy(pWeaveDest+(FldHeight-1)*dst_pitch*2,
              pCopySrc+(FldHeight-1)*src_pitch, rowsize, 
              1, dst_pitch*2, src_pitch);

#ifdef USE_VERTICAL_FILTER
// Vertical Filter currently not implemented for DScaler !!
	// copy 1st and last lines the copy field
	Fieldcopy(pCopyDest, pCopySrc, rowsize, 
              1, dst_pitch*2, src_pitch);
	Fieldcopy(pCopyDest+(FldHeight-1)*dst_pitch*2,
              pCopySrc+(FldHeight-1)*src_pitch, rowsize, 
              1, dst_pitch*2, src_pitch);
#else

	// copy all of the copy field
	Fieldcopy(pCopyDest, pCopySrc, rowsize, 
              FldHeight, dst_pitch*2, src_pitch);
#endif	
	// then go fill in the hard part, being variously lazy depending upon
	// SearchEffort

    if(UseStrangeBob == 0) {
		if (SearchEffort == 0)
		{
            SEFUNC(0);
		}
		else if (SearchEffort <= 1)
		{
			SEFUNC(1);
		}
	/*	else if (SearchEffort <= 2)
		{
			SEFUNC(2);
		}
	*/
		else if (SearchEffort <= 3)
		{
			SEFUNC(3);
		}
		else if (SearchEffort <= 5)
		{
			SEFUNC(5);
		}
		else if (SearchEffort <= 9)
		{
			SEFUNC(9);
		}
		else if (SearchEffort <= 11)
		{
			SEFUNC(11);
		}
		else if (SearchEffort <= 13)
		{
			SEFUNC(13);
		}
		else if (SearchEffort <= 15)
		{
			SEFUNC(15);
		}
		else if (SearchEffort <= 19)
		{
			SEFUNC(19);
		}
		else if (SearchEffort <= 21)
		{
			SEFUNC(21);
		}
		else 
		{
			SEFUNC(Max);
		}
	}
	else
	{
		if (SearchEffort == 0)
		{
			SEFUNC(0_SB);
		}
		else if (SearchEffort <= 1)
		{
			SEFUNC(1_SB);
		}
	/*	else if (SearchEffort <= 2)
		{
			SEFUNC(2_SB);
		}
	*/
		else if (SearchEffort <= 3)
		{
			SEFUNC(3_SB);
		}
		else if (SearchEffort <= 5)
		{
			SEFUNC(5_SB);
		}
		else if (SearchEffort <= 9)
		{
			SEFUNC(9_SB);
		}
		else if (SearchEffort <= 11)
		{
			SEFUNC(11_SB);
		}
		else if (SearchEffort <= 13)
		{
			SEFUNC(13_SB);
		}
		else if (SearchEffort <= 15)
		{
			SEFUNC(15_SB);
		}
		else if (SearchEffort <= 19)
		{
			SEFUNC(19_SB);
		}
		else if (SearchEffort <= 21)
		{
			SEFUNC(21_SB);
		}
		else 
		{
			SEFUNC(Max_SB);
		}
	}

    __asm__ __volatile__("emms");
}


#include "TomsMoCompAll2.inc"

#define USE_STRANGE_BOB

#include "TomsMoCompAll2.inc"

#undef USE_STRANGE_BOB
