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

#ifndef LIRCCLIENT_H_INCLUDED
#define LIRCCLIENT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "commands.h"

/**
 * These functions are static, there is no object.  This is because
 * lirc_client uses static data and will only allow one client per
 * process.
 */

/**
 * Initialize lirc client.
 */
int lirc_open( void );

/**
 * Shutdown our lirc client.
 */
void lirc_shutdown( void );

/**
 * Poll commands from lirc.  Call this often.
 */
void lirc_poll( commands_t *commands );

#ifdef __cplusplus
};
#endif
#endif /* LIRCCLIENT_H_INCLUDED */
