%define tvtime_ver 0.9.7
%define beta beta
%define tvtime_rpm_ver 1

%if "{%rh_ver}" >= "8.0"
%define desktop_file 1
%else
%define desktop_file 0
%endif

Summary: A high quality TV viewer.
Name: tvtime
Version: %{tvtime_ver}
Release: %{?beta:0.%{beta}.}%{tvtime_rpm_ver}
URL: http://tvtime.sourceforge.net
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Applications/Multimedia
BuildRoot: %{_tmppath}/%{name}-root
BuildRequires: freetype-devel zlib-devel libstdc++-devel libpng-devel XFree86-libs libgcc freetype-devel glibc-debug

%description
tvtime is a high quality television application for use with video capture cards. tvtime processes the input from a capture card and displays it on a computer monitor or projector.

%prep
%setup -q

%build
%configure
make %{_smp_mflags}

%install
# - Copy RedHat 8.0 desktop file to appropriate directory here
# - Copy RedHat 8.0 menu item icon file to appropriate directory here
rm -rf %{buildroot}
%makeinstall

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README docs/DESIGN docs/TODO
%{_bindir}/tvtime
%{_datadir}/tvtime
%{_mandir}/man1/tvtime.1
%{_mandir}/man5/tvtime.5

%changelog
* Sun Feb 23 2003 Paul Jara
- Initial build.
