Summary: A high quality TV viewer.
Name: tvtime
Version: 0.9.5
Release: 3
URL: http://tvtime.sourceforge.net
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Applications/Multimedia
BuildRoot: %{_tmppath}/%{name}-root

%description
tvtime is a high quality television application for use with video capture cards. tvtime processes the input from a capture card and displays it on a computer monitor or projector.

%prep
%setup -q

%build
%configure
make %{_smp_mflags}

%install
rm -rf %{buildroot}
%makeinstall

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog NEWS README
%{_bindir}/tvtime
%{_datadir}/tvtime

%changelog
* Sat Nov 9 2002 Paul Jara
- Should work better with RPMs installed in non-standard locations
* Fri Nov 8 2002 Paul Jara
- Removed the post-uninstall script which is no longer necessary for proper RPM removal.
* Tue Nov 4 2002 Paul Jara
- Initial build.
