EXTRA_DIST = stubs.s

noinst_LTLIBRARIES = $(wine_lib)

##
##
AM_CFLAGS = $(X_CFLAGS) -fno-omit-frame-pointer \
	-Wmissing-prototypes -Wimplicit-function-declaration \
	-DWIN32_PATH=\"@w32_path@\" -I$(srcdir)/.. -D__WINE__ \
	-Ddbg_printf=__vprintf -DTRACE=__vprintf -fno-omit-frame-pointer

#DEBUG_CFLAGS = @DEBUG_CFLAGS@ $(X_CFLAGS) -fno-omit-frame-pointer \
#	-Wmissing-prototypes -Wimplicit-function-declaration \
#	-DWIN32_PATH=\\\"@w32_path@\\\" -I$(srcdir)/.. -D__WINE__ \
#	-Ddbg_printf=__vprintf -DTRACE=__vprintf -fno-omit-frame-pointer


if HAVE_W32DLL
wine_lib = libwine.la
endif

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

libwine_la_LIBADD = stubs.lo

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
	winuser.h

stubs.lo: stubs.s
	$(CC) -c $(srcdir)/stubs.s -o stubs.lo

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in

