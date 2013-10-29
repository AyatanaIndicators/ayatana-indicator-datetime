TESTS =
CLEANFILES =
BUILT_SOURCES =
check_PROGRAMS =

###
###
###

# stock UMB tests on user-visible strings
include $(srcdir)/Makefile.am.strings

check_LIBRARIES = libgtest.a
nodist_libgtest_a_SOURCES = \
  $(GTEST_SOURCE)/gtest-all.cc \
  $(GTEST_SOURCE)/gtest_main.cc

AM_CPPFLAGS = $(GTEST_CPPFLAGS) -I${top_srcdir}/src -Wall -Werror
AM_CXXFLAGS = $(GTEST_CXXFLAGS)

###
###
###

TEST_LIBS = \
        libgtest.a \
        $(SERVICE_LIBS) \
        $(COVERAGE_LDFLAGS) \
        $(XORG_GTEST_LDFLAGS)

TEST_CPPFLAGS = \
        $(AM_CPPFLAGS) \
        $(SERVICE_CFLAGS)

BUILT_SOURCES += gschemas.compiled
CLEANFILES += gschemas.compiled
gschemas.compiled: Makefile
	@glib-compile-schemas --targetdir=$(abs_builddir) $(top_builddir)/data

###
###
###

TESTS += test-indicator
check_PROGRAMS += test-indicator
test_indicator_SOURCES = test-indicator.cc
test_indicator_LDADD = $(TEST_LIBS)
test_indicator_CPPFLAGS = $(TEST_CPPFLAGS) -DSCHEMA_DIR="\"$(top_builddir)/tests/\""

###
###
###

TESTS += test-utils
check_PROGRAMS += test-utils
test_utils_SOURCES = test-utils.cc
test_utils_LDADD = $(top_builddir)/src/libindicator-datetime-service.a $(TEST_LIBS)
test_utils_CPPFLAGS = $(TEST_CPPFLAGS) -DSCHEMA_DIR="\"$(top_builddir)/tests/\""
