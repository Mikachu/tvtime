
TVTIME_CFLAGS = -I$(top_srcdir)/src
OPT_CFLAGS = -Wall -I. -pedantic

AM_CFLAGS = $(TVTIME_CFLAGS) $(OPT_CFLAGS)

noinst_LTLIBRARIES = libtvtimeplugins.la
libtvtimeplugins_la_SOURCES = plugins.h linear.c weave.c double.c \
	linearblend.c scalerbob.c dscaler_greedy2frame.c \
	dscaler_twoframe.c dscaler_greedyh.c dscaler_greedy.c \
	dscaler_videobob.c dscaler_videoweave.c dscaler_tomsmocomp.c \
	gamedither.c
libtvtimeplugins_la_LDFLAGS = -static

