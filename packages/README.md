# Package builder

This directory contains files to build packages for several Linux
distributions.

## Build using Docker images

Packages for a specific release can be built using Docker. Use the
`build` script for that:

```
./build VENDOR RELEASE
```

If you want to build all supported vendors and releases, you can use
the `build_all` script.

Packages will be output in `_out` directory.

## Manual build

### APK

[APKBUILD]() containts all definition to build APK packages for Alpine
linux.

From a fresh install setup the build environment for a reqular user
named `pkg-builder` (user name is up to you):

```
adduser pkg-builder
addgroup pkg-builder abuild
echo '%abuild ALL=(ALL) NOPASSWD:/sbin/apk, /bin/mkdir -p /etc/apk/keys, /bin/cp -i *.pub /etc/apk/keys/' > /etc/sudoers.d/abuild
chmod 0400 /etc/sudoers.d/abuild
apk add alpine-sdk
```

Build the package as `pkg-builder`:

```
abuild-keygen -nai
abuild -r
```

### DEB

The [debian]() directory contains all definitions to build Debian and
Ubuntu packages.

Package building for Debian like distibutions is a bit tricky. In this
case we want to add the codename to the version number in order to
differentiate builds.

To build package for version 1.0.4, run following commands:

```
apt-get install devscripts build-essential wget
source /etc/os-release
wget https://github.com/AgentD/squashfs-tools-ng/archive/v1.0.4/squashfs-tools-ng-1.0.4.tar.gz -O squashfs-tools-ng_1.0.4+$VERSION_CODENAME.orig.tar.gz
tar xfz squashfs-tools-ng_1.0.4+$VERSION_CODENAME.orig.tar.gz
cd squashfs-tools-ng-1.0.4
ln -s packages/debian
DEBFULLNAME="$USER" DEBEMAIL="$USER@localhost" dch -v 1.0.4+$VERSION_CODENAME-1 -D $VERSION_CODENAME "Build 1.0.4 for $VERSION_CODENAME."
mk-build-deps --install --tool='apt-get --no-install-recommends --yes' debian/control
rm *.deb
debuild
debuild -- clean
```


### PKG

[PKGBUILD]() contains all definition to build Archlinux packages.

Run following commands:

```
sudo pacman -S --noconfirm fakeroot binutils namcap
makepkg --noconfirm -Cfsir PKGBUILD
```

You can check the packages using `namcap`:

```
namcap -i squashfs-tools-*.pkg.tar.zst  PKGBUILD
```

### RPM

[squashfs-tools-ng.spec]() contains all definitions to build RPM
packages.

#### CentOS, Fedora

Run following commands:

```
yum install -y rpm-build spectool
rpmdev-setuptree
spectool -g -R squashfs-tools-ng.spec
rpmspec --parse squashfs-tools-ng.spec | grep BuildRequires | cut -d' ' -f2  | xargs sudo yum install -y
rpmbuild --clean -ba squashfs-tools-ng.spec
```

### OpenSUSE

Run following commands:

```
zypper install -y rpm-build
rpmspec --parse squashfs-tools-ng.spec | grep Source0 | awk '{print $2}' | xargs  wget -N -P $(rpm --eval '%{_sourcedir}')
rpmspec --parse squashfs-tools-ng.spec | grep BuildRequires | cut -d' ' -f2  | xargs sudo zypper install -y
rpmbuild --clean -ba squashfs-tools-ng.spec
```

Note:
* `spectool` does not natively exists on OpenSUSE, hence source has to
   be downloaded manually.
* `zypper` is used intead of `yum`.


