# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT(tvtime, 0.9.3, http://www.sourceforge.net/projects/tvtime)
AC_CONFIG_SRCDIR([src/tvtime.c])
AM_INIT_AUTOMAKE(tvtime,0.9.3)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_CANONICAL_HOST
if test x"$host_alias" = x""; then host_alias="$host"; fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([errno.h fcntl.h inttypes.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([atexit gettimeofday memset strerror])

AM_PATH_SDL(1.2.2, , AC_MSG_ERROR([Requires SDL 1.2 or later]))



dnl ---------------------------------------------
dnl zlib
dnl ---------------------------------------------
dnl Test for libz
AC_CHECK_LIB(z, gzsetparams,
        [ AC_CHECK_HEADER(zlib.h,
          have_zlib=yes
          ZLIB_LIBS="-lz",)], AC_MSG_ERROR(zlib needed))

AM_CONDITIONAL(HAVE_ZLIB, [test x"$have_zlib" = "xyes"])
AC_SUBST(ZLIB_LIBS)


dnl ---------------------------------------------
dnl libpng
dnl ---------------------------------------------
dnl Test for libpng
AC_CHECK_LIB(png, png_read_png,
        [ AC_CHECK_HEADER(png.h,
          have_libpng=yes
          PNG_LIBS="-lpng",)], AC_MSG_ERROR(libpng needed), "$ZLIB_LIBS")

AM_CONDITIONAL(HAVE_LIBPNG, [test x"$have_libpng" = "xyes"])
AC_SUBST(PNG_LIBS)



AC_ARG_ENABLE([warnings],
    [  --disable-warnings      do not treat warnings as errors])
if test x"$enable_warnings" != x"no"; then
    dnl compiler warnings
    OPT_CFLAGS="$CFLAGS -Wall -Werror"
    dnl FIXME
    dnl AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS])
fi

dnl This stuff looked kinda cool in the mpeg2dec source tree.

dnl GCC-specific flags
if test x"$GCC" = x"yes"; then

    dnl -Wall -Werror moved to the end to not disturb the configure script

    dnl -O3
    changequote(<<,>>)
    OPT_CFLAGS=`echo "$CFLAGS"|sed "s/-O[0-9]*//g;"`
    changequote([,])
    OPT_CFLAGS="$OPT_CFLAGS -O3"
    AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS])

    dnl -fomit-frame-pointer
    OPT_CFLAGS="$CFLAGS -fomit-frame-pointer"
    AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS])

    dnl arch-specific flags
    case "$host_alias" in
    i?86-* | k?-*)
	AC_DEFINE([ARCH_X86],,[x86 architecture])
	case "$host_alias" in
	i386-*) OPT_CFLAGS="$CFLAGS -mcpu=i386";;
	i486-*) OPT_CFLAGS="$CFLAGS -mcpu=i486";;
	i586-*) OPT_CFLAGS="$CFLAGS -mcpu=pentium";;
	i686-*) OPT_CFLAGS="$CFLAGS -mcpu=pentiumpro";;
	k6-*)   OPT_CFLAGS="$CFLAGS -mcpu=k6";;
	esac
	AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS]);;
    ppc-* | powerpc-*)
	OPT_CFLAGS="$CFLAGS -Wa,-m7400"
	AC_TRY_CFLAGS([$OPT_CFLAGS],
	    [CFLAGS=$OPT_CFLAGS; AC_DEFINE([ARCH_PPC],,[ppc architecture])]);;
    sparc-* | sparc64-*)
	OPT_CFLAGS="$CFLAGS -mcpu=ultrasparc -mvis -Wa,-Av9"
	AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS]);;
    mips-sgi-irix6.*)	dnl do we need to be that specific ?
	OPT_CFLAGS="$CFLAGS -mabi=64"
	AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS]);;
    esac
elif test x"$CC" = x"tcc"; then
    dnl TenDRA portability checking compiler
    CFLAGS="-Xp -Yansi -f`pwd`/include/tendra.h"
    enable_mlib=no
    no_x=yes
    enable_sdl=no
else
    dnl non-gcc flags - we probably need exact configuration triplets here.
    case "$host_alias" in
    mips-sgi-irix6.*)
	OPT_CFLAGS="$CFLAGS -64"
	AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS]);;
    sparc-sun-solaris*)
	OPT_CFLAGS="$CFLAGS -xCC -fast -xO5"
	AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS]);;
    esac
fi

dnl Checks for library functions.
AC_CHECK_FUNCS([memalign])
AC_TYPE_SIGNAL

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_C_BIGENDIAN
AC_C_ATTRIBUTE_ALIGNED

dnl builtin([include],[libmpeg2/configure.incl])

AC_CONFIG_FILES([
	Makefile
	docs/Makefile
	plugins/Makefile
	src/Makefile
])
AC_OUTPUT
