AC_DEFUN([AC_TEST_ZSTD_STREAM], [
	AC_MSG_CHECKING([whether zstd supports stream compression])
	AC_LANG_PUSH([C])
	ac_zstd_have_stream="no"
	ac_zstd_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $ZSTD_CFLAGS"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <zstd.h>], [ZSTD_EndDirective op = ZSTD_e_end; ZSTD_compressStream2(NULL, NULL, NULL, op);])],
			  ac_zstd_have_stream="yes"
			  AC_MSG_RESULT([yes]), AC_MSG_RESULT([no]))

        AS_IF([test "x$ac_zstd_have_stream" = "xyes"],
		[AC_DEFINE(HAVE_ZSTD_STREAM, 1, [Does zstd support stream compression?])])

	AM_CONDITIONAL([HAVE_ZSTD_STREAM], [test "x$ac_zstd_have_stream" = "xyes"])
	CFLAGS=$ac_zstd_save_CFLAGS
	AC_LANG_POP([C])
])
