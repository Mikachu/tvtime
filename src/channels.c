/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "channels.h"

struct frequency_table_s
{
    char *name;
    int minchan;
    int maxchan;
    int channels[ 256 ];
};

frequency_table_t *frequency_table_new( const char *name )
{
    frequency_table_t *table = (frequency_table_t *) malloc( sizeof( frequency_table_t ) );
    if( !table ) {
        return 0;
    }

    table->name = strdup( name );
    table->minchan = 0;
    table->maxchan = 255;
    memset( table->channels, 0, sizeof( table->channels ) );

    return table;
}

void frequency_table_delete( frequency_table_t *table )
{
    if( table->name ) free( table->name );
    free( table );
}

const char *frequency_table_get_name( frequency_table_t *table )
{
    if( table->name ) {
        return table->name;
    } else {
        return "Nameless frequency table";
    }
}

void frequency_table_set_min_channel_number( frequency_table_t *table, int min )
{
    table->minchan = min;
}

void frequency_table_set_max_channel_number( frequency_table_t *table, int max )
{
    table->maxchan = max;
}

void frequency_table_set_valid_norm( frequency_table_t *table, const char *norm )
{
}

void frequency_table_add_channel( frequency_table_t *table, int channelnum, int freq )
{
    int pos = channelnum - table->minchan;
    if( pos >= 0 && pos < ( sizeof( table->channels ) / sizeof( int ) ) ) {
        table->channels[ pos ] = freq;
    }
}

int frequency_table_get_min_channel_number( frequency_table_t *table )
{
    return table->minchan;
}

int frequency_table_get_max_channel_number( frequency_table_t *table )
{
    return table->maxchan;
}

int frequency_table_get_channel_frequency( frequency_table_t *table, int channelnum )
{
    int pos = channelnum - table->minchan;

    if( pos >= 0 && pos < ( sizeof( table->channels ) / sizeof( int ) ) ) {
        return table->channels[ channelnum ];
    } else {
        return 0;
    }
}


void channels_read_channel_file( channels_t *channels, const char *filename )
{
    FILE *channelfile;
    char line[ 256 ];
    char *Pos;
    char *Pos1;
    char *eol_ptr;
    int channelCounter = 0;
    frequency_table_t *curtable = 0;

    channelfile = fopen( filename, "r" );
    if( !channelfile ) {
        return;
    }

    while( fgets( line, sizeof( line ), channelfile ) ) {

        /* Find end of line. */
        eol_ptr = strstr( line, ";" );
        if( !eol_ptr ) eol_ptr = strstr( line, "\n" );
        if( eol_ptr ) *eol_ptr = '\0';
        if( eol_ptr == line ) continue;

        /* If we're starting a new table, finish the old one initialize the new one. */
        if( ( Pos = strstr( line, "[" ) ) && ( Pos1 = strstr( line, "]" ) ) && Pos1 > Pos ) {
            char *dummy;

            if( curtable ) {
                channels_add_frequency_table( channels, curtable );
            }

            Pos++;
            dummy = Pos;
            dummy[ Pos1 - Pos ] = '\0';                          
            channelCounter = 0;
            curtable = frequency_table_new( dummy );
        } else {

            if( !curtable ) {
                fclose( channelfile );                
                return;
            }
            
            if( ( Pos = strstr( line, "ChannelLow=" ) ) ) {                
                frequency_table_set_min_channel_number( curtable, atoi( Pos + strlen( "ChannelLow=" ) ) );
            } else if( ( Pos = strstr( line, "ChannelHigh=" ) ) ) {
                frequency_table_set_max_channel_number( curtable, atoi( Pos + strlen( "ChannelHigh=" ) ) );
            } else if( ( Pos = strstr( line, "Format=" ) ) ) {
                frequency_table_set_valid_norm( curtable, Pos + strlen( "Format=" ) );
            } else {
                Pos = line;
                while( *Pos != '\0' ) {
                    if( ( *Pos >= '0' ) && ( *Pos <= '9' ) ) {       
                        int freq = atoi( Pos );

                        // Increment channels for 0 freqs, so that you see gaps...
                        // but do not add the channel (that is the legacy behaviour)
                        if( freq > 0 ) {
                            int channelNumber = frequency_table_get_min_channel_number( curtable ) + channelCounter;
                            frequency_table_add_channel( curtable, channelNumber, freq );
                        }
                        channelCounter++;
                        break;
                    }
                    Pos++;
                }
            }
        }
    }

    if( curtable ) {
        channels_add_frequency_table( channels, curtable );
    }

    fclose( channelfile );
}

