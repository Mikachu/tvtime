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

if ARCH_386
DSCALER_SRCS = dscalerapi.h greedyhmacros.h kdetv_greedyh.c \
	kdetv_tomsmocomp.c
EXTRA_DIST = greedyh.asm \
	tomsmocomp/SearchLoop0A.inc tomsmocomp/SearchLoopBottom.inc \
	tomsmocomp/SearchLoopEdgeA.inc tomsmocomp/SearchLoopEdgeA8.inc \
	tomsmocomp/SearchLoopOddA.inc tomsmocomp/SearchLoopOddA2.inc \
	tomsmocomp/SearchLoopOddA6.inc tomsmocomp/SearchLoopOddAH.inc \
	tomsmocomp/SearchLoopOddAH2.inc tomsmocomp/SearchLoopTop.inc \
	tomsmocomp/SearchLoopVA.inc tomsmocomp/SearchLoopVAH.inc \
	tomsmocomp/StrangeBob.inc tomsmocomp/TomsMoCompAll.inc \
	tomsmocomp/TomsMoCompAll2.inc tomsmocomp/WierdBob.inc \
	tomsmocomp/tomsmocompfilter_3dnow.cpp \
	tomsmocomp/tomsmocompfilter_mmx.cpp \
	tomsmocomp/tomsmocompfilter_sse.cpp \
	tomsmocomp/tomsmocompmacros.h
else
DSCALER_SRCS = 
endif

noinst_LTLIBRARIES = libtvtimeplugins.la
libtvtimeplugins_la_SOURCES = plugins.h linear.c weavetff.c \
	weavebff.c double.c linearblend.c scalerbob.c greedy.c \
	vfir.c weave.c $(DSCALER_SRCS)
libtvtimeplugins_la_LDFLAGS = -static

