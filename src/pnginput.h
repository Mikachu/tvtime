/**
 * Copyright (C) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#ifndef PNGINPUT_H_INCLUDED
#define PNGINPUT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pnginput_s pnginput_t;

pnginput_t *pnginput_new( const char *filename );
void pnginput_delete( pnginput_t *pnginput );
unsigned int pnginput_get_width( pnginput_t *pnginput );
unsigned int pnginput_get_height( pnginput_t *pnginput );
unsigned char *pnginput_get_scanline( pnginput_t *pnginput, int num );
int pnginput_has_alpha( pnginput_t *pnginput );

#ifdef __cplusplus
};
#endif
#endif /* PNGINPUT_H_INCLUDED */
