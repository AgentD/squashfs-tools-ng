## Spec file to build squashf-tools-ng RPM package.

# OpenSUSE has no dist macro
%if 0%{?suse_version} > 0
%global dist .sles%{suse_version}
%endif

Name: squashfs-tools-ng
Version: 1.1.2
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


%if 0%{?centos} > 7 || 0%{?fedora} >= 32 || 0%{?suse_version} >= 1500
%global use_zstd 1
%endif



# rpm-build / rpmdevtools
BuildRequires: gcc
BuildRequires: automake
BuildRequires: autoconf
BuildRequires: libtool
BuildRequires: doxygen
BuildRequires: libselinux-devel
BuildRequires: zlib-devel
BuildRequires: xz-devel
BuildRequires: lzo-devel
BuildRequires: libattr-devel

# Need to be explicitly declared on Fedora
BuildRequires: make

# OpenSUSE has a different lz4 and bzip2 devel package names
%if 0%{?suse_version} > 0
BuildRequires: liblz4-devel
BuildRequires: libbz2-devel
%else
BuildRequires: lz4-devel
BuildRequires: bzip2-devel
%endif

%if 0%{?use_zstd}
BuildRequires: libzstd-devel
Requires: libzstd
%endif

Requires: %{name}-lib%{?_isa} = %{version}-%{release}
Requires: zlib
Requires: xz
Requires: lzo
Requires: libattr
Requires: libselinux

# OpenSUSE has a different lz4 and bzip2 package names
%if 0%{?suse_version} > 0
Requires: liblz4
Requires: libbz2
%else
BuildRequires: lz4
BuildRequires: bzip2
%endif

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
* Sat Jun 26 2021 David Oberhollenzer <goliath@infraroot.at> - 1.1.2-1
- Bump to version 1.1.2.
* Sat Jan 23 2021 David Oberhollenzer <goliath@infraroot.at> - 1.0.4-1
- Bump to version 1.0.4.
* Thu Nov 05 2020 David Oberhollenzer <goliath@infraroot.at> - 1.0.3-1
- Bump to version 1.0.3.
* Thu Oct 01 2020 Sébastien Gross <seb•ɑƬ•chezwam•ɖɵʈ•org> - 1.0.2-1
- Add Fedora support.
- Add OpenSUSE support.
- Bump to version 1.0.2.
* Thu Aug 20 2020 Sébastien Gross <seb•ɑƬ•chezwam•ɖɵʈ•org> - 1.0.1-1
- First package release.
