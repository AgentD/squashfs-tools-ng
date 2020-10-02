# Package builder

This directory contains files to build packages for several Linux
distributions.

# APK

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

# PKG

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

# RPM

[squashfs-tools-ng.spec]() contains all definitions to build RPM
packages.

## CentOS, Fedora

Run following commands:

```
yum install -y rpm-build spectool
rpmdev-setuptree
spectool -g -R squashfs-tools-ng.spec
rpmspec --parse squashfs-tools-ng.spec | grep BuildRequires | cut -d' ' -f2  | xargs sudo yum install -y
rpmbuild --clean -ba squashfs-tools-ng.spec
```

## OpenSUSE

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


