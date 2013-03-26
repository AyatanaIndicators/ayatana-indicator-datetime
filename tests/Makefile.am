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
        $(top_builddir)/src/.libs/libdatetime.so \
        $(INDICATOR_LIBS) \
        $(COVERAGE_LDFLAGS) \
        $(XORG_GTEST_LDFLAGS) \
        libgtest.a

TEST_CPPFLAGS = \
        $(AM_CPPFLAGS) \
        $(INDICATOR_CFLAGS)

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
