/**
 * Copyright (c) 2003 Billy Biggs <vektor@dumbterm.net>.
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
#include "utils.h"
#include "tvtimeconf.h"

int main( int argc, char **argv )
{
    config_t *cfg;

    /*
     * Setup i18n. This has to be done as early as possible in order
     * to show startup messages in the users preferred language.
     */
    setup_i18n();

    cfg = config_new();
    if( !cfg ) {
        fprintf( stderr, _("%s: Cannot allocate memory.\n"), argv[ 0 ] );
        return 1;
    }

    config_parse_tvtime_config_command_line( cfg, argc, argv );
    config_delete( cfg );
    return 0;
}

