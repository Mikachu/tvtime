
EXTRA_DIST = stubs.s wrapper.S

# WARNING: DO NOT USE AM_CFLAGS HERE UNLESS YOU UNDERSTAND THE PROBLEM WITH
# DEFAULT XINE FLAGS AND KNOW HOW TO FIX IT.
# CFLAGS = $(X_CFLAGS) -fno-omit-frame-pointer

CFLAGS = -fno-omit-frame-pointer \
	-Wmissing-prototypes -Wimplicit-function-declaration \
	-D__WINE__ -Ddbg_printf=__vprintf -DTRACE=__vprintf

wine_lib = libwine.la

noinst_LTLIBRARIES = $(wine_lib)

libwine_la_SOURCES = \
	afl.c \
	driver.c \
	elfdll.c \
	ext.c \
	ldt_keeper.c \
	module.c \
	pe_image.c  \
	pe_resource.c \
	resource.c \
	registry.c \
	vfl.c \
	win32.c 

libwine_la_LIBADD = stubs.lo wrapper.lo

noinst_HEADERS = \
	avifmt.h \
	basetsd.h \
	com.h \
	debugtools.h \
	driver.h \
	elfdll.h \
	ext.h \
	heap.h \
	ldt.h \
	ldt_keeper.h \
	loader.h \
	mmreg.h \
	module.h \
	msacm.h \
	msacmdrv.h \
	ntdef.h \
	pe_image.h \
	poppack.h \
	pshpack1.h \
	pshpack2.h \
	pshpack4.h \
	pshpack8.h \
	registry.h \
	resource.h \
	vfw.h \
	win32.h\
	wineacm.h \
	winbase.h \
	windef.h \
	windows.h \
	winerror.h \
	winestring.h \
	winnt.h \
	winreg.h \
	winuser.h \
	wrapper.h

stubs.lo: stubs.s
	$(CC) -c $(srcdir)/stubs.s -o stubs.lo

wrapper.lo: wrapper.S
	$(CC) -c $(srcdir)/wrapper.S -o wrapper.lo

