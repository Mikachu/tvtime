# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.52)
AC_INIT(tvtime, 0.9.11, http://tvtime.net/)
AC_CONFIG_SRCDIR([src/tvtime.c])
AM_INIT_AUTOMAKE(tvtime,0.9.11)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE
AC_CANONICAL_HOST
if test x"$host_alias" = x""; then host_alias="$host"; fi

# Check for compilers.
AC_PROG_CC
AC_CHECK_PROG(found_cc, "$CC", yes, no)
test "x$found_cc" = "xyes" || exit 1

# AC_PROG_CXX
# AC_CHECK_PROG(found_cxx, "$CXX", yes, no)
# test "x$found_cxx" = "xyes" || exit 1

# Check for libtool.
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([ctype.h dirent.h errno.h fcntl.h getopt.h langinfo.h math.h netinet/in.h pwd.h signal.h stdint.h stdio.h stdlib.h string.h sys/ioctl.h sys/mman.h sys/resource.h sys/stat.h sys/time.h sys/wait.h sys/types.h unistd.h wordexp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MMAP
AC_CHECK_FUNCS([asprintf atexit fork execlp getopt_long getpriority gettimeofday seteuid memset setreuid setpriority signal sigemptyset sigaction strerror vsscanf wordexp wordfree])

# Check for gettext
AM_GNU_GETTEXT

dnl ---------------------------------------------
dnl libsdl
dnl ---------------------------------------------
AC_ARG_WITH([sdl], 
            AC_HELP_STRING([--without-sdl], [Use this to disable our SDL output layer (enabled by default)]),
            [CHECK_SDL="$withval"], [CHECK_SDL="yes"])
if test "$CHECK_SDL" = "yes" ; then
    AM_PATH_SDL(1.2.2,AC_DEFINE([HAVE_SDL],,[Define this to include SDL support]),)
fi

dnl ---------------------------------------------
dnl libmpeg2
dnl ---------------------------------------------
AC_ARG_WITH([libmpeg2],
            AC_HELP_STRING([--with-libmpeg2],
                [Use this to enable MPEG2 input (disabled by default)]),
            [USE_MPEG2="$withval"], [USE_MPEG2="no"])
if test "$USE_MPEG2" != "no" ; then
    AC_DEFINE([HAVE_LIBMPEG2],,[Define this to enable MPEG2 input])
fi
AM_CONDITIONAL(HAVE_LIBMPEG2, [test x"$USE_MPEG2" != "xno"])

dnl ---------------------------------------------
dnl dynamic linker
dnl ---------------------------------------------
AC_CHECK_LIB(c, dlopen,
	     DYNAMIC_LD_LIBS="",
	     AC_CHECK_LIB(dl, dlopen,
             	          DYNAMIC_LD_LIBS="-ldl",
	                  AC_MSG_ERROR(dynamic linker needed)))
AC_SUBST(DYNAMIC_LD_LIBS)

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
          PNG_LIBS="-lpng $ZLIB_LIBS -lm",)], AC_MSG_ERROR(libpng needed), "$ZLIB_LIBS" -lm)

AM_CONDITIONAL(HAVE_LIBPNG, [test x"$have_libpng" = "xyes"])
AC_SUBST(PNG_LIBS)


dnl ---------------------------------------------
dnl freetype
dnl ---------------------------------------------
dnl Test for freetype
AC_PATH_PROG(FREETYPE_CONFIG, freetype-config, no)
if test "$FREETYPE_CONFIG" = "no" ; then
  AC_MSG_ERROR(freetype2 needed and freetype-config not found)
fi

dnl ---------------------------------------------
dnl libxml2
dnl ---------------------------------------------
dnl Test for libxml2

AC_PATH_PROG(LIBXML2_CONFIG,xml2-config,no)
if test "$LIBXML2_CONFIG" = "no" ; then
	AC_MSG_ERROR(libxml2 needed and xml2-config not found)
else
	LIBXML_VERSION=`$LIBXML2_CONFIG --version`
	export MAJOR=0 ; export MINOR=0 ; export MICRO=0
	MAJOR=`echo $LIBXML_VERSION | awk 'BEGIN {FS = "."} {print $1}'`
	MINOR=`echo $LIBXML_VERSION | awk 'BEGIN {FS = "."} {print $2}'`
	MICRO=`echo $LIBXML_VERSION | awk 'BEGIN {FS = "."} {print $3}'`
dnl	if test $MAJOR -lt 2 ; then
dnl		AC_MSG_ERROR([Found libxml2 with major version number = $MAJOR < 2])
dnl	fi
dnl	if test $MINOR -lt 4 ; then
dnl		AC_MSG_ERROR([Found libxml2 with minor version number = $MINOR < 4])
dnl	fi
dnl	if test $MICRO -lt 4 ; then
dnl		AC_MSG_ERROR([Found libxml2 with micro version number = $MICRO < 26])
dnl	fi
	XML2_LIBS="`$LIBXML2_CONFIG --libs`"
	XML2_FLAG="`$LIBXML2_CONFIG --cflags`"
	AC_DEFINE(HAVE_LIBXML2,,[LIBXML2 support])	
fi
AC_SUBST(XML2_LIBS)
AC_SUBST(XML2_FLAG)


