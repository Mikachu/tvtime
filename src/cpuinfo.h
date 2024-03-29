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

#ifndef CPUINFO_H_INCLUDED
#define CPUINFO_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Prints out information about the CPU: empirically measured speed
 * in MHz and chip information from the cpuid.
 */
void cpuinfo_print_info( void );

/**
 * Returns the empirically measured CPU speed in MHz.
 */
double cpuinfo_get_speed( void );

#ifdef __cplusplus
};
#endif

#endif /* CPUINFO_H_INCLUDED */
