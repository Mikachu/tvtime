# src/wine/Makefile.am is used to build the WINE glue code.
# Copyright (C) 2002, 2003  Billy Biggs
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

EXTRA_DIST = stubs.s wrapper.S

# WARNING: DO NOT USE AM_CFLAGS HERE UNLESS YOU UNDERSTAND THE PROBLEM WITH
# DEFAULT XINE FLAGS AND KNOW HOW TO FIX IT.
# Simon: I think I understand, and we don't use xine flags.
# AM_CFLAGS = $(X_CFLAGS) -fno-omit-frame-pointer

AM_CFLAGS = -Wall \
	    -fno-omit-frame-pointer \
	    -D__WINE__ \
	    -Ddbg_printf=__vprintf \
	    -DTRACE=__vprintf \
	    -DWIN32_PATH="\"$(pkgdatadir)\""

wine_lib = libwine.la

noinst_LTLIBRARIES = $(wine_lib)

libwine_la_SOURCES = afl.c driver.c elfdll.c ext.c ldt_keeper.c module.c \
	pe_image.c pe_resource.c resource.c registry.c vfl.c win32.c 

libwine_la_LIBADD = stubs.lo wrapper.lo

noinst_HEADERS = avifmt.h basetsd.h com.h debugtools.h driver.h elfdll.h \
	ext.h heap.h ldt.h ldt_keeper.h loader.h mmreg.h module.h msacm.h \
	msacmdrv.h ntdef.h pe_image.h poppack.h pshpack1.h pshpack2.h \
	pshpack4.h pshpack8.h registry.h resource.h vfw.h win32.h wineacm.h \
	winbase.h windef.h windows.h winerror.h winestring.h winnt.h winreg.h \
	winuser.h wrapper.h

stubs.lo: stubs.s
	$(LTCOMPILE) -c -o $@ $^

wrapper.lo: wrapper.S
	$(LTCOMPILE) -c -o $@ $^

