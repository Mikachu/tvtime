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

#include <stdlib.h>
#include "hashtable.h"

struct hashtable_s {
	size_t size;
	struct hashtable_object *table;
};

struct hashtable_iterator_s {
	hashtable_t *ht;
	int index;
};

struct hashtable_object {
	enum { NEVER_USED, CLEARED, IN_USE } status;
	int index;
	void *data;
};

hashtable_t *hashtable_init( size_t size )
{
	hashtable_t *ht;
	int i;

	ht = malloc (sizeof (*ht));
	if (ht == NULL)
		return NULL;
	
	ht->table = malloc (sizeof (*(ht->table)) * size);
	if (ht->table == NULL) {
                free (ht);
                return NULL;
        }
	 
	ht->size = size;
	for (i = 0; i < size; i++) {
		ht->table [i].status = NEVER_USED;
		ht->table [i].index = 0;
		ht->table [i].data = NULL;
	}
	return ht;
}

static struct hashtable_object * hashtable_probe( hashtable_t *ht, int index, int probe )
{
	// Quadratic probing
	return &ht->table[(index + probe*probe) % ht->size];
}    

static struct hashtable_object *hashtable_find( hashtable_t *ht, int index )
{
	struct hashtable_object *obj;
	int probe;

	for (probe = 0; probe < ht->size; probe++) {
		obj = hashtable_probe (ht, index, probe);
		switch (obj->status) {
		case NEVER_USED:
			// Object does not exist.
			return NULL;
		case CLEARED:
			if (obj->index == index) {
				// Object used to exist, but doesn't any more.
				return NULL;
			} else {
				// An object used to be here, but not this one.
				// Continue probing.
				break;
			}
		case IN_USE:
			if (obj->index == index) {
				// Eureka!
				return obj;
			} else {
				// An object is here, but not the one we're
				// looking for. Continue probing.
				break;
			}
		default:
			// Hash table is broken. What now?
			return NULL;
		}
	}
	// If we broke out of the loop, it means the hash table is full, and
	// that we didn't find the object. What now?
	return NULL;
}

void *hashtable_lookup( hashtable_t *ht, int index )
{
	struct hashtable_object *obj = hashtable_find (ht, index);

	if (obj == NULL)
		return NULL;
	else
		return obj->data;
}

// BEWARE. Untested code. (pvz)
int hashtable_resize( hashtable_t *htold )
{
        // Resizes the hash table to be 5 times bigger than before.
	// Better by 5 than by 4, since 4 == 2*2. The fewer factors
	// in a hash table size the better. (Optimally, the number
	// is prime, but calculating prime numbers runtime is expensive.)
        hashtable_t *htnew = hashtable_init (htold->size * 5);
	struct hashtable_object *p;

        if (htnew == NULL)
                return 0;
        for (p = &htold->table[0]; p < &htold->table[htold->size]; p++) {
                if (p->status == IN_USE)
                        hashtable_insert (htnew, p->index, p->data);
        }
        free (htold->table);
        htold->table = htnew->table;
        htold->size = htnew->size;
        free (htnew);
        return 1;
}

int hashtable_insert( hashtable_t *ht, int index, void *data )
{
	int probe = 0;
	struct hashtable_object *obj;

	while ((obj = hashtable_probe (ht, index, probe))->status == IN_USE)
		if (obj->index == index) // object already existed. overwrite.
			break;
		else if (probe < ht->size)
			probe++;
		else {
                        // The hash table is full. Resize it.
                        // BEWARE, untested code. (pvz)
                        if (!hashtable_resize (ht)) {
                                // Out of memory!
                                return 0;
                        } else {
                                // Let's try this again.
                                probe = 0;
                                continue;
                        }
                }
	obj->status = IN_USE;
	obj->index = index;
	obj->data = data;
	return 1;
}

int hashtable_delete( hashtable_t *ht, int index )
{
	struct hashtable_object *obj = hashtable_find (ht, index);

	if (obj == NULL)
		return 0;
	else {
		obj->status = CLEARED;
		return 1;
	}
}

void hashtable_destroy( hashtable_t *ht )
{
	free (ht->table);
	free (ht);
}

hashtable_iterator_t *hashtable_iterator_init (hashtable_t *ht)
{
	hashtable_iterator_t *iter = malloc (sizeof *iter);

	if (iter == NULL)
		return NULL;
	iter->ht = ht;
	iter->index = 0;
	return iter;
}

void *hashtable_iterator_go (hashtable_iterator_t *iter,
			     int preinc, int postinc, int *index)
{
	// TODO: Support decrementation
	int i;
	void *ret = NULL;

	*index = -1;
	// First, check if the iterator is on an object in the hash table
	if (iter->ht->table[iter->index].status != IN_USE)
		preinc++;
	for (i = 0; i < preinc; i++) {	
		while (iter->ht->table[iter->index].status != IN_USE) {
			iter->index++;
			if (iter->index >= iter->ht->size)
				return NULL;
		}
	}
	*index = iter->ht->table[iter->index].index;
	ret = iter->ht->table[iter->index++].data;
	for (i = 0; i < postinc; i++) {
		while (iter->ht->table[iter->index].status != IN_USE) {
			iter->index++;
			if (iter->index >= iter->ht->size)
				return ret;
		}
	}
	return ret;
}

void hashtable_iterator_destroy (hashtable_iterator_t *iter)
{
	free (iter);
}
