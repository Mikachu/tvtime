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

#include "vtscreen.h"
typedef struct vtdata_s vtdata_t;

vtdata_t *vtdata_new( const char *filename, vtscreen_t *vts, int verbose );
void vtdata_delete( vtdata_t *vtd );
void vtdata_process_frame( vtdata_t *vtd, int printdebug );
