tvtimedatadir = $(datadir)/tvtime
tvtimesysconfdir = $(sysconfdir)/tvtime
pkgsysconfdir = $(sysconfdir)/tvtime

TTF_CFLAGS = `$(FREETYPE_CONFIG) --cflags`
TTF_LIBS = `$(FREETYPE_CONFIG) --libs`

PLUGINS_CFLAGS = -I$(top_srcdir)/plugins
PLUGINS_LIBS = -L$(top_builddir)/plugins

# directfb output currently disabled
# if HAVE_DIRECTFB
# DIRECTFB_CFLAGS = `directfb-config --cflags`
# DIRECTFB_LIBS = `directfb-config --libs`
# else
# DIRECTFB_CFLAGS = 
# DIRECTFB_LIBS = 
# endif

OPT_CFLAGS = -Wall -I. -pedantic -DDATADIR="\"$(tvtimedatadir)\"" -DCONFDIR="\"$(tvtimesysconfdir)\""

AM_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS) $(XML2_FLAG)
AM_CXXFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS) $(XML2_FLAG)
AM_LDFLAGS = $(SDL_LIBS) $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) \
	$(PLUGINS_LIBS) $(DIRECTFB_LIBS) $(X11_LIBS) $(XML2_LIBS) $(DYNAMIC_LD_LIBS) -lm

COMMON_SRCS = mixer.c videoinput.c rtctimer.c ttfont.cpp efs.cpp osdtools.c \
	parser.c tvtimeconf.c pngoutput.c tvtimeosd.c input.c cpu_accel.c \
	speedy.c pnginput.c menu.c deinterlace.c videocorrection.c \
	videotools.c attributes.h deinterlace.h efs.h input.h menu.h mixer.h \
	mm_accel.h mmx.h osdtools.h parser.h pnginput.h pngoutput.h rtctimer.h \
	speedy.h taglines.h ttfont.h tvtimeconf.h tvtimeosd.h videocorrection.h \
	videoinput.h videotools.h outputapi.h performance.h performance.c \
	vbidata.h vbidata.c credits.c credits.h xvoutput.h xvoutput.c display.h \
	display.c wm_state.h wm_state.c console.h console.c speedtools.h \
	vbiscreen.h vbiscreen.c fifo.h fifo.c commands.h commands.c \
	station.h station.c bands.h configsave.h configsave.c x11tools.c \
	x11tools.h debug_print.h tvtimeconf_xml.c vgasync.h vgasync.c

bin_PROGRAMS = tvtime 
EXTRA_PROGRAMS = expandpng timingtest

tvtime_SOURCES = $(COMMON_SRCS) tvtime.c
tvtime_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la

expandpng_SOURCES = pnginput.h pnginput.c pngoutput.h pngoutput.c expandpng.c
expandpng_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la

timingtest_SOURCES = $(COMMON_SRCS) timingtest.c
timingtest_LDADD = $(top_builddir)/plugins/libtvtimeplugins.la

CLEANFILES = expandpng timingtest

.PHONY: ../plugins/libtvtimeplugins.la
../plugins/libtvtimeplugins.la:
	( $(MAKE) $(AM_MAKEFLAGS) -C `dirname "$@"` `basename "$@"` )
