
TVTIME_CFLAGS = -I$(top_srcdir)/src
OPT_CFLAGS = -Wall -I. -O3 -rdynamic

AM_CFLAGS = $(TVTIME_CFLAGS) $(OPT_CFLAGS)

libtvtimeplugins_a_SOURCES = plugins.h greedy2frame.c twoframe.c linear.c \
	weave.c videobob.c greedy.c double.c
noinst_LIBRARIES = libtvtimeplugins.a

