
TVTIME_CFLAGS = -I$(top_srcdir)/src
OPT_CFLAGS = -Wall -I. -pedantic

AM_CFLAGS = $(TVTIME_CFLAGS) $(OPT_CFLAGS)

noinst_LTLIBRARIES = libtvtimeplugins.la
libtvtimeplugins_la_SOURCES = plugins.h greedy2frame.c twoframe.c linear.c \
	weave.c videobob.c greedy.c double.c linearblend.c scalerbob.c \
	simplemo.c
libtvtimeplugins_la_LDFLAGS = -static
