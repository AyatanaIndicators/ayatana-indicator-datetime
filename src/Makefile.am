
libexec_PROGRAMS = indicator-datetime-service

indicator_datetime_service_SOURCES = \
	datetime-interface.c \
	datetime-interface.h \
	datetime-service-server.h \
	calendar-menu-item.c \
	calendar-menu-item.h \
	datetime-service.c \
	dbus-shared.h
indicator_datetime_service_CFLAGS = \
	-Wall \
	-Werror \
	$(SERVICE_CFLAGS) \
	-DTIMEZONE_FILE="\"/etc/timezone\""
indicator_datetime_service_LDADD = \
	$(SERVICE_LIBS)

datetimelibdir = $(INDICATORDIR)
datetimelib_LTLIBRARIES = libdatetime.la
libdatetime_la_SOURCES = \
	datetime-service-client.h \
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

datetime-service-client.h: $(srcdir)/datetime-service.xml
	dbus-binding-tool \
		--prefix=_datetime_service_client \
		--mode=glib-client \
		--output=datetime-service-client.h \
		$(srcdir)/datetime-service.xml

datetime-service-server.h: $(srcdir)/datetime-service.xml
	dbus-binding-tool \
		--prefix=_datetime_service_server \
		--mode=glib-server \
		--output=datetime-service-server.h \
		$(srcdir)/datetime-service.xml

BUILT_SOURCES = \
	datetime-service-client.h \
	datetime-service-server.h

CLEANFILES = \
	$(BUILT_SOURCES)

EXTRA_DIST = \
	datetime-service.xml
