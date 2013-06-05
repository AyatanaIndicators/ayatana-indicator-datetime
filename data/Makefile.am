#SUBDIRS = icons

gsettings_SCHEMAS = \
	com.canonical.indicator.datetime.gschema.xml
@GSETTINGS_RULES@

dbus_servicesdir = $(DBUSSERVICEDIR)
dbus_services_DATA = indicator-datetime.service

%.service: %.service.in
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	indicator-datetime.service.in

CLEANFILES = indicator-datetime.service

if BUILD_CCPANEL
pkgdata_DATA = datetime-dialog.ui

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_DATA = gnome-indicator-datetime-panel.desktop

EXTRA_DIST += \
	gnome-indicator-datetime-panel.desktop

CLEANFILES += gnome-indicator-datetime-panel.desktop
endif

EXTRA_DIST += \
	datetime-dialog.ui \
	gnome-indicator-datetime-panel.desktop.in
