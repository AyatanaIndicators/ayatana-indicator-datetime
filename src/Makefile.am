
if BUILD_CCPANEL
ccpaneldir = $(CCPANELDIR)
ccpanel_LTLIBRARIES = libindicator-datetime.la
endif

libexec_PROGRAMS = indicator-datetime-service

indicator_datetime_service_SOURCES = \
	planner.c \
	planner.h \
	planner-eds.c \
	planner-eds.h \
	service.c \
	service.h \
	main.c \
	timezone.c \
	timezone.h \
	timezone-file.c \
	timezone-file.h \
	timezone-geoclue.c \
	timezone-geoclue.h \
	utils.c \
	utils.h \
	dbus-shared.h \
	settings-shared.h

indicator_datetime_service_CFLAGS = \
	-Wall \
        -Wextra -Wno-missing-field-initializers \
	-Werror \
	$(SERVICE_CFLAGS) \
	$(COVERAGE_CFLAGS) \
	-DTIMEZONE_FILE="\"/etc/timezone\"" \
	-DG_LOG_DOMAIN=\"Indicator-Datetime\"
indicator_datetime_service_LDADD = \
	$(SERVICE_LIBS)
indicator_datetime_service_LDFLAGS = \
	$(COVERAGE_LDFLAGS)

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
