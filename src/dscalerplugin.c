/**
 * Copyright (C) 2003  Joachim Koenig <Joachim.Koenig@tecnomen.fi>,
 *                     Billy Biggs <vektor@dumbterm.net>.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "wine/driver.h"
#include "wine/mmreg.h"
#include "wine/ldt_keeper.h"
#include "wine/winbase.h"

#include "DS_Filter.h"
#include "DS_Deinterlace.h"

#include "speedy.h"
#include "mm_accel.h"

void __cdecl dscaler_memcpy( void *output, void *input, size_t size )
{
    speedy_memcpy( output, input, size );
}

FILTER_METHOD *load_dscaler_filter( char *filename )
{
    int i;
    GETFILTERPLUGININFO* pfnGetFilterPluginInfo;
    FILTER_METHOD* pMethod;
    HMODULE hPlugInMod;
    long CpuFeatureFlags = 0;

    hPlugInMod = LoadLibraryA( filename );
    if( !hPlugInMod ) {
        fprintf( stderr, " Loading %s failed.", filename );
        return 0;
    }

    pfnGetFilterPluginInfo = (GETFILTERPLUGININFO *) GetProcAddress( hPlugInMod, "GetFilterPluginInfo" );
    if( !pfnGetFilterPluginInfo ) {
        fprintf( stderr, "GetPluginInfo failed\n" );
        return 0;
    }

    if( speedy_get_accel() & MM_ACCEL_X86_MMX ) {
        CpuFeatureFlags |= FEATURE_MMX;
    }
    if( speedy_get_accel() & MM_ACCEL_X86_3DNOW ) {
        CpuFeatureFlags |= FEATURE_3DNOW;
    }
    if( speedy_get_accel() & MM_ACCEL_X86_MMXEXT ) {
        CpuFeatureFlags |= FEATURE_MMXEXT;
    }

    pMethod = pfnGetFilterPluginInfo( CpuFeatureFlags );
    if( pMethod ) {
        if(pMethod->SizeOfStructure == sizeof(FILTER_METHOD) &&
            pMethod->FilterStructureVersion >= FILTER_VERSION_3 &&
            pMethod->InfoStructureVersion == DEINTERLACE_INFO_CURRENT_VERSION)
        {

            printf("Info Version: %ld\n",pMethod->InfoStructureVersion);
            printf("Name: %s\n",pMethod->szName);
            printf("nSettings %ld\n",pMethod->nSettings);
            printf("History %d\n",pMethod->HistoryRequired);

            pMethod->hModule = hPlugInMod;

            // read in settings
            for(i = 0; i < pMethod->nSettings; i++) {
                /**
                // Setting_ReadFromIni(&(pMethod->pSettings[i]));
                // printf("Parameter %s: %ld\n",
                          pMethod->pSettings[i].szDisplayName,*(pMethod->pSettings[i].pValue));
                */
            }

            if( pMethod->pfnPluginStart ) {
                pMethod->pfnPluginStart();
            }

            return pMethod;
        } else {
            printf( "Plugin %s obsolete", filename );
        }
    } else {
        printf( "Plugin %s not compatible with your CPU", filename );
    }

    return 0;
}

void unload_dscaler_filter( FILTER_METHOD *method )
{
}

DEINTERLACE_METHOD *load_dscaler_deinterlacer( char *filename )
{
    GETDEINTERLACEPLUGININFO *pfnGetDeinterlacePluginInfo;
    DEINTERLACE_METHOD *pMethod;
    HMODULE hPlugInMod;
    long CpuFeatureFlags = 0;

    hPlugInMod = LoadLibrary( filename );
    if( !hPlugInMod ) {
        return 0;
    }

    pfnGetDeinterlacePluginInfo = (GETDEINTERLACEPLUGININFO*) GetProcAddress( hPlugInMod, "GetDeinterlacePluginInfo" );
    if( !pfnGetDeinterlacePluginInfo ) {
        return 0;
    }

    if( speedy_get_accel() & MM_ACCEL_X86_MMX ) {
        CpuFeatureFlags |= FEATURE_MMX;
    }
    if( speedy_get_accel() & MM_ACCEL_X86_3DNOW ) {
        CpuFeatureFlags |= FEATURE_3DNOW;
    }
    if( speedy_get_accel() & MM_ACCEL_X86_MMXEXT ) {
        CpuFeatureFlags |= FEATURE_MMXEXT;
    }

    pMethod = pfnGetDeinterlacePluginInfo( CpuFeatureFlags );
    if( pMethod ) {
        if(pMethod->SizeOfStructure == sizeof(DEINTERLACE_METHOD) &&
            pMethod->DeinterlaceStructureVersion >= DEINTERLACE_VERSION_3) {

            pMethod->hModule = hPlugInMod;

            /*
            fprintf( stderr, "\n======\n" );
            fprintf( stderr, "Name:       %s\n", pMethod->szName );
            fprintf( stderr, "ShortName:  %s\n", pMethod->szShortName );
            fprintf( stderr, "History:    %ld\n", pMethod->nFieldsRequired );
            fprintf( stderr, "Halfheight: %d\n", pMethod->bIsHalfHeight );
            fprintf( stderr, "FieldDiff:  %d\n", pMethod->bNeedFieldDiff );
            fprintf( stderr, "CombFactor: %d\n", pMethod->bNeedCombFactor );

            fprintf( stderr, "Settings:   %ld\n", pMethod->nSettings );

            int i;
            for( i = 0; i < pMethod->nSettings; i++) {
                // fprintf( stderr, "[%d] %s\n", i, pMethod->pSettings[i].szDisplayName );
                // Setting_ReadFromIni(&(pMethod->pSettings[i]));

                if( pMethod->pSettings[ i ].szDisplayName ) {
                    printf("Parameter %s: %ld\n", pMethod->pSettings[i].szDisplayName,
                       *(pMethod->pSettings[i].pValue));
                }
            }
            */

            if( pMethod->pfnPluginInit ) {
                pMethod->pfnPluginInit();
            }

            return pMethod;
        }
    }

    return 0;
}


void unload_dscaler_deinterlacer( DEINTERLACE_METHOD *method )
{
}

