
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include "speedy.h"
#include "deinterlace.h"

typedef struct methodlist_item_s methodlist_item_t;

struct methodlist_item_s
{
    deinterlace_method_t *method;
    void *dlhandle;
    methodlist_item_t *next;
};

static methodlist_item_t *methodlist = 0;

void register_deinterlace_method_handle( deinterlace_method_t *method, void *dlhandle )
{
    methodlist_item_t **dest;

    if( !methodlist ) {
        dest = &methodlist;
    } else {
        methodlist_item_t *cur = methodlist;
        while( cur->next ) cur = cur->next;
        dest = &(cur->next);
    }

    *dest = (methodlist_item_t *) malloc( sizeof( methodlist_item_t ) );
    if( *dest ) {
        (*dest)->method = method;
        (*dest)->next = 0;
        (*dest)->dlhandle = dlhandle;
    } else {
        fprintf( stderr, "deinterlace: Can't allocate memory.\n" );
    }
}

void register_deinterlace_method( deinterlace_method_t *method )
{
    register_deinterlace_method_handle( method, 0 );
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
        plugin_init = dlsym( handle, "deinterlace_plugin_init" );
        if( plugin_init ) {
            deinterlace_method_t *method = plugin_init( speedy_get_accel() );
            if( method ) {
                register_deinterlace_method_handle( method, handle );
            }
        }
    }
}

