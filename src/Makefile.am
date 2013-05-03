
if BUILD_CCPANEL
ccpaneldir = $(CCPANELDIR)
ccpanel_LTLIBRARIES = libindicator-datetime.la
endif

libexec_PROGRAMS = indicator-datetime-service

indicator_datetime_service_SOURCES = \
	datetime-interface.c \
	datetime-interface.h \
	gen-datetime-service.xml.c \
	datetime-service.c \
	location.c \
	location.h \
	location-geoclue.c \
	location-geoclue.h \
	utils.c \
	utils.h \
	dbus-shared.h \
	settings-shared.h
indicator_datetime_service_CFLAGS = \
	-Wall \
	-Werror \
	$(SERVICE_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-DTIMEZONE_FILE="\"/etc/timezone\"" \
	-DG_LOG_DOMAIN=\"Indicator-Datetime\"
indicator_datetime_service_LDADD = \
	$(SERVICE_LIBS)
indicator_datetime_service_LDFLAGS = \
	$(COVERAGE_LDFLAGS)

datetimelibdir = $(INDICATORDIR)
datetimelib_LTLIBRARIES = libdatetime.la
libdatetime_la_SOURCES = \
	gen-datetime-service.xml.h \
	dbus-shared.h \
	settings-shared.h \
	utils.c \
	utils.h \
	indicator-datetime.c
libdatetime_la_CFLAGS = \
	$(INDICATOR_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-Wall -Werror \
	-DTIMEZONE_FILE="\"/etc/timezone\"" \
	-DG_LOG_DOMAIN=\"Indicator-Datetime\"
libdatetime_la_LIBADD = \
	$(INDICATOR_LIBS)
libdatetime_la_LDFLAGS = \
	$(COVERAGE_LDFLAGS) \
	-module \
	-avoid-version

if BUILD_CCPANEL
libindicator_datetime_la_SOURCES =\
	datetime-prefs.c \
	datetime-prefs-locations.c \
	datetime-prefs-locations.h \
	utils.c \
	utils.h \
	settings-shared.h
libindicator_datetime_la_CFLAGS = \
	-Wall \
	-Werror \
	$(PREF_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-DTIMEZONE_FILE="\"/etc/timezone\"" \
	-DPKGDATADIR="\"$(pkgdatadir)\""
libindicator_datetime_la_LIBADD = \
	$(PREF_LIBS)
libindicator_datetime_la_LDFLAGS = \
	$(COVERAGE_LDFLAGS) \
	-module -avoid-version
endif

gen-%.xml.c: %.xml
	@echo "Building $@ from $<"
	@echo "const char * _$(subst -,_,$(subst .,_,$(basename $(notdir $<)))) = " > $@
	@sed -e "s:\":\\\\\":g" -e s:^:\": -e s:\$$:\\\\n\": $< >> $@
	@echo ";" >> $@

gen-%.xml.h: %.xml
	@echo "Building $@ from $<"
	@echo "extern const char * _$(subst -,_,$(subst .,_,$(basename $(notdir $<))));" > $@

BUILT_SOURCES = \
	gen-datetime-service.xml.c \
	gen-datetime-service.xml.h


CLEANFILES = \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	datetime-service.xml
