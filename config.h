/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 *
 * Mixer routines stolen from mplayer, http://mplayer.sourceforge.net.
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

#ifndef CONFIG_H
#define CONFIG_H

#include "parser.h"

struct config_t {
    parser_file_t pf;

    int outputwidth;
    int inputwidth;
    int verbose;
    int aspect;
    int debug;
    int apply_luma_correction;
    int output420;
    char *v4ldev;
    int inputnum;
    double luma_correction;
    char *norm;
    char *freq;
    int tuner_number;
};

int config_new( struct config_t *ct, const char *filename );
int config_init( struct config_t *ct );
int config_dump( struct config_t *ct );

#endif
