/**
 * Copyright (C) 2002 Doug Bell <drbell@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef PARSER_H_INCLUDED
#define PARSER_H_INCLUDED

struct parser_file_s {
    FILE *fh;
    long file_length;
    char *file_contents;
    struct nv_pair *nv_pairs;
    int num_pairs;
    char *filename;
};

typedef struct parser_file_s parser_file_t;

int parser_new( parser_file_t *pf, const char *filename );
const char *parser_get( parser_file_t *pf, const char *name, const char *def );
void parser_delete( parser_file_t *pf );
int parser_dump( parser_file_t *pf );

#endif /* PARSER_H_INCLUDED */
