/**
 * Copyright (C) 2002, 2004 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef TVTIME_PLUGINS_H_INCLUDED
#define TVTIME_PLUGINS_H_INCLUDED

/**
 * tvtime was going to have a plugin system, but there
 * was never any interest in it outside of tvtime, so instead
 * we include all deinterlacer methods right in the tvtime
 * executable.
 */

void linear_plugin_init( void );
void scalerbob_plugin_init( void );
void linearblend_plugin_init( void );
void vfir_plugin_init( void );
void dscaler_tomsmocomp_plugin_init( void );
void dscaler_greedyh_plugin_init( void );
void greedy_plugin_init( void );
void weave_plugin_init( void );
void weavetff_plugin_init( void );
void weavebff_plugin_init( void );

#endif /* TVTIME_PLUGINS_H_INCLUDED */
