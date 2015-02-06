#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.6])
AC_INIT([vmon], [0.2.0], [fromani@redhat.com])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

LT_INIT

COMMIT=`git log -n 1 --format=%H 2> /dev/null`
AC_SUBST(COMMIT)
AC_SUBST(RELEASE, 1)
AM_INIT_AUTOMAKE([-Wno-portability])

AC_ARG_ENABLE(debug,
              [  --enable-debug          Enable debugging information],
              USE_DEBUG="$enableval", USE_DEBUG="no")

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
VMON_CFLAGS="--pedantic -Wall -Wextra -Werror -Wno-missing-field-initializers -D__STDC_FORMAT_MACROS -D_GNU_SOURCE -DG_LOG_DOMAIN=\\\"vmon\\\""

if test $USE_DEBUG = yes ; then
	DEBUG=1
	VMON_CFLAGS+=" -g -O0"
else
	DEBUG=0
fi

AC_SUBST([VMON_CFLAGS])
AM_PATH_PYTHON([2.6])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/statvfs.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([getpagesize memset mkdir rmdir])

PKG_CHECK_MODULES(GLIB2, [glib-2.0])
PKG_CHECK_MODULES(GTHREAD2, [gthread-2.0])
PKG_CHECK_MODULES(LIBVIRT, [libvirt-qemu])
PKG_CHECK_MODULES(UUID, [uuid])

AC_OUTPUT([Makefile
           vmon.spec
	   contrib/Makefile
	   contrib/jsmn/Makefile
           src/Makefile
	   tests/Makefile])
