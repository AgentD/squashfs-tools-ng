#!/bin/bash

set -e

W32_DIR="$(pwd)/out32"
W32_PREFIX="i686-w64-mingw32"

W64_DIR="$(pwd)/out64"
W64_PREFIX="x86_64-w64-mingw32"

download() {
	echo "-- fetching $PKG_TAR -- "

	[ -f "$PKG_TAR" ] || {
		curl -s -L "$PKG_URL/$PKG_TAR" > "$PKG_TAR"
		echo "$PKG_HASH  $PKG_TAR" | sha256sum -c --quiet "-"
	}

	[ -d "$PKG_DIR" ] || {
		case "$PKG_TAR" in
		*.zip)
			unzip "$PKG_TAR" -d "$PKG_DIR"
			;;
		*)
			tar -xf "$PKG_TAR"
			;;
		esac
	}
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
	    --enable-shared --disable-static
make -j
make install-strip
make clean

./configure CFLAGS="-O2" --prefix="$W64_DIR" --host="$W64_PREFIX" \
	    --enable-shared --disable-static
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
make -j BUILD_STATIC="no" BUILD_SHARED="yes" CC="${W32_PREFIX}-gcc" \
     DLLTOOL="${W32_PREFIX}-dlltool" WINDRES="${W32_PREFIX}-windres" \
     OS="Windows_NT" WINBASED="yes" PREFIX="$W32_DIR" \
     lib-release
make BUILD_STATIC="no" BUILD_SHARED="yes" CC="${W32_PREFIX}-gcc" \
     DLLTOOL="${W32_PREFIX}-dlltool" WINDRES="${W32_PREFIX}-windres" \
     OS="Windows_NT" WINBASED="yes" PREFIX="$W32_DIR" \
     -C "lib" install

make clean

make -j BUILD_STATIC="no" BUILD_SHARED="yes" CC="${W64_PREFIX}-gcc" \
     DLLTOOL="${W64_PREFIX}-dlltool" WINDRES="${W64_PREFIX}-windres" \
     OS="Windows_NT" WINBASED="yes" PREFIX="$W64_DIR" \
     lib-release
make BUILD_STATIC="no" BUILD_SHARED="yes" CC="${W64_PREFIX}-gcc" \
     DLLTOOL="${W64_PREFIX}-dlltool" WINDRES="${W64_PREFIX}-windres" \
     OS="Windows_NT" WINBASED="yes" PREFIX="$W64_DIR" \
     -C "lib" install
popd

################################## get zstd ##################################

PKG_DIR="zstd-v1.4.4-win32"
PKG_TAR="zstd-v1.4.4-win32.zip"
PKG_URL="https://github.com/facebook/zstd/releases/download/v1.4.4"
PKG_HASH="60d4cd6510e7253d33f47a68554a003b50dba05d1db89e16ef32bc26b126b92c"

download
mv "$PKG_DIR/dll/libzstd.dll" "$W32_DIR/bin"
mv "$PKG_DIR/dll/libzstd.lib" "$W32_DIR/lib/libzstd.dll.a"
mv "$PKG_DIR/include"/*.h "$W32_DIR/include"

cat > "$W32_DIR/lib/pkgconfig/libzstd.pc" <<_EOF
prefix=$W32_DIR
libdir=$W32_DIR/lib
includedir=$W32_DIR/include

Name: zstd
Description: fast lossless compression algorithm library
URL: http://www.zstd.net/
Version: 1.4.4
Libs: -L$W32_DIR/lib -lzstd
Cflags: -I$W32_DIR/include
_EOF

PKG_DIR="zstd-v1.4.4-win64"
PKG_TAR="zstd-v1.4.4-win64.zip"
PKG_URL="https://github.com/facebook/zstd/releases/download/v1.4.4"
PKG_HASH="bb1591db8376fb5360640088e0cc9920c6da9cd0f5fd4e9229316261808c1581"

download
mv "$PKG_DIR/dll/libzstd.dll" "$W64_DIR/bin"
mv "$PKG_DIR/dll/libzstd.lib" "$W64_DIR/lib/libzstd.dll.a"
mv "$PKG_DIR/include"/*.h "$W64_DIR/include"

cat > "$W64_DIR/lib/pkgconfig/libzstd.pc" <<_EOF
prefix=$W64_DIR
libdir=$W64_DIR/lib
includedir=$W64_DIR/include

Name: zstd
Description: fast lossless compression algorithm library
URL: http://www.zstd.net/
Version: 1.4.4
Libs: -L$W64_DIR/lib -lzstd
Cflags: -I$W64_DIR/include
_EOF

################################ build 32 bit ################################

export PKG_CONFIG_SYSROOT_DIR="$W32_DIR"
export PKG_CONFIG_LIBDIR="$W32_DIR/lib/pkgconfig"
export PKG_CONFIG_PATH="$W32_DIR/lib/pkgconfig"

./autogen.sh
./configure CFLAGS="-O2 -DNDEBUG" LZO_CFLAGS="-I$W32_DIR/include" \
	    LZO_LIBS="-L$W32_DIR/lib -llzo2" \
	    --prefix="$W32_DIR" --host="$W32_PREFIX"
make -j
make install-strip

################################ build 64 bit ################################

export PKG_CONFIG_SYSROOT_DIR="$W64_DIR"
export PKG_CONFIG_LIBDIR="$W64_DIR/lib/pkgconfig"
export PKG_CONFIG_PATH="$W64_DIR/lib/pkgconfig"

./configure CFLAGS="-O2 -DNDEBUG" LZO_CFLAGS="-I$W64_DIR/include" \
	    LZO_LIBS="-L$W64_DIR/lib -llzo2" \
	    --prefix="$W64_DIR" --host="$W64_PREFIX"
make clean
make -j
make install-strip
