AC_INIT([indicator-datetime],
        [12.10.2],
        [http://bugs.launchpad.net/indicator-datetime],
        [indicator-datetime],
        [http://launchpad.net/indicator-datetime])
AC_COPYRIGHT([Copyright 2009-2011 Canonical])

AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([check-news 1.11 -Wall])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_HEADER_STDC
AM_PROG_AR

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_CHECK_LIB([m],[pow])

AC_ARG_ENABLE([deprecations],
  [AS_HELP_STRING([--enable-deprecations],
    [allow deprecated API usage @<:@default=yes@:>@])],
  [],
  [enable_deprecations=yes])
AS_IF([test "x$enable_deprecations" = xno],
  [CFLAGS="$CFLAGS -DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE -DGTK_DISABLE_SINGLE_INCLUDES"]
)

PKG_PROG_PKG_CONFIG

###########################
# Dependencies
###########################

GLIB_REQUIRED_VERSION=2.35.4
GIO_REQUIRED_VERSION=2.25.11
GEOCLUE_REQUIRED_VERSION=0.12.0
ICAL_REQUIRED_VERSION=0.48
ECAL_REQUIRED_VERSION=3.5
EDS_REQUIRED_VERSION=3.5

GTK3_REQUIRED_VERSION=3.1.4

PKG_CHECK_MODULES(SERVICE, glib-2.0 >= $GLIB_REQUIRED_VERSION
                           gio-2.0 >= $GIO_REQUIRED_VERSION
                           geoclue >= $GEOCLUE_REQUIRED_VERSION
                           libical >= $ICAL_REQUIRED_VERSION
                           libecal-1.2 >= $ECAL_REQUIRED_VERSION
                           libedataserver-1.2 >= EDS_REQUIRED_VERSION)

###########################
# Control Center panel
###########################

AC_ARG_WITH([ccpanel],
            AS_HELP_STRING([--with-ccpanel], [enable Control Center panel]),,
            with_ccpanel=auto)

if test x"$with_ccpanel" != x"no" ; then
   PKG_CHECK_MODULES([PREF],
                     [gio-2.0 >= $GIO_REQUIRED_VERSION
                      gtk+-3.0 >= $GTK3_REQUIRED_VERSION
                      timezonemap
                      libgnome-control-center
                      polkit-gobject-1],
                     [have_ccpanel=yes],
                     [have_ccpanel=no])
   if test x${have_ccpanel} = xyes; then
      AC_DEFINE(HAVE_CCPANEL, 1, [Define to 1 to enable Control Center panel])
      PKG_CHECK_MODULES(LIBMAP, gio-2.0 >= $GIO_REQUIRED_VERSION
                                gtk+-3.0 >= $GTK3_REQUIRED_VERSION)
   fi
   if test x${with_ccpanel} = xyes && test x${have_ccpanel} = xno; then
     AC_MSG_ERROR([Control Center panel configured but not found])
   fi
else
   have_ccpanel=no
fi
AM_CONDITIONAL(BUILD_CCPANEL, test x${have_ccpanel} = xyes)

###########################
# Grab the GSettings Macros
###########################

GLIB_GSETTINGS

###########################
# gcov coverage reporting
###########################

m4_include([m4/gcov.m4])
AC_TDD_GCOV
AM_CONDITIONAL([HAVE_GCOV], [test "x$ac_cv_check_gcov" = xyes])
AM_CONDITIONAL([HAVE_LCOV], [test "x$ac_cv_check_lcov" = xyes])
AM_CONDITIONAL([HAVE_GCOVR], [test "x$ac_cv_check_gcovr" = xyes])
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_LDFLAGS)

###########################
# Check to see if we're local
###########################

with_localinstall="no"
AC_ARG_ENABLE(localinstall, AS_HELP_STRING([--enable-localinstall], [install all of the files localy instead of system directories (for distcheck)]), with_localinstall=$enableval, with_localinstall=no)

###########################
# Control Center Info
###########################

AS_IF([test "x$have_ccpanel" = "xyes"],
	[
	AS_IF([test "x$with_localinstall" = "xyes"],
		[
		CCPANELDIR="${libdir}/control-center-1/panels/"
		],
		[
		CCPANELDIR=`$PKG_CONFIG --variable=extensiondir libgnome-control-center`
		])

	AC_SUBST(CCPANELDIR)
	])
###########################
# DBus Service Info
###########################

if test "x$with_localinstall" = "xyes"; then
	DBUSSERVICEDIR="${datadir}/dbus-1/services/"
else
	DBUSSERVICEDIR=`$PKG_CONFIG --variable=session_bus_services_dir dbus-1`
fi
AC_SUBST(DBUSSERVICEDIR)

###########################
# Google Test framework
###########################

AC_ARG_ENABLE([tests],
    [AS_HELP_STRING([--disable-tests], [Disable test scripts and tools (default=auto)])],
    [enable_tests=${enableval}],
    [enable_tests=auto])
if test "x$enable_tests" != "xno"; then
    m4_include([m4/gtest.m4])
    CHECK_GTEST
    CHECK_XORG_GTEST
    if test "x$enable_tests" = "xauto"; then
        enable_tests=${have_gtest}
    elif test "x$enable_tests" = "xyes" && test "x$have_gtest" != "xyes"; then
        AC_MSG_ERROR([tests were requested but gtest is not installed.])
    fi
fi
AM_CONDITIONAL([BUILD_TESTS],[test "x$enable_tests" = "xyes"])

##############################
# Custom Junk
##############################

AC_DEFUN([AC_DEFINE_PATH], [
	test "x$prefix" = xNONE && prefix="$ac_default_prefix"
	test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
	ac_define_path=`eval echo [$]$2`
	ac_define_path=`eval echo [$]ac_define_path`
	$1="$ac_define_path"
	AC_SUBST($1)
	ifelse($3, ,
		AC_DEFINE_UNQUOTED($1, "$ac_define_path"),
		AC_DEFINE_UNQUOTED($1, "$ac_define_path", $3))
])

###########################
# Internationalization
###########################
IT_PROG_INTLTOOL([0.41.0])

GETTEXT_PACKAGE=indicator-datetime
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the default get text domain])
AC_DEFINE_PATH(GNOMELOCALEDIR, "${datadir}/locale", [locale directory])

AM_GLIB_GNU_GETTEXT

###########################
# Files
###########################

AC_CONFIG_FILES([
Makefile
src/Makefile
data/Makefile
tests/Makefile
po/Makefile.in
])
AC_OUTPUT

###########################
# Results
###########################

AC_MSG_NOTICE([

Date and Time Indicator Configuration:

	Prefix:        $prefix
	CC Panel:      $have_ccpanel
	CC Panel Dir:  $CCPANELDIR
	Unit Tests:    $enable_tests
	gcov:          $use_gcov
])
