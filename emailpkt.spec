%define reldate 20090805
%define reltype C
# may be one of: C (current), R (release), S (stable)

Name: emailpkt
Version: 1.9.%{reldate}%{reltype}
Release: 1
Group: Applications/FTN
Summary: IEM mailer
URL: http://husky.sf.net
License: GPL
Requires: fidoconfig >= 1.9, smapi >= 2.5
Source: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
EmailPkt mailer for the Husky Project software.

%prep
%setup -q -n %{name}

%build
make

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} install

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_prefix}/*

