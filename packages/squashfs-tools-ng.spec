## Spec file to build squashf-tools-ng RPM package.
##
## Install following packages:
##   - yum install -y rpm-build spectool
##   - spectool -g -R squashfs-tools-ng.spec
##   - rpmspec --parse squashfs-tools-ng.spec | grep BuildRequires | cut -d' ' -f2  | xargs sudo yum install -y
##
## Note: tools like yum-builddep does not seem to work when installing
## build requirements.
##
## Run:
##   - rpmbuild --clean -ba squashfs-tools-ng.spec
##

Name: squashfs-tools-ng
Version: 1.0.1
Release: 1%{?dist}
License: GPLv3+
URL: https://github.com/AgentD/squashfs-tools-ng

Source0: https://github.com/AgentD/squashfs-tools-ng/archive/v%{version}/%{name}-%{version}.tar.gz

Summary: New set of tools for working with SquashFS images
%description
SquashFS is a highly compressed read-only filesystem for Linux,
optimized for small size and high packing density. It is widely used
in embedded systems and bootable live media.

SquashFS supports many different compression formats, such as zstd,
xz, zlib or lzo for both data and metadata compression. It has many
features expected from popular filesystems, such as extended
attributes and support for NFS export.

As the name suggests, this is not the original user space tooling for
SquashFS. Here are some of the features that primarily distinguish
this package from the original:

 - reproducible SquashFS images, i.e. deterministic packing without
   any local time stamps,
 - Linux `gen_init_cpio` like file listing for micro managing the
   file system contents, permissions, and ownership without having to
   replicate the file system (and especially permissions) locally,
 - support for SELinux contexts file (see selabel_file(5)) to generate
   SELinux labels.


%if 0%{?el} > 7
%global use_zstd 1
%endif

# rpm-build / rpmdevtools
BuildRequires: gcc
BuildRequires: automake
BuildRequires: autoconf
BuildRequires: libtool
BuildRequires: doxygen
BuildRequires: zlib-devel
BuildRequires: xz-devel
BuildRequires: lzo-devel
BuildRequires: libattr-devel
BuildRequires: lz4-devel

%if 0%{?use_zstd}
BuildRequires: libzstd-devel
Requires: libzstd
%endif

Requires: %{name}-lib%{?_isa} = %{version}-%{release}
Requires: zlib
Requires: xz
Requires: lzo
Requires: libattr
Requires: lz4

#Recommends: squashfs-tools
   
%package        lib
Summary:        New set of tools for working with SquashFS images - shared library
Group:          System Environment/Libraries
%description     lib
This package contains the C libraries needed to run executables that
use the squashfs-tools-ng library.
 
%package        devel
Summary:        New set of tools for working with SquashFS images - development
Group:          Development/Libraries
Requires:       %{name}-lib = %{version}-%{release}
%description    devel
This package contains the C development headers and library files
needed to compile programs using the squashfs-tools-ng library.
   
%package        devel-doc
Summary:        New set of tools for working with SquashFS images - documentation
Group:          Development/Libraries
%description    devel-doc
This package contains the C development documentation files for the
the squashfs-tools-ng library.

%prep
%setup -q
%autosetup
%build
./autogen.sh
%configure
make %{?_smp_mflags}
make doxygen-doc
%install
%make_install


%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_bindir}/*
%{_mandir}/man1/*
%doc COPYING.md CHANGELOG.md README.md


%files lib
%defattr(-,root,root,-)
%{_libdir}/*.so*

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
%{_libdir}/*.a
%{_libdir}/*.la
%{_libdir}/pkgconfig/*.pc

%files devel-doc
%defattr(-,root,root,-)
%doc doxygen-doc/html
%doc doxygen-doc/*.tag


%changelog
* Thu Aug 20 2020 Sébastien Gross <seb•ɑƬ•chezwam•ɖɵʈ•org> - 1.0.1-1
- First package release.

