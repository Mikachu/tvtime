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

struct hashtable {
	size_t size;
	struct hashtable_object *table;
};

struct hashtable *hashtable_init (size_t size);
//struct hashtable_object *hashtable_find (struct hashtable *ht, int index);
void *hashtable_lookup (struct hashtable *ht, int index);
int hashtable_insert (struct hashtable *ht, int index, void *data);
int hashtable_delete (struct hashtable *ht, int index);
void hashtable_destroy (struct hashtable *ht);
void hashtable_freeall (struct hashtable *ht);
