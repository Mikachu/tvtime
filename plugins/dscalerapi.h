///////////////////////////////////////////////////////////////////////////////
// $Id$
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000 John Adcock.  All rights reserved.
/////////////////////////////////////////////////////////////////////////////
// This header file is free software; you can redistribute it and/or modify it
// under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details
///////////////////////////////////////////////////////////////////////////////

/**
 * This file is an adaptation of the DS_ApiCommon.h file from DScaler.  I have
 * removed many of the features which are not required by the tvtime port at
 * the current time, and just store here the structures needed by the
 * deinterlacers.  We may try to converge back with DScaler or KDETV's
 * adaptation in the future.  -Billy Biggs
 */

#ifndef __DS_APICOMON_H__
#define __DS_APICOMON_H__ 1

#include <stdlib.h>

/** Deinterlace functions return true if the overlay is ready to be displayed.
*/
typedef void (MEMCPY_FUNC)(void* pOutput, const void* pInput, size_t nSize);

#define MAX_PICTURE_HISTORY 10

#define PICTURE_PROGRESSIVE 0
#define PICTURE_INTERLACED_ODD 1
#define PICTURE_INTERLACED_EVEN 2
#define PICTURE_INTERLACED_MASK (PICTURE_INTERLACED_ODD | PICTURE_INTERLACED_EVEN)

/** Structure containing a single field or frame
    from the source.

    This may be modified
*/

typedef struct
{
    // pointer to the start of data for this picture
    unsigned char* pData;
    // see PICTURE_ flags
    unsigned int Flags;
} TPicture;


#define DEINTERLACE_INFO_CURRENT_VERSION 400

/** Structure used to transfer all the information used by plugins
    around in one chunk
*/
typedef struct
{
    /** The most recent pictures 
        PictureHistory[0] is always the most recent.
        Pointers are NULL if the picture in question isn't valid, e.g. because
        the program just started or a picture was skipped.
    */
    TPicture* PictureHistory[MAX_PICTURE_HISTORY];

    /// Current overlay buffer pointer.
    unsigned char *Overlay;

    /// Overlay pitch (number of bytes between scanlines).
    unsigned int OverlayPitch;

    /** Number of bytes of actual data in each scanline.  May be less than
        OverlayPitch since the overlay's scanlines might have alignment
        requirements.  Generally equal to FrameWidth * 2.
    */
    unsigned int LineLength;

    /// Number of pixels in each scanline.
    int FrameWidth;

    /// Number of scanlines per frame.
    int FrameHeight;

    /** Number of scanlines per field.  FrameHeight / 2, mostly for
        cleanliness so we don't have to keep dividing FrameHeight by 2.
    */
    int FieldHeight;

    /// Function pointer to optimized memcpy function
    MEMCPY_FUNC* pMemcpy;

    /** distance between lines in image
        need not match the pixel width
    */
    unsigned int InputPitch;
} TDeinterlaceInfo;

#endif
