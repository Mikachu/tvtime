# docs/Makefile.am distributes the tvtime documentation.
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

pkgsysconfdir = $(sysconfdir)/@PACKAGE@

man_MANS = tvtime.1 tvtime-command.1 tvtime.xml.5 stationlist.xml.5 \
	tvtime-configure.1 tvtime-scanner.1
docs_html = html/cctext.png html/hardware.html html/help.html \
	html/index.html html/little-screenshot.jpg html/nyse.png \
	html/screenshot-small.jpg html/setws.png html/sun-greedy.png \
	html/sun-linear.png html/toshiba-16x9-screenshot.jpg \
	html/tvtime-logo-banner.png html/tvtime.css \
	html/usage.html html/why.html html/wsshot-small.jpg
EXTRA_DIST = default.tvtime.xml example.lircrc $(man_MANS) $(docs_html) \
	tvtime.16x16.png tvtime.32x32.png tvtime.48x48.png tvtime.32x32.xpm

# We use $(datadir)/applications from the freedesktop spec
# http://www.freedesktop.org/standards/desktop-entry-spec/
desktopdatadir = $(datadir)/applications
dist_desktopdata_DATA = net-tvtime.desktop

# We use $(datadir)/icons/hicolor as our theme from the freedesktop spec
# http://www.freedesktop.org/standards/menu-spec/
install-data-local:
	$(INSTALL) -d '$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps'
	$(INSTALL) -d '$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps'
	$(INSTALL) -d '$(DESTDIR)$(datadir)/icons/hicolor/48x48/apps'
	$(INSTALL) -d '$(DESTDIR)$(datadir)/pixmaps'
	$(INSTALL_DATA) '$(srcdir)/tvtime.16x16.png' '$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/tvtime.png'
	$(INSTALL_DATA) '$(srcdir)/tvtime.32x32.png' '$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/tvtime.png'
	$(INSTALL_DATA) '$(srcdir)/tvtime.32x32.xpm' '$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/tvtime.xpm'
	$(INSTALL_DATA) '$(srcdir)/tvtime.48x48.png' '$(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/tvtime.png'
	$(INSTALL_DATA) '$(srcdir)/tvtime.48x48.png' '$(DESTDIR)$(datadir)/pixmaps/tvtime.png'

.PHONY: install-exec-hook
install-exec-hook:
	-mkdir -p -- '$(DESTDIR)$(pkgsysconfdir)' || touch noconfdir
	@if [ -f noconfdir ]; then (                                         \
	  echo 'Warning: I could not create the directory'                   \
	       '`$(DESTDIR)$(pkgsysconfdir)'"'"                             ;\
	  echo '         because you do not have the required permissions.' ;\
	); fi
	-if [ -f noconfdir ]; then                                           \
	   rm -f noconfdir;                                                  \
	 else                                                                \
	   $(INSTALL) -m 'u=rw,go=r' 'default.tvtime.xml'                    \
	     '$(DESTDIR)$(pkgsysconfdir)/tvtime.xml' || touch noconfdir;     \
	 fi
	@if [ -f noconfdir ]; then rm -f noconfdir; (                        \
	  echo 'Warning: I could not create the default tvtime'              \
	       'configuration file'                                         ;\
	  echo '         because you do not have the required permissions.' ;\
	); fi

uninstall-hook:
	-rmdir -p '$(DESTDIR)$(pkgsysconfdir)'
	-rm -f '$(DESTDIR)$(datadir)/icons/hicolor/16x16/apps/tvtime.png'
	-rm -f '$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/tvtime.png'
	-rm -f '$(DESTDIR)$(datadir)/icons/hicolor/32x32/apps/tvtime.xpm'
	-rm -f '$(DESTDIR)$(datadir)/icons/hicolor/48x48/apps/tvtime.png'
	-rm -f '$(DESTDIR)$(datadir)/pixmaps/tvtime.png'
