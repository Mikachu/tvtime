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

/**
 * Class providing a resizing hash table for storing pointers to objects
 * whose memory is externally managed.
 */

typedef struct hashtable_s hashtable_t;

/**
 * Create a new hash table with the given initial size.
 */
hashtable_t *hashtable_init( size_t size );

/**
 * Frees the memory used by the table.  Does not free any of the
 * inserted elements.
 */
void hashtable_destroy( hashtable_t *ht );

/**
 * Looks up and returns an element from the hash table.  Returns 0 on
 * error.
 */
void *hashtable_lookup( hashtable_t *ht, int index );

/**
 * Insert a new element into the hash table, resizing the hash table if
 * necessary.  Returns 0 on error.
 */
int hashtable_insert( hashtable_t *ht, int index, void *data );

/**
 * Deletes an element from the hash table.  Will not free the memory if
 * the object was a pointer.
 */
int hashtable_delete( hashtable_t *ht, int index );

/**
 * Class providing an iterator for hashtable_t
 */

typedef struct hashtable_iterator_s hashtable_iterator_t;

/**
 * Constructs an interator.
 */

hashtable_iterator_t *hashtable_iterator_init (hashtable_t *ht);

/**
 * Iterates through the hash table preinc steps, returns what it finds,
 * and then increments postinc steps.
 * It will put the hashtable index in *index.
 */

void *hashtable_iterator_go (hashtable_iterator_t *iter,
                             int preinc, int postinc, int *index);

/**
 * Destroys an iterator.
 */

void hashtable_iterator_destroy (hashtable_iterator_t *iter);

#ifdef __cplusplus
};
#endif
#endif /* HASHTABLE_H_INCLUDED */
