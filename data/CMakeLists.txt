##
## indicator variant
##
if (ENABLE_LOMIRI_FEATURES)
    set (INDICATOR_VARIANT_NAME "lomiri-indicator-datetime")
else ()
    set (INDICATOR_VARIANT_NAME "${CMAKE_PROJECT_NAME}")
endif ()

##
##  GSettings schema
##

set (SCHEMA_NAME "org.ayatana.indicator.datetime.gschema.xml")
set (SCHEMA_FILE_IN    "${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA_NAME}.in")
set (SCHEMA_FILE       "${CMAKE_CURRENT_BINARY_DIR}/${SCHEMA_NAME}")
# generate the .xml.in file so cmake constants are included
configure_file(${SCHEMA_FILE_IN} ${SCHEMA_FILE})

# let GSettings do the rest
find_package(GSettings REQUIRED)
add_schema (${SCHEMA_NAME})

##
##  Systemd Unit File
##

pkg_check_modules(SYSTEMD systemd)
if (${SYSTEMD_FOUND})

    pkg_get_variable(SYSTEMD_USER_DIR systemd systemduserunitdir)
    message (STATUS "${SYSTEMD_USER_DIR} is the systemd user unit file install dir")

    set (SYSTEMD_USER_NAME "${INDICATOR_VARIANT_NAME}.service")
    set (SYSTEMD_USER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SYSTEMD_USER_NAME}")
    set (SYSTEMD_USER_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${SYSTEMD_USER_NAME}.in")

    # build it
    configure_file ("${SYSTEMD_USER_FILE_IN}" "${SYSTEMD_USER_FILE}")

    # install it
    install (FILES "${SYSTEMD_USER_FILE}"
             DESTINATION "${SYSTEMD_USER_DIR}")
endif()

##
##  XDG Autostart File
##

if (NOT ENABLE_LOMIRI_FEATURES)
    # where to install
    set (XDG_AUTOSTART_DIR "/etc/xdg/autostart")
    message (STATUS "${XDG_AUTOSTART_DIR} is the DBus Service File install dir")

    set (XDG_AUTOSTART_NAME "${CMAKE_PROJECT_NAME}.desktop")
    set (XDG_AUTOSTART_FILE "${CMAKE_CURRENT_BINARY_DIR}/${XDG_AUTOSTART_NAME}")
    set (XDG_AUTOSTART_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${XDG_AUTOSTART_NAME}.in")

    # build it
    configure_file ("${XDG_AUTOSTART_FILE_IN}" "${XDG_AUTOSTART_FILE}")

    # install XDG autostart
    install (FILES "${XDG_AUTOSTART_FILE}"
             DESTINATION "${XDG_AUTOSTART_DIR}")
endif()

##
##  Ayatana Indicator File
##

# where to install
set (AYATANA_INDICATOR_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/ayatana/indicators")
message (STATUS "${AYATANA_INDICATOR_DIR} is the Ayatana Indicator install dir")

set (AYATANA_INDICATOR_NAME "org.ayatana.indicator.datetime")
set (AYATANA_INDICATOR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${AYATANA_INDICATOR_NAME}")

install (FILES "${AYATANA_INDICATOR_FILE}"
         DESTINATION "${AYATANA_INDICATOR_DIR}")
