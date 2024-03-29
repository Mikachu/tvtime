/**
 * Copyright (c) 2002, 2003 Billy Biggs <vektor@dumbterm.net>
 * Copyright (c) 2002 Doug Bell <drbell@users.sourceforge.net>
 *
 * CC code from Nathan Laredo's ccdecode.
 * Lots of 'hey what does this mean?' code from
 * Billy Biggs and Doug Bell, like all the crap with
 * XDS and stuff.  Some help from Zapping's vbi library by
 * Michael H. Schimek and others, released under the GPL.
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

#ifndef VBIDATA_H_INCLUDED
#define VBIDATA_H_INCLUDED

#include <vbiscreen.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The VBI data object handles parsing of the VBI for NTSC sources.  We
 * handle XDS and closed caption parsing.  Closed captions are sent to
 * the vbiscreen object which handles rendering of the caption output.
 */

typedef struct vbidata_s vbidata_t;

#define CAPTURE_OFF 0
#define CAPTURE_CC1 1
#define CAPTURE_CC2 2
#define CAPTURE_CC3 4
#define CAPTURE_CC4 5
#define CAPTURE_T1  6
#define CAPTURE_T2  7
#define CAPTURE_T3  8
#define CAPTURE_T4  9

vbidata_t *vbidata_new( const char *filename, vbiscreen_t *vs, int verbose );

void vbidata_delete( vbidata_t *vbi );
void vbidata_reset( vbidata_t *vbi );
void vbidata_capture_mode( vbidata_t *vbi, int mode );
void vbidata_process_frame( vbidata_t *vbi, int printdebug );

void vbidata_capture_xds( vbidata_t *vbi, int xds );

const char *vbidata_get_program_name( vbidata_t *vbi );
const char *vbidata_get_program_type( vbidata_t *vbi );
const char *vbidata_get_program_rating( vbidata_t *vbi );
const char *vbidata_get_program_start_time( vbidata_t *vbi );
const char *vbidata_get_program_length( vbidata_t *vbi );

const char *vbidata_get_network_name( vbidata_t *vbi );
const char *vbidata_get_network_call_letters( vbidata_t *vbi );

#ifdef __cplusplus
};
#endif
#endif /* VBIDATA_H_INCLUDED */
