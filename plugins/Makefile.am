
TVTIME_CFLAGS = -I$(top_srcdir)/src
OPT_CFLAGS = -Wall -I. -pedantic

AM_CFLAGS = $(TVTIME_CFLAGS) $(OPT_CFLAGS)

noinst_LTLIBRARIES = libtvtimeplugins.la
libtvtimeplugins_la_SOURCES = plugins.h greedy2frame.c linear.c \
	weave.c greedy.c double.c linearblend.c scalerbob.c \
	dscaler_greedy2frame.c dscaler_twoframe.c dscaler_greedyh.c \
	dscaler_greedy.c dscaler_videobob.c dscaler_videoweave.c \
	dscaler_oldgame.c dscaler_tomsmocomp.c
libtvtimeplugins_la_LDFLAGS = -static

