#!/bin/bash

set -e

W32_DIR="$(pwd)/out32"
W32_PREFIX="i686-w64-mingw32"

W64_DIR="$(pwd)/out64"
W64_PREFIX="x86_64-w64-mingw32"

download() {
	echo "-- fetching $PKG_TAR -- "
	curl -s -L "$PKG_URL/$PKG_TAR" > "$PKG_TAR"
	echo "$PKG_HASH  $PKG_TAR" | sha256sum -c --quiet "-"
	tar -xf "$PKG_TAR"
}

################################## get zlib ##################################

PKG_DIR="zlib-1.2.11"
PKG_TAR="${PKG_DIR}.tar.xz"
PKG_URL="https://zlib.net"
PKG_HASH="4ff941449631ace0d4d203e3483be9dbc9da454084111f97ea0a2114e19bf066"

download

pushd "$PKG_DIR"
make -j PREFIX="${W32_PREFIX}-" -f win32/Makefile.gcc libz.a zlib1.dll
make PREFIX="${W32_PREFIX}-" prefix="" DESTDIR="$W32_DIR" SHARED_MODE="1" \
     INCLUDE_PATH="/include" LIBRARY_PATH="/lib" BINARY_PATH="/bin" \
     -f win32/Makefile.gcc install
make PREFIX="${W32_PREFIX}-" -f win32/Makefile.gcc clean

make -j PREFIX="${W64_PREFIX}-" -f win32/Makefile.gcc libz.a zlib1.dll
make PREFIX="${W64_PREFIX}-" prefix="" DESTDIR="$W64_DIR" SHARED_MODE="1" \
     INCLUDE_PATH="/include" LIBRARY_PATH="/lib" BINARY_PATH="/bin" \
     -f win32/Makefile.gcc install
popd

################################### get xz ###################################

PKG_DIR="xz-5.2.4"
PKG_TAR="${PKG_DIR}.tar.xz"
PKG_URL="https://tukaani.org/xz"
PKG_HASH="9717ae363760dedf573dad241420c5fea86256b65bc21d2cf71b2b12f0544f4b"

download

pushd "$PKG_DIR"
./configure CFLAGS="-O2" --prefix="$W32_DIR" --host="$W32_PREFIX" \
	    --disable-xz --disable-xzdec --disable-lzmadec \
	    --disable-lzmainfo --disable-links \
	    --disable-scripts --disable-doc
make -j
make install-strip
make clean

./configure CFLAGS="-O2" --prefix="$W64_DIR" --host="$W64_PREFIX" \
	    --disable-xz --disable-xzdec --disable-lzmadec \
	    --disable-lzmainfo --disable-links \
	    --disable-scripts --disable-doc
make -j
make install-strip
popd

################################## get lzo ###################################

PKG_DIR="lzo-2.10"
PKG_TAR="${PKG_DIR}.tar.gz"
PKG_URL="http://www.oberhumer.com/opensource/lzo/download"
PKG_HASH="c0f892943208266f9b6543b3ae308fab6284c5c90e627931446fb49b4221a072"

download

pushd "$PKG_DIR"
./configure CFLAGS="-O2" --prefix="$W32_DIR" --host="$W32_PREFIX" \
	    --disable-shared --enable-static
make -j
make install-strip
make clean

./configure CFLAGS="-O2" --prefix="$W32_DIR" --host="$W32_PREFIX" \
	    --disable-shared --enable-static
make -j
make install-strip
popd

################################## get lz4 ###################################

PKG_DIR="lz4-1.9.2"
PKG_TAR="v1.9.2.tar.gz"
PKG_URL="https://github.com/lz4/lz4/archive"
PKG_HASH="658ba6191fa44c92280d4aa2c271b0f4fbc0e34d249578dd05e50e76d0e5efcc"

download

pushd "$PKG_DIR"
make -j BUILD_STATIC="yes" CC="${W32_PREFIX}-gcc" \
     DLLTOOL="${W32_PREFIX}-dlltool" OS="Windows_NT" lib-release
make PREFIX="$W32_DIR" -C "lib" install

make clean

make -j BUILD_STATIC="yes" CC="${W64_PREFIX}-gcc" \
     DLLTOOL="${W64_PREFIX}-dlltool" OS="Windows_NT" lib-release
make PREFIX="$W64_DIR" -C "lib" install
popd

################################ build 32 bit ################################

export PKG_CONFIG_SYSROOT_DIR="$W32_DIR"
export PKG_CONFIG_LIBDIR="$W32_DIR/lib/pkgconfig"
export PKG_CONFIG_PATH="$W32_DIR/lib/pkgconfig"

./autogen.sh
./configure CFLAGS="-O2 -DNDEBUG" --prefix="$W32_DIR" --host="$W32_PREFIX"
make -j
make install-strip

################################ build 64 bit ################################

export PKG_CONFIG_SYSROOT_DIR="$W64_DIR"
export PKG_CONFIG_LIBDIR="$W64_DIR/lib/pkgconfig"
export PKG_CONFIG_PATH="$W64_DIR/lib/pkgconfig"

./configure CFLAGS="-O2 -DNDEBUG" --prefix="$W64_DIR" --host="$W64_PREFIX"
make clean
make -j
make install-strip
