
tvtimedatadir = $(datadir)/tvtime
dist_tvtimedata_DATA = menubg.png testlogo.png FreeSansBold.ttf

TTF_CFLAGS = `freetype-config --cflags`
TTF_LIBS = `freetype-config --libs`

PLUGINS_CFLAGS = -I$(top_srcdir)/plugins
PLUGINS_LIBS = -L$(top_builddir)/plugins -ltvtimeplugins

DIRECTFB_CFLAGS = `directfb-config --cflags`
DIRECTFB_LIBS = `directfb-config --libs`

OPT_CFLAGS = -Wall -I. -O3 -rdynamic -DDATADIR="\"$(tvtimedatadir)\""

AM_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) $(PLUGINS_CFLAGS)
AM_CXXFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) $(PLUGINS_CFLAGS)
AM_LDFLAGS = $(SDL_LIBS) $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) $(PLUGINS_LIBS) -lm

COMMON_SRCS = frequencies.c mixer.c videoinput.c sdloutput.c \
	rtctimer.c ttfont.cpp efs.cpp osd.c parser.c tvtimeconf.c \
	pngoutput.c tvtimeosd.c input.c cpu_accel.c speedy.c \
	pnginput.c menu.c deinterlace.c videocorrection.c \
	videotools.c attributes.h deinterlace.h efs.h frequencies.h \
	input.h menu.h mixer.h mm_accel.h mmx.h osd.h parser.h pnginput.h \
	pngoutput.h rtctimer.h sdloutput.h speedy.h taglines.h ttfont.h \
	tvtimeconf.h tvtimeosd.h videocorrection.h videoinput.h videotools.h \
	outputapi.h

bin_PROGRAMS = tvtime

tvtime_SOURCES = $(COMMON_SRCS) tvtime.c

