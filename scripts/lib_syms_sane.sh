#!/bin/bash

SPC="[[:space:]]\+"
HEXNUM="[0-9a-fA-F]\+"
NUM="[0-9]\+"

WINE_PATTERN="^${SPC}${HEXNUM}${SPC}${NUM}${SPC}"
ELF_PATTERN="^${SPC}${NUM}:${SPC}${HEXNUM}${SPC}${NUM}${SPC}"

symbols_from_file() {
	case "$1" in
	*.dll)
		winedump -j export "$1" | grep "$WINE_PATTERN" | \
			sed "s/$WINE_PATTERN//g"
		;;
	*)
		readelf -TW -s "$1" | grep "$ELF_PATTERN" | grep -v "UND" | \
			sed "s/$ELF_PATTERN//g" | grep -o "${NUM}${SPC}.\+$" | \
			sed "s/^${NUM}${SPC}//g"
	esac
}

symbols_from_file "$1" | grep -q "^sqfs_.*"

if [ "$?" != "0" ]; then
	symbols_from_file "$1"
	echo "ERROR: No symbols with sqfs_ prefix found!"
	exit 1
fi

symbols_from_file "$1" | grep -v "^sqfs_"

if [ "$?" != "1" ]; then
	echo "ERROR: Symbols without sqfs_ prefix found!"
	exit 1
fi

exit 0
