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

struct hashtable_object {
	enum { NEVER_USED, CLEARED, IN_USE } status;
	int index;
	void *data;
};

struct hashtable *
hashtable_init (size_t size)
{
	struct hashtable *ht;
	ht = malloc (sizeof (*ht));
	if (ht == NULL)
		return NULL;
	
	ht->table = malloc (sizeof (*(ht->table)) * size);
	if (ht->table == NULL) {
                free (ht);
                return NULL;
        }
	 
	ht->size = size;
	for (int i = 0; i < size; i++) {
		ht->table [i].status = NEVER_USED;
		ht->table [i].index = 0;
		ht->table [i].data = NULL;
	}
	return ht;
}

static struct hashtable_object *
hashtable_probe (struct hashtable *ht, int index, int probe)
{
	// Quadratic probing
	return &ht->table[(index + probe*probe) % ht->size];
}    

static struct hashtable_object *
hashtable_find (struct hashtable *ht, int index)
{
	struct hashtable_object *obj;
	for (int probe = 0; probe < ht->size; probe++) {
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

void *
hashtable_lookup (struct hashtable *ht, int index)
{
	struct hashtable_object *obj = hashtable_find (ht, index);
	if (obj == NULL)
		return NULL;
	else
		return obj->data;
}

// BEWARE. Untested code. (pvz)
int
hashtable_resize (struct hashtable *htold)
{
        // Resizes the hash table to be 4 times bigger than before.
        struct hashtable *htnew = hashtable_init (htold->size * 4);
        if (htnew == NULL)
                return 1;
        for (struct hashtable_object *p = &htold->table[0];
             p < &htold->table[htold->size]; p++) {
                if (p->status == IN_USE)
                        hashtable_insert (htnew, p->index, p->data);
        }
        free (htold->table);
        htold->table = htnew->table;
        htold->size = htnew->size;
        free (htnew);
        return 0;
}

int
hashtable_insert (struct hashtable *ht, int index, void *data)
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
                        if (hashtable_resize (ht) == 1) {
                                // Out of memory!
                                return 1;
                        } else {
                                // Let's try this again.
                                probe = 0;
                                continue;
                        }
                }
	obj->status = IN_USE;
	obj->index = index;
	obj->data = data;
	return 0;
}

int
hashtable_delete (struct hashtable *ht, int index)
{
	struct hashtable_object *obj = hashtable_find (ht, index);
	if (obj == NULL)
		return 1;
	else {
		obj->status = CLEARED;
		return 0;
	}
}

// Caution: Only use when you're certain all elements have been allocated
// using malloc.
void
hashtable_freeall (struct hashtable *ht)
{
        for (struct hashtable_object *p = ht->table;
             p < ht->table + ht->size; p++)
                if (p->status == IN_USE)
                        free (p->data);
}

void
hashtable_destroy (struct hashtable *ht)
{
	free (ht->table);
	free (ht);
}
