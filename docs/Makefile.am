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

man_MANS = tvtime.1 tvtime-command.1 tvtime.xml.5 stationlist.xml.5
EXTRA_DIST = default.tvtime.xml example.lircrc quotes.txt $(man_MANS)

.PHONY: install-exec-hook
install-exec-hook:
	-$(INSTALL) -d -m 'u=rwx,go=rx' '$(DESTDIR)$(pkgsysconfdir)'         \
	   || touch noconfdir
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
