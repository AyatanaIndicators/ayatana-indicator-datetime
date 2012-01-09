AC_INIT([indicator-datetime],
        [0.3.1],
        [http://bugs.launchpad.net/indicator-datetime],
        [indicator-datetime],
        [http://launchpad.net/indicator-datetime])
AC_COPYRIGHT([Copyright 2009-2011 Canonical])

AC_PREREQ([2.64])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 -Wall dist-xz])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

# Check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_HEADER_STDC

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

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

PKG_PROG_PKG_CONFIG

###########################
# GTK+ version option
###########################

AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk],
    [Which version of gtk to use for the indicator @<:@default=3@:>@])],
  [],
  [with_gtk=3])

###########################
# Dependencies
###########################

INDICATOR_REQUIRED_VERSION=0.3.19
DBUSMENUGLIB_REQUIRED_VERSION=0.1.1
DBUSMENUGTK_REQUIRED_VERSION=0.3.94
GIO_REQUIRED_VERSION=2.25.11
INDICATOR_DISPLAY_OBJECTS=0.2.2
GEOCLUE_REQUIRED_VERSION=0.12.0
ECAL_REQUIRED_VERSION=2.30
EDS_REQUIRED_VERSION=2.30
ICAL_REQUIRED_VERSION=0.44
CAIRO_REQUIRED_VERSION=1.10
GDK_REQUIRED_VERSION=2.22
GLIB_REQUIRED_VERSION=2.29.19
GTK3_REQUIRED_VERSION=3.1.4
GCONF_REQUIRED_VERSION=2.31

AS_IF([test "x$with_gtk" = x3],
    [PKG_CHECK_MODULES(INDICATOR, indicator3-0.4 >= $INDICATOR_REQUIRED_VERSION
                                  glib-2.0 >= $GLIB_REQUIRED_VERSION
                                  dbusmenu-glib-0.4 >= $DBUSMENUGLIB_REQUIRED_VERSION
                                  dbusmenu-gtk3-0.4 >= $DBUSMENUGTK_REQUIRED_VERSION
                                  libido3-0.1 >= $INDICATOR_DISPLAY_OBJECTS)
        ],
      [test "x$with_gtk" = x2],
    [PKG_CHECK_MODULES(INDICATOR, indicator-0.4 >= $INDICATOR_REQUIRED_VERSION
                                  glib-2.0 >= $GLIB_REQUIRED_VERSION
                                  dbusmenu-glib-0.4 >= $DBUSMENUGLIB_REQUIRED_VERSION
                                  dbusmenu-gtk-0.4 >= $DBUSMENUGTK_REQUIRED_VERSION
                                  libido-0.1 >= $INDICATOR_DISPLAY_OBJECTS)
        ],
    [AC_MSG_FAILURE([Value for --with-gtk was neither 2 nor 3])]
)

PKG_CHECK_MODULES(SERVICE, indicator3-0.4 >= $INDICATOR_REQUIRED_VERSION
                           glib-2.0 >= $GLIB_REQUIRED_VERSION
                           dbusmenu-glib-0.4 >= $DBUSMENUGLIB_REQUIRED_VERSION
                           dbusmenu-gtk3-0.4 >= $DBUSMENUGTK_REQUIRED_VERSION
                           libido3-0.1 >= $INDICATOR_DISPLAY_OBJECTS
                           gio-2.0 >= $GIO_REQUIRED_VERSION
                           geoclue >= $GEOCLUE_REQUIRED_VERSION
                           libecal-1.2 >= $ECAL_REQUIRED_VERSION
                           libical >= $ICAL_REQUIRED_VERSION
                           libedataserver-1.2 >= EDS_REQUIRED_VERSION
                           libedataserverui-3.0 >= EDS_REQUIRED_VERSION
                           cairo >= CAIRO_REQUIRED_VERSION
                           gdk-3.0 >= GDK_REQUIRED_VERSION
                           gconf-2.0 >= GCONF_REQUIRED_VERSION)

PKG_CHECK_MODULES(PREF, gio-2.0 >= $GIO_REQUIRED_VERSION
	                      gtk+-3.0 >= $GTK3_REQUIRED_VERSION
	                      timezonemap
	                      libgnome-control-center
	                      polkit-gobject-1)

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

AS_IF([test "x$with_localinstall" = "xyes"],
	[
	INDICATORDIR="${libdir}/indicators/2/"
	INDICATORICONSDIR="${datadir}/libindicate/icons/"
	],
	[AS_IF([test "x$with_gtk" = "x2"],
		[
		INDICATORDIR=`$PKG_CONFIG --variable=indicatordir indicator-0.4`
		INDICATORICONSDIR=`$PKG_CONFIG --variable=iconsdir indicator-0.4`
		],
		[
		INDICATORDIR=`$PKG_CONFIG --variable=indicatordir indicator3-0.4`
		INDICATORICONSDIR=`$PKG_CONFIG --variable=iconsdir indicator3-0.4`
		])])

AC_SUBST(INDICATORDIR)
AC_SUBST(INDICATORICONSDIR)

###########################
# Control Center Info
###########################

AS_IF([test "x$with_localinstall" = "xyes"],
	[
	CCPANELDIR="${libdir}/control-center-1/panels/"
	],
	[
	CCPANELDIR=`$PKG_CONFIG --variable=extensiondir libgnome-control-center`
	])

AC_SUBST(CCPANELDIR)

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
	Indicator Dir: $INDICATORDIR
	CC Panel Dir:  $CCPANELDIR
])
