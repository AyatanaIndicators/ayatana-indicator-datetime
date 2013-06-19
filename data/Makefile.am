BUILT_SOURCES=
CLEANFILES=
EXTRA_DIST=

#
# the indicator bus file
#

indicatorsdir = $(prefix)/share/unity/indicators
indicators_DATA = com.canonical.indicator.datetime

#
# the gsettings
#

gsettings_SCHEMAS = com.canonical.indicator.datetime.gschema.xml
@GSETTINGS_RULES@
EXTRA_DIST += $(gsettings_SCHEMAS)

#
# the dbus service file
#

dbus_servicesdir = $(DBUSSERVICEDIR)
dbus_services_DATA = indicator-datetime.service
dbus_services_in = $(dbus_services_DATA:.service=.service.in)
$(dbus_services_DATA): $(dbus_services_in)
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@
BUILT_SOURCES += $(dbus_services_DATA)
CLEANFILES += $(dbus_services_DATA)
EXTRA_DIST += $(dbus_services_in)

#
# the gnome-control-center panel
#

if BUILD_CCPANEL
 pkgdata_DATA = datetime-dialog.ui
 @INTLTOOL_DESKTOP_RULE@
 desktopdir = $(datadir)/applications
 desktop_DATA = gnome-indicator-datetime-panel.desktop
 EXTRA_DIST += $(desktop_DATA)
 CLEANFILES += $(desktop_DATA)
endif
EXTRA_DIST += $(desktop_DATA:.desktop=.desktop.in)
