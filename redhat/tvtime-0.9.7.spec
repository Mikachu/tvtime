# Some useful constants
%define ver 0.9.7
%define beta beta
%define rpm_ver 3
%define docsdir docs
%define rhdocsdir redhat
%define icon %{docsdir}/tvtime-icon-black.png
%define desktop_filename %{rhdocsdir}/custom-tvtime.desktop

# Check if we're running RedHat 8.0 or higher
%{!?rh_ver:%define rh_ver %(cut -d' ' -f5 /etc/redhat-release )}

Summary: A high quality TV viewer.
Name: tvtime
Version: %{ver}
Release: %{?beta:0.%{beta}.}%{rpm_ver}
URL: http://%{name}.sourceforge.net
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Applications/Multimedia
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: freetype-devel zlib-devel libstdc++-devel libpng-devel XFree86-libs libgcc freetype-devel glibc-debug textutils
Requires: sh-utils man groff desktop-file-utils

%description
%{name} is a high quality television application for use with video capture cards. %{name} processes the input from a capture card and displays it on a computer monitor or projector.

%prep
%setup -q

%build
%configure
%{__make} %{_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%makeinstall

# On RedHat 8.0+ distributions, add a menu entry
%if "%{rh_ver}" >= "8.0"

# Copy icon
install -D -m 644 %{icon} %{buildroot}%{_datadir}/pixmaps/%{name}-logo.png

# Copy desktop file
mkdir -p %{buildroot}%{_datadir}/applications
desktop-file-install --vendor custom --delete-original --dir %{buildroot}%{_datadir}/applications --add-category X-Red-Hat-Extra --add-category Application --add-category AudioVideo %{desktop_filename}
%endif

# Add man pages
mkdir -p %{_mandir}/man1
mkdir -p %{_mandir}/man5
install -D -m 644 %{docsdir}/%{name}.1 %{buildroot}%{_mandir}/man1/%{name}.1i
install -D -m 644 %{docsdir}/%{name}rc.5 %{buildroot}%{_mandir}/man5/%{name}rc.5

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README docs/DESIGN docs/TODO docs/default.tvtimerc
%{_bindir}/%{name}
%{_bindir}/timingtest
%{_datadir}/%{name}
%{_datadir}/pixmaps/%{name}-logo.png
%{_datadir}/applications/*%{name}.desktop
%{_mandir}/man1/%{name}.1*
%{_mandir}/man5/%{name}rc.5*

%changelog
* Mon Feb 24 2003 Paul Jara <rascasse at sourceforge.net>
- Added default.tvtimerc to docs directory
- Sync'd with latest CVS version
- tvscanner replaced with timingtest
* Mon Feb 24 2003 Paul Jara <rascasse at sourceforge.net>
- Added man pages for tvtime and tvtimerc
- Macro-ized some common shell commands
- Added icon and menu entry for RedHat 8.0+
* Sun Feb 23 2003 Paul Jara <rascasse at sourceforge.net>
- Initial build.
