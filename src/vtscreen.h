/*
 * Copyright (c) 2003 Achim Schneider <batchall@mordor.ch>
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * See COPYING in the program root directory for more details.
 */

#ifndef VTSCREEN_H_INCLUDED
#define VTSCREEN_H_INCLUDED
#include <ctype.h>
#include <libzvbi.h>
typedef struct vtscreen_s vtscreen_t;
vtscreen_t *vtscreen_new( int video_width, int video_height, 
                            double pixel_aspect, int verbose );
void vtscreen_delete( vtscreen_t *vts );

void vtscreen_register_decoder( vtscreen_t *vts, vbi_decoder *dec );
void vtscreen_notify_update( vtscreen_t *vts, vbi_pgno pgno, vbi_pgno subno );
void vtscreen_composite_packed422_scanline( vtscreen_t *vts,
                                             uint8_t *output,
                                             int width, int xpos, 
                                             int scanline );

#endif
