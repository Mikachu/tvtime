/**
 * Copyright (c) 2004 Billy Biggs <vektor@dumbterm.net>.
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

#include <gtk/gtk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# include "gettext.h"
#else
# define _(string) string
#endif
#include <videodev.h>
#include <videodev2.h>

typedef struct v4l_device_s v4l_device_t;
struct v4l_device_s
{
    char name[ 256 ];
    char device[ 256 ];
    char driver[ 256 ];
    v4l_device_t *next;
};

static v4l_device_t *devices = 0;

void probe_device( const char *device )
{
    struct video_capability caps_v4l1;
    struct v4l2_capability caps_v4l2;
    int isv4l2 = 0;
    int fd;
    int i;
    v4l_device_t *cur = malloc( sizeof( v4l_device_t ) );

    if( !cur ) {
        fprintf( stderr, "Cannot allocate memory.\n" );
        return;
    }

    snprintf( cur->device, sizeof( cur->device ), device );
    snprintf( cur->driver, sizeof( cur->driver ), "Unknown" );
    snprintf( cur->name, sizeof( cur->name ), "Unknown" );

    /* First, open the device. */
    fd = open( device, O_RDWR );
    if( fd < 0 ) {
        fprintf( stderr, "Cannot open capture device %s: %s\n",
                 device, strerror( errno ) );
        free( cur );
        return;
    }

    /**
     * Next, ask for its capabilities.  This will also confirm it's a V4L2 
     * device. 
     */
    if( ioctl( fd, VIDIOC_QUERYCAP, &caps_v4l2 ) < 0 ) {
        /* Can't get V4L2 capabilities, maybe this is a V4L1 device? */
        if( ioctl( fd, VIDIOCGCAP, &caps_v4l1 ) < 0 ) {
            fprintf( stderr, "videoinput: %s is not a video4linux device.\n",
                     device );
            close( fd );
            free( cur );
            return;
        } else {
/*
            fprintf( stderr, "videoinput: Using video4linux driver '%s'.\n"
                             "videoinput: Card type is %x, audio %d.\n",
                     caps_v4l1.name, caps_v4l1.type, caps_v4l1.audios );
*/
            snprintf( cur->name, sizeof( cur->name ), caps_v4l1.name );
        }
        //snprintf( drivername, sizeof( vidin->drivername ), "%s", caps_v4l1.name );
    } else {
/*
        fprintf( stderr, "videoinput: Using video4linux2 driver '%s', card '%s' (bus %s).\n"
                         "videoinput: Version is %u, capabilities %x.\n",
                 caps_v4l2.driver, caps_v4l2.card, caps_v4l2.bus_info,
                 caps_v4l2.version, caps_v4l2.capabilities );
*/
        snprintf( cur->name, sizeof( cur->name ), caps_v4l2.card );
        snprintf( cur->driver, sizeof( cur->driver ), caps_v4l2.driver );
        isv4l2 = 1;
        /*
        snprintf( vidin->drivername, sizeof( vidin->drivername ),
                  "%s (card %s, bus %s) - %u",
                  caps_v4l2.driver, caps_v4l2.card,
                  caps_v4l2.bus_info, caps_v4l2.version );
        */
    }

    if( !isv4l2 ) {
        /* Check if this is a bttv-based card.  Code taken from xawtv. */
#define BTTV_VERSION            _IOR('v' , BASE_VIDIOCPRIVATE+6, int)
        /* dirty hack time / v4l design flaw -- works with bttv only
         * this adds support for a few less common PAL versions */
        if( !(ioctl( fd, BTTV_VERSION, &i ) < 0) ) {
            snprintf( cur->driver, sizeof( cur->driver ), "bttv" );
        }
#undef BTTV_VERSION
    }

    cur->next = devices;
    devices = cur;
}

