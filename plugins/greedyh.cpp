/**
 * Copyright (C) 2004 Billy Biggs <vektor@dumbterm.net>
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

#include "greedyh.h"
#include "greedyhmacros.h"

#define MAXCOMB_DEFAULT          5
#define MOTIONTHRESHOLD_DEFAULT 25
#define MOTIONSENSE_DEFAULT     30

class GreedyHImageFilter
{
public:
    GreedyHImageFilter() {}
    virtual ~GreedyHImageFilter() {}

#define IS_SSE
#define SSE_TYPE SSE
#define FUNCT_NAME filterDScaler_SSE
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_SSE
#undef FUNCT_NAME

#define IS_3DNOW
#define FUNCT_NAME filterDScaler_3DNOW
#define SSE_TYPE 3DNOW
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_3DNOW
#undef FUNCT_NAME

#define IS_3DNOW
#define SSE_TYPE MMX
#define FUNCT_NAME filterDScaler_MMX
#include "greedyh.asm"
#undef SSE_TYPE
#undef IS_3DNOW
#undef FUNCT_NAME

    unsigned int GreedyMaxComb;
    unsigned int GreedyMotionThreshold;
    unsigned int GreedyMotionSense;
};

static GreedyHImageFilter *filter;

void greedyh_init( void )
{
    filter = new GreedyHImageFilter();
    filter->GreedyMaxComb = MAXCOMB_DEFAULT;
    filter->GreedyMotionThreshold = MOTIONTHRESHOLD_DEFAULT;
    filter->GreedyMotionSense = MOTIONSENSE_DEFAULT;
}

void greedyh_filter_mmx( TDeinterlaceInfo* pInfo )
{
    filter->filterDScaler_MMX( pInfo );
}

void greedyh_filter_3dnow( TDeinterlaceInfo* pInfo )
{
    filter->filterDScaler_3DNOW( pInfo );
}

void greedyh_filter_sse( TDeinterlaceInfo* pInfo )
{
    filter->filterDScaler_SSE( pInfo );
}

