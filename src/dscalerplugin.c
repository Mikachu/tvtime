
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "wine/driver.h"
#include "wine/mmreg.h"
#include "wine/ldt_keeper.h"
#include "wine/winbase.h"

#include "DS_Filter.h"
#include "DS_Deinterlace.h"


long CpuFeatureFlags = 0xffffffff;
static FILTER_METHOD *Filters[10];
static int NumFilters;

void LoadFilterPlugin(char* szFileName)
{
int i;
    GETFILTERPLUGININFO* pfnGetFilterPluginInfo;
    FILTER_METHOD* pMethod;
    HMODULE hPlugInMod;

    hPlugInMod = LoadLibraryA(szFileName);
    if(hPlugInMod == NULL)
    {
        printf( " Loading %s failed.", szFileName);
        return;
    }

    pfnGetFilterPluginInfo = (GETFILTERPLUGININFO*)GetProcAddress(hPlugInMod, "GetFilterPluginInfo");
    if(pfnGetFilterPluginInfo == NULL)
    {
printf("GetPluginInfo failed\n");
        return;
    }

    pMethod = pfnGetFilterPluginInfo(CpuFeatureFlags);

    if(pMethod != NULL)
    {
        if(pMethod->SizeOfStructure == sizeof(FILTER_METHOD) &&
            pMethod->FilterStructureVersion >= FILTER_VERSION_3 &&
            pMethod->InfoStructureVersion == DEINTERLACE_INFO_CURRENT_VERSION)
        {
printf("Info Version: %ld\n",pMethod->InfoStructureVersion);
printf("Name: %s\n",pMethod->szName);
printf("nSettings %ld\n",pMethod->nSettings);
printf("History %d\n",pMethod->HistoryRequired);
            Filters[NumFilters] = pMethod;
            pMethod->hModule = hPlugInMod;

            // read in settings
            for(i = 0; i < pMethod->nSettings; i++)
            {
//                Setting_ReadFromIni(&(pMethod->pSettings[i]));
//		  printf("Parameter %s: %ld\n",pMethod->pSettings[i].szDisplayName,*(pMethod->pSettings[i].pValue));
            }

            if(pMethod->pfnPluginStart != NULL)
            {
printf("init function start\n");
                pMethod->pfnPluginStart();
printf("init function end\n");
            }
            NumFilters++;
        }
        else
        {
            printf( "Plugin %s obsolete", szFileName);
        }
    }
    else
    {
        printf( "Plugin %s not compatible with your CPU", szFileName);
    }
}


DEINTERLACE_METHOD *LoadDeintPlugin( char *filename )
{
    GETDEINTERLACEPLUGININFO *pfnGetDeinterlacePluginInfo;
    DEINTERLACE_METHOD *pMethod;
    HMODULE hPlugInMod;
	int i;

    hPlugInMod = LoadLibrary( filename );
    if(hPlugInMod == NULL)
    {
        return 0;
    }

    pfnGetDeinterlacePluginInfo = (GETDEINTERLACEPLUGININFO*)GetProcAddress(hPlugInMod, "GetDeinterlacePluginInfo");
    if(pfnGetDeinterlacePluginInfo == NULL)
    {
        return 0;
    }

    pMethod = pfnGetDeinterlacePluginInfo(CpuFeatureFlags);
    if(pMethod != NULL)
    {
        if(pMethod->SizeOfStructure == sizeof(DEINTERLACE_METHOD) &&
            pMethod->DeinterlaceStructureVersion >= DEINTERLACE_VERSION_3)
        {
fprintf( stderr, "Name: %s\n",pMethod->szName );
fprintf( stderr, "nSettings %ld\n",pMethod->nSettings );
printf( stderr, "History %ld\n",pMethod->nFieldsRequired );
            pMethod->hModule = hPlugInMod;

            // read in settings
            for( i = 0; i < pMethod->nSettings; i++)
            {
          //      Setting_ReadFromIni(&(pMethod->pSettings[i]));
	//	  printf("Parameter %s: %ld\n",pMethod->pSettings[i].szDisplayName,*(pMethod->pSettings[i].pValue));
            }
            if(pMethod->pfnPluginInit != NULL)
            {
                pMethod->pfnPluginInit();
            }

            return pMethod;
        }
    }

    return 0;
}


