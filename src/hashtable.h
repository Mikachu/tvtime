/**
 * Copyright (c) 2003 Per von Zweigbergk <pvz@e.kth.se>
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

#ifndef HASHTABLE_H_INCLUDED
#define HASHTABLE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hashtable_s hashtable_t;

hashtable_t *hashtable_init( size_t size );
void *hashtable_lookup( hashtable_t *ht, int index );
int hashtable_insert( hashtable_t *ht, int index, void *data );
int hashtable_delete( hashtable_t *ht, int index );
void hashtable_destroy( hashtable_t *ht );
void hashtable_freeall( hashtable_t *ht );

#ifdef __cplusplus
};
#endif
#endif /* HASHTABLE_H_INCLUDED */
