/**
 * Copyright (c) 2003 Achim Schneider <batchall@mordor.ch>
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
#include "station.h"
#include "freq.h"

typedef struct node {
	struct node *next, *prev;
	station_info_t *info;
} node_t;

typedef node_t *node;


node first= NULL;
node current= NULL;

int debug= 0;
int verbose= 0;

node newNode(node next, node prev, station_info_t *info) {
	node_t *n;
	if ( NULL != ( n= malloc(sizeof *n) ) ) {
		n->next= next;
		n->prev= prev;
		n->info= info;
		return n;
	}
	return 0;
}

station_info_t *newInfo( int pos, const char *name, const char *band, const char *channel, int freq) {
	station_info_t *i;

	if ( NULL != ( i= malloc(sizeof *i) ) ) {
		i->pos= pos;
		i->band= band;
		i->channel= channel;
		i->freq= freq;
		strncpy(i->name, name, 32);
		i->name[31]= '\0';
		return i;
	}
	return 0;
}
	

int insert(station_info_t *i) {
	if ( NULL == first ) {
		if (( first= newNode(NULL,NULL,i) )) {
			first->next= first;
			first->prev= first;
			return 1;
		}
		return 0;
	} else {
		node n;
		node rp= first;
		do {
			if ( rp->info->pos == i->pos ) {
				fprintf(stderr, "station: Position %d already in use\n", i->pos);
				return 0;
			}
			
			if ( rp->info->pos >  i->pos ) break;
			
		} while ( ( rp= rp->next) != first );

		if (( n= newNode( rp, rp->prev, i ) )) {
			rp->prev->next= n;
			rp->prev= n;

			if ( rp == first )
				if ( n->info->pos < first->info->pos )
					first= n;
			return 1;
		}
		fprintf(stderr, "station: Out of Memory\n");
		return 0;
	}
}

void station_dump() {
	node rp= first;
	printf("#\tBand\tChannel\tFreq\tName\n" );
	do {	
		printf("%d\t%s\t%s\t%d\t%s\n",rp->info->pos, rp->info->band,
			rp->info->channel, rp->info->freq, rp->info->name);
	} while ( ( rp= rp->next) != first );
}



int station_init(config_t *ct) {
	debug= config_get_debug(ct);
	verbose= config_get_verbose(ct);
	
	// this is a config file parser
	station_add( 1, "vhf E2-E12", "e4", "ard" );
	station_add( 2, "VHF E2-E12", "E8", "zdf" );
	station_add( 3, "VHF E2-E12", "E6", "ndr" );

	station_add_band("US Cable",0);//US_CABLE_HRC);
	
	if ( NULL == first ) {
		insert(newInfo( 1, "dummy", "none", "none", 0 ));
	}
	current=first;
	if ( verbose ) {
		station_dump();
	}
	return 1;
}


int station_set(int pos) {
	node rp= first;
	do {
		if ( rp->info->pos == pos ) {
			current= rp;
			return 1;
		}
	} while ( (rp= rp->next) != first );
	return 0;
}

void station_next( void )
{
    current = current->next;
}

void station_prev( void )
{
    current = current->prev;
}

station_info_t *station_getInfo( void )
{
    return current->info;
}



int station_adds(char *pos, char *band, char *channel, char *name);
int station_add(int pos, const char *band, const char *channel, char *name) {
	int freq;
	station_info_t *i;

	if ( 0 == ( freq= freq_byName(&band,&channel,US_CABLE_NOMINAL) ) ) {
		fprintf(stderr, "station: No frequency known for %s %s\n", band, channel);
		return 0;
	}

	if ( 0 == ( i= newInfo( pos, name, band, channel, freq ) ) ) {
		fprintf(stderr, "station: Out of Memory");
		return 0;
	}


	if ( insert(i) ) 
		return 1;

	free(i);
	return 0;
}

int isFreePos( int pos ) {
	node rp= first;
	do {
		if ( pos == rp->info->pos ) return 0;
	} while ( ( rp= rp->next) != first );
	return 1;
}

int getNextPos() {
	return first->prev->info->pos + 1;
}


void freq_callback( char *band, char *channel, unsigned int freq ) {
	int pos;
	station_info_t *i;

	
	if ( 1 == sscanf(channel,"%d", &pos) ) {
		if ( !isFreePos(pos) ) 
			pos= getNextPos();
	} else
		pos= getNextPos();
	
	if ( 0 == ( i= newInfo( pos, channel, band, channel, freq ) ) ) {
		fprintf(stderr, "station: Out of Memory");
		return;
	}


	if ( insert(i) ) 
		return;

	free(i);
	return;

	
}


int station_add_band( char* band, int us_cable ) {
	freq_for_band( band, us_cable, freq_callback );
	return 1;
}

int station_scan(int band) {
	return 0;
}

int station_writeConfig(FILE config) {
	return 0;
}
