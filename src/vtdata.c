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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "vtdata.h"

#include <libzvbi.h>

struct vtdata_s {
    vbi_capture *cap;
    vbi_decoder *dec;
    vbi_raw_decoder *par;
    
    int src_w, src_h;
    
    vbi_sliced *sliced;
    vtscreen_t *vts;
    
    int verbose;
};

void vtevent(vbi_event *ev, void *data) 
{
    vtscreen_notify_update( ((vtdata_t *)data)->vts, ev->ev.ttx_page.pgno, ev->ev.ttx_page.subno );
}

vtdata_t *vtdata_new( const char *filename, vtscreen_t *vts, int verbose ) 
{
    unsigned int services= VBI_SLICED_TELETEXT_B;
    char *errstr;
    vtdata_t *vtd= malloc( sizeof( struct vtdata_s ) );
    if( !vtd ) return 0;
    
    vtd->cap = vbi_capture_v4l_new ( "/dev/vbi",
						   625,
						   &services,
						   /* strict */ -1,
						   &errstr,
						   /* trace */ verbose);
    if( !vtd->cap ) {
        fprintf( stderr, "vtdata: %s\n", errstr );
        free( errstr );
        free( vtd );
        return 0;
    }

    vtd->dec= vbi_decoder_new();
    if( !vtd->dec ) {
        fprintf( stderr, "vtdata: Couldn't init VBI decoder\n" );
        vbi_capture_delete( vtd->cap );
        free( vtd );
        return 0;
    }
    vtscreen_register_decoder( vts, vtd->dec );
   	
    vtd->par = vbi_capture_parameters( vtd->cap );
    vtd->src_w = vtd->par->bytes_per_line / 1;
	vtd->src_h = vtd->par->count[0] + vtd->par->count[1]; 
    
    vtd->sliced = malloc(sizeof(vbi_sliced) * vtd->src_h);
    if( !vtd->sliced ) {
        fprintf( stderr, "vtdata: Couldn't create input buffer: %s\n", strerror(errno) );
        vbi_capture_delete( vtd->cap );
        vbi_decoder_delete( vtd->dec );
        free( vtd );
    }

    vbi_event_handler_register( vtd->dec, VBI_EVENT_TTX_PAGE, &vtevent, vtd );
    
    
    vtd->vts= vts;
    vtd->verbose= verbose;

    return vtd;
}

void vtdata_delete( vtdata_t *vtd ) 
{
    vbi_capture_delete( vtd->cap );
    free( vtd );
}
void vtdata_process_frame( vtdata_t *vtd, int printdebug ) 
{
    int r;
    double timestamp;
    struct timeval timeout;
    int lines;
    r= vbi_capture_read_sliced( vtd->cap, vtd->sliced, &lines, &timestamp, &timeout ); 
    switch (r) {
		case -1:
			fprintf(stderr, "vtdata: VBI read error: %s\n", strerror(errno));
			return;
		case 0: 
			fprintf(stderr, "vtdata: VBI read timeout\n");
			return;
		case 1:
			break;
    }
    vbi_decode( vtd->dec, vtd->sliced, lines, timestamp );

    //fprintf(stderr, "vtdata: processing frame\n");
}
