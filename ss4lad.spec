%define _topdir         /home/ukv/rpmbuild
%define name            ss4lad 
%define release         1
%define version         1.0
%define buildroot %{_topdir}/%{name}-%{version}-root

Name: %{name}
Version: %{version}
Release: %{release}
Summary: SMTP Server For Ladoga
Group: Applications/Productivity
License: GPL
Source0: %{name}-%{version}.tar.gz
BuildArch: i386
BuildRoot: %{buildroot}

%description
This package basically does nothing, but it potentially could
do something useful.

%prep
%setup -q -n %{name}-%{version}

%build
make

%install
mkdir -p $RPM_BUILD_ROOT/usr/local/bin
install %{name} $RPM_BUILD_ROOT/usr/local/bin

%files
%defattr(-,root,root)
/usr/local/bin/%{name}

%clean
make clean
rm -rf $RPM_BUILD_ROOT

%changelog
* Tue Mar 6 2012 ukv626
- Initial build
