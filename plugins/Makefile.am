# plugins/Makefile.am distributes the plugins that come with tvtime.
# Copyright (C) 2002, 2003  Billy Biggs
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

TVTIME_CFLAGS = -I$(top_srcdir)/src
OPT_CFLAGS = -Wall -I. -pedantic

AM_CFLAGS = $(TVTIME_CFLAGS) $(OPT_CFLAGS)

noinst_LTLIBRARIES = libtvtimeplugins.la
libtvtimeplugins_la_SOURCES = plugins.h linear.c weavetff.c \
	weavebff.c double.c linearblend.c scalerbob.c \
	dscaler_greedyh.c dscaler_tomsmocomp.c vfir.c
libtvtimeplugins_la_LDFLAGS = -static

