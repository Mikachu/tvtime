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

#ifndef MENU_H_INCLUDED
#define MENU_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_s menu_t;

menu_t *menu_new( const char *name );
void menu_delete( menu_t *menu );

void menu_set_text( menu_t *menu, int line, const char *text );
void menu_set_enter_command( menu_t *menu, int line, int command,
                             const char *argument );
void menu_set_back_command( menu_t *menu, int command, const char *argument );
void menu_set_cursor( menu_t *menu, int cursor );
void menu_set_default_cursor( menu_t *menu, int cursor );
void menu_reset_num_lines( menu_t *menu );

const char *menu_get_name( menu_t *menu );
int menu_get_num_lines( menu_t *menu );
int menu_get_cursor( menu_t *menu );
int menu_get_default_cursor( menu_t *menu );

const char *menu_get_text( menu_t *menu, int line );

int menu_get_enter_command( menu_t *menu, int line );
const char *menu_get_enter_argument( menu_t *menu, int line );
int menu_get_back_command( menu_t *menu );
const char *menu_get_back_argument( menu_t *menu );

#ifdef __cplusplus
};
#endif
#endif /* MENU_H_INCLUDED */
