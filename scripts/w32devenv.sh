#!/bin/bash

set -e

VERSION=$(grep AC_INIT configure.ac | grep -o \\[[0-9.]*\\] | tr -d [])

W64_DIR="$(pwd)/w32deps"
W64_PREFIX="x86_64-w64-mingw32"

mkdir -p "$W64_DIR"

################################ get binaries ################################

PKG_DIR="squashfs-tools-ng-${VERSION}-mingw64"
PKG_TAR="${PKG_DIR}.zip"
PKG_URL="https://infraroot.at/pub/squashfs/windows"
PKG_HASH="029d75c28af656e3deb35dac1e433b30e278f42077a8fea3e2e9d4dc34dc458c"

[ -f "$W64_DIR/$PKG_TAR" ] || {
	curl -s -L "$PKG_URL/$PKG_TAR" > "$W64_DIR/$PKG_TAR"
	echo "$PKG_HASH  $W64_DIR/$PKG_TAR" | sha256sum -c --quiet "-"
}

[ -d "$W64_DIR/$PKG_DIR" ] || {
	unzip "$W64_DIR/$PKG_TAR" -d "$W64_DIR"
}

pushd "$W64_DIR/$PKG_DIR"
rm bin/*.exe
rm bin/libsquashfs.dll
rm -r include/sqfs
rm lib/libsquashfs.dll.a
rm lib/libsquashfs.a

mv bin "$W64_DIR"
mv lib "$W64_DIR"
mv include "$W64_DIR"
popd

################################ build 64 bit ################################

export PKG_CONFIG_PATH="$W64_DIR/lib/pkgconfig"

./autogen.sh
./configure CFLAGS="-O2" LZO_CFLAGS="-I$W64_DIR/include" \
	    LZO_LIBS="-L$W64_DIR/lib -llzo2" \
	    BZIP2_CFLAGS="-I$W64_DIR/include" \
	    BZIP2_LIBS="-L$W64_DIR/lib -lbz2" \
	    --prefix="$W64_DIR" --host="$W64_PREFIX"
