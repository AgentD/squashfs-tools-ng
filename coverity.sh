#!/bin/sh

COVERITY_PATH="$HOME/Downloads/cov-analysis-linux64-2021.12.1"
TOKEN=$(cat "$HOME/.coverity/squashfs-tools-ng")
EMAIL=$(git log --follow --pretty=format:"%ae" -- coverity.sh | head -n 1)

DESCRIPTION=$(git describe --always --tags --dirty)
VERSION=$(grep AC_INIT configure.ac | grep -o \\[[0-9.]*\\] | tr -d [])

export PATH="$COVERITY_PATH/bin:$PATH"

if [ $# -eq 1 ]; then
	case "$1" in
	--32bit)
		./autogen.sh
		./configure CFLAGS="-m32" CXXFLAGS="-m32" LDFLAGS="-m32"
		DESCRIPTION="$DESCRIPTION-32bit"
		;;
	*)
		echo "Unknown option `$1`" >&2
		exit 1
		;;
	esac
else
	./autogen.sh
	./configure
fi

make clean
cov-build --dir cov-int make -j
tar czvf squashfs-tools-ng.tgz cov-int

echo "Uploading tarball with the following settings:"
echo "Email: $EMAIL"
echo "Version: $VERSION"
echo "Description: $DESCRIPTION"

curl --form token="$TOKEN" \
     --form email="$EMAIL" \
     --form file=@squashfs-tools-ng.tgz \
     --form version="$VERSION" \
     --form description="$DESCRIPTION" \
     https://scan.coverity.com/builds?project=squashfs-tools-ng
