/**
 * Copyright (c) 2002 Billy Biggs <vektor@dumbterm.net>.
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

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include "deinterlace.h"

typedef struct methodlist_item_s methodlist_item_t;

struct methodlist_item_s
{
    deinterlace_method_t *method;
    methodlist_item_t *next;
};

static methodlist_item_t *methodlist = 0;

void register_deinterlace_method( deinterlace_method_t *method )
{
    methodlist_item_t **dest = &methodlist;
    methodlist_item_t *cur = methodlist;

    while( cur ) {
        if( cur->method == method ) return;
        dest = &(cur->next);
        cur = cur->next;
    }

    *dest = malloc( sizeof( methodlist_item_t ) );
    if( *dest ) {
        (*dest)->method = method;
        (*dest)->next = 0;
    } else {
        fprintf( stderr, "deinterlace: Can't allocate memory.\n" );
    }
}

int get_num_deinterlace_methods( void )
{
    methodlist_item_t *cur = methodlist;
    int count = 0;
    while( cur ) {
        count++;
        cur = cur->next;
    }
    return count;
}

deinterlace_method_t *get_deinterlace_method( int i )
{
    methodlist_item_t *cur = methodlist;

    if( !cur ) return 0;
    while( i-- ) {
        if( !cur->next ) return 0;
        cur = cur->next;
    }

    return cur->method;
}

void register_deinterlace_plugin( const char *filename )
{
    void *handle = dlopen( filename, RTLD_NOW );

    if( !handle ) {
        fprintf( stderr, "deinterlace: Can't load plugin '%s': %s\n",
                 filename, dlerror() );
    } else {
        deinterlace_plugin_init_t plugin_init;
        plugin_init = (deinterlace_plugin_init_t) dlsym( handle, "deinterlace_plugin_init" );
        if( plugin_init ) {
            plugin_init();
        }
    }
}

void filter_deinterlace_methods( int accel, int fields_available )
{
    methodlist_item_t *prev = 0;
    methodlist_item_t *cur = methodlist;

    while( cur ) {
        methodlist_item_t *next = cur->next;
        int drop = 0;

        if( (cur->method->accelrequired & accel) != cur->method->accelrequired ) {
            /* This method is no good, drop it from the list. */
            fprintf( stderr, "deinterlace: %s disabled: required "
                     "CPU accelleration features unavailable.\n",
                     cur->method->short_name );
            drop = 1;
        }
        if( cur->method->fields_required > fields_available ) {
            /* This method is no good, drop it from the list. */
            fprintf( stderr, "deinterlace: %s disabled: requires "
                     "%d field buffers, only %d available.\n",
                     cur->method->short_name, cur->method->fields_required,
                     fields_available );
            drop = 1;
        }

        if( drop ) {
            if( prev ) {
                prev->next = next;
            } else {
                methodlist = next;
            }
            free( cur );
        } else {
            prev = cur;
        }
        cur = next;
    }
}

