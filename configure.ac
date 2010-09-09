
AC_INIT(indicator-datetime, 0.0.6, ted@canonical.com)
AC_COPYRIGHT([Copyright 2009,2010 Canonical])

AC_PREREQ(2.53)

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(indicator-datetime, 0.0.6)

AM_MAINTAINER_MODE

IT_PROG_INTLTOOL([0.35.0])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_C_O
AC_STDC_HEADERS
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_SUBST(VERSION)
AC_CONFIG_MACRO_DIR([m4])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

PKG_PROG_PKG_CONFIG

###########################
# Dependencies
###########################

INDICATOR_REQUIRED_VERSION=0.3.0
DBUSMENUGLIB_REQUIRED_VERSION=0.1.1
DBUSMENUGTK_REQUIRED_VERSION=0.1.1
GIO_REQUIRED_VERSION=2.25.11
# Note: the GIO check below also ensures the proper glib with gsettings support is present
INDICATOR_DISPLAY_OBJECTS=0.1.10

PKG_CHECK_MODULES(INDICATOR, indicator >= $INDICATOR_REQUIRED_VERSION
                             dbusmenu-glib >= $DBUSMENUGLIB_REQUIRED_VERSION
                             dbusmenu-gtk >= $DBUSMENUGTK_REQUIRED_VERSION
                             libido-0.1 >= $INDICATOR_DISPLAY_OBJECTS
                             gio-2.0 >= $GIO_REQUIRED_VERSION)

AC_SUBST(INDICATOR_CFLAGS)
AC_SUBST(INDICATOR_LIBS)

###########################
# Grab the GSettings Macros
###########################

GLIB_GSETTINGS

###########################
# Check to see if we're local
###########################

with_localinstall="no"
AC_ARG_ENABLE(localinstall, AS_HELP_STRING([--enable-localinstall], [install all of the files localy instead of system directories (for distcheck)]), with_localinstall=$enableval, with_localinstall=no)

###########################
# Indicator Info
###########################

if test "x$with_localinstall" = "xyes"; then
	INDICATORDIR="${libdir}/indicators/2/"
	INDICATORICONSDIR="${datadir}/libindicate/icons/"
else
	INDICATORDIR=`$PKG_CONFIG --variable=indicatordir indicator`
	INDICATORICONSDIR=`$PKG_CONFIG --variable=iconsdir indicator`
fi
AC_SUBST(INDICATORDIR)
AC_SUBST(INDICATORICONSDIR)

###########################
# DBus Service Info
###########################

if test "x$with_localinstall" = "xyes"; then
	DBUSSERVICEDIR="${datadir}/dbus-1/services/"
else
	DBUSSERVICEDIR=`$PKG_CONFIG --variable=session_bus_services_dir dbus-1`
fi
AC_SUBST(DBUSSERVICEDIR)

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

GETTEXT_PACKAGE=indicator-datetime
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of the default get text domain])
AC_DEFINE_PATH(GNOMELOCALEDIR, "${datadir}/locale", [locale directory])

AM_GLIB_GNU_GETTEXT

###########################
# Files
###########################

AC_OUTPUT([
Makefile
src/Makefile
data/Makefile
tests/Makefile
po/Makefile.in
])

###########################
# Results
###########################

AC_MSG_NOTICE([

Date and Time Indicator Configuration:

	Prefix:        $prefix
	Indicator Dir: $INDICATORDIR
])
