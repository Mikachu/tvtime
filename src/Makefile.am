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

SUBDIRS = wine

pkgsysconfdir = $(sysconfdir)/@PACKAGE@

rundir = $(localstatedir)/run
pkgrundir = $(rundir)/@PACKAGE@

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

OPT_CFLAGS = -Wall -I. -pedantic -DDATADIR="\"$(pkgdatadir)\"" -DCONFDIR="\"$(pkgsysconfdir)\"" -DFIFODIR="\"$(pkgrundir)\"" -D_LARGEFILE64_SOURCE

AM_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS) $(XML2_FLAG) \
	$(FONT_CFLAGS)
AM_LDFLAGS = $(SDL_LIBS) $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) \
	$(PLUGINS_LIBS) $(DIRECTFB_LIBS) $(X11_LIBS) $(XML2_LIBS) $(DYNAMIC_LD_LIBS) -lm

COMMON_SRCS = mixer.c videoinput.c rtctimer.c leetft.c osdtools.c tvtimeconf.c \
	pngoutput.c tvtimeosd.c input.c cpu_accel.c speedy.c pnginput.c \
	deinterlace.c videocorrection.c videotools.c attributes.h deinterlace.h \
	leetft.h input.h mixer.h mm_accel.h mmx.h osdtools.h pnginput.h \
	pngoutput.h rtctimer.h speedy.h taglines.h tvtimeconf.h tvtimeosd.h \
	videocorrection.h videoinput.h videotools.h performance.h performance.c \
	vbidata.h vbidata.c credits.c credits.h console.h console.c speedtools.h \
	vbiscreen.h vbiscreen.c fifo.h fifo.c commands.h commands.c videofilter.h \
	videofilter.c station.h station.c bands.h vsync.h vsync.c dscalerplugin.h \
	dscalerplugin.c DS_ApiCommon.h DS_Deinterlace.h DS_Filter.h utils.h \
	utils.c diffcomp.h diffcomp.c rvrreader.h rvrreader.c ree.h ree.c \
	pulldown.h pulldown.c hashtable.h hashtable.c cpuinfo.h cpuinfo.c \
	videodev.h videodev2.h menu.c menu.h outputfilter.h outputfilter.c

OUTPUT_SRCS = display.c display.h outputapi.h xvoutput.h xvoutput.c \
	dfboutput.h dfboutput.c mgaoutput.h mgaoutput.c mga_vid.h \
	xcommon.h xcommon.c xmgaoutput.h xmgaoutput.c sdloutput.h \
	sdloutput.c

bin_PROGRAMS = tvtime tvtime-command tvtime-configure tvtime-scanner
EXTRA_PROGRAMS = expandpng timingtest rvr

tvtime_SOURCES = $(COMMON_SRCS) $(OUTPUT_SRCS) tvtime.c
tvtime_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la -lpthread
# -lmysqlclient

tvtime_command_SOURCES = $(COMMON_SRCS) tvtime-command.c
tvtime_command_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la -lpthread

tvtime_configure_SOURCES = $(COMMON_SRCS) tvtime-configure.c
tvtime_configure_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la -lpthread

tvtime_scanner_SOURCES = $(COMMON_SRCS) tvtime-scanner.c
tvtime_scanner_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la -lpthread

expandpng_SOURCES = pnginput.h pnginput.c pngoutput.h pngoutput.c expandpng.c
expandpng_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la

timingtest_SOURCES = $(COMMON_SRCS) timingtest.c
timingtest_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la -lpthread

rvr_SOURCES = $(COMMON_SRCS) reepktq.h reepktq.c rvrmain.c
rvr_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la wine/libwine.la -lpthread

CLEANFILES = expandpng timingtest rvr

# Satisfy extra-directory dependencies.  Unfortunately, this causes
# libtool to re-link each and every time.  (Recursive make is evil!)
external_dependencies = ../plugins/libtvtimeplugins.la wine/libwine.la
.PHONY: $(external_dependencies)
$(external_dependencies):
	( $(MAKE) $(AM_MAKEFLAGS) -C `dirname "$@"` `basename "$@"` )

# Hook into the installation and uninstallation to create the required
# FIFO directories.
.PHONY: install-exec-hook uninstall-hook
install-exec-hook:
	-$(INSTALL) -d -m 'ug=rwx,o=rxt' '$(DESTDIR)$(pkgrundir)'            \
	   || touch nofifodir
	@if [ -f nofifodir ]; then (                                         \
	  echo 'Warning: I could not create the directory'                   \
	       '`$(DESTDIR)$(pkgrundir)'"'"                                 ;\
	  echo '         You do not have permission to create the'           \
	       'directory.  See the'                                        ;\
          echo '         `Quirks when Compiling tvtime'"' section of the"    \
	       '`INSTALL'"' file."); fi
	-if [ -f nofifodir ]; then                                           \
	   rm -f nofifodir;                                                  \
	 else                                                                \
	   $(INSTALL) -d -m 'ug=rwx,o=rxt' -g '@PKGRUNDIRGROUP@'             \
	     '$(DESTDIR)$(pkgrundir)' || touch nofifodir;                    \
	 fi
	@if [ -f nofifodir ]; then rm -f nofifodir; (                        \
	  echo 'Warning: I could not create the directory'                   \
	       '`$(DESTDIR)$(pkgrundir)'"'"                                 ;\
	  echo '         with group owner `@PKGRUNDIRGROUP@'"'."            ;\
          echo '         You are not a member of the `@PKGRUNDIRGROUP@'"'"   \
	       'group.  See the'                                            ;\
          echo '         `Quirks when Compiling tvtime'"' section of the"    \
	       '`INSTALL'"' file."); fi

uninstall-hook:
	-rm -rf '$(DESTDIR)$(pkgrundir)'
	-rmdir -p '$(DESTDIR)$(rundir)'

