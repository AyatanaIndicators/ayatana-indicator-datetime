#SUBDIRS = icons

gsettings_SCHEMAS = \
	org.ayatana.indicator.datetime.gschema.xml
@GSETTINGS_RULES@

dbus_servicesdir = $(DBUSSERVICEDIR)
dbus_services_DATA = indicator-datetime.service

%.service: %.service.in
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	indicator-datetime.service.in

CLEANFILES = indicator-datetime.service