dnl ---------------------------------------------
dnl directfb
dnl ---------------------------------------------
dnl Test for directfb

AC_ARG_WITH([directfb], 
            AC_HELP_STRING([--without-directfb],
                           [Use this to disable our DirectFB output layer (enabled by default)]),
            [CHECK_DFB="$withval"], [CHECK_DFB="yes"])
DIRECTFB_CONFIG="no"
if test "$CHECK_DFB" = "yes" ; then
    AC_PATH_PROG(DIRECTFB_CONFIG, directfb-config, no)
    if test "$DIRECTFB_CONFIG" != "no" ; then
      AC_DEFINE([HAVE_DIRECTFB],,[Define this to include DirectFB support])
      CFLAGS="`directfb-config --cflags` $CFLAGS"
      AC_CHECK_DECL(DLBM_TRIPLE, [AC_DEFINE([DIRECTFB_HAS_TRIPLE],,[DirectFB Supports triple buffering])],,[#include <directfb.h>])
      AC_CHECK_DECL(DLCAPS_FIELD_PARITY, [AC_DEFINE([DIRECTFB_HAS_FIELD_PARITY],,[DirectFB Supports setting field parity])],,[#include <directfb.h>])
      AC_CHECK_DECL(DLCAPS_FLICKER_FILTERING, [AC_DEFINE([DIRECTFB_HAS_FLICKER_FILTERING],,[DirectFB Supports setting flicker filtering])],,[#include <directfb.h>])
      DIRECTFB_VERSION_STRING=`directfb-config --version`
      DIRECTFB_MAJOR=`directfb-config --version | awk 'BEGIN {FS = "."} {print $1}'`
      DIRECTFB_MINOR=`directfb-config --version | awk 'BEGIN {FS = "."} {print $2}'`
      DIRECTFB_MICRO=`directfb-config --version | awk 'BEGIN {FS = "."} {print $3}'`
      AC_DEFINE_UNQUOTED(DIRECTFB_VERSION_STRING, "${DIRECTFB_VERSION_STRING}", [DirectFB Version String])
      AC_DEFINE_UNQUOTED(DIRECTFB_MAJOR, ${DIRECTFB_MAJOR}, [DirectFB Major Version])
      AC_DEFINE_UNQUOTED(DIRECTFB_MINOR, ${DIRECTFB_MINOR}, [DirectFB Minor Version])
      AC_DEFINE_UNQUOTED(DIRECTFB_MICRO, ${DIRECTFB_MICRO}, [DirectFB Micro Version])
    fi
fi
AM_CONDITIONAL(HAVE_DIRECTFB, [test "$DIRECTFB_CONFIG" != "no"])

dnl ---------------------------------------------
dnl check for X11, Xv and XF86VidModeExtension
dnl ---------------------------------------------
AC_PATH_XTRA
if test x"$no_x" != x"yes"; then
    dnl check for Xshm
    AC_CHECK_LIB([Xext],[XShmCreateImage],
	[AC_DEFINE([HAVE_X11],,[X11 support])
	X11_CFLAGS="$X_CFLAGS"
	X11_LIBS="$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS -lXext"

	dnl check for Xv
	AC_CHECK_LIB([Xv],[XvShmCreateImage],
	    [AC_DEFINE([HAVE_XV],,[Xv support])
	    X11_LIBS="$X11_LIBS -lXv"],,
	    [$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS -lXext])

	dnl check for Xinerama
	AC_CHECK_LIB([Xinerama],[XineramaQueryScreens],
	    [AC_DEFINE([HAVE_XINERAMA],,[Xinerama support])
	    X11_LIBS="$X11_LIBS -lXinerama"],,
	    [$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS -lXext])

	dnl check for XTest
        AC_CHECK_LIB([Xtst],[XTestFakeKeyEvent],
            [AC_DEFINE([HAVE_XTESTEXTENSION],,[XTest support])
            X11_LIBS="$X11_LIBS -lXtst"],,
	    [$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS -lXext])

	dnl check for Xvidmode
	AC_CHECK_LIB([Xxf86vm],[XF86VidModeGetModeLine],
	    [AC_DEFINE([HAVE_XF86VIDMODE],,[XF86VidModeExtension support])
	    X11_LIBS="$X11_LIBS -lXxf86vm"],,
	    [$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS -lXext])],,
	[$X_PRE_LIBS $X_LIBS -lX11 $X_EXTRA_LIBS])
else
    AC_MSG_ERROR(X needed and no X libraries or development headers found)
fi
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

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

    dnl -std=gnu99
    OPT_CFLAGS="$CFLAGS -std=gnu99"
    AC_TRY_CFLAGS([$OPT_CFLAGS],[CFLAGS=$OPT_CFLAGS])

    dnl arch-specific flags
    case "$host_alias" in
    i?86-* | k?-*)
	AC_DEFINE([ARCH_X86],,[x86 architecture])
        ARCH_X86=yes
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
AM_CONDITIONAL(ARCH_X86, [test "$ARCH_X86" = "yes"])

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
	data/Makefile
	plugins/Makefile
	src/Makefile
	src/wine/Makefile
	intl/Makefile
	po/Makefile.in
	m4/Makefile 
])
AC_OUTPUT
