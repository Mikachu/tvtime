
tvtimedatadir = $(datadir)/tvtime
dist_tvtimedata_DATA = menubg.png testlogo.png FreeSansBold.ttf credits.png

TTF_CFLAGS = `$(FREETYPE_CONFIG) --cflags`
TTF_LIBS = `$(FREETYPE_CONFIG) --libs`

PLUGINS_CFLAGS = -I$(top_srcdir)/plugins
PLUGINS_LIBS = -L$(top_builddir)/plugins -ltvtimeplugins

# directfb output currently disabled
# if HAVE_DIRECTFB
# DIRECTFB_CFLAGS = `directfb-config --cflags`
# DIRECTFB_LIBS = `directfb-config --libs`
# else
# DIRECTFB_CFLAGS = 
# DIRECTFB_LIBS = 
# endif

OPT_CFLAGS = -Wall -I. -O3 -rdynamic -pedantic -DDATADIR="\"$(tvtimedatadir)\""

AM_CFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS)
AM_CXXFLAGS = $(SDL_CFLAGS) $(TTF_CFLAGS) $(PNG_CFLAGS) $(OPT_CFLAGS) \
	$(PLUGINS_CFLAGS) $(DIRECTFB_CFLAGS) $(X11_CFLAGS)
AM_LDFLAGS = $(SDL_LIBS) $(TTF_LIBS) $(ZLIB_LIBS) $(PNG_LIBS) \
	$(PLUGINS_LIBS) $(DIRECTFB_LIBS) $(X11_LIBS) $(DYNAMIC_LD_LIBS) -lm

COMMON_SRCS = mixer.c videoinput.c rtctimer.c ttfont.cpp efs.cpp osdtools.c \
	parser.c tvtimeconf.c pngoutput.c tvtimeosd.c input.c cpu_accel.c \
	speedy.c pnginput.c menu.c deinterlace.c videocorrection.c \
	videotools.c attributes.h deinterlace.h efs.h frequencies.h \
	input.h menu.h mixer.h mm_accel.h mmx.h osdtools.h parser.h pnginput.h \
	pngoutput.h rtctimer.h speedy.h taglines.h ttfont.h \
	tvtimeconf.h tvtimeosd.h videocorrection.h videoinput.h videotools.h \
	outputapi.h performance.h performance.c \
	credits.c credits.h xvoutput.h xvoutput.c display.h display.c \
	wm_state.h wm_state.c console.h console.c

bin_PROGRAMS = tvtime

tvtime_SOURCES = $(COMMON_SRCS) tvtime.c

expandpng_SOURCES = pnginput.h pnginput.c pngoutput.h pngoutput.c expandpng.c

