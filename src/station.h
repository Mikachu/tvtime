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

#ifndef STATION_H_INCLUDED
#define STATION_H_INCLUDED

#include "tvtimeconf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct station_info {
    int pos;
    char name[32];
    const char *band;
    const char *channel;
    int freq;
} station_info_t;


int station_init(config_t *ct);

int station_set(int pos);

void station_next( void );
void station_prev( void );
int station_hasChanged( void );

station_info_t *station_getInfo();

int station_adds(char *pos, char *band, char *channel, char *name);
int station_add(int pos, const char *band, const char *channel, char* name);

int station_add_band( char *band, int us_cable );

int station_scan(int band);

int station_writeConfig(FILE config);

#ifdef __cplusplus
};
#endif
#endif // STATION_H_INCLUDED
