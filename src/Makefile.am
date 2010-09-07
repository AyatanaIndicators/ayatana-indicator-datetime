
libexec_PROGRAMS = indicator-datetime-service

indicator_datetime_service_SOURCES = \
	calendar-menu-item.c \
	calendar-menu-item.h \
	datetime-service.c \
	dbus-shared.h
indicator_datetime_service_CFLAGS = \
	-Wall \
	-Werror \
	$(INDICATOR_CFLAGS)
indicator_datetime_service_LDADD = \
	$(INDICATOR_LIBS)

datetimelibdir = $(INDICATORDIR)
datetimelib_LTLIBRARIES = libdatetime.la
libdatetime_la_SOURCES = \
	dbus-shared.h \
	indicator-datetime.c
libdatetime_la_CFLAGS = \
	$(INDICATOR_CFLAGS) \
	-Wall -Werror
libdatetime_la_LIBADD = \
	$(INDICATOR_LIBS)
libdatetime_la_LDFLAGS = \
	-module \
	-avoid-version

EXTRA_DIST = $(libdatetime_la_SOURCES)
