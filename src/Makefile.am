
tvtimedatadir = $(datadir)/tvtime

TTF_CFLAGS = `freetype-config --cflags`
TTF_LIBS = `freetype-config --libs`

PNG_CFLAGS =
PNG_LIBS = -lpng

PLUGINS_CFLAGS = -I../plugins
PLUGINS_LIBS = -L../plugins -ltvtimeplugins

OPT_CFLAGS = -Wall -I. -O3 -rdynamic -DDATADIR="$(tvtimedatadir)"

AM_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) $(PLUGINS_CFLAGS)
AM_CXXFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) $(PLUGINS_CFLAGS)
LDFLAGS = $(SDL_LIBS) $(TTF_LIBS) $(PNG_LIBS) $(PLUGINS_LIBS) -lm

COMMON_SRCS = frequencies.c mixer.c videoinput.c sdloutput.c \
	rtctimer.c ttfont.cpp efs.cpp osd.c parser.c tvtimeconf.c \
	pngoutput.c tvtimeosd.c input.c cpu_accel.c speedy.c \
	pnginput.c menu.c deinterlace.c videocorrection.c \
	videotools.c

bin_PROGRAMS = tvtime

tvtime_SOURCES = $(COMMON_SRCS) tvtime.c

tvtimedata_DATA = menubg.png testlogo.png FreeSansBold.ttf
