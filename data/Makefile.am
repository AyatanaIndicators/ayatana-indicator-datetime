#SUBDIRS = icons

gsettings_SCHEMAS = \
	com.canonical.indicator.datetime.gschema.xml
@GSETTINGS_RULES@

dbus_servicesdir = $(DBUSSERVICEDIR)
dbus_services_DATA = indicator-datetime.service

%.service: %.service.in
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

pkgdata_DATA = datetime-dialog.ui

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	datetime-dialog.ui \
	indicator-datetime.service.in

CLEANFILES = indicator-datetime.service
