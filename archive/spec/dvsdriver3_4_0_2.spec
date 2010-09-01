%define redhat		%(test ! -f /etc/redhat-release ; echo $?)
%define redhat		%(test ! -f /etc/fedora-release ; echo $?)
%define mandrake	%(test ! -f /etc/mandrake-release ; echo $?)
%define suse		%(test ! -f /etc/SuSE-release ; echo $?)

Summary: DVS driver and utilities for DVS SDI I/O cards
Name: dvsdriver
Version: 3.4.0.2
Release: 1
License: GPL
Group: System Environment/Daemons
Source: sdk%{PACKAGE_VERSION}.tar.gz
#Source: sdk3.4.0.2.tar.gz
#Source1: dvsdriver_extras.tar.gz
#Source2: sdk%{PACKAGE_VERSION}_x86_64.tar.gz
Url: http://www.dvs.de/customersupport/dec/videoboards/sdk3.4.0.2.tar.gz
BuildRoot: %{_tmppath}/%{name}-root
%if %{suse}
BuildRequires: kernel-source
%else
BuildRequires: kernel-devel
%endif
PreReq: /sbin/chkconfig /sbin/service
Provides: svram

%description
DVS driver, developer includes, library and example programs.

%prep
rm -rf $RPM_BUILD_ROOT
# setup unpacks the tarball and cd's into it
%setup -q -n sdk%{PACKAGE_VERSION}

%build
cd linux-x86_64/driver
./driver_create

%install
# get and check $INGEX_DIR environment variable
%define INGEX_DIR %(echo $INGEX_DIR)
if [ "$INGEX_DIR" = "" ]
then
	echo "Error: INGEX_DIR environment variable not set"
	exit 1
fi
export INGEX_ARCHIVE_DIR=$INGEX_DIR/archive


mkdir -p $RPM_BUILD_ROOT/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/lib
cp linux-x86_64/lib/* $RPM_BUILD_ROOT/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/lib

mkdir -p $RPM_BUILD_ROOT/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/bin
cp linux-x86_64/bin/* $RPM_BUILD_ROOT/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/bin

mkdir -p $RPM_BUILD_ROOT/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/driver
cp linux-x86_64/driver/* $RPM_BUILD_ROOT/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/driver

mkdir -p $RPM_BUILD_ROOT/etc/init.d
cp $INGEX_ARCHIVE_DIR/spec/dvs_setup $RPM_BUILD_ROOT/etc/init.d

%clean
rm -rf $RPM_BUILD_ROOT

%post

chmod 744 /etc/init.d/dvs_setup

# remove old version if any
/sbin/chkconfig --del dvs_setup 2>/dev/null || true
# add boot-time script
/sbin/chkconfig --add dvs_setup

#  Start the startup script
/sbin/service dvs_setup restart


%preun
if [ "$1" = "0" ]; then
	/sbin/service dvs_setup stop > /dev/null 2>&1
	/sbin/chkconfig --del dvs_setup
fi
exit 0

%files
%defattr(-,root,root)
/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/lib
/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/bin
/home/ingex/dvs/sdk%{PACKAGE_VERSION}/linux-x86_64/driver
/etc/init.d/dvs_setup

%changelog
* Fri Jan 22 2010 Tim Jobling (Cambridge Imaging Systems)
- First version, based in-part on dvsdriver27p57.spec