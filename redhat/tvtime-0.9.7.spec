%define tvtime_ver 0.9.7
%define beta beta
%define tvtime_rpm_ver 1
%define icon docs/tvtime-icon-black.png
%{!?rh_ver:%define rh_ver %(cut -d' ' -f5 /etc/redhat-release )}

%if "{%rh_ver}" >= "8.0"
#%define desktop_file 1
%define desktop_filename redhat/custom-tvtime.desktop
#%else
#%define desktop_file 0
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
BuildPrereq: sh-utils
Requires: desktop-file-utils fileutils sh-utils

%description
tvtime is a high quality television application for use with video capture cards. tvtime processes the input from a capture card and displays it on a computer monitor or projector.

%prep
%setup -q

%build
%configure
%{__make} %{_smp_mflags}

%install
%{__rm} -rf %{buildroot}
%makeinstall
%if "%{rh_ver}" >= "8.0"

# Copy icon
install -D -m 644 %{icon} %{buildroot}%{_datadir}/pixmaps/%{name}-logo.png

# Copy desktop file
mkdir -p %{buildroot}%_datadir}/applications
desktop-file-install --vendor custom --delete-original --dir %{buildroot}%{_datadir}/applications --add-category X-Red-Hat-Extra --add-category Application --add-category AudioVideo %{desktop_filename}
%else
echo RH8.0 version test failed
%endif

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README docs/DESIGN docs/TODO
%{_bindir}/%{name}
%{_bindir}/tvscanner
%{_datadir}/%{name}
%{_datadir}/pixmaps/%{name}-logo.png
%{_datadir}/applications/*%{name}.desktop
#%{_mandir}/tvtime.1
#%{_mandir}/tvtime.5

%changelog
* Mon Feb 24 2003 Paul Jara
- Macro-ized some common shell commands
- Added icon and menu entry for RedHat 8.0+
* Sun Feb 23 2003 Paul Jara
- Initial build.
