BUILT_SOURCES=
CLEANFILES=
EXTRA_DIST=

#
# the indicator bus file
#

indicatorsdir = $(prefix)/share/unity/indicators
dist_indicators_DATA = com.canonical.indicator.datetime

#
# the gsettings
#

gsettings_SCHEMAS = com.canonical.indicator.datetime.gschema.xml
@GSETTINGS_RULES@
EXTRA_DIST += $(gsettings_SCHEMAS)

#
# the upstart job file
#

upstart_jobsdir = $(datadir)/upstart/sessions
upstart_jobs_DATA = indicator-datetime.conf
upstart_jobs_in = $(upstart_jobs_DATA:.conf=.conf.in)
$(upstart_jobs_DATA): $(upstart_jobs_in)
	$(AM_V_GEN) $(SED) -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@
BUILT_SOURCES += $(upstart_jobs_DATA)
CLEANFILES += $(upstart_jobs_DATA)
EXTRA_DIST += $(upstart_jobs_in)

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
EXTRA_DIST += datetime-dialog.ui $(desktop_DATA:.desktop=.desktop.in)
