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
			unzip "$PKG_TAR"
			;;
		*)
			tar -xf "$PKG_TAR"
			;;
		esac
	}
}

################################## get zlib ##################################

PKG_DIR="zlib-1.2.12"
PKG_TAR="${PKG_DIR}.tar.xz"
PKG_HASH="7db46b8d7726232a621befaab4a1c870f00a90805511c0e0090441dac57def18"

download

mkdir -p "$W32_DIR/bin" "$W32_DIR/include" "$W32_DIR/lib/pkgconfig"
mkdir -p "$W64_DIR/bin" "$W64_DIR/include" "$W64_DIR/lib/pkgconfig"

cp "$PKG_DIR/zlib.h" "$PKG_DIR/zconf.h" "$W32_DIR/include"
cp "$PKG_DIR/zlib.h" "$PKG_DIR/zconf.h" "$W64_DIR/include"

pushd "$PKG_DIR"
obj="adler32.o compress.o crc32.o deflate.o gzclose.o gzlib.o gzread.o"
obj="$obj gzwrite.o infback.o inffast.o inflate.o inftrees.o trees.o"
obj="$obj uncompr.o zutil.o"

for outfile in $obj; do
	infile="$(basename $outfile .o).c"
	${W32_PREFIX}-gcc -O3 -c "$infile" -o "$outfile"
done

${W32_PREFIX}-windres --define GCC_WINDRES -o zlibrc.o win32/zlib1.rc
${W32_PREFIX}-gcc -shared -Wl,--out-implib,libz.dll.a -o zlib1.dll \
	     win32/zlib.def $obj zlibrc.o
${W32_PREFIX}-strip zlib1.dll
${W32_PREFIX}-ar rcs libz.a $obj

rm *.o
mv zlib1.dll "$W32_DIR/bin"
mv libz.a libz.dll.a "$W32_DIR/lib"

cat > "$W32_DIR/lib/pkgconfig/zlib.pc" <<_EOF
prefix=$W32_DIR
libdir=$W32_DIR/lib
sharedlibdir=$W32_DIR/bin
includedir=$W32_DIR/include

Name: zlib
Description: zlib compression library
Version: 1.2.12
Libs: -L$W32_DIR/lib -L$W32_DIR/bin -lz
Cflags: -I$W32_DIR/include
_EOF

for outfile in $obj; do
	infile="$(basename $outfile .o).c"
	${W64_PREFIX}-gcc -O3 -c "$infile" -o "$outfile"
done

${W64_PREFIX}-windres --define GCC_WINDRES -o zlibrc.o win32/zlib1.rc
${W64_PREFIX}-gcc -shared -Wl,--out-implib,libz.dll.a -o zlib1.dll \
	     win32/zlib.def $obj zlibrc.o
${W64_PREFIX}-strip zlib1.dll
${W64_PREFIX}-ar rcs libz.a $obj

rm *.o
mv zlib1.dll "$W64_DIR/bin"
mv libz.a libz.dll.a "$W64_DIR/lib"

cat > "$W64_DIR/lib/pkgconfig/zlib.pc" <<_EOF
prefix=$W64_DIR
libdir=$W64_DIR/lib
sharedlibdir=$W64_DIR/bin
includedir=$W64_DIR/include

Name: zlib
Description: zlib compression library
Version: 1.2.12
Libs: -L$W64_DIR/lib -L$W64_DIR/bin -lz
Cflags: -I$W64_DIR/include
_EOF
popd

################################### get xz ###################################

PKG_DIR="xz-5.2.5"
PKG_TAR="${PKG_DIR}.tar.xz"
PKG_HASH="3e1e518ffc912f86608a8cb35e4bd41ad1aec210df2a47aaa1f95e7f5576ef56"

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

################################# get bzip2 ##################################

PKG_DIR="bzip2-1.0.8"
PKG_TAR="${PKG_DIR}.tar.gz"
PKG_HASH="ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269"

download

pushd "$PKG_DIR"
${W32_PREFIX}-gcc -O2 -c blocksort.c
${W32_PREFIX}-gcc -O2 -c huffman.c
${W32_PREFIX}-gcc -O2 -c crctable.c
${W32_PREFIX}-gcc -O2 -c randtable.c
${W32_PREFIX}-gcc -O2 -c compress.c
${W32_PREFIX}-gcc -O2 -c decompress.c
${W32_PREFIX}-gcc -O2 -c bzlib.c
${W32_PREFIX}-ar cq libbz2.a *.o
${W32_PREFIX}-ranlib libbz2.a
cp libbz2.a "$W32_DIR/lib"
cp bzlib.h "$W32_DIR/include"

rm *.o *.a
${W64_PREFIX}-gcc -O2 -c blocksort.c
${W64_PREFIX}-gcc -O2 -c huffman.c
${W64_PREFIX}-gcc -O2 -c crctable.c
${W64_PREFIX}-gcc -O2 -c randtable.c
${W64_PREFIX}-gcc -O2 -c compress.c
${W64_PREFIX}-gcc -O2 -c decompress.c
${W64_PREFIX}-gcc -O2 -c bzlib.c
${W64_PREFIX}-ar cq libbz2.a *.o
${W64_PREFIX}-ranlib libbz2.a
cp libbz2.a "$W64_DIR/lib"
cp bzlib.h "$W64_DIR/include"
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

PKG_DIR="zstd-v1.5.2-win32"
PKG_TAR="${PKG_DIR}.zip"
PKG_HASH="d0a5361401607f2f85706989fbc69ebb760c34d2337e72573a303433898c3196"

