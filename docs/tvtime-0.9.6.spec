Summary: A high quality TV viewer.
Name: tvtime
Version: 0.9.6
Release: 1
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
%doc AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README ./docs/DESIGN ./docs/TODO
%{_bindir}/tvtime
%{_datadir}/tvtime

%changelog
* Thu Nov 14 2002 Paul Jara
- Initial build.
