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

if ARCH_X86
SUBDIRS = wine
else
SUBDIRS =
endif

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
tmpdir = /tmp
localedir = $(datadir)/locale

TTF_CFLAGS = `$(FREETYPE_CONFIG) --cflags`
TTF_LIBS = `$(FREETYPE_CONFIG) --libs`

PLUGINS_CFLAGS = -I$(top_srcdir)/plugins
PLUGINS_LIBS = -L$(top_builddir)/plugins

# Set the following if you want to specify an additional font directory
# FONT_CFLAGS = -DFONTDIR='/usr/share/fonts/truetype/freefont/'

if HAVE_DIRECTFB
DIRECTFB_CFLAGS = `directfb-config --cflags`
DIRECTFB_LIBS = `directfb-config --libs`
else
DIRECTFB_CFLAGS = 
DIRECTFB_LIBS = 
endif

if HAVE_LIBMPEG2
MPEG2_CFLAGS = 
MPEG2_LIBS = -lmpeg2
else
MPEG2_CFLAGS = 
MPEG2_LIBS = 
endif

OPT_CFLAGS = -Wall -I. -pedantic -DDATADIR="\"$(pkgdatadir)\"" \
	-DCONFDIR="\"$(pkgsysconfdir)\"" -DFIFODIR="\"$(tmpdir)\"" \
	-D_LARGEFILE64_SOURCE -DLOCALEDIR="\"$(localedir)\""

if ARCH_X86
DSCALER_SRCS = DS_ApiCommon.h DS_Deinterlace.h DS_Filter.h dscalerplugin.h \
	dscalerplugin.c
else
DSCALER_SRCS =
endif

COMMON_SRCS = mixer.c videoinput.c rtctimer.c leetft.c osdtools.c tvtimeconf.c \
	pngoutput.c tvtimeosd.c input.c cpu_accel.c speedy.c pnginput.c \
	deinterlace.c videocorrection.c videotools.c attributes.h \
	deinterlace.h leetft.h input.h mixer.h mm_accel.h mmx.h osdtools.h \
	pnginput.h pngoutput.h rtctimer.h speedy.h taglines.h tvtimeconf.h \
	tvtimeosd.h videocorrection.h videoinput.h videotools.h performance.h \
	performance.c vbidata.h vbidata.c credits.c credits.h console.h \
	console.c speedtools.h vbiscreen.h vbiscreen.c fifo.h fifo.c \
	commands.h commands.c videofilter.h videofilter.c station.h station.c \
	bands.h vsync.h vsync.c utils.h utils.c diffcomp.h diffcomp.c \
	rvrreader.h rvrreader.c ree.h ree.c pulldown.h pulldown.c \
	hashtable.h hashtable.c cpuinfo.h cpuinfo.c videodev.h videodev2.h \
	menu.c menu.h outputfilter.h outputfilter.c dfbmatroxtv.c xmltv.h \
	xmltv.c gettext.h tvtimeglyphs.h mpeg2input.h mpeg2input.c

OUTPUT_SRCS = xfullscreen.h xfullscreen.c outputapi.h xvoutput.h xvoutput.c \
	dfboutput.h dfboutput.c mgaoutput.h mgaoutput.c mga_vid.h \
	xcommon.h xcommon.c sdloutput.h sdloutput.c

bin_PROGRAMS = tvtime tvtime-command tvtime-configure tvtime-scanner
EXTRA_PROGRAMS = expandpng timingtest rvr

tvtime_SOURCES = $(COMMON_SRCS) $(DSCALER_SRCS) $(OUTPUT_SRCS) tvtime.c
tvtime_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS) $(XML2_FLAG) \
	$(FONT_CFLAGS) $(MPEG2_CFLAGS) $(AM_CFLAGS)
tvtime_LDFLAGS  = $(SDL_LIBS) $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) \
	$(PLUGINS_LIBS) $(DIRECTFB_LIBS) $(X11_LIBS) $(XML2_LIBS) \
	$(DYNAMIC_LD_LIBS) $(MPEG2_LIBS) -lm
if ARCH_X86
tvtime_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la \
	-lpthread
else
tvtime_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la -lpthread
endif

tvtime_command_SOURCES = utils.h utils.c tvtimeconf.h tvtimeconf.c \
	tvtime-command.c
tvtime_command_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
tvtime_command_LDFLAGS  = $(ZLIB_LIBS) $(XML2_LIBS)
tvtime_configure_SOURCES = utils.h utils.c tvtimeconf.h tvtimeconf.c \
	tvtime-configure.c
tvtime_configure_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
tvtime_configure_LDFLAGS  = $(ZLIB_LIBS) $(XML2_LIBS)
tvtime_scanner_SOURCES = utils.c videoinput.c tvtimeconf.h tvtimeconf.c \
	station.c tvtime-scanner.c
tvtime_scanner_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
tvtime_scanner_LDFLAGS  = $(ZLIB_LIBS) $(XML2_LIBS)

expandpng_SOURCES = pnginput.h pnginput.c pngoutput.h pngoutput.c expandpng.c
timingtest_SOURCES = $(COMMON_SRCS) timingtest.c
timingtest_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS) $(XML2_FLAG) \
	$(FONT_CFLAGS) $(AM_CFLAGS)
timingtest_LDFLAGS  = $(SDL_LIBS) $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) \
	$(PLUGINS_LIBS) $(DIRECTFB_LIBS) $(X11_LIBS) $(XML2_LIBS) \
	$(DYNAMIC_LD_LIBS) -lm
timingtest_LDADD = -lpthread

rvr_SOURCES = reepktq.h reepktq.c rtctimer.h rtctimer.c utils.h utils.c \
	videoinput.h videoinput.c tvtimeconf.h tvtimeconf.c rvrmain.c \
	diffcomp.h diffcomp.c
rvr_CFLAGS = $(OPT_CFLAGS) $(XML2_FLAG) $(AM_CFLAGS)
rvr_LDADD = $(ZLIB_LIBS) $(XML2_LIBS) -lpthread

CLEANFILES = expandpng timingtest rvr

# Satisfy extra-directory dependencies.  Unfortunately, this causes
# libtool to re-link each and every time.  (Recursive make is evil!)
if ARCH_X86
external_dependencies = ../plugins/libtvtimeplugins.la wine/libwine.la
else
external_dependencies = ../plugins/libtvtimeplugins.la
endif
.PHONY: $(external_dependencies)
$(external_dependencies):
	( $(MAKE) $(AM_MAKEFLAGS) -C `dirname "$@"` `basename "$@"` )

