#!/bin/bash

set -e

usage() {
        cat <<EOF

Usage: ${0##*/} <new_version>

<new_version>    - release version to create in X.Y.Z format
EOF
        exit 0
}

fatal() {
        printf "Error: %s\n" "$1" >&2
        exit 1
}

askyesno() {
	local question="$1"
	local choice=""

	read -p "$question [y/N]? " choice

	case "$choice" in
	y|Y)
		echo "yes"
		;;
	*)
		;;
	esac

	return 0
}

##### get the command line arguments
[ $# -eq 0 ] && usage
[ $# -eq 1 ] || fatal "Insufficient or too many argumetns"

new_ver="$1"; shift

VER_REGEX="\([0-9]\+.[0-9]\+.[0-9]\+\)"
echo "$new_ver" | grep -q -x "$VER_REGEX" ||
        fatal "please, provide new version in X.Y.Z format"

##### parse the old version information
old_ver="$(grep AC_INIT configure.ac | grep -o \\[[0-9.]*\\] | tr -d [])"
old_so_ver="$(grep LIBSQUASHFS_SO_VERSION configure.ac | \
	      grep -o \\[[0-9:]*\\] | tr -d [])"

current=$(echo "${old_so_ver}" | cut -d: -f1)
revision=$(echo "${old_so_ver}" | cut -d: -f2)
age=$(echo "${old_so_ver}" | cut -d: -f3)

echo "$old_ver" | grep -q -x "$VER_REGEX" ||
        fatal "error reading old version number"

VER_REGEX="\([0-9]\+:[0-9]\+:[0-9]\+\)"
echo "$old_so_ver" | grep -q -x "$VER_REGEX" ||
        fatal "error reading old so version"

[ "x$current:$revision:$age" = "x$old_so_ver" ] || \
	fatal "Error parsing old so version"

##### generate new library SO version
libchanges=$(git diff --numstat v${old_ver}..HEAD lib/sqfs/ | wc -l)

if [ $libchanges -gt 0 ]; then
	echo "Detected changes to library code, updating so version"
	added=$(askyesno "Have any public interfaces been changed")
	changed=$(askyesno "Have any public interfaces been added")
	removed=$(askyesno "Have any public interfaces been removed")

	revision=$((revision+1))

	if [ -n "$added" -o -n "$removed" -o -n "$changed" ]; then
		current=$((current+1))
		revision=0
	fi

	if [ -n "$added" ]; then
		age=$((age+1))
	fi

	if [ -n "$removed" -o -n "$changed" ]; then
		age=0
	fi
else
	echo "No changes to library code detected, keeping old so version"
fi

new_so_ver="${current}:${revision}:${age}"

echo ""
echo "current package version:      $old_ver"
echo "current library so-version:   $old_so_ver"
echo ""
echo "creating package version:     $new_ver"
echo "With library so-version:      $new_so_ver"
echo ""

if [ "x$(askyesno "Is this ok")" != "xyes" ]; then
	echo "Aborting"
	exit
fi

tag_name="v$new_ver"
release_name="squashfs-tools-ng-${new_ver}"

# Make sure the git index is up-to-date
[ -z "$(git status --porcelain)" ] || fatal "Git index is not up-to-date"

# Make sure the tag does not exist
[ -z "$(git tag -l "$tag_name")" ] || fatal "Tag $tag_name already exists"

# Change the version in the configure.ac
sed -i -e "s/$old_ver/$new_ver/g" configure.ac
sed -i -e "s/$old_so_ver/$new_so_ver/g" configure.ac

# Commit the change, create new signed tag
echo "Generating signed release commit tag $tag_name"

git commit -s -m "Release version $new_ver" configure.ac
git tag -m "Release $new_ver" -s "$tag_name"

# Prepare signed tarball
./autogen.sh
./configure
make -j distcheck

echo "Signing the tarball"
gpg -o "$release_name.tar.gz.asc" --detach-sign \
    -a "$release_name.tar.gz"
gpg -o "$release_name.tar.xz.asc" --detach-sign \
    -a "$release_name.tar.xz"

echo "Generating doxygen documentation"
make doxygen-doc
tar czf "doxydoc-$new_ver.tar.gz" -C doxygen-doc/html .