download
mv "$PKG_DIR/dll/libzstd.dll" "$W32_DIR/bin"
mv "$PKG_DIR/dll/libzstd.dll.a" "$W32_DIR/lib/libzstd.dll.a"
mv "$PKG_DIR/include"/*.h "$W32_DIR/include"

cat > "$W32_DIR/lib/pkgconfig/libzstd.pc" <<_EOF
prefix=$W32_DIR
libdir=$W32_DIR/lib
includedir=$W32_DIR/include

Name: zstd
Description: fast lossless compression algorithm library
URL: http://www.zstd.net/
Version: 1.5.2
Libs: -L$W32_DIR/lib -lzstd
Cflags: -I$W32_DIR/include
_EOF

PKG_DIR="zstd-v1.5.2-win64"
PKG_TAR="${PKG_DIR}.zip"
PKG_HASH="2faf3b9061b731f8d37c5b3bb4a6f08be89af43f62bdd93f784a85af7d7c4f5b"

download
mv "$PKG_DIR/dll/libzstd.dll" "$W64_DIR/bin"
mv "$PKG_DIR/dll/libzstd.dll.a" "$W64_DIR/lib/libzstd.dll.a"
mv "$PKG_DIR/include"/*.h "$W64_DIR/include"

cat > "$W64_DIR/lib/pkgconfig/libzstd.pc" <<_EOF
prefix=$W64_DIR
libdir=$W64_DIR/lib
includedir=$W64_DIR/include

Name: zstd
Description: fast lossless compression algorithm library
URL: http://www.zstd.net/
Version: 1.5.2
Libs: -L$W64_DIR/lib -lzstd
Cflags: -I$W64_DIR/include
_EOF

################################## get lz4 ##################################

PKG_DIR="lz4-1.9.4"
PKG_TAR="${PKG_DIR}.tar.gz"
PKG_HASH="0b0e3aa07c8c063ddf40b082bdf7e37a1562bda40a0ff5272957f3e987e0e54b"

download

pushd "$PKG_DIR/lib"
make TARGET_OS=Windows CC=${W32_PREFIX}-gcc WINDRES=${W32_PREFIX}-windres
mv dll/*.lib "$W32_DIR/lib"
mv dll/*.dll "$W32_DIR/bin"
cp *.h "$W32_DIR/include"
rm *.o *.a

make TARGET_OS=Windows CC=${W64_PREFIX}-gcc WINDRES=${W64_PREFIX}-windres
mv dll/*.lib "$W64_DIR/lib"
mv dll/*.dll "$W64_DIR/bin"
cp *.h "$W64_DIR/include"
rm *.o *.a
popd

cat > "$W32_DIR/lib/pkgconfig/liblz4.pc" <<_EOF
prefix=$W32_DIR
libdir=$W32_DIR/lib
includedir=$W32_DIR/include

Name: lz4
Description: fast lossless compression algorithm library
URL: https://lz4.github.io/lz4/
Version: 1.9.4
Libs: -L$W32_DIR/lib -llz4
Cflags: -I$W32_DIR/include
_EOF

cat > "$W64_DIR/lib/pkgconfig/liblz4.pc" <<_EOF
prefix=$W64_DIR
libdir=$W64_DIR/lib
includedir=$W64_DIR/include

Name: lz4
Description: fast lossless compression algorithm library
URL: https://lz4.github.io/lz4/
Version: 1.9.4
Libs: -L$W64_DIR/lib -llz4
Cflags: -I$W64_DIR/include
_EOF

################################ build 32 bit ################################

export PKG_CONFIG_PATH="$W32_DIR/lib/pkgconfig"

./autogen.sh
./configure CFLAGS="-O2" LZO_CFLAGS="-I$W32_DIR/include" \
	    LZO_LIBS="-L$W32_DIR/lib -llzo2" \
	    BZIP2_CFLAGS="-I$W32_DIR/include" \
	    BZIP2_LIBS="-L$W32_DIR/lib -lbz2" \
	    --prefix="$W32_DIR" --host="$W32_PREFIX"
cp "$W32_DIR/bin/"*.dll .
make -j check
rm *.dll

./configure CFLAGS="-O2 -DNDEBUG" LZO_CFLAGS="-I$W32_DIR/include" \
	    LZO_LIBS="-L$W32_DIR/lib -llzo2" \
	    BZIP2_CFLAGS="-I$W32_DIR/include" \
	    BZIP2_LIBS="-L$W32_DIR/lib -lbz2" \
	    --prefix="$W32_DIR" --host="$W32_PREFIX"
make clean
make -j
make install-strip

################################ build 64 bit ################################

export PKG_CONFIG_PATH="$W64_DIR/lib/pkgconfig"

./configure CFLAGS="-O2" LZO_CFLAGS="-I$W64_DIR/include" \
	    LZO_LIBS="-L$W64_DIR/lib -llzo2" \
	    BZIP2_CFLAGS="-I$W64_DIR/include" \
	    BZIP2_LIBS="-L$W64_DIR/lib -lbz2" \
	    --prefix="$W64_DIR" --host="$W64_PREFIX"
make clean
cp "$W64_DIR/bin/"*.dll .
make -j check
rm *.dll

./configure CFLAGS="-O2 -DNDEBUG" LZO_CFLAGS="-I$W64_DIR/include" \
	    LZO_LIBS="-L$W64_DIR/lib -llzo2" \
	    BZIP2_CFLAGS="-I$W64_DIR/include" \
	    BZIP2_LIBS="-L$W64_DIR/lib -lbz2" \
	    --prefix="$W64_DIR" --host="$W64_PREFIX"
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
