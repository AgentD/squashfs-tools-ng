#!/bin/bash

set -e

VERSION=$(grep AC_INIT configure.ac | grep -o \\[[0-9.]*\\] | tr -d [])

W32_ZIP_NAME="squashfs-tools-ng-${VERSION}-mingw32"
W64_ZIP_NAME="squashfs-tools-ng-${VERSION}-mingw64"

W32_DIR="$(pwd)/$W32_ZIP_NAME"
W32_PREFIX="i686-w64-mingw32"

W64_DIR="$(pwd)/$W64_ZIP_NAME"
W64_PREFIX="x86_64-w64-mingw32"

PKG_URL="https://infraroot.at/pub/squashfs/windows"

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

################################### get xz ###################################

PKG_DIR="xz-5.2.4"
PKG_TAR="${PKG_DIR}.tar.xz"
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

################################## get zstd ##################################

PKG_DIR="zstd-v1.4.4-win32"
PKG_TAR="${PKG_DIR}.zip"
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
PKG_TAR="${PKG_DIR}.zip"
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

export PKG_CONFIG_PATH="$W32_DIR/lib/pkgconfig"

./autogen.sh
./configure CFLAGS="-O2" LZO_CFLAGS="-I$W32_DIR/include" \
	    LZO_LIBS="-L$W32_DIR/lib -llzo2" \
	    --prefix="$W32_DIR" --host="$W32_PREFIX" --with-builtin-lz4 \
	    --with-builtin-zlib
cp "$W32_DIR/bin/"*.dll .
make -j check
rm *.dll

./configure CFLAGS="-O2 -DNDEBUG" LZO_CFLAGS="-I$W32_DIR/include" \
	    LZO_LIBS="-L$W32_DIR/lib -llzo2" \
	    --prefix="$W32_DIR" --host="$W32_PREFIX" --with-builtin-lz4 \
	    --with-builtin-zlib
make clean
make -j
make install-strip

################################ build 64 bit ################################

export PKG_CONFIG_PATH="$W64_DIR/lib/pkgconfig"

./configure CFLAGS="-O2" LZO_CFLAGS="-I$W64_DIR/include" \
	    LZO_LIBS="-L$W64_DIR/lib -llzo2" \
	    --prefix="$W64_DIR" --host="$W64_PREFIX" --with-builtin-lz4 \
	    --with-builtin-zlib
make clean
cp "$W64_DIR/bin/"*.dll .
make -j check
rm *.dll

./configure CFLAGS="-O2 -DNDEBUG" LZO_CFLAGS="-I$W64_DIR/include" \
	    LZO_LIBS="-L$W64_DIR/lib -llzo2" \
	    --prefix="$W64_DIR" --host="$W64_PREFIX" --with-builtin-lz4 \
	    --with-builtin-zlib
make clean
make -j
make install-strip

############################# package everything #############################

cp -r licenses "$W64_DIR"
cp README.md COPYING.md CHANGELOG.md "$W64_DIR"

cp -r licenses "$W32_DIR"
cp README.md COPYING.md CHANGELOG.md "$W32_DIR"

rm -r "$W32_DIR/lib/pkgconfig" "$W64_DIR/lib/pkgconfig"
rm "$W32_DIR/lib"/*.la "$W64_DIR/lib"/*.la

${W32_PREFIX}-strip --discard-all "$W32_DIR/bin"/*.dll "$W32_DIR/bin"/*.exe
${W64_PREFIX}-strip --discard-all "$W64_DIR/bin"/*.dll "$W64_DIR/bin"/*.exe

zip -r "${W32_ZIP_NAME}.zip" "$W32_ZIP_NAME/bin" "$W32_ZIP_NAME/lib"
zip -g -r -l "${W32_ZIP_NAME}.zip" "$W32_ZIP_NAME/include"
zip -g -r -l "${W32_ZIP_NAME}.zip" "$W32_ZIP_NAME/licenses" $W32_ZIP_NAME/*.md

zip -r "${W64_ZIP_NAME}.zip" "$W64_ZIP_NAME/bin" "$W64_ZIP_NAME/lib"
zip -g -r -l "${W64_ZIP_NAME}.zip" "$W64_ZIP_NAME/include"
zip -g -r -l "${W64_ZIP_NAME}.zip" "$W64_ZIP_NAME/licenses" $W64_ZIP_NAME/*.md

############################# sign the packages ##############################
gpg -o "${W64_ZIP_NAME}.zip.asc" --detach-sign -a "${W64_ZIP_NAME}.zip"
gpg -o "${W32_ZIP_NAME}.zip.asc" --detach-sign -a "${W32_ZIP_NAME}.zip"
