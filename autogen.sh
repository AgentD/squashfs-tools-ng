#!/bin/sh

DIE=0

(autoreconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile this package."
	echo "Download the appropriate package for your distribution,"
	echo "or see http://www.gnu.org/software/autoconf"
	DIE=1
}

(libtoolize --version) < /dev/null > /dev/null 2>&1 ||
(glibtoolize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile this package."
	echo "Download the appropriate package for your distribution,"
	echo "or see http://www.gnu.org/software/libtool"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile this package."
	echo "Download the appropriate package for your distribution,"
	echo "or see http://www.gnu.org/software/automake"
	DIE=1
}

if ! test -f "$(aclocal --print-ac-dir)"/pkg.m4; then
	echo
	echo "You must have pkg-config installed to compile this package."
	echo "Download the appropriate package for your distribution,"
	echo "or see https://www.freedesktop.org/wiki/Software/pkg-config/"
	DIE=1
fi

if test "$DIE" -eq 1; then
	exit 1
fi

autoreconf --force --install --symlink

if ! grep -q pkg.m4 aclocal.m4; then
	cat <<EOF
Couldn't find pkg.m4 from pkg-config. Install the appropriate package for
your distribution or set ACLOCAL_PATH to the directory containing pkg.m4.
EOF
	exit 1
fi
