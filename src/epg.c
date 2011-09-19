/**
 * Copyright (c) 2005 Marijn van Galen <M.P.vanGalen@ewi.TUDelft.nl>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
#else
# define _(string) (string)
#endif

#include "commands.h"
#include "utils.h"
#include "xmltv.h"
#include "station.h"

/**
 * Draws electronic programguide based on XMLTV data
 *
 * PRE: osd             : initialized tvtime osd
 *      page            : number of the page we want to show, starting with 1
 *      stationmgr      : actual stationmanager
 *      xmltv           : initialized xmltv object
 * 
 * RETURNS: pagenumber of the page showing if there is one, 0 otherwise
 *
 */
 
int epg_show_nowandnext( tvtime_osd_t* osd, int page, station_mgr_t *stationmgr, xmltv_t *xmltv )
{
    if (!page)
        return 0;
    if ( xmltv ){
        const int buf_length = 255;
        const int max_num_lines = 15;
        const int num_station_per_page = max_num_lines / 3;
        const int num_stations = station_get_num_stations(stationmgr);
        char *old_channel = strdup(xmltv_get_channel(xmltv));
        char buf[buf_length+1];
        int i, count, cur = 0;
      
  
        /* List Header */
        snprintf(buf, buf_length, "Now showing and next on (%d/%d):", page, num_stations/num_station_per_page);
        tvtime_osd_list_set_text( osd, cur++, buf);
        tvtime_osd_list_set_hilight(osd, -1);	
     
        for (i = (page-1) * num_station_per_page, count = 0; (i < num_stations) && (count < num_station_per_page ); i++, count++) {
            const char *xmltv_id = 0;
            if (!(xmltv_id = station_get_xmltv_id( stationmgr, i))) 
                xmltv_id = xmltv_lookup_channel(xmltv, station_get_name( stationmgr, i));
            xmltv_set_channel( xmltv, xmltv_id);
            xmltv_refresh( xmltv );
            /* Channel number + name */
            snprintf(buf, buf_length, "[%s] %s:", station_get_channel(stationmgr,i), station_get_name( stationmgr, i));		
            tvtime_osd_list_set_multitext( osd, cur++, buf, 1);
            
            if (xmltv_get_title( xmltv )) {
                char start_time[50];
                char end_time[50];
                time_t start_timestamp = xmltv_get_start_time( xmltv );
                time_t end_timestamp = xmltv_get_end_time( xmltv );
                strftime( start_time, 50, "%H:%M", localtime( &start_timestamp ) );
                strftime( end_time, 50, "%H:%M", localtime( &end_timestamp ) );
    
                    
                /* Highlight if current channel */
                if (station_get_current_id(stationmgr) == i+1)
                    tvtime_osd_list_set_hilight(osd, cur-1);	
                
                /* starttime of current program + Now showing program */
                snprintf(buf, buf_length, "*     %s %s", start_time, xmltv_get_title( xmltv ));		
                if (xmltv_get_sub_title( xmltv )){
                    strncat(buf," (",buf_length-strlen(buf));  
                    strncat(buf,xmltv_get_sub_title( xmltv ),buf_length-strlen(buf));
                    strncat(buf,")",buf_length-strlen(buf));  
                }
                tvtime_osd_list_set_multitext( osd, cur++, buf, 1);
    
                /* endtime of current programme + Next program */	    
                snprintf(buf, buf_length, "*     %s %s", end_time, xmltv_get_next_title( xmltv ) ? xmltv_get_next_title( xmltv ) : "");		
                tvtime_osd_list_set_multitext( osd, cur++, buf, 1);
                
            } else {
                /* No XMLTV information for this channel */
                tvtime_osd_list_set_text( osd, cur++, "");
                tvtime_osd_list_set_text( osd, cur++, "");
            }
            
        }
        tvtime_osd_list_set_lines( osd, cur );
        tvtime_osd_show_list( osd, 1, 1 );
        
        xmltv_set_channel(xmltv, old_channel);
        free(old_channel);
        xmltv_refresh( xmltv );
     
        if (cur > 1){
            return page;
        } else {
            return 0;
        }
    } else {
        tvtime_osd_list_set_text( osd, 0, "No XMLTV Program Guide information available" );
        tvtime_osd_list_set_lines( osd, 1 );
        tvtime_osd_show_list( osd, 1, 1 );
        return !page;
    }
}

 
int epg_show_perchannel( tvtime_osd_t* osd, int page, station_mgr_t *stationmgr, xmltv_t *xmltv, int channel )
{
    if (!page)
        return 0;
    if ( xmltv ){
        const int buf_length = 255;
        const int max_num_lines = 15;
//        const int num_station_per_page = max_num_lines / 3;
        const int num_stations = station_get_num_stations( stationmgr );
        char *old_channel = strdup( xmltv_get_channel( xmltv ) );
        char buf[buf_length+1];
        int cur = 0;
        time_t curtime = time( 0 );
        const char *xmltv_id = 0;
      
        if (channel > num_stations)  
            channel = 1;
        else if (channel < 1 )
            channel = num_stations;
        
        if (!(xmltv_id = station_get_xmltv_id( stationmgr, channel-1 ))) 
            xmltv_id = xmltv_lookup_channel( xmltv, station_get_name( stationmgr, channel-1 ));
        xmltv_set_channel( xmltv, xmltv_id);
        xmltv_refresh_withtime( xmltv, curtime );

        /* List header with Channel number + name */
        snprintf(buf, buf_length, "%d Next on [%s] %s:", channel, station_get_channel(stationmgr,channel-1), station_get_name( stationmgr, channel-1));
        tvtime_osd_list_set_text( osd, cur++, buf );
        tvtime_osd_list_set_hilight(osd, -1);	

        while (cur < max_num_lines) {
            xmltv_refresh_withtime( xmltv, curtime );
            
            if (xmltv_get_title( xmltv )) {
                char start_time[50];
                time_t start_timestamp = xmltv_get_start_time( xmltv );
                time_t end_timestamp = xmltv_get_end_time( xmltv );
                strftime( start_time, 50, "%H:%M", localtime( &start_timestamp ) );
                    
                /* starttime of current program + Now showing program */
                snprintf(buf, buf_length, "%s %s", start_time, xmltv_get_title( xmltv ));		
                if (xmltv_get_sub_title( xmltv )){
                    strncat(buf," (",buf_length-strlen(buf));  
                    strncat(buf,xmltv_get_sub_title( xmltv ),buf_length-strlen(buf));
                    strncat(buf,")",buf_length-strlen(buf));  
                }
                tvtime_osd_list_set_multitext( osd, cur++, buf, 1);
    
                if (!xmltv_get_next_title( xmltv )) {
                    char end_time[50];
                    /* no next program, print endtime of current programme */	    
                    strftime( end_time, 50, "%H:%M", localtime( &end_timestamp ) );
                    snprintf(buf, buf_length, "%s %s", end_time, "");		
                    tvtime_osd_list_set_multitext( osd, cur++, buf, 1);
                }
            curtime = end_timestamp;
                
            } else {
                /* No XMLTV information for this channel */
                tvtime_osd_list_set_text( osd, cur++, "");
            }
            
        }
        tvtime_osd_list_set_lines( osd, cur );
        tvtime_osd_show_list( osd, 1, 1 );
        
        xmltv_set_channel(xmltv, old_channel);
        free(old_channel);
        xmltv_refresh( xmltv );
     
    } else {
        tvtime_osd_list_set_text( osd, 0, "No XMLTV Program Guide information available" );
        tvtime_osd_list_set_lines( osd, 1 );
        tvtime_osd_show_list( osd, 1, 1 );
    }
    return channel;
}