GtkWidget *build_capture_card_list( void )
{
    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkWidget *tree;
    GtkTreeIter iter;
    v4l_device_t *cur = devices;

    store = gtk_list_store_new( 3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
    tree = gtk_tree_view_new_with_model( GTK_TREE_MODEL( store ) );
    //gtk_tree_view_set_headers_visible( GTK_TREE_VIEW( tree ), FALSE );

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes( "Name", renderer, "text", 0, 0 );
    gtk_tree_view_append_column( GTK_TREE_VIEW( tree ), column );

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes( "Driver", renderer, "text", 1, 0 );
    gtk_tree_view_append_column( GTK_TREE_VIEW( tree ), column );

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes( "Device", renderer, "text", 2, 0 );
    gtk_tree_view_append_column( GTK_TREE_VIEW( tree ), column );

    while( cur ) {
        gtk_list_store_append( store, &iter );
        gtk_list_store_set( store, &iter, 0, cur->name, 1, cur->driver, 2, cur->device, -1 );
        cur = cur->next;
    }

/*
    gtk_tree_store_append( wv->store, &iter, 0 );
    gtk_tree_store_set( wv->store, &iter,
                        ID_COLUMN, id,
                        NAME_COLUMN, name,
                        X_COLUMN, xtext,
                        Y_COLUMN, ytext,
                        W_COLUMN, wtext,
                        H_COLUMN, htext,
                        EVENTS_COLUMN, eventmask,
                        OVERRIDE_COLUMN, override, -1 );
*/

    return tree;
}

GtkWidget *build_location_setup( void )
{
    GtkWidget *hbox = gtk_hbox_new( FALSE, 6 );
    GtkWidget *label;
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *optionmenu = gtk_option_menu_new();
    GtkWidget *item;

    label = gtk_label_new( "Television standard:" );
    gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );

    item = gtk_menu_item_new_with_label( "NTSC (North America)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "NTSC-JP (Japan)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "PAL (Europe)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "SECAM (France)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "PAL-M (Brazil)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "PAL-Nc (Argentina)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "PAL-N (South America)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );
    item = gtk_menu_item_new_with_label( "PAL-60 (NTSC-to-PAL converters)" );
    gtk_menu_shell_append( GTK_MENU_SHELL( menu ), item );

    gtk_option_menu_set_menu( GTK_OPTION_MENU( optionmenu ), menu );
    gtk_box_pack_start( GTK_BOX( hbox ), optionmenu, FALSE, FALSE, 0 );

    return hbox;
}

GtkWidget *build_xmltv_settings( void )
{
    GtkWidget *hbox = gtk_hbox_new( FALSE, 6 );
    GtkWidget *label;
    GtkWidget *entry;
    GtkWidget *browse;

    label = gtk_label_new( "XMLTV File:" );
    gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );

    entry = gtk_entry_new();
    gtk_box_pack_start( GTK_BOX( hbox ), entry, TRUE, TRUE, 0 );

    browse = gtk_button_new_from_stock( GTK_STOCK_OPEN );
    //browse = gtk_button_new_with_label( "Browse" );
    gtk_box_pack_start( GTK_BOX( hbox ), browse, FALSE, FALSE, 0 );

    return hbox;
}

int main( int argc, char **argv )
{
    GtkWidget *dialog;
    GtkWidget *label;
    GtkWidget *location_setup;
    GtkWidget *capture_cards;
    GtkWidget *xmltv_settings;
    GtkWidget *alignment;
    GtkWidget *hbox;
    gint result;

    gtk_init( &argc, &argv );

    probe_device( "/dev/video0" );
    probe_device( "/dev/video1" );
    probe_device( "/dev/video2" );
    probe_device( "/dev/video3" );

    dialog = gtk_dialog_new_with_buttons( "tvtime Configuration",
               0, 0, GTK_STOCK_OK, GTK_RESPONSE_OK,
               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 0 );

    /* HIG setup. */
    gtk_container_set_border_width( GTK_CONTAINER( dialog ), 6 );
    gtk_dialog_set_has_separator( GTK_DIALOG( dialog ), 0 );
    gtk_box_set_spacing( GTK_BOX( GTK_DIALOG( dialog )->vbox ), 12 );
    gtk_dialog_set_default_response( GTK_DIALOG( dialog ), GTK_RESPONSE_CANCEL );

    label = gtk_label_new( "<b>Capture card</b>" );
    gtk_label_set_use_markup( GTK_LABEL( label ), 1 );
    alignment = gtk_alignment_new( 0.0, 0.5, 0.0, 0.0 );
    gtk_container_add( GTK_CONTAINER( alignment ), label );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), alignment, FALSE, FALSE, 0 );

    hbox = gtk_hbox_new( TRUE, 0 );
    capture_cards = build_capture_card_list();
    gtk_box_pack_start( GTK_BOX( hbox ), capture_cards, TRUE, TRUE, 6 );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog )->vbox ), hbox );

    label = gtk_label_new( "<b>Location</b>" );
    gtk_label_set_use_markup( GTK_LABEL( label ), 1 );
    alignment = gtk_alignment_new( 0.0, 0.5, 0.0, 0.0 );
    gtk_container_add( GTK_CONTAINER( alignment ), label );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), alignment, FALSE, FALSE, 0 );

    hbox = gtk_hbox_new( TRUE, 0 );
    location_setup = build_location_setup();
    gtk_box_pack_start( GTK_BOX( hbox ), location_setup, TRUE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), hbox, FALSE, FALSE, 0 );

    label = gtk_label_new( "<b>Program Listings</b>" );
    gtk_label_set_use_markup( GTK_LABEL( label ), 1 );
    alignment = gtk_alignment_new( 0.0, 0.5, 0.0, 0.0 );
    gtk_container_add( GTK_CONTAINER( alignment ), label );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), alignment, FALSE, FALSE, 0 );

    hbox = gtk_hbox_new( TRUE, 0 );
    xmltv_settings = build_xmltv_settings();
    gtk_box_pack_start( GTK_BOX( hbox ), xmltv_settings, TRUE, TRUE, 6 );
    gtk_box_pack_start( GTK_BOX( GTK_DIALOG( dialog )->vbox ), hbox, FALSE, FALSE, 0 );


    gtk_widget_show_all( dialog );

    result = gtk_dialog_run( GTK_DIALOG( dialog ) );
    gtk_widget_destroy( dialog );

    fprintf( stderr, "dialog result: %d\n", result );


/*
    probe_device( "/dev/video0" );
    probe_device( "/dev/video1" );
    probe_device( "/dev/video2" );
    probe_device( "/dev/video3" );

    dialog = gtk_message_dialog_new( 0,
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "Error you suck" );
    gtk_dialog_run( GTK_DIALOG( dialog ) );
    gtk_widget_destroy( dialog );
*/

    return 0;
}

