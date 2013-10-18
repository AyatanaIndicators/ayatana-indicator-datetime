##
##  GSettings schema
##

include (UseGSettings)
set (SCHEMA_NAME "com.canonical.indicator.datetime.gschema.xml")
set (SCHEMA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SCHEMA_NAME}")
add_schema (${SCHEMA_FILE})


##
##  DBus Service File
##

# where to install
set (DBUS_SERVICE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/dbus-1/services")
message (STATUS "${DBUS_SERVICE_DIR} is the DBus Service File install dir")

set (SERVICE_NAME "${CMAKE_PROJECT_NAME}.service")
set (SERVICE_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SERVICE_NAME}")
set (SERVICE_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${SERVICE_NAME}.in")

# build it
set (pkglibexecdir "${CMAKE_INSTALL_FULL_PKGLIBEXECDIR}")
configure_file ("${SERVICE_FILE_IN}" "${SERVICE_FILE}")

# install it
install (FILES "${SERVICE_FILE}"
         DESTINATION "${DBUS_SERVICE_DIR}")


##
##  Unity Indicator File
##

# where to install
set (UNITY_INDICATOR_DIR "${CMAKE_INSTALL_FULL_DATAROOTDIR}/unity/indicators")
message (STATUS "${UNITY_INDICATOR_DIR} is the Unity Indicator install dir")

set (UNITY_INDICATOR_NAME "com.canonical.indicator.datetime")
set (UNITY_INDICATOR_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${UNITY_INDICATOR_NAME}")

install (FILES "${UNITY_INDICATOR_FILE}"
         DESTINATION "${UNITY_INDICATOR_DIR}")


##
##  FIXME: gnome-control-center panel
##

if (BUILD_PANEL)

  install (FILES "datetime-dialog.ui"
           DESTINATION "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}")

  # generate the desktop file using intltool
  set (DESKTOP_NAME "gnome-indicator-datetime-panel.desktop")
  set (DESKTOP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_NAME}")
  set (DESKTOP_FILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/${DESKTOP_NAME}.in")
  set (ENV{LC_ALL} "C")
  execute_process (COMMAND intltool-merge -quiet --desktop-style --utf8 "${CMAKE_SOURCE_DIR}/po" "${DESKTOP_FILE_IN}" "${DESKTOP_FILE}")
  install (FILES ${DESKTOP_FILE}
           DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

endif ()
