# data/Makefile.am distributes the shared data files tvtime needs.
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

tvtimedatadir = $(pkgdatadir)
dist_tvtimedata_DATA = FreeMonoBold.ttf tvtimeSansBold.ttf \
	DI_GreedyH.dll DI_TomsMoComp.dll \
	tvtimelogo-small.png tvtimelogo-large.png \
	tvtimelogo_0000.png tvtimelogo_0001.png tvtimelogo_0002.png \
	tvtimelogo_0003.png tvtimelogo_0004.png tvtimelogo_0005.png \
	tvtimelogo_0006.png tvtimelogo_0007.png tvtimelogo_0008.png \
	tvtimelogo_0009.png tvtimelogo_0010.png tvtimelogo_0011.png \
	tvtimelogo_0012.png tvtimelogo_0013.png tvtimelogo_0014.png \
	tvtimelogo_0015.png tvtimelogo_0016.png tvtimelogo_0017.png \
	tvtimelogo_0018.png tvtimelogo_0019.png tvtimelogo_0020.png \
	tvtimelogo_0021.png tvtimelogo_0022.png tvtimelogo_0023.png \
	tvtimelogo_0024.png tvtimelogo_0025.png tvtimelogo_0026.png \
	tvtimelogo_0027.png tvtimelogo_0028.png tvtimelogo_0029.png \
	tvtimelogo_0030.png tvtimelogo_0031.png \
	filmstrip_0000.png filmstrip_0001.png filmstrip_0002.png \
	filmstrip_0003.png filmstrip_0004.png

EXTRA_DIST = COPYING.DScaler COPYING.tvtimeSansBold COPYING.FreeMonoBold \
	tvtimeSansBold.sfd FreeMonoBold.sfd

.PHONY: uninstall-hook
uninstall-hook:
	-rmdir -p '$(DESTDIR)$(tvtimedatadir)'
