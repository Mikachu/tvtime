# src/Makefile.am builds the tvtime binaries
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
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330 - Boston, MA 02111-1307, USA

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
tmpdir = /tmp
localedir = $(datadir)/locale

# Set the following if you want to specify an additional font directory
# FONT_CFLAGS = -DFONTDIR='/usr/share/fonts/truetype/freefont/'

OPT_CFLAGS = -Wall -pedantic -I. -DDATADIR="\"$(pkgdatadir)\"" \
	-DCONFDIR="\"$(pkgsysconfdir)\"" -DFIFODIR="\"$(tmpdir)\"" \
	-D_LARGEFILE64_SOURCE -DLOCALEDIR="\"$(localedir)\""

COMMON_SRCS = mixer.c videoinput.c rtctimer.c leetft.c osdtools.c tvtimeconf.c \
	pngoutput.c tvtimeosd.c input.c cpu_accel.c speedy.c pnginput.c \
	deinterlace.c videotools.c attributes.h deinterlace.h leetft.h \
	input.h mixer.h mm_accel.h mmx.h osdtools.h pnginput.h pngoutput.h \
	rtctimer.h speedy.h taglines.h tvtimeconf.h tvtimeosd.h videoinput.h \
	videotools.h performance.h performance.c vbidata.h vbidata.c \
	speedtools.h vbiscreen.h vbiscreen.c fifo.h fifo.c commands.h \
	commands.c videofilter.h videofilter.c station.h station.c bands.h \
	utils.h utils.c pulldown.h pulldown.c hashtable.h hashtable.c \
	cpuinfo.h cpuinfo.c menu.c menu.h \
	outputfilter.h outputfilter.c xmltv.h xmltv.c gettext.h tvtimeglyphs.h \
	copyfunctions.h copyfunctions.c epg.h epg.c

if ARCH_X86
DSCALER_SRCS = $(top_srcdir)/plugins/dscalerapi.h \
	$(top_srcdir)/plugins/greedyhmacros.h \
	$(top_srcdir)/plugins/kdetv_greedyh.c \
	$(top_srcdir)/plugins/kdetv_tomsmocomp.c \
	$(top_srcdir)/plugins/greedyh.h \
	$(top_srcdir)/plugins/greedyh.cpp \
	$(top_srcdir)/plugins/tomsmocomp.h \
	$(top_srcdir)/plugins/tomsmocomp.cpp
else
DSCALER_SRCS = 
endif

PLUGIN_CFLAGS = -I$(top_srcdir)/plugins
PLUGIN_SRCS =  $(top_srcdir)/plugins/plugins.h \
	$(top_srcdir)/plugins/linear.c \
	$(top_srcdir)/plugins/scalerbob.c \
	$(top_srcdir)/plugins/linearblend.c \
	$(top_srcdir)/plugins/vfir.c \
	$(top_srcdir)/plugins/greedy.c \
	$(top_srcdir)/plugins/weave.c \
	$(top_srcdir)/plugins/weavetff.c \
	$(top_srcdir)/plugins/weavebff.c \
	$(DSCALER_SRCS)

OUTPUT_SRCS = xfullscreen.h xfullscreen.c outputapi.h xvoutput.h xvoutput.c \
	xcommon.h xcommon.c

bin_PROGRAMS = tvtime tvtime-command tvtime-configure tvtime-scanner

tvtime_SOURCES = $(COMMON_SRCS) $(OUTPUT_SRCS) $(PLUGIN_SRCS) tvtime.c
tvtime_CFLAGS = $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGIN_CFLAGS) $(X11_CFLAGS) $(XML2_FLAG) \
	$(FONT_CFLAGS) $(AM_CFLAGS)
tvtime_LDADD  = $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) \
	$(X11_LIBS) $(XML2_LIBS) -lm -lsupc++

tvtime_command_SOURCES = utils.h utils.c tvtimeconf.h tvtimeconf.c \
	tvtime-command.c
tvtime_command_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
tvtime_command_LDADD  = $(ZLIB_LIBS) $(XML2_LIBS)
tvtime_configure_SOURCES = utils.h utils.c tvtimeconf.h tvtimeconf.c \
	tvtime-configure.c
tvtime_configure_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
tvtime_configure_LDADD  = $(ZLIB_LIBS) $(XML2_LIBS)
tvtime_scanner_SOURCES = utils.h utils.c videoinput.h videoinput.c \
	tvtimeconf.h tvtimeconf.c station.h station.c tvtime-scanner.c \
	mixer.h mixer.c
tvtime_scanner_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
tvtime_scanner_LDADD  = $(ZLIB_LIBS) $(XML2_LIBS)

